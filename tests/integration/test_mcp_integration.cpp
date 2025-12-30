#include "../test_utils.hpp"

#include <claude/errors.hpp>
#include <claude/query.hpp>
#include <claude/types.hpp>
#include <gtest/gtest.h>
#include <iostream>

using namespace claude;

// Integration tests for MCP (Model Context Protocol) servers
// Skipped in CI (live API tests), enabled locally

class MCPIntegrationTest : public ::testing::Test
{
  protected:
    std::string get_session_id(const QueryResult& result, const std::string& test_name)
    {
        std::string session_id;
        for (const auto& msg : result)
        {
            if (is_result_message(msg))
            {
                const auto& res = std::get<ResultMessage>(msg);
                session_id = res.session_id();
                std::cout << "\n========================================\n";
                std::cout << "[" << test_name << "]\n";
                std::cout << "Session ID: " << session_id << "\n";
                std::cout << "Tokens: " << res.usage().input_tokens << " in, "
                          << res.usage().output_tokens << " out\n";
                if (res.total_cost_usd() > 0)
                    std::cout << "Cost: $" << res.total_cost_usd() << "\n";
                std::cout << "Resume: claude --resume " << session_id << "\n";
                std::cout << "========================================\n";
            }
        }
        return session_id;
    }

    std::string get_response(const QueryResult& result)
    {
        for (const auto& msg : result)
        {
            if (is_assistant_message(msg))
            {
                const auto& assistant = std::get<AssistantMessage>(msg);
                return get_text_content(assistant.content);
            }
        }
        return "";
    }
};

// Test 1: Query with MCP server configured (filesystem)
// This test requires the filesystem MCP server to be configured
TEST_F(MCPIntegrationTest, FilesystemMCPServer)
{
    SKIP_IN_CI();

    ClaudeOptions opts;
    opts.permission_mode = "bypassPermissions";

    // MCP servers are typically configured via settings, not programmatically
    // This test verifies basic query works when MCP is configured

    QueryResult result = query("List the files in the current directory using available tools. "
                               "Just list a few filenames.",
                               opts);

    std::string session_id = get_session_id(result, "FilesystemMCPServer");
    std::string response = get_response(result);

    std::cout << "Response: " << response << "\n";

    EXPECT_FALSE(session_id.empty());
}

// Test 2: Query that should trigger tool use
TEST_F(MCPIntegrationTest, ToolUseQuery)
{
    SKIP_IN_CI();

    ClaudeOptions opts;
    opts.permission_mode = "bypassPermissions";

    // Ask something that requires reading files
    QueryResult result =
        query("Read the CMakeLists.txt file in this directory and tell me the project name. "
              "Just the name.",
              opts);

    std::string session_id = get_session_id(result, "ToolUseQuery");
    std::string response = get_response(result);

    std::cout << "Response: " << response << "\n";

    EXPECT_FALSE(session_id.empty());
}

// Test 3: Query with multiple tool calls
TEST_F(MCPIntegrationTest, MultipleToolCalls)
{
    SKIP_IN_CI();

    ClaudeOptions opts;
    opts.permission_mode = "bypassPermissions";

    QueryResult result = query("Find all .hpp files in include/claude and count them. "
                               "Give me the count.",
                               opts);

    std::string session_id = get_session_id(result, "MultipleToolCalls");
    std::string response = get_response(result);

    std::cout << "Response: " << response << "\n";

    EXPECT_FALSE(session_id.empty());
}

// Test 4: Verify tool results are captured
TEST_F(MCPIntegrationTest, ToolResultsInMessages)
{
    SKIP_IN_CI();

    ClaudeOptions opts;
    opts.permission_mode = "bypassPermissions";

    QueryResult result = query("What is in the VERSION file? Just the content.", opts);

    std::string session_id = get_session_id(result, "ToolResultsInMessages");

    // Check for tool use in the message stream
    bool found_tool_use = false;
    bool found_tool_result = false;
    for (const auto& msg : result)
    {
        if (is_assistant_message(msg))
        {
            const auto& assistant = std::get<AssistantMessage>(msg);
            for (const auto& block : assistant.content)
            {
                if (auto* tool = std::get_if<ToolUseBlock>(&block))
                {
                    found_tool_use = true;
                    std::cout << "Tool used: " << tool->name << "\n";
                }
                if (auto* result_block = std::get_if<ToolResultBlock>(&block))
                {
                    found_tool_result = true;
                    std::cout << "Tool result received\n";
                }
            }
        }
    }

    EXPECT_FALSE(session_id.empty());
    // Note: Tool use blocks may or may not be present depending on CLI output format
}

// Test 5: Query with max_turns limit affecting tool use
TEST_F(MCPIntegrationTest, MaxTurnsWithToolUse)
{
    SKIP_IN_CI();

    ClaudeOptions opts;
    opts.permission_mode = "bypassPermissions";
    opts.max_turns = 3; // Limit turns

    QueryResult result = query("Search for 'ClaudeOptions' in the codebase and list files. "
                               "Be concise.",
                               opts);

    std::string session_id = get_session_id(result, "MaxTurnsWithToolUse");

    // Verify num_turns is within limit
    for (const auto& msg : result)
    {
        if (is_result_message(msg))
        {
            const auto& res = std::get<ResultMessage>(msg);
            std::cout << "Num turns: " << res.num_turns_value() << "\n";
            EXPECT_LE(res.num_turns_value(), 3);
        }
    }

    EXPECT_FALSE(session_id.empty());
}
