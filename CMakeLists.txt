cmake_minimum_required(VERSION 3.20)
project(claude_sdk VERSION 0.1.8 LANGUAGES CXX)

# Enable folder organization in IDEs (Visual Studio, Xcode, etc.)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# C++ Standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Make .clang-format visible in Visual Studio solution
set(CLAUDE_CONFIG_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/.clang-format
)
source_group("Configuration Files" FILES ${CLAUDE_CONFIG_FILES})

# Options
option(CLAUDE_BUILD_TESTS "Build tests" ON)
option(CLAUDE_BUILD_EXAMPLES "Build examples" ON)

# Optional: root of fastmcpp checkout to enable integration and examples that depend on it
set(FASTMCPP_ROOT "" CACHE PATH "Path to fastmcpp project root (optional)")

# Find dependencies
find_package(nlohmann_json CONFIG)
if(NOT TARGET nlohmann_json::nlohmann_json)
    include(FetchContent)
    FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.11.3
    )
    FetchContent_MakeAvailable(nlohmann_json)
endif()

# Library target
add_library(claude_sdk
    # Core sources (Python parity)
    src/version.cpp
    src/errors.cpp
    src/types.cpp
    src/query.cpp
    src/client.cpp
    src/protocol/control.cpp
    src/internal/subprocess/process.cpp
    src/internal/message_parser.cpp
    src/internal/transport/subprocess_transport.cpp
    # Extensions (C++ conveniences)
    src/ext/session_storage.cpp
    ${CLAUDE_CONFIG_FILES}
)

target_include_directories(claude_sdk
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(claude_sdk
    PUBLIC
        nlohmann_json::nlohmann_json
)

# Set target properties
set_target_properties(claude_sdk PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    FOLDER "Libraries"
)

# Tests
if(CLAUDE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Bring in fastmcpp (optional) if caller provided FASTMCPP_ROOT
if(FASTMCPP_ROOT)
    if(EXISTS "${FASTMCPP_ROOT}/CMakeLists.txt")
        # Ensure fastmcpp does not register or build its own tests/examples in this inclusion
        set(FASTMCPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
        set(FASTMCPP_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        add_subdirectory(${FASTMCPP_ROOT} ${CMAKE_CURRENT_BINARY_DIR}/fastmcpp EXCLUDE_FROM_ALL)
    else()
        message(WARNING "FASTMCPP_ROOT provided but no CMakeLists.txt found at: ${FASTMCPP_ROOT}")
    endif()
endif()

# Examples
if(CLAUDE_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Development tools
add_subdirectory(tools)

# Optional: Add format target for command-line formatting
find_program(CLANG_FORMAT_EXECUTABLE NAMES clang-format)
if(CLANG_FORMAT_EXECUTABLE)
    file(GLOB_RECURSE ALL_SOURCE_FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/examples/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp
    )
    add_custom_target(format
        COMMAND ${CLANG_FORMAT_EXECUTABLE} -i ${ALL_SOURCE_FILES}
        COMMENT "Running clang-format on all source files"
        VERBATIM
    )
    set_target_properties(format PROPERTIES FOLDER "Development")
endif()

# Install
install(TARGETS claude_sdk
    EXPORT claude_sdk_targets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY include/
    DESTINATION include
)
