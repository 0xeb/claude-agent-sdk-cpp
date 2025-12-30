#include "../test_utils.hpp"

#include <claude/errors.hpp>
#include <claude/query.hpp>
#include <gtest/gtest.h>

using namespace claude;

// Integration tests for query() function
// Skipped in CI (live API tests), enabled locally

TEST(QueryTest, BasicQuery)
{
    SKIP_IN_CI();

    ClaudeOptions opts;
    opts.permission_mode = "bypassPermissions";

    QueryResult result = query("What is 2+2? Answer in one word.", opts);

    // Should have messages
    EXPECT_GT(result.messages().size(), 0);

    // Check for assistant message
    bool found_assistant = false;
    for (const auto& msg : result)
    {
        if (is_assistant_message(msg))
        {
            found_assistant = true;
            const auto& assistant = std::get<AssistantMessage>(msg);
            std::string text = get_text_content(assistant.content);
            EXPECT_FALSE(text.empty());
            std::cout << "Assistant: " << text << "\n";
        }
    }

    EXPECT_TRUE(found_assistant);
}

TEST(QueryTest, WithOptions)
{
    SKIP_IN_CI();

    ClaudeOptions opts;
    opts.model = "claude-sonnet-4-5";
    opts.permission_mode = "bypassPermissions";
    opts.max_turns = 1;

    QueryResult result = query("Hello Claude!", opts);

    EXPECT_GT(result.messages().size(), 0);
}

TEST(QueryTest, ErrorHandling)
{
    SKIP_IN_CI();

    ClaudeOptions opts;
    opts.permission_mode = "bypassPermissions";

    // Empty prompt should fail
    EXPECT_THROW(query("", opts), ClaudeError);
}

TEST(QueryTest, ResultMessage)
{
    SKIP_IN_CI();

    ClaudeOptions opts;
    opts.permission_mode = "bypassPermissions";

    QueryResult result = query("Test", opts);

    // Should have a result message with usage stats
    bool found_result = false;
    for (const auto& msg : result)
    {
        if (is_result_message(msg))
        {
            found_result = true;
            const auto& result_msg = std::get<ResultMessage>(msg);
            EXPECT_FALSE(result_msg.result.session_id.empty());
            EXPECT_GT(result_msg.result.usage.input_tokens, 0);
            std::cout << "Tokens: " << result_msg.result.usage.input_tokens << " in, "
                      << result_msg.result.usage.output_tokens << " out\n";
        }
    }

    EXPECT_TRUE(found_result);
}

TEST(QueryTest, IteratorPattern)
{
    SKIP_IN_CI();

    ClaudeOptions opts;
    opts.permission_mode = "bypassPermissions";

    QueryResult result = query("Count to 3", opts);

    // Range-based for loop
    int count = 0;
    for (const auto& msg : result)
    {
        ++count;
        EXPECT_NO_THROW(if (is_assistant_message(msg)) {
            const auto& assistant = std::get<AssistantMessage>(msg);
        });
    }

    EXPECT_GT(count, 0);

    // Iterator operations
    auto it = result.begin();
    EXPECT_NE(it, result.end());
    ++it; // Pre-increment
    // it++;  // Post-increment (optional, may not reach end)
}
