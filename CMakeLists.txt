cmake_minimum_required(VERSION 3.10)
project(OpenAppSec VERSION 1.0)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(YAJL REQUIRED)
find_package(nlohmann_json 3.2.0 REQUIRED)

# Add source directories
add_subdirectory(components)
add_subdirectory(nodes)

# Set include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${YAJL_INCLUDE_DIRS}
)

# Main registration manager executable
add_executable(attachment-registration-manager
    nodes/attachment_registration_manager/main.cc
    components/attachment-intakers/attachment_registrator/attachment_registrator.cc
)

# Link libraries
target_link_libraries(attachment-registration-manager
    PRIVATE
    ${YAJL_LIBRARIES}
    nlohmann_json::nlohmann_json
    pthread
)

# Install targets
install(TARGETS attachment-registration-manager
    RUNTIME DESTINATION bin
)

# Install configuration
install(FILES conf/custom-server-config.json
    DESTINATION conf
    PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
)

# Create directories needed for runtime
install(DIRECTORY
    DESTINATION /dev/shm/check-point
    DIRECTORY_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE 
                         GROUP_READ GROUP_WRITE GROUP_EXECUTE
                         WORLD_READ WORLD_WRITE WORLD_EXECUTE
)
