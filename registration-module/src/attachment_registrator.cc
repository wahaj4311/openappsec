// Copyright (C) 2022 Check Point Software Technologies Ltd. All rights reserved.

// Licensed under the Apache License, Version 2.0 (the "License");
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <nlohmann/json.hpp>
using json = nlohmann::json;

#include "attachment_registrator.h"
#include "i_config.h"
#include "custom_server_config.h"

#include "debug.h"
USE_DEBUG_FLAG(D_ATTACHMENT_REGISTRATION);

#include <iostream>
#include <fstream>
#include <map>
#include <sstream>
#include <fstream>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/un.h>
#include <climits>
#include <unordered_map>
#include <unistd.h>
#include <utility>
#include <limits>
#include <chrono>

using namespace std::chrono;

#include "common.h"
#include "config.h"
#include "singleton.h"
#include "i_mainloop.h"
#include "buffer.h"
#include "enum_array.h"
#include "attachments/nginx_attachment_common.h"
#include "i_socket_is.h"
#include "i_shell_cmd.h"
#include "maybe_res.h"
#include "attachment_types.h"
#include "component.h"
#include "i_messaging.h"
#include "i_encryptor.h"
#include "enum_range.h"

using namespace std;

#ifndef SHARED_KEEP_ALIVE_PATH
#define SHARED_KEEP_ALIVE_PATH "/dev/shm/check-point/cp-nano-keep-alive"
#endif

static const AlertInfo alert(AlertTeam::CORE, "attachment registrator");

class AttachmentRegistrator::Impl
{
public:
    void
    init()
    {
        auto config_api = Singleton::Consume<I_Config>::by<AttachmentRegistrator>();

        i_socket = Singleton::Consume<I_Socket>::by<AttachmentRegistrator>();
        Singleton::Consume<I_MainLoop>::by<AttachmentRegistrator>()->addOneTimeRoutine(
            I_MainLoop::RoutineType::System,
            [this] ()
            {
                while(!initSocket()) {
                    Singleton::Consume<I_MainLoop>::by<AttachmentRegistrator>()->yield(seconds(1));
                }
            },
            "Initialize attachment registration IPC"
        );

        uint expiration_timeout = config_api->getProfileAgentSettingWithDefault<uint>(
            300,
            "attachmentRegistrator.expirationCheckSeconds"
        );
        Singleton::Consume<I_MainLoop>::by<AttachmentRegistrator>()->addRecurringRoutine(
            I_MainLoop::RoutineType::Timer,
            seconds(expiration_timeout),
            [this] () { handleExpiration(); },
            "Attachment's expiration handler",
            true
        );
    }

    void
    fini()
    {
        if (server_sock > 0) {
            i_socket->closeSocket(server_sock);
            server_sock = -1;
        }

        if (shared_registration_path != "") unlink(shared_registration_path.c_str());
    }

private:
    bool
    registerAttachmentProcess(
        const uint8_t &uid,
        const string &family_id,
        const uint8_t num_of_members,
        const AttachmentType type)
    {
        registered_attachments[family_id] = vector<bool>(num_of_members, true);

        const int cmd_tmout = 900;
        I_ShellCmd *shell_cmd = Singleton::Consume<I_ShellCmd>::by<AttachmentRegistrator>();
        Maybe<string, string> registration_res = shell_cmd->getExecOutput(
            genRegCommand(family_id, num_of_members, type),
            cmd_tmout
        );
        if (!registration_res.ok()) {
            dbgWarning(D_ATTACHMENT_REGISTRATION)
                << "Failed to register attachment."
                << "Error: " << registration_res.getError()
                << ", Attachment Type: " << static_cast<int>(type)
                << ", Attachment id: " << static_cast<int>(uid)
                <<", Family id: " << family_id
                << ", Total number of instances: " << static_cast<int>(num_of_members);
            return false;
        }
        return true;
    }

