#include "../test_utils.hpp"

#include <claude/errors.hpp>
#include <claude/query.hpp>
#include <claude/types.hpp>
#include <gtest/gtest.h>
#include <iostream>

using namespace claude;

// Integration tests for SandboxSettings
// NOTE: Sandbox is primarily a Linux feature (uses bubblewrap/landlock)
// On macOS/Windows, sandbox settings are passed but may not enforce restrictions
// Skipped in CI (live API tests), enabled locally

class SandboxIntegrationTest : public ::testing::Test
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
                std::cout << "Resume: claude --resume " << session_id << "\n";
                std::cout << "========================================\n";
            }
        }
        return session_id;
    }
};

// Test 1: Sandbox enabled with autoAllowBashIfSandboxed
// When sandbox is enabled and autoAllowBashIfSandboxed=true, bash commands should run
// without permission prompts (within sandbox restrictions)
TEST_F(SandboxIntegrationTest, SandboxWithAutoAllowBash)
{
    SKIP_IN_CI();

    ClaudeOptions opts;
    opts.permission_mode = "bypassPermissions";

    SandboxSettings sandbox;
    sandbox.enabled = true;
    sandbox.autoAllowBashIfSandboxed = true;
    opts.sandbox = sandbox;

    QueryResult result =
        query("Run 'echo hello' and tell me what it outputs. Just the output, nothing else.", opts);

    std::string session_id = get_session_id(result, "SandboxWithAutoAllowBash");

    EXPECT_FALSE(session_id.empty());
}

// Test 2: Sandbox with excluded commands
// Commands in excludedCommands list should run outside the sandbox
TEST_F(SandboxIntegrationTest, SandboxWithExcludedCommands)
{
    SKIP_IN_CI();

    ClaudeOptions opts;
    opts.permission_mode = "bypassPermissions";

    SandboxSettings sandbox;
    sandbox.enabled = true;
    sandbox.excludedCommands = std::vector<std::string>{"git", "cmake"};
    opts.sandbox = sandbox;

    QueryResult result =
        query("Run 'git --version' and tell me the version. Just the version.", opts);

    std::string session_id = get_session_id(result, "SandboxWithExcludedCommands");

    EXPECT_FALSE(session_id.empty());
}

// Test 3: Sandbox with network configuration
// Test that network settings are passed correctly
TEST_F(SandboxIntegrationTest, SandboxWithNetworkConfig)
{
    SKIP_IN_CI();

    ClaudeOptions opts;
    opts.permission_mode = "bypassPermissions";

    SandboxSettings sandbox;
    sandbox.enabled = true;

    SandboxNetworkConfig network;
    network.allowLocalBinding = true;
    sandbox.network = network;

    opts.sandbox = sandbox;

    // Simple query to verify sandbox config is accepted
    QueryResult result = query("What is 2+2? Answer with just the number.", opts);

    std::string session_id = get_session_id(result, "SandboxWithNetworkConfig");

    EXPECT_FALSE(session_id.empty());
}

// Test 4: Sandbox disabled explicitly
// When sandbox.enabled = false, commands should run without sandbox restrictions
TEST_F(SandboxIntegrationTest, SandboxDisabled)
{
    SKIP_IN_CI();

    ClaudeOptions opts;
    opts.permission_mode = "bypassPermissions";

    SandboxSettings sandbox;
    sandbox.enabled = false;
    opts.sandbox = sandbox;

    QueryResult result = query("What is your name? One word.", opts);

    std::string session_id = get_session_id(result, "SandboxDisabled");

    EXPECT_FALSE(session_id.empty());
}

// Test 5: Full sandbox configuration
// Test complex sandbox configuration with multiple settings
TEST_F(SandboxIntegrationTest, FullSandboxConfig)
{
    SKIP_IN_CI();

    ClaudeOptions opts;
    opts.permission_mode = "bypassPermissions";

    SandboxSettings sandbox;
    sandbox.enabled = true;
    sandbox.autoAllowBashIfSandboxed = true;
    sandbox.excludedCommands = std::vector<std::string>{"git"};

    SandboxNetworkConfig network;
    network.allowLocalBinding = true;
    network.allowAllUnixSockets = false;
    sandbox.network = network;

    SandboxIgnoreViolations ignore;
    ignore.file = std::vector<std::string>{"/tmp/*"};
    sandbox.ignoreViolations = ignore;

    opts.sandbox = sandbox;

    QueryResult result = query("Echo 'sandbox test'. Just the output.", opts);

    std::string session_id = get_session_id(result, "FullSandboxConfig");

    EXPECT_FALSE(session_id.empty());

    // Verify we got a result message
    bool found_result = false;
    for (const auto& msg : result)
    {
        if (is_result_message(msg))
        {
            found_result = true;
            const auto& res = std::get<ResultMessage>(msg);
            std::cout << "Tokens: " << res.usage().input_tokens << " in, "
                      << res.usage().output_tokens << " out\n";
        }
    }
    EXPECT_TRUE(found_result);
}
