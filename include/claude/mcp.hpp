#ifndef CLAUDE_MCP_HPP
#define CLAUDE_MCP_HPP

/**
 * @file mcp.hpp
 * @brief High-level template-based MCP (Model Context Protocol) server API
 *
 * This header provides a Python-like API for creating MCP tools and servers
 * with automatic type deduction and JSON schema generation.
 *
 * Features:
 * - Automatic type deduction from lambda/function signatures
 * - Automatic JSON Schema generation for input/output
 * - Type-safe parameter extraction and validation
 * - Variadic template server creation
 * - Compatible with claude::ClaudeOptions::sdk_mcp_handlers
 *
 * Example usage:
 * @code
 * #include <claude/claude.hpp>
 * #include <claude/mcp.hpp>
 *
 * using namespace claude::mcp;
 *
 * // Define tools with automatic type deduction
 * auto add = make_tool("add", "Add two numbers",
 *     [](double a, double b) -> double { return a + b; }
 * );
 *
 * auto greet = make_tool("greet", "Greet a user",
 *     [](std::string name) -> std::string {
 *         return "Hello, " + name + "!";
 *     }
 * );
 *
 * // Create server with variadic templates
 * auto server = create_server("myserver", "1.0.0", add, greet);
 *
 * // Use with Claude
 * claude::ClaudeOptions opts;
 * opts.sdk_mcp_handlers["myserver"] = server;
 * claude::ClaudeClient client(opts);
 * @endcode
 *
 * Limitations:
 * - Parameter names are auto-generated as arg0, arg1, etc. (C++ doesn't preserve names)
 * - Only primitive types and std::vector/std::map are supported by default
 * - Custom types require manual TypeToSchema specialization
 * - Template errors can be cryptic (use simple types for better error messages)
 *
 * For simpler, non-template-based approach, see fastmcpp integration in examples/.
 */

// Core MCP components
#include <claude/mcp/argument_extractor.hpp>
#include <claude/mcp/server.hpp>
#include <claude/mcp/tool.hpp>
#include <claude/mcp/type_traits.hpp>

namespace claude
{

/**
 * @namespace claude::mcp
 * @brief Template-based MCP server creation utilities
 *
 * This namespace provides high-level functions for creating MCP tools and servers
 * with automatic type deduction and schema generation.
 */
namespace mcp
{

// All public API is already available from included headers:
//
// Tool Creation:
// - make_tool(name, desc, func) -> ToolWrapper<Func>
// - make_tool(name, desc, func, param_names) -> ToolWrapperWithNames<Func>
//
// Server Creation:
// - create_server(name, version, tools...) -> std::function<json(const json&)>
// - ServerBuilder for fluent interface
//
// Type System:
// - TypeToSchema<T> - Maps C++ types to JSON Schema
// - FunctionTraits<Func> - Extracts function signature information
//
// Advanced:
// - Tool - Type-erased tool interface
// - JsonInvoker<Func> - JSON-based function invoker

} // namespace mcp
} // namespace claude

#endif // CLAUDE_MCP_HPP