    void
    replyWithRelevantHandler(
        I_Socket::socketFd socket,
        const uint8_t &uid,
        const string &family_id,
        const AttachmentType type)
    {
        string handler_path = genHandlerPath(uid, family_id, type);

        if (handler_path.size() > std::numeric_limits<uint8_t>::max()) {
             dbgWarning(D_ATTACHMENT_REGISTRATION) << "Handler path too long: " << handler_path;
             return;
        }
        uint8_t path_size = static_cast<uint8_t>(handler_path.size());
        vector<char> path_size_data(reinterpret_cast<char *>(&path_size), reinterpret_cast<char *>(&path_size) + 1);
        if (!i_socket->writeData(socket, path_size_data)) {
            dbgWarning(D_ATTACHMENT_REGISTRATION) << "Failed to send handler path size to attachment";
            return;
        }

        dbgDebug(D_ATTACHMENT_REGISTRATION)
            << "Successfully sent handler path size to attachment. Size: "
            << static_cast<int>(path_size);

        vector<char> path_data(handler_path.data(), handler_path.data() + handler_path.size());
        if (!i_socket->writeData(socket, path_data)) {
            dbgWarning(D_ATTACHMENT_REGISTRATION)
                << "Failed to send handler path data to attachment. Path: "
                << handler_path;
            return;
        }

        dbgDebug(D_ATTACHMENT_REGISTRATION)
            << "Successfully sent handler path data to attachment. Path: "
            << handler_path;
    }

    string
    genHandlerPath(const uint8_t &uid, const string &family_id, const AttachmentType type) const
    {
        static const string handler_path_format = "/dev/shm/check-point/cp-nano-";
        stringstream handler_path;
        handler_path << handler_path_format;
        switch(type) {
            case (AttachmentType::SQUID_ATT_ID): {
                handler_path << "squid-http-transaction-handler-";
                break;
            }
            case (AttachmentType::NGINX_ATT_ID): {
                handler_path << "http-transaction-handler-";
                break;
            }
            default:
                {
                    assert(false && "Unsupported Attachment");
                    break;
                }
        }

        if (!family_id.empty()) handler_path << family_id << "_";
        handler_path << static_cast<int>(uid);

        return handler_path.str();
    }

    string
    genRegCommand(const string &family_id, const uint8_t num_of_members, const AttachmentType type) const
    {
        assert(num_of_members > 0);

        static const string registration_format = "/etc/cp/watchdog/cp-nano-watchdog --register ";
        stringstream registration_command;
        registration_command<< registration_format;
        switch(type) {
            case (AttachmentType::SQUID_ATT_ID):
            case (AttachmentType::NGINX_ATT_ID):{
                registration_command << "/etc/cp/HttpTransactionHandler/cp-nano-http-transaction-handler";
                break;
            }
            default:
                {
                    assert(false && "Unsupported Attachment");
                    break;
                }
        }

        if (!family_id.empty()) registration_command << " --family " << family_id;
        registration_command << " --count " << num_of_members;

        return registration_command.str();
    }

