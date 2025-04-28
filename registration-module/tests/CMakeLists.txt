# Find GTest package
find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})

# Add source files
set(TEST_SOURCES
    registration_test.cc
    ${CMAKE_SOURCE_DIR}/src/attachment_registrator.cc
)

# Test executable
add_executable(registration_tests ${TEST_SOURCES})

# Link test executable with Google Test and our registration library
target_link_libraries(registration_tests
    PRIVATE
    ${GTEST_LIBRARIES}
    ${YAJL_LIBRARIES}
    nlohmann_json::nlohmann_json
    pthread
    registration_manager
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
)

# Add test to CTest
add_test(NAME registration_tests COMMAND registration_tests) 