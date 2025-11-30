# Try to find GTest; if not found, fetch it locally for this build.
find_package(GTest CONFIG)
if(NOT TARGET GTest::gtest)
    message(STATUS "GTest not found; fetching GoogleTest via FetchContent")
    include(FetchContent)
    FetchContent_Declare(
      googletest
      GIT_REPOSITORY https://github.com/google/googletest.git
      GIT_TAG v1.14.0
    )
    # For Windows: prevent overriding parent settings
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
endif()

add_executable(test_claude
    test_version.cpp
    unit/test_subprocess.cpp
    unit/test_types.cpp
    unit/test_parser.cpp
    unit/test_transport_cli_path.cpp
    unit/test_control.cpp
    unit/test_mcp_templates.cpp
    unit/test_parity_fields.cpp
    unit/test_temp_file_security.cpp
    integration/sdk_mcp_integration.cpp
    integration/sdk_mcp_integration_errors.cpp
    integration/test_query.cpp
    integration/test_client.cpp
    integration/test_control_protocol.cpp
    extra/test_multithreading.cpp
    # Extension tests
    ext/test_session_storage.cpp
)

target_link_libraries(test_claude
    PRIVATE
        claude_sdk
        GTest::gtest
        GTest::gtest_main
)

# Access internal headers for targeted unit tests
target_include_directories(test_claude
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)

set_target_properties(test_claude PROPERTIES FOLDER "Tests")

add_test(NAME test_claude COMMAND test_claude)
set_tests_properties(test_claude PROPERTIES LABELS "conformance")

# Add reasonable timeouts to avoid hangs in CI or constrained environments
# Allow more time for this umbrella suite in CI/constrained environments
set_tests_properties(test_claude PROPERTIES TIMEOUT 600)

# Optional: fastmcpp integration tests (requires fastmcpp)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../fastmcpp/CMakeLists.txt")
    # Ensure fastmcpp does not register its own tests/examples here
    set(FASTMCPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(FASTMCPP_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    # Add fastmcpp as subdirectory only to obtain the core target
    if(NOT TARGET fastmcpp_core)
        add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../fastmcpp ${CMAKE_CURRENT_BINARY_DIR}/fastmcpp EXCLUDE_FROM_ALL)
    endif()

    add_executable(test_fastmcpp_integration
        integration/test_mcp_fastmcpp.cpp
    )

    target_link_libraries(test_fastmcpp_integration
        PRIVATE
            claude_sdk
            fastmcpp_core
            GTest::gtest
            GTest::gtest_main
    )

    # Define FASTMCPP_AVAILABLE to enable tests
    target_compile_definitions(test_fastmcpp_integration PRIVATE FASTMCPP_AVAILABLE)

    set_target_properties(test_fastmcpp_integration PROPERTIES FOLDER "Tests")

    add_test(NAME test_fastmcpp_integration COMMAND test_fastmcpp_integration)
    set_tests_properties(test_fastmcpp_integration PROPERTIES LABELS "integration")
    # Guard against potential hangs in integration paths
    set_tests_properties(test_fastmcpp_integration PROPERTIES TIMEOUT 600)
endif()

# If Claude CLI is not available in this environment, disable tests that require it
find_program(CLAUDE_CLI_EXECUTABLE NAMES claude)
if(NOT CLAUDE_CLI_EXECUTABLE)
    message(STATUS "Claude CLI not found in PATH; disabling CLI-dependent tests")
    set_tests_properties(test_claude PROPERTIES DISABLED TRUE)
    if(TARGET test_fastmcpp_integration)
        set_tests_properties(test_fastmcpp_integration PROPERTIES DISABLED TRUE)
    endif()
endif()