    bool
    initSocket()
    {
        auto config_api = Singleton::Consume<I_Config>::by<AttachmentRegistrator>();

        shared_registration_path = config_api->getConfigurationWithDefault<string>(
            "/dev/shm/check-point/cp-nano-attachment-registration",
            "Attachment Registration",
            "Registration IPC Path"
        );

        size_t last_slash_idx = shared_registration_path.find_last_of("/");
        if (last_slash_idx != string::npos) {
            string directory_path = shared_registration_path.substr(0, last_slash_idx);
            mkdir(directory_path.c_str(), 0777);
        }

        if (server_sock < 0) {
            server_sock = getNewSocket(shared_registration_path);
            if (server_sock < 0) {
                dbgWarning(D_ATTACHMENT_REGISTRATION)
                    << "Failed to create server socket. Path: "
                    << shared_registration_path;
                return false;
            }

            Singleton::Consume<I_MainLoop>::by<AttachmentRegistrator>()->addFileRoutine(
                I_MainLoop::RoutineType::RealTime,
                server_sock,
                [this] () { handleAttachmentRegistration(); },
                "Attachment's registration handler",
                true
            );
        }

        string shared_expiration_path = config_api->getConfigurationWithDefault<string>(
            SHARED_KEEP_ALIVE_PATH,
            "Attachment Registration",
            "Keep Alive IPC Path"
        );

        last_slash_idx = shared_expiration_path.find_last_of("/");
        if (last_slash_idx != string::npos) {
            string keep_alive_dir_path = shared_expiration_path.substr(0, last_slash_idx);
             mkdir(keep_alive_dir_path.c_str(), 0777);
        }

        if (keep_alive_sock < 0) {
            keep_alive_sock = getNewSocket(shared_expiration_path);
            if (keep_alive_sock < 0) {
                dbgWarning(D_ATTACHMENT_REGISTRATION) << "Failed to create keep-alive socket. Path: " << shared_expiration_path;
                return false;
            }

            Singleton::Consume<I_MainLoop>::by<AttachmentRegistrator>()->addFileRoutine(
                I_MainLoop::RoutineType::System,
                keep_alive_sock,
                [this] () { handleKeepAlives(); },
                "Attachment keep alive registration",
                true
            );
        }
        return true;
    }

    I_Socket::socketFd
    getNewSocket(const string &path)
    {
        Maybe<I_Socket::socketFd> new_socket = i_socket->genSocket(
            I_Socket::SocketType::UNIX,
            false,
            true,
            path
        );
        if (!new_socket.ok()) {
            dbgWarning(D_ATTACHMENT_REGISTRATION) << "Failed to open a socket. Error: " << new_socket.getErr();
            return -1;
        }

        assert(new_socket.unpack() > 0);
        dbgTrace(D_ATTACHMENT_REGISTRATION) << "Generated socket. FD: " << new_socket.unpack();
        return new_socket.unpack();
    }

    void
    handleKeepAlives()
    {
        Maybe<I_Socket::socketFd> accepted_socket = i_socket->acceptSocket(keep_alive_sock, false);
        if (!accepted_socket.ok()) {
            dbgWarning(D_ATTACHMENT_REGISTRATION)
                << "Failed to accept new keep-alive request socket: "
                << accepted_socket.getErr();
            return;
        }

        I_Socket::socketFd client_socket = accepted_socket.unpack();
        assert(client_socket > 0);
        dbgTrace(D_ATTACHMENT_REGISTRATION) << "Accepted keep-alive socket. FD: " << client_socket;
        auto close_socket_on_exit = make_scope_exit([&]() { if(client_socket > 0) i_socket->closeSocket(client_socket); });

        Maybe<uint8_t> attachment_id_maybe = readNumericParam(client_socket);
        if (!attachment_id_maybe.ok()) {
            dbgWarning(D_ATTACHMENT_REGISTRATION) << "Failed to read attachment ID for keep-alive: " << attachment_id_maybe.getErr();
            return;
        }
        uint8_t attachment_id = attachment_id_maybe.unpack();

        Maybe<string> family_id_maybe = readStringParam(client_socket);
        if (!family_id_maybe.ok()) {
            dbgWarning(D_ATTACHMENT_REGISTRATION) << "Failed to read family ID for keep-alive: " << family_id_maybe.getErr();
            return;
        }
        string family_id = family_id_maybe.unpack();

        if (family_id.empty()) {
             dbgTrace(D_ATTACHMENT_REGISTRATION) << "Received keep-alive with empty family ID for attachment ID: " << static_cast<int>(attachment_id);
             return;
        }

        auto family_members_it = registered_attachments.find(family_id);
        if (family_members_it == registered_attachments.end()) {
            dbgWarning(D_ATTACHMENT_REGISTRATION)
                << "Keep-alive for unregistered family. Adding new family. Family ID: "
                << family_id
                << ", Instance ID: " << static_cast<int>(attachment_id);
            size_t required_size = static_cast<size_t>(attachment_id) + 1;
            registered_attachments[family_id].resize(required_size, false);
            registered_attachments[family_id][attachment_id] = true;
            return;
        }

        if (attachment_id >= family_members_it->second.size()) {
            dbgWarning(D_ATTACHMENT_REGISTRATION)
                << "Keep-alive for out-of-bounds instance ID. Resizing family vector. Family ID: "
                << family_id
                << ", Instance ID: " << static_cast<int>(attachment_id)
                << ", Current size: " << family_members_it->second.size();
            size_t required_size = static_cast<size_t>(attachment_id) + 1;
            family_members_it->second.resize(required_size, false);
        }
        family_members_it->second[attachment_id] = true;
        dbgTrace(D_ATTACHMENT_REGISTRATION) << "Keep-alive received for Family: " << family_id << ", Instance: " << static_cast<int>(attachment_id);
    }

