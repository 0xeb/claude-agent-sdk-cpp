#ifndef CLAUDE_MCP_DECORATORS_HPP
#define CLAUDE_MCP_DECORATORS_HPP

#include <claude/mcp/server.hpp>
#include <claude/mcp/tool.hpp>
#include <string>
#include <vector>

// Lightweight decorator-style helpers to mirror Python ergonomics
// Usage:
//   auto add = CLAUDE_TOOL("add", "Add two numbers", [](double a, double b){ return a+b; });
//   auto echo = CLAUDE_TOOL_WITH_PARAMS("echo", "Echo a string", [](std::string s){ return s; },
//   "text"); auto handler = CLAUDE_SERVER("demo", "0.1.0", add, echo);

#define CLAUDE_TOOL(NAME, DESC, FUNC)                                                              \
    ::claude::mcp::Tool(::claude::mcp::make_tool(std::string(NAME), std::string(DESC), (FUNC)))

#define CLAUDE_TOOL_WITH_PARAMS(NAME, DESC, FUNC, /*param names...*/...)                           \
    ::claude::mcp::Tool(::claude::mcp::make_tool(std::string(NAME), std::string(DESC), (FUNC),     \
                                                 std::vector<std::string>{__VA_ARGS__}))

#define CLAUDE_SERVER(NAME, VERSION, /*tools...*/...)                                              \
    ::claude::mcp::create_server(std::string(NAME), std::string(VERSION), __VA_ARGS__)

#endif // CLAUDE_MCP_DECORATORS_HPP
