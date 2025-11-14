#ifndef CLAUDE_HPP
#define CLAUDE_HPP

// Main header that includes everything

#include <claude/client.hpp>
#include <claude/errors.hpp>
#include <claude/hooks.hpp>
#include <claude/query.hpp>
#include <claude/types.hpp>
#include <claude/version.hpp>

// Optional: High-level MCP (Model Context Protocol) template-based API
// Provides Python-like syntax for creating MCP tools and servers
// Include this if you want to use claude::mcp::make_tool() and create_server()
// For a simpler, non-template approach, see fastmcpp integration examples
#include <claude/mcp.hpp>

// Convenience: bring main symbols into claude namespace
namespace claude
{
// All types and functions are already in claude namespace
}

#endif // CLAUDE_HPP
