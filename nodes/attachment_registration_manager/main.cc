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

#include "components_list.h"
#include "attachment_registrator.h"
#include <fstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class CustomServerConfig {
public:
    static void loadConfig() {
        try {
            std::ifstream config_file("conf/custom-server-config.json");
            if (config_file.is_open()) {
                json config = json::parse(config_file);
                registration_endpoint = config["server"]["custom_registration_endpoint"];
                api_port = config["server"]["api_port"];
                alternative_port = config["server"]["alternative_api_port"];
            }
        } catch (const std::exception& e) {
            // Fallback to default values if config loading fails
            registration_endpoint = "http://localhost:8124/register";
            api_port = 8124;
            alternative_port = 8127;
        }
    }

    static std::string getRegistrationEndpoint() { return registration_endpoint; }
    static uint getApiPort() { return api_port; }
    static uint getAlternativePort() { return alternative_port; }

private:
    static std::string registration_endpoint;
    static uint api_port;
    static uint alternative_port;
};

std::string CustomServerConfig::registration_endpoint;
uint CustomServerConfig::api_port;
uint CustomServerConfig::alternative_port;

int
main(int argc, char **argv)
{
    CustomServerConfig::loadConfig();
    
    NodeComponents<AttachmentRegistrator> comps;
    comps.registerGlobalValue<bool>("Is Rest primary routine", true);
    comps.registerGlobalValue<uint>("Nano service API Port Primary", CustomServerConfig::getApiPort());
    comps.registerGlobalValue<uint>("Nano service API Port Alternative", CustomServerConfig::getAlternativePort());
    return comps.run("Attachment Registration Manager", argc, argv);
}
