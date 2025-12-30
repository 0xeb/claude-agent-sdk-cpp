#include "../test_utils.hpp"

#include <claude/errors.hpp>
#include <claude/query.hpp>
#include <claude/types.hpp>
#include <gtest/gtest.h>
#include <iostream>

using namespace claude;

// Integration tests for AgentDefinition
// Skipped in CI (live API tests), enabled locally

class AgentIntegrationTest : public ::testing::Test
{
  protected:
    // Helper to extract and print session ID from result
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
                std::cout << "Resume with: claude --resume " << session_id << "\n";
                std::cout << "========================================\n";
            }
        }
        return session_id;
    }

    // Helper to extract assistant response text
    std::string get_response_text(const QueryResult& result)
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

// Test 1: Single agent with tools (code reviewer pattern)
TEST_F(AgentIntegrationTest, CodeReviewerAgent)
{
    SKIP_IN_CI();

    ClaudeOptions opts;
    opts.permission_mode = "bypassPermissions";

    AgentDefinition code_reviewer;
    code_reviewer.description = "Reviews code for best practices and potential issues";
    code_reviewer.prompt = "You are a code reviewer. Analyze code for bugs, performance issues, "
                           "security vulnerabilities, and adherence to best practices. Be concise.";
    code_reviewer.tools = std::vector<std::string>{"Read", "Grep"};
    code_reviewer.model = "sonnet";
    opts.agents["code-reviewer"] = code_reviewer;

    QueryResult result =
        query("Use the code-reviewer agent to list 3 main types in this SDK's types.hpp. "
              "Just list the type names, nothing else.",
              opts);

    std::string session_id = get_session_id(result, "CodeReviewerAgent");
    std::string response = get_response_text(result);

    std::cout << "Response: " << response << "\n";

    EXPECT_FALSE(session_id.empty()) << "Should have a session ID";
    // Note: response may be empty if agent uses tools without text output
}

// Test 2: Agent with model override
TEST_F(AgentIntegrationTest, DocumentationWriterAgent)
{
    SKIP_IN_CI();

    ClaudeOptions opts;
    opts.permission_mode = "bypassPermissions";

    AgentDefinition doc_writer;
    doc_writer.description = "Writes comprehensive documentation";
    doc_writer.prompt = "You are a technical documentation expert. Write clear, "
                        "comprehensive documentation with examples. Be concise.";
    doc_writer.tools = std::vector<std::string>{"Read"};
    doc_writer.model = "haiku"; // Use faster model for docs
    opts.agents["doc-writer"] = doc_writer;

    QueryResult result =
        query("Use the doc-writer agent to explain what ClaudeOptions is in one sentence.", opts);

    std::string session_id = get_session_id(result, "DocumentationWriterAgent");
    std::string response = get_response_text(result);

    std::cout << "Response: " << response << "\n";

    EXPECT_FALSE(session_id.empty());
    // Note: response may be empty if agent uses tools without text output
}

// Test 3: Agent without model (inherits from parent)
TEST_F(AgentIntegrationTest, AnalyzerAgentNoModel)
{
    SKIP_IN_CI();

    ClaudeOptions opts;
    opts.permission_mode = "bypassPermissions";
    opts.model = "claude-sonnet-4-20250514"; // Parent model

    AgentDefinition analyzer;
    analyzer.description = "Analyzes code structure and patterns";
    analyzer.prompt = "You are a code analyzer. Examine code structure and patterns. Be concise.";
    analyzer.tools = std::vector<std::string>{"Read", "Grep", "Glob"};
    // No model set - should inherit from parent
    opts.agents["analyzer"] = analyzer;

    QueryResult result =
        query("Use the analyzer agent to count how many .cpp files are in the examples/ directory. "
              "Just give me the number.",
              opts);

    std::string session_id = get_session_id(result, "AnalyzerAgentNoModel");
    std::string response = get_response_text(result);

    std::cout << "Response: " << response << "\n";

    EXPECT_FALSE(session_id.empty());
    EXPECT_FALSE(response.empty());
}

// Test 4: Multiple agents defined (choose one to use)
TEST_F(AgentIntegrationTest, MultipleAgentsDefined)
{
    SKIP_IN_CI();

    ClaudeOptions opts;
    opts.permission_mode = "bypassPermissions";

    // Define analyzer agent
    AgentDefinition analyzer;
    analyzer.description = "Analyzes code structure";
    analyzer.prompt = "You are a code analyzer. Be concise.";
    analyzer.tools = std::vector<std::string>{"Read", "Grep", "Glob"};
    opts.agents["analyzer"] = analyzer;

    // Define tester agent
    AgentDefinition tester;
    tester.description = "Creates and runs tests";
    tester.prompt = "You are a testing expert. Be concise.";
    tester.tools = std::vector<std::string>{"Read", "Bash"};
    tester.model = "sonnet";
    opts.agents["tester"] = tester;

    // Define helper agent
    AgentDefinition helper;
    helper.description = "General purpose helper";
    helper.prompt = "You are a helpful assistant. Be concise.";
    helper.tools = std::vector<std::string>{"Read"};
    opts.agents["helper"] = helper;

    // Use the analyzer
    QueryResult result = query(
        "Use the analyzer agent to find the main include file for this SDK. Just the filename.",
        opts);

    std::string session_id = get_session_id(result, "MultipleAgentsDefined");
    std::string response = get_response_text(result);

    std::cout << "Response: " << response << "\n";

    EXPECT_FALSE(session_id.empty());
}

