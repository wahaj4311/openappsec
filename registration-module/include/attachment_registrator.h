#ifndef ATTACHMENT_REGISTRATOR_H
#define ATTACHMENT_REGISTRATOR_H

#include <string>
#include <memory>
#include <nlohmann/json.hpp>
#include "component.h"
#include "singleton.h"
#include "i_mainloop.h"      // Included for Singleton::Consume<I_MainLoop>
#include "i_shell_cmd.h"     // Included for Singleton::Consume<I_ShellCmd>
#include "i_socket_is.h"     // Included for Singleton::Consume<I_Socket>
#include "i_messaging.h"     // Included for Singleton::Consume<I_Messaging>
#include "i_encryptor.h"   // Included for Singleton::Consume<I_Encryptor>
#include "i_config.h"      // Included for Singleton::Consume<I_Config>

using json = nlohmann::json;

class AttachmentRegistrator :
    public Component,
    public Singleton::Consume<I_Socket>,
    public Singleton::Consume<I_MainLoop>,
    public Singleton::Consume<I_ShellCmd>,
    public Singleton::Consume<I_Messaging>,
    public Singleton::Consume<I_Encryptor>,
    public Singleton::Consume<Config::I_Config>
{
public:
    AttachmentRegistrator();
    ~AttachmentRegistrator();

    void init();
    void fini();
    static void preload();

    // Mock methods for testing - Made static (Restored)
    static std::string getAgentName() { return "test_agent"; }
    static std::string getAgentType() { return "test_type"; }
    static std::string getPlatform() { return "linux"; }
    static std::string getArchitecture() { return "x86_64"; }
    static std::string getRegistrationToken() { return "test_token"; }

private:
    class Impl;
    std::unique_ptr<Impl> pimpl;
};

#endif // ATTACHMENT_REGISTRATOR_H 