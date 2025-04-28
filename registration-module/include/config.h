#ifndef CONFIG_H
#define CONFIG_H

#include <string>

template<typename T>
T getConfigurationWithDefault(T default_value, const std::string& section, const std::string& key) {
    return default_value;
}

#endif // CONFIG_H 