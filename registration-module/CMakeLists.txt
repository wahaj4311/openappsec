cmake_minimum_required(VERSION 3.10)
project(RegistrationModule VERSION 1.0)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Options for build configuration
option(BUILD_TESTS "Build test cases" ON)

# Find required packages
find_package(PkgConfig REQUIRED)
pkg_check_modules(YAJL REQUIRED yajl)
find_package(nlohmann_json 3.11.2 REQUIRED)

# Add source files
set(SOURCES
    src/main.cc
    src/attachment_registrator.cc
    src/custom_server_config.cc
)

# Define the main library
add_library(registration_manager SHARED ${SOURCES})

# Set include directories for the library
target_include_directories(registration_manager PUBLIC
    # Core directories first
    /home/xox/self/openappsec/core/include                       # Main core include
    /home/xox/self/openappsec/core/include/config                 # For I_Config
    /home/xox/self/openappsec/core/include/general                # For Singleton, Debug, Maybe, etc.
    /home/xox/self/openappsec/core/include/interfaces             # For I_MainLoop, I_ShellCmd, I_Socket
    /home/xox/self/openappsec/core/include/internal               # For instance_awareness.h
    /home/xox/self/openappsec/core/include/services_sdk/resources # For Component, Debug flags
    /home/xox/self/openappsec/core/include/services_sdk/interfaces # For I_Messaging, I_Encryptor
    /home/xox/self/openappsec/core/include/services_sdk/utilities # For flags.h
    /home/xox/self/openappsec/core/include/attachments            # General attachments includes (for subdirs)
    # Components directory
    /home/xox/self/openappsec/components/include                # For signal_handler.h
    # Project directory
    ${CMAKE_CURRENT_SOURCE_DIR}/include                       # Project's own include dir
    # External directories
    /home/xox/self/openappsec/external                       # For cereal headers
    ${YAJL_INCLUDE_DIRS}
)

# Link libraries
target_link_libraries(registration_manager PRIVATE
    core_config
    core_mainloop
    core_singleton
    core_debug
    core_common
    core_buffer
    core_socket
    core_shellcmd
    core_messaging
    core_encryptor
    pthread
    core_lib
    ${YAJL_LIBRARIES}
    nlohmann_json::nlohmann_json
)

# Install targets
install(TARGETS registration_manager
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

# Install configuration
install(FILES conf/custom-server-config.json
    DESTINATION conf
    PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
)

# Tests configuration
if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/include/config
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/include/general
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/include/rest
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/include/services_sdk/resources
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/include/services_sdk/interfaces
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/include/services_sdk/resources/config
    /usr/include/nginx
) 