    void
    handleExpiration()
    {
        I_ShellCmd *shell_cmd = Singleton::Consume<I_ShellCmd>::by<AttachmentRegistrator>();
        vector<string> deleted_families;
        for (auto &family_pair : registered_attachments) {
            const string &family_id = family_pair.first;
            if (family_id.empty()) continue;

            bool is_family_inactive = true;
            vector<bool> &family_members = family_pair.second;
            for (const bool member : family_members) {
                if (member) {
                    is_family_inactive = false;
                    break;
                }
            }

            if (is_family_inactive) {
                static const string unregister_format = "/etc/cp/watchdog/cp-nano-watchdog --un-register ";
                stringstream unregister_command;
                unregister_command << unregister_format;
                unregister_command << "/etc/cp/HttpTransactionHandler/cp-nano-http-transaction-handler";
                unregister_command << " --family " << family_id;

                Maybe<string> res = shell_cmd->getExecOutput(unregister_command.str());
                if (!res.ok()) {
                    dbgWarning(D_ATTACHMENT_REGISTRATION)
                        << "Failed to un-register attachment. Family id: "
                        << family_id << ". Error: " << res.getErr();
                } else {
                    deleted_families.push_back(family_id);
                    dbgInfo(D_ATTACHMENT_REGISTRATION) << "Initiated un-registration for inactive family: " << family_id;
                }
            } else {
                fill(family_members.begin(), family_members.end(), false);
                dbgTrace(D_ATTACHMENT_REGISTRATION) << "Marked family potentially inactive for next cycle: " << family_id;
            }
        }

        for (const string &family : deleted_families) {
            registered_attachments.erase(family);
            dbgInfo(D_ATTACHMENT_REGISTRATION)
                << "Removed inactive attachments family from map. Family id: "
                << family;
        }
    }

