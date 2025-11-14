# Development tools for Claude Code SDK
# These are utilities for SDK development and protocol analysis

# Debug Protocol Analysis Tool
add_executable(debug
    debug.cpp
)

target_link_libraries(debug
    PRIVATE
        claude_sdk
)

set_target_properties(debug PROPERTIES
    FOLDER "Tools"
    VS_DEBUGGER_COMMAND_ARGUMENTS "--auto-approve"
)

# Installation rules for tools (optional)
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/install" CACHE PATH "Installation directory" FORCE)
endif()

install(TARGETS
    debug
    DESTINATION tools
)
