#include <gtest/gtest.h>
#include "attachment_registrator.h"
#include <nlohmann/json.hpp>
#include <fstream>

using json = nlohmann::json;

class RegistrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create test configuration file
        std::ofstream config_file("conf/custom-server-config.json");
        json config = {
            {"server", {
                {"custom_registration_endpoint", "http://test-server/register"},
                {"api_port", 8124},
                {"alternative_api_port", 8127},
                {"allow_external_ip", true}
            }},
            {"authentication", {
                {"token_endpoint", "http://test-server/token"},
                {"client_id", "test_client"},
                {"client_secret", "test_secret"}
            }}
        };
        config_file << config.dump(4);
        config_file.close();
    }

    void TearDown() override {
        // Cleanup test files
        std::remove("conf/custom-server-config.json");
    }

    AttachmentRegistrator registrator;
};

TEST_F(RegistrationTest, ConfigurationLoadTest) {
    CustomServerConfig::loadConfig();
    EXPECT_EQ(CustomServerConfig::getRegistrationEndpoint(), "http://test-server/register");
    EXPECT_EQ(CustomServerConfig::getApiPort(), 8124);
    EXPECT_EQ(CustomServerConfig::getAlternativePort(), 8127);
}

TEST_F(RegistrationTest, RegistrationRequestTest) {
    json registration_data = {
        {"agent_name", registrator.getAgentName()},
        {"agent_type", registrator.getAgentType()},
        {"platform", registrator.getPlatform()},
        {"architecture", registrator.getArchitecture()},
        {"token", registrator.getRegistrationToken()}
    };

    EXPECT_EQ(registration_data["agent_name"], "test_agent");
    EXPECT_EQ(registration_data["agent_type"], "test_type");
    EXPECT_EQ(registration_data["platform"], "linux");
    EXPECT_EQ(registration_data["architecture"], "x86_64");
    EXPECT_EQ(registration_data["token"], "test_token");
}

TEST_F(RegistrationTest, InitializationTest) {
    EXPECT_NO_THROW(registrator.init());
    EXPECT_NO_THROW(registrator.fini());
}

TEST_F(RegistrationTest, ConfigFileFormatTest) {
    std::ifstream config_file("conf/custom-server-config.json");
    ASSERT_TRUE(config_file.is_open());
    
    json config;
    config_file >> config;
    
    EXPECT_TRUE(config.contains("server"));
    EXPECT_TRUE(config.contains("authentication"));
    EXPECT_TRUE(config["server"].contains("custom_registration_endpoint"));
    EXPECT_TRUE(config["server"].contains("api_port"));
    EXPECT_TRUE(config["server"].contains("alternative_api_port"));
    EXPECT_TRUE(config["server"].contains("allow_external_ip"));
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
} 