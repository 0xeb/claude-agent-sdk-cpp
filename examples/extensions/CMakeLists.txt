# C++-specific / non-mirrored examples. Per project policy these use the
# templated MCP API (claude::mcp::make_tool, server().add_tool().build())
# rather than decorator macros, and do not contain ".." parent paths in
# include directives.

add_executable(file_session_store_demo file_session_store_demo.cpp)
target_link_libraries(file_session_store_demo PRIVATE claude_sdk)
set_target_properties(file_session_store_demo PROPERTIES FOLDER "Examples/Extensions")
