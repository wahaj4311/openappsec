#ifndef __CUSTOM_SERVER_CONFIG_H__
#define __CUSTOM_SERVER_CONFIG_H__

// Placeholder for the CustomServerConfig class definition
// We will move the definition from attachment_registrator.h here

#include <string> // Assuming it needs string

class CustomServerConfig {
public:
    static void loadConfig();
    static std::string getRegistrationEndpoint();
    static int getApiPort();
    static int getAlternativePort();

private:
    // Add member variables and potentially private methods if needed
    static std::string registration_endpoint;
    static int api_port;
    static int alternative_port;
};

#endif // __CUSTOM_SERVER_CONFIG_H__ 