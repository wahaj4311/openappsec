#ifndef COMPONENTS_LIST_H
#define COMPONENTS_LIST_H

#include <string>

class NodeComponents {
public:
    template<typename T>
    void registerGlobalValue(const std::string& key, T value) {
        // Mock implementation
    }

    template<typename T>
    int run(const std::string& name, int argc, char** argv) {
        return 0;
    }
};

#endif // COMPONENTS_LIST_H 