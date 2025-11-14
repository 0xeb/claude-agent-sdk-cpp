add_executable(quick_start
    quick_start.cpp
)

target_link_libraries(quick_start
    PRIVATE
        claude_sdk
)

set_target_properties(quick_start PROPERTIES FOLDER "Examples")

add_executable(streaming_mode
    streaming_mode.cpp
)

target_link_libraries(streaming_mode
    PRIVATE
        claude_sdk
)

set_target_properties(streaming_mode PROPERTIES FOLDER "Examples")

add_executable(include_partial_messages
    include_partial_messages.cpp
)

target_link_libraries(include_partial_messages
    PRIVATE
        claude_sdk
)

set_target_properties(include_partial_messages PROPERTIES FOLDER "Examples")

add_executable(hooks
    hooks.cpp
)

target_link_libraries(hooks
    PRIVATE
        claude_sdk
)

set_target_properties(hooks PROPERTIES
    FOLDER "Examples"
    VS_DEBUGGER_COMMAND_ARGUMENTS "--auto-approve"
)

add_executable(tool_permission_callback
    tool_permission_callback.cpp
)

target_link_libraries(tool_permission_callback
    PRIVATE
        claude_sdk
)

set_target_properties(tool_permission_callback PROPERTIES FOLDER "Examples")

if(WIN32)
  add_executable(error_handling
      error_handling.cpp
  )

  target_link_libraries(error_handling
      PRIVATE
          claude_sdk
  )

  set_target_properties(error_handling PROPERTIES FOLDER "Examples")
endif()

add_executable(system_prompt
    system_prompt.cpp
)

target_link_libraries(system_prompt
    PRIVATE
        claude_sdk
)

set_target_properties(system_prompt PROPERTIES FOLDER "Examples")

add_executable(setting_sources
    setting_sources.cpp
)

target_link_libraries(setting_sources
    PRIVATE
        claude_sdk
)

set_target_properties(setting_sources PROPERTIES FOLDER "Examples")

add_executable(stderr_callback_example
    stderr_callback_example.cpp
)

target_link_libraries(stderr_callback_example
    PRIVATE
        claude_sdk
)

set_target_properties(stderr_callback_example PROPERTIES FOLDER "Examples")

add_executable(mcp_calculator
    mcp_calculator.cpp
)

target_link_libraries(mcp_calculator
    PRIVATE
        claude_sdk
)

set_target_properties(mcp_calculator PROPERTIES FOLDER "Examples")

add_executable(plugin_example
    plugin_example.cpp
)

target_link_libraries(plugin_example
    PRIVATE
        claude_sdk
)

set_target_properties(plugin_example PROPERTIES FOLDER "Examples")

add_executable(max_budget_usd
    max_budget_usd.cpp
)

target_link_libraries(max_budget_usd
    PRIVATE
        claude_sdk
)

set_target_properties(max_budget_usd PROPERTIES FOLDER "Examples")

add_executable(max_thinking_tokens
    max_thinking_tokens.cpp
)

target_link_libraries(max_thinking_tokens
    PRIVATE
        claude_sdk
)

set_target_properties(max_thinking_tokens PROPERTIES FOLDER "Examples")

# Advanced and non-mirrored examples (including decorators and MCP showcases)
# are not built as part of the core SDK example set and are maintained
# separately from this directory.
