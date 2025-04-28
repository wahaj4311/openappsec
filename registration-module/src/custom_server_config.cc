#include "custom_server_config.h"

// Basic implementation - needs actual logic from attachment_registrator.h/cc

std::string CustomServerConfig::registration_endpoint; // Default? Check original
int CustomServerConfig::api_port = 8080; // Default value
int CustomServerConfig::alternative_port = 8081; // Default value

void
CustomServerConfig::loadConfig()
{
    // Placeholder - Add logic to load config (e.g., from file or env)
    // Need to get I_Config here, maybe pass it in?
    // For now, just assign defaults.
    registration_endpoint = "http://localhost:8124/register"; // Example default
    api_port = 8124;
    alternative_port = 8127;
}

std::string
CustomServerConfig::getRegistrationEndpoint()
{
    return registration_endpoint;
}

int
CustomServerConfig::getApiPort()
{
    return api_port;
}

int
CustomServerConfig::getAlternativePort()
{
    return alternative_port;
} 