    bool
    handleAttachmentRegistration()
    {
        auto custom_endpoint = CustomServerConfig::getRegistrationEndpoint();
        if (custom_endpoint.empty()) {
            dbgWarning(D_ATTACHMENT_REGISTRATION) << "Registration endpoint is not configured.";
            return false;
        }

        try {
            json registration_data = {
                {"agent_name", AttachmentRegistrator::getAgentName()},
                {"agent_type", AttachmentRegistrator::getAgentType()},
                {"platform", AttachmentRegistrator::getPlatform()},
                {"architecture", AttachmentRegistrator::getArchitecture()},
                {"token", AttachmentRegistrator::getRegistrationToken()}
            };

            dbgInfo(D_ATTACHMENT_REGISTRATION) << "Sending registration request to: " << custom_endpoint;
            dbgTrace(D_ATTACHMENT_REGISTRATION) << "Registration data: " << registration_data.dump();

            I_Messaging *messaging_api = Singleton::Consume<I_Messaging>::by<AttachmentRegistrator>();
            Maybe<HTTPResponse> response = messaging_api->sendSyncMessage(
                HTTPMethod::POST,
                custom_endpoint,
                registration_data,
                MessageCategory::GENERIC
            );

            if (!response.ok()) {
                dbgWarning(D_ATTACHMENT_REGISTRATION)
                    << "Failed to send registration request: " << response.getErr();
                return false;
            }

            auto unpacked_response = response.unpack();
            dbgInfo(D_ATTACHMENT_REGISTRATION) << "Registration response status: " << unpacked_response.getHTTPStatusCode();
            dbgTrace(D_ATTACHMENT_REGISTRATION) << "Response body: " << unpacked_response.getBody();

            if (unpacked_response.getHTTPStatusCode() != HTTPStatusCode::HTTP_OK) {
                dbgWarning(D_ATTACHMENT_REGISTRATION)
                    << "Attachment registration failed."
                    << "Status: " << unpacked_response.getHTTPStatusCode();
                return false;
            }

            try {
                if (unpacked_response.getBody().empty()) {
                     dbgWarning(D_ATTACHMENT_REGISTRATION) << "Registration response body is empty.";
                     return false;
                }
                json response_json = json::parse(unpacked_response.getBody());
                dbgInfo(D_ATTACHMENT_REGISTRATION) << "Registration successful. Response data: " << response_json.dump();
            } catch (const json::parse_error& e) {
                dbgWarning(D_ATTACHMENT_REGISTRATION) << "Failed to parse registration response JSON: " << e.what();
                dbgTrace(D_ATTACHMENT_REGISTRATION) << "Raw response body: " << unpacked_response.getBody();
                return false;
            }
            return true;
        } catch (const std::exception& e) {
            dbgWarning(D_ATTACHMENT_REGISTRATION)
                << "Exception during registration: " << e.what();
            return false;
        }
    }

    Maybe<uint8_t>
    readNumericParam(I_Socket::socketFd socket)
    {
        Maybe<vector<char>> param_to_read = i_socket->receiveData(socket, sizeof(uint8_t));
        if (!param_to_read.ok()) {
            dbgWarning(D_ATTACHMENT_REGISTRATION) << "Failed to read numeric param: " << param_to_read.getErr();
            return genError<std::string>("Failed to read numeric parameter");
        }
        if (param_to_read.unpack().empty()) {
             dbgWarning(D_ATTACHMENT_REGISTRATION) << "Received empty data when reading numeric param.";
             return genError<std::string>("Received empty data for numeric parameter");
        }
        return *reinterpret_cast<const uint8_t *>(param_to_read.unpack().data());
    }

    Maybe<AttachmentType>
    readAttachmentType(I_Socket::socketFd socket)
    {
        Maybe<uint8_t, uint8_t> attachment_type_val = readParamSize(socket);
        if (!attachment_type_val.ok()) return genError<std::string>("Failed to read attachment type size");

        uint8_t type_val = attachment_type_val.unpack();
        dbgTrace(D_ATTACHMENT_REGISTRATION)
            << "Successfully received attachment type. Value: " << static_cast<int>(type_val);

        return convertToEnum<AttachmentType>(type_val);
    }

