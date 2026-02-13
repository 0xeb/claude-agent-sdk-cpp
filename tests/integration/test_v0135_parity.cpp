/**
 * @file test_v0135_parity.cpp
 * @brief Integration tests for v0.1.35 parity features
 *
 * Tests ThinkingConfig, EffortLevel, and the always-streaming one-shot
 * query path against a live Claude CLI.
 *
 * These tests require:
 * - CLAUDE_AGENT_SDK_RUN_LIVE_TESTS=1 environment variable
 * - `claude` CLI available in PATH or CLAUDE_CLI_PATH set
 */

#include "../test_utils.hpp"

#include <claude/errors.hpp>
#include <claude/query.hpp>
#include <gtest/gtest.h>
#include <iostream>

using namespace claude;

// ============================================================================
// ThinkingConfig Integration Tests
// ============================================================================

TEST(V0135Parity, ThinkingConfigAdaptiveWorks)
{
    SKIP_IN_CI();

    ClaudeOptions opts;
    opts.permission_mode = "bypassPermissions";
    opts.max_turns = 1;
    opts.thinking = ThinkingConfigAdaptive{};

    QueryResult result = query("What is 3 + 7? Answer with just the number.", opts);
    EXPECT_GT(result.messages().size(), 0);

    bool found_text = false;
    for (const auto& msg : result)
    {
        if (is_assistant_message(msg))
        {
            const auto& assistant = std::get<AssistantMessage>(msg);
            std::string text = get_text_content(assistant.content);
            std::cout << "[ThinkingAdaptive] Response: " << text << "\n";
            if (!text.empty())
                found_text = true;
        }
    }
    EXPECT_TRUE(found_text) << "Expected non-empty assistant response";
}

TEST(V0135Parity, ThinkingConfigEnabledWorks)
{
    SKIP_IN_CI();

    ClaudeOptions opts;
    opts.permission_mode = "bypassPermissions";
    opts.max_turns = 1;
    opts.thinking = ThinkingConfigEnabled(4096);

    QueryResult result = query("What is the capital of France? One word answer.", opts);
    EXPECT_GT(result.messages().size(), 0);

    bool found_text = false;
    for (const auto& msg : result)
    {
        if (is_assistant_message(msg))
        {
            const auto& assistant = std::get<AssistantMessage>(msg);
            std::string text = get_text_content(assistant.content);
            std::cout << "[ThinkingEnabled] Response: " << text << "\n";
            if (!text.empty())
                found_text = true;
        }
    }
    EXPECT_TRUE(found_text);
}

TEST(V0135Parity, ThinkingConfigDisabledWorks)
{
    SKIP_IN_CI();

    ClaudeOptions opts;
    opts.permission_mode = "bypassPermissions";
    opts.max_turns = 1;
    opts.thinking = ThinkingConfigDisabled{};

    QueryResult result = query("Say hello.", opts);
    EXPECT_GT(result.messages().size(), 0);

    bool found_text = false;
    for (const auto& msg : result)
    {
        if (is_assistant_message(msg))
        {
            const auto& assistant = std::get<AssistantMessage>(msg);
            std::string text = get_text_content(assistant.content);
            std::cout << "[ThinkingDisabled] Response: " << text << "\n";
            if (!text.empty())
                found_text = true;
        }
    }
    EXPECT_TRUE(found_text);
}

// ============================================================================
// EffortLevel Integration Tests
// NOTE: --effort flag requires Claude CLI >= 2.2.0. Tests gracefully handle
// older CLI versions by catching errors and printing a skip message.
// ============================================================================

TEST(V0135Parity, EffortLevelLow)
{
    SKIP_IN_CI();

    ClaudeOptions opts;
    opts.permission_mode = "bypassPermissions";
    opts.max_turns = 1;
    opts.effort = Effort::Low;

    try
    {
        QueryResult result = query("What is 2+2?", opts);
        if (result.messages().empty())
        {
            std::cout << "[EffortLow] No messages returned — CLI may not support --effort yet\n";
            GTEST_SKIP() << "CLI does not support --effort flag (need >= 2.2.0)";
        }

        bool found_result = false;
        for (const auto& msg : result)
        {
            if (is_result_message(msg))
            {
                found_result = true;
                const auto& result_msg = std::get<ResultMessage>(msg);
                std::cout << "[EffortLow] Cost: $" << result_msg.result.cost.total << "\n";
            }
        }
        EXPECT_TRUE(found_result);
    }
    catch (const ClaudeError& e)
    {
        std::cout << "[EffortLow] Error (likely unsupported --effort flag): " << e.what() << "\n";
        GTEST_SKIP() << "CLI does not support --effort flag";
    }
}

