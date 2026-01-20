/**
 * @file test_max_thinking_tokens.cpp
 * @brief Integration tests for max_thinking_tokens option
 *
 * Tests that --max-thinking-tokens is correctly passed to the CLI.
 * This is a regression test for a bug where --max-thinking-tokens was
 * placed AFTER --print -- (which ends option parsing per POSIX).
 *
 * Bug: BUG-1 (kb/bugs/bug-1.md)
 * Fix: subprocess_cli.cpp - moved max_thinking_tokens before input mode section
 */

#include "../test_utils.hpp"

#include <claude/errors.hpp>
#include <claude/query.hpp>
#include <gtest/gtest.h>
#include <iostream>

using namespace claude;

/**
 * Test that max_thinking_tokens option is accepted and doesn't cause errors.
 *
 * This test verifies that:
 * 1. The option is correctly passed to the CLI (before --print --)
 * 2. The query completes successfully
 * 3. No errors are thrown due to malformed CLI arguments
 */
TEST(MaxThinkingTokensTest, OptionIsAccepted)
{
    SKIP_IN_CI();

    ClaudeOptions opts;
    opts.permission_mode = "bypassPermissions";
    opts.max_turns = 1;
    opts.max_thinking_tokens = 1024; // Set thinking tokens limit

    // This should NOT throw - if it does, the --max-thinking-tokens flag
    // is likely being placed after --print -- (the bug we fixed)
    QueryResult result = query("What is 2+2? Answer briefly.", opts);

    // Should have messages
    EXPECT_GT(result.messages().size(), 0);

    // Should have assistant response with non-empty text
    bool found_text = false;
    for (const auto& msg : result)
    {
        if (is_assistant_message(msg))
        {
            const auto& assistant = std::get<AssistantMessage>(msg);
            std::string text = get_text_content(assistant.content);
            std::cout << "[MaxThinkingTokens] Response: " << text << "\n";
            if (!text.empty())
                found_text = true;
        }
    }

    EXPECT_TRUE(found_text) << "Expected at least one non-empty assistant response";
}

/**
 * Test with extended thinking model (if available).
 *
 * Extended thinking models may actually use the max_thinking_tokens limit.
 * This test uses claude-sonnet-4-5 which supports extended thinking.
 */
TEST(MaxThinkingTokensTest, WithExtendedThinkingModel)
{
    SKIP_IN_CI();

    ClaudeOptions opts;
    opts.permission_mode = "bypassPermissions";
    opts.max_turns = 1;
    opts.model = "claude-sonnet-4-5"; // Supports extended thinking
    opts.max_thinking_tokens = 2048;

    QueryResult result = query("Think step by step: what is 15 * 17?", opts);

    EXPECT_GT(result.messages().size(), 0);

    // Check for result message with token usage
    bool found_result = false;
    for (const auto& msg : result)
    {
        if (is_result_message(msg))
        {
            found_result = true;
            const auto& result_msg = std::get<ResultMessage>(msg);
            std::cout << "[MaxThinkingTokens] Input tokens: "
                      << result_msg.result.usage.input_tokens
                      << ", Output tokens: " << result_msg.result.usage.output_tokens << "\n";
        }
    }

    EXPECT_TRUE(found_result);
}

/**
 * Test that zero/unset max_thinking_tokens doesn't break anything.
 *
 * When max_thinking_tokens is not set (nullopt), the flag should not
 * be added to the command line at all.
 */
TEST(MaxThinkingTokensTest, UnsetDoesNotBreak)
{
    SKIP_IN_CI();

    ClaudeOptions opts;
    opts.permission_mode = "bypassPermissions";
    opts.max_turns = 1;
    // max_thinking_tokens NOT set (nullopt)

    QueryResult result = query("Hello!", opts);

    EXPECT_GT(result.messages().size(), 0);
}

/**
 * Test with a prompt containing dashes (edge case).
 *
 * This ensures that the -- separator and prompt handling work correctly
 * even when max_thinking_tokens is set.
 */
TEST(MaxThinkingTokensTest, PromptWithDashes)
{
    SKIP_IN_CI();

    ClaudeOptions opts;
    opts.permission_mode = "bypassPermissions";
    opts.max_turns = 1;
    opts.max_thinking_tokens = 1024; // Minimum required by API

    // Prompt with dashes that could be confused for flags
    QueryResult result = query("What does --help do in CLI tools? Answer in one sentence.", opts);

    EXPECT_GT(result.messages().size(), 0);

    bool found_text = false;
    for (const auto& msg : result)
    {
        if (is_assistant_message(msg))
        {
            const auto& assistant = std::get<AssistantMessage>(msg);
            std::string text = get_text_content(assistant.content);
            std::cout << "[MaxThinkingTokens] Dashes prompt response: " << text << "\n";
            if (!text.empty())
                found_text = true;
        }
    }

    EXPECT_TRUE(found_text) << "Expected at least one non-empty assistant response";
}