    Maybe<string>
    readStringParam(I_Socket::socketFd socket)
    {
        Maybe<uint8_t, uint8_t> param_size = readParamSize(socket);
        if (!param_size.ok()) return genError<std::string>("Failed to read string param size");

        uint8_t size = param_size.unpack();
        if (size == 0) {
             dbgTrace(D_ATTACHMENT_REGISTRATION) << "Received string size 0. Returning empty string.";
             return string("");
        }

        dbgTrace(D_ATTACHMENT_REGISTRATION) << "Received string size: " << static_cast<int>(size);

        Maybe<vector<char>> param_to_read = i_socket->receiveData(socket, size);
        if (!param_to_read.ok()) {
             dbgWarning(D_ATTACHMENT_REGISTRATION) << "Failed to read string data: " << param_to_read.getErr();
             return genError<string>("Failed to read string data");
        }

        if (param_to_read.unpack().size() != size) {
             dbgWarning(D_ATTACHMENT_REGISTRATION)
                  << "Received incorrect amount of string data. Expected: " << static_cast<int>(size)
                  << ", Got: " << param_to_read.unpack().size();
             return genError<string>("Received incorrect amount of string data");
        }
        return string(param_to_read.unpack().begin(), param_to_read.unpack().end());
    }

    Maybe<vector<char>, string>
    readParam(I_Socket::socketFd socket) const
    {
        Maybe<uint8_t, uint8_t> param_size = readParamSize(socket);
        if (!param_size.ok()) return genError<std::string>("Failed to read generic param size");

        uint8_t size = param_size.unpack();
        if (size == 0) {
            dbgTrace(D_ATTACHMENT_REGISTRATION) << "Received parameter size 0. Returning empty vector.";
            return vector<char>();
        }

        dbgTrace(D_ATTACHMENT_REGISTRATION) << "Received parameter size: " << static_cast<int>(size);

        Maybe<vector<char>> param_to_read = i_socket->receiveData(socket, size);
        if (!param_to_read.ok()) {
            dbgWarning(D_ATTACHMENT_REGISTRATION) << "Failed to read parameter data: " << param_to_read.getErr();
            return genError<string>("Failed to read parameter data");
        }

        if (param_to_read.unpack().size() != size) {
            dbgWarning(D_ATTACHMENT_REGISTRATION)
                << "Received incorrect amount of parameter data. Expected: " << static_cast<int>(size)
                << ", Got: " << param_to_read.unpack().size();
            return genError<string>("Received incorrect amount of parameter data");
        }
        return param_to_read.unpack();
    }

    Maybe<uint8_t, uint8_t>
    readParamSize(I_Socket::socketFd socket) const
    {
        Maybe<vector<char>, vector<char>> size_data = i_socket->receiveData(socket, 1);
        if (!size_data.ok()) return Error<uint8_t>(1);
        if (size_data.unpack().size() != 1) return Error<uint8_t>(2);
        return *reinterpret_cast<const uint8_t *>(size_data.unpack().data());
    }

    I_Socket::socketFd server_sock = -1;
    I_Socket::socketFd keep_alive_sock = -1;
    I_Socket *i_socket = nullptr;
    map<string, vector<bool>> registered_attachments;
    string shared_registration_path;
};

AttachmentRegistrator::AttachmentRegistrator() : Component("AttachmentRegistrator"), pimpl(make_unique<Impl>()) {
     CustomServerConfig::loadConfig();
}

AttachmentRegistrator::~AttachmentRegistrator() {}

void AttachmentRegistrator::init() { pimpl->init(); }

void AttachmentRegistrator::fini() { pimpl->fini(); }

void
AttachmentRegistrator::preload()
{
     auto config_api = Singleton::Consume<Config::I_Config>::by<AttachmentRegistrator>();
     config_api->registerExpectedConfiguration<string>({ "Attachment Registration", "Registration IPC Path" });
     config_api->registerExpectedConfiguration<string>({ "Attachment Registration", "Keep Alive IPC Path" });
     config_api->registerExpectedConfiguration<string>({ "customServer", "registrationEndpoint" });
     config_api->registerExpectedConfiguration<uint>({ "customServer", "apiPort" });
     config_api->registerExpectedConfiguration<uint>({ "customServer", "alternativeApiPort" });
     config_api->registerExpectedConfiguration<uint>({ "attachmentRegistrator", "expirationCheckSeconds" });
}

// Mock methods for testing - Made static
static std::string getAgentName() { return "test_agent"; }