TEST(V0135Parity, EffortLevelMax)
{
    SKIP_IN_CI();

    ClaudeOptions opts;
    opts.permission_mode = "bypassPermissions";
    opts.max_turns = 1;
    opts.effort = Effort::Max;

    try
    {
        QueryResult result = query("What is 2+2?", opts);
        if (result.messages().empty())
        {
            GTEST_SKIP() << "CLI does not support --effort flag (need >= 2.2.0)";
        }

        bool found_text = false;
        for (const auto& msg : result)
        {
            if (is_assistant_message(msg))
            {
                const auto& assistant = std::get<AssistantMessage>(msg);
                std::string text = get_text_content(assistant.content);
                std::cout << "[EffortMax] Response: " << text << "\n";
                if (!text.empty())
                    found_text = true;
            }
        }
        EXPECT_TRUE(found_text);
    }
    catch (const ClaudeError& e)
    {
        std::cout << "[EffortMax] Error: " << e.what() << "\n";
        GTEST_SKIP() << "CLI does not support --effort flag";
    }
}

TEST(V0135Parity, EffortAndThinkingCombined)
{
    SKIP_IN_CI();

    ClaudeOptions opts;
    opts.permission_mode = "bypassPermissions";
    opts.max_turns = 1;
    opts.thinking = ThinkingConfigAdaptive{};
    opts.effort = Effort::High;

    try
    {
        QueryResult result = query("What is 15 * 17?", opts);
        if (result.messages().empty())
        {
            GTEST_SKIP() << "CLI does not support --effort flag (need >= 2.2.0)";
        }

        bool found_text = false;
        for (const auto& msg : result)
        {
            if (is_assistant_message(msg))
            {
                const auto& assistant = std::get<AssistantMessage>(msg);
                std::string text = get_text_content(assistant.content);
                std::cout << "[EffortHigh+Adaptive] Response: " << text << "\n";
                if (!text.empty())
                    found_text = true;
            }
        }
        EXPECT_TRUE(found_text);
    }
    catch (const ClaudeError& e)
    {
        std::cout << "[EffortAndThinking] Error: " << e.what() << "\n";
        GTEST_SKIP() << "CLI does not support --effort flag";
    }
}

// ============================================================================
// Always-Streaming One-Shot Tests
// ============================================================================

TEST(V0135Parity, OneShotQueryViaStreaming)
{
    // This is the critical test: one-shot queries now send the prompt via stdin
    // as a user message JSON, not via --print CLI arg. If the protocol change
    // is wrong, this test will fail.
    SKIP_IN_CI();

    ClaudeOptions opts;
    opts.permission_mode = "bypassPermissions";
    opts.max_turns = 1;

    QueryResult result = query("What color is the sky? Answer in one word.", opts);
    EXPECT_GT(result.messages().size(), 0);

    bool found_text = false;
    bool found_result = false;
    for (const auto& msg : result)
    {
        if (is_assistant_message(msg))
        {
            const auto& assistant = std::get<AssistantMessage>(msg);
            std::string text = get_text_content(assistant.content);
            std::cout << "[OneShotStreaming] Response: " << text << "\n";
            if (!text.empty())
                found_text = true;
        }
        if (is_result_message(msg))
        {
            found_result = true;
            const auto& result_msg = std::get<ResultMessage>(msg);
            std::cout << "[OneShotStreaming] Session: " << result_msg.result.session_id
                      << ", Cost: $" << result_msg.result.cost.total << "\n";
        }
    }

    EXPECT_TRUE(found_text) << "Expected non-empty assistant response";
    EXPECT_TRUE(found_result) << "Expected result message";
}

TEST(V0135Parity, OneShotWithDashesInPrompt)
{
    // Verify that prompts with dashes work correctly in the new streaming mode
    // (previously handled by -- separator in --print mode)
    SKIP_IN_CI();

    ClaudeOptions opts;
    opts.permission_mode = "bypassPermissions";
    opts.max_turns = 1;

    QueryResult result =
        query("What does the --help flag typically do in CLI tools? One sentence.", opts);
    EXPECT_GT(result.messages().size(), 0);

    bool found_text = false;
    for (const auto& msg : result)
    {
        if (is_assistant_message(msg))
        {
            const auto& assistant = std::get<AssistantMessage>(msg);
            std::string text = get_text_content(assistant.content);
            std::cout << "[DashesPrompt] Response: " << text << "\n";
            if (!text.empty())
                found_text = true;
        }
    }
    EXPECT_TRUE(found_text);
}