// Test 5: Agent with Bash tool
TEST_F(AgentIntegrationTest, AgentWithBashTool)
{
    SKIP_IN_CI();

    ClaudeOptions opts;
    opts.permission_mode = "bypassPermissions";

    AgentDefinition builder;
    builder.description = "Builds and checks code";
    builder.prompt = "You are a build expert. You can run bash commands. Be concise.";
    builder.tools = std::vector<std::string>{"Bash", "Read"};
    builder.model = "sonnet";
    opts.agents["builder"] = builder;

    QueryResult result = query(
        "Use the builder agent to check the CMake version installed. Just give the version number.",
        opts);

    std::string session_id = get_session_id(result, "AgentWithBashTool");
    std::string response = get_response_text(result);

    std::cout << "Response: " << response << "\n";

    EXPECT_FALSE(session_id.empty());
}

// Test 6: Agent with all standard tools
TEST_F(AgentIntegrationTest, AgentWithAllTools)
{
    SKIP_IN_CI();

    ClaudeOptions opts;
    opts.permission_mode = "bypassPermissions";

    AgentDefinition full_agent;
    full_agent.description = "Full-featured agent with all tools";
    full_agent.prompt = "You are a versatile assistant with access to all standard tools. "
                        "Use them wisely. Be concise.";
    full_agent.tools =
        std::vector<std::string>{"Read", "Write", "Edit", "Bash", "Grep", "Glob", "WebFetch"};
    full_agent.model = "sonnet";
    opts.agents["full-agent"] = full_agent;

    QueryResult result = query("Use the full-agent to tell me the SDK version from version.hpp. "
                               "Just the version string.",
                               opts);

    std::string session_id = get_session_id(result, "AgentWithAllTools");
    std::string response = get_response_text(result);

    std::cout << "Response: " << response << "\n";

    EXPECT_FALSE(session_id.empty());
}

// Test 7: Verify session can be retrieved after agent runs
TEST_F(AgentIntegrationTest, SessionContainsAgentInteraction)
{
    SKIP_IN_CI();

    ClaudeOptions opts;
    opts.permission_mode = "bypassPermissions";

    AgentDefinition counter;
    counter.description = "Counts things";
    counter.prompt = "You count things and report the count. Be concise.";
    counter.tools = std::vector<std::string>{"Glob"};
    counter.model = "haiku";
    opts.agents["counter"] = counter;

    QueryResult result = query("Use the counter agent to count .hpp files in the include/claude "
                               "directory. Just the count.",
                               opts);

    // Check that result contains session info and usage stats
    bool found_result = false;
    for (const auto& msg : result)
    {
        if (is_result_message(msg))
        {
            found_result = true;
            const auto& res = std::get<ResultMessage>(msg);
            std::cout << "\n========================================\n";
            std::cout << "[SessionContainsAgentInteraction]\n";
            std::cout << "Session ID: " << res.session_id() << "\n";
            std::cout << "Input tokens: " << res.usage().input_tokens << "\n";
            std::cout << "Output tokens: " << res.usage().output_tokens << "\n";
            if (res.total_cost_usd() > 0)
                std::cout << "Cost: $" << res.total_cost_usd() << "\n";
            std::cout << "Resume: claude --resume " << res.session_id() << "\n";
            std::cout << "========================================\n";

            EXPECT_FALSE(res.session_id().empty());
            EXPECT_GT(res.usage().input_tokens, 0);
        }
    }

    EXPECT_TRUE(found_result) << "Should have a ResultMessage with session info";
}

// Test 8: Agent with special characters in name
TEST_F(AgentIntegrationTest, AgentWithSpecialName)
{
    SKIP_IN_CI();

    ClaudeOptions opts;
    opts.permission_mode = "bypassPermissions";

    AgentDefinition agent;
    agent.description = "Agent with hyphenated name";
    agent.prompt = "You are helpful. Be concise.";
    agent.tools = std::vector<std::string>{"Read"};
    opts.agents["my-special-agent_v2"] = agent;

    QueryResult result = query("Use the my-special-agent_v2 agent to say hello in one word.", opts);

    std::string session_id = get_session_id(result, "AgentWithSpecialName");

    EXPECT_FALSE(session_id.empty());
}
