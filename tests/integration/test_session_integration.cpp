#include "../test_utils.hpp"

#include <chrono>
#include <claude/client.hpp>
#include <claude/errors.hpp>
#include <claude/query.hpp>
#include <claude/types.hpp>
#include <gtest/gtest.h>
#include <iostream>
#include <thread>

using namespace claude;

// Integration tests for session management
// Skipped in CI (live API tests), enabled locally

class SessionIntegrationTest : public ::testing::Test
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

// Test 1: Session ID is returned from query
TEST_F(SessionIntegrationTest, SessionIdReturned)
{
    SKIP_IN_CI();

    ClaudeOptions opts;
    opts.permission_mode = "bypassPermissions";

    QueryResult result = query("Remember this number: 42. Just say 'OK'.", opts);

    std::string session_id = get_session_id(result, "SessionIdReturned");

    EXPECT_FALSE(session_id.empty()) << "Query should return a session ID";

    // Session ID format: UUID
    EXPECT_GT(session_id.length(), 30) << "Session ID should be UUID-like";
}

// Test 2: Resume a session and continue conversation
TEST_F(SessionIntegrationTest, ResumeSession)
{
    SKIP_IN_CI();

    ClaudeOptions opts;
    opts.permission_mode = "bypassPermissions";

    // First query - establish context
    std::cout << "=== First Query (establishing context) ===\n";
    QueryResult result1 = query("My favorite color is blue. Just say 'Noted'.", opts);

    std::string session_id = get_session_id(result1, "ResumeSession-First");
    ASSERT_FALSE(session_id.empty()) << "First query should return session ID";

    // Small delay to ensure session is saved
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Second query - resume and verify context
    std::cout << "\n=== Second Query (resuming session) ===\n";
    ClaudeOptions opts2;
    opts2.permission_mode = "bypassPermissions";
    opts2.resume = session_id; // Resume this session

    QueryResult result2 = query("What is my favorite color?", opts2);

    std::string session_id2 = get_session_id(result2, "ResumeSession-Second");
    std::string response = get_response(result2);

    std::cout << "Response: " << response << "\n";

    EXPECT_FALSE(session_id2.empty());
    // The response should mention "blue" if session was resumed correctly
}

// Test 3: Multiple turns in same session via session_id
TEST_F(SessionIntegrationTest, MultiTurnConversation)
{
    SKIP_IN_CI();

    ClaudeOptions opts;
    opts.permission_mode = "bypassPermissions";

    // Turn 1
    std::cout << "=== Turn 1 ===\n";
    QueryResult result1 =
        query("I'm going to give you a list. First item: apple. Say 'Got it'.", opts);
    std::string session_id = get_session_id(result1, "MultiTurn-1");
    ASSERT_FALSE(session_id.empty());

    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    // Turn 2 - resume
    std::cout << "\n=== Turn 2 ===\n";
    ClaudeOptions opts2;
    opts2.permission_mode = "bypassPermissions";
    opts2.resume = session_id;

    QueryResult result2 = query("Second item: banana. Say 'Got it'.", opts2);
    get_session_id(result2, "MultiTurn-2");

    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    // Turn 3 - ask about the list
    std::cout << "\n=== Turn 3 ===\n";
    ClaudeOptions opts3;
    opts3.permission_mode = "bypassPermissions";
    opts3.resume = session_id;

    QueryResult result3 = query("What items are in my list? Just list them.", opts3);
    std::string final_session_id = get_session_id(result3, "MultiTurn-3");
    std::string response = get_response(result3);

    std::cout << "Response: " << response << "\n";

    EXPECT_FALSE(final_session_id.empty());
}

// Test 4: Session with conversation_id
TEST_F(SessionIntegrationTest, ConversationId)
{
    SKIP_IN_CI();

    ClaudeOptions opts;
    opts.permission_mode = "bypassPermissions";

    QueryResult result = query("Hello!", opts);

    // Check for conversation_id in result
    for (const auto& msg : result)
    {
        if (is_result_message(msg))
        {
            const auto& res = std::get<ResultMessage>(msg);
            std::cout << "\n========================================\n";
            std::cout << "[ConversationId]\n";
            std::cout << "Session ID: " << res.session_id() << "\n";
            std::cout << "Conversation ID: " << res.conversation_id() << "\n";
            std::cout << "========================================\n";

            EXPECT_FALSE(res.session_id().empty());
            // Conversation ID may be empty depending on CLI version
        }
    }
}

// Test 5: New session each time (no resume)
TEST_F(SessionIntegrationTest, NewSessionEachQuery)
{
    SKIP_IN_CI();

    ClaudeOptions opts;
    opts.permission_mode = "bypassPermissions";

    // First query
    QueryResult result1 = query("Say 'one'.", opts);
    std::string session1 = get_session_id(result1, "NewSession-1");

    // Second query - new session (no session_id set)
    QueryResult result2 = query("Say 'two'.", opts);
    std::string session2 = get_session_id(result2, "NewSession-2");

    EXPECT_FALSE(session1.empty());
    EXPECT_FALSE(session2.empty());
    EXPECT_NE(session1, session2) << "Each query should create a new session";
}

// Test 6: Session with usage tracking
TEST_F(SessionIntegrationTest, SessionUsageTracking)
{
    SKIP_IN_CI();

    ClaudeOptions opts;
    opts.permission_mode = "bypassPermissions";

    QueryResult result = query("What is 2+2?", opts);

    for (const auto& msg : result)
    {
        if (is_result_message(msg))
        {
            const auto& res = std::get<ResultMessage>(msg);
            std::cout << "\n========================================\n";
            std::cout << "[SessionUsageTracking]\n";
            std::cout << "Session ID: " << res.session_id() << "\n";
            std::cout << "Input tokens: " << res.usage().input_tokens << "\n";
            std::cout << "Output tokens: " << res.usage().output_tokens << "\n";
            std::cout << "Cache creation tokens: " << res.usage().cache_creation_input_tokens
                      << "\n";
            std::cout << "Cache read tokens: " << res.usage().cache_read_input_tokens << "\n";
            std::cout << "Duration (ms): " << res.duration_ms_value() << "\n";
            std::cout << "API Duration (ms): " << res.duration_api_ms_value() << "\n";
            std::cout << "Num turns: " << res.num_turns_value() << "\n";
            if (res.total_cost_usd() > 0)
                std::cout << "Cost: $" << res.total_cost_usd() << "\n";
            std::cout << "========================================\n";

            EXPECT_FALSE(res.session_id().empty());
            EXPECT_GT(res.usage().input_tokens, 0);
            EXPECT_GT(res.usage().output_tokens, 0);
        }
    }
}

// Test 7: Multi-turn using ClaudeClient (alternative to query() + opts.resume)
// Shows persistent connection pattern vs separate calls
TEST_F(SessionIntegrationTest, ClientMultiTurnSession)
{
    SKIP_IN_CI();

    ClaudeOptions opts;
    opts.permission_mode = "bypassPermissions";

    ClaudeClient client(opts);
    client.connect();

    std::string session_id;

    // Turn 1: Establish context
    std::cout << "=== Turn 1 (ClaudeClient) ===\n";
    client.send_query("Remember: my pet's name is Max. Just say 'OK'.");
    for (const auto& msg : client.receive_messages())
    {
        if (is_result_message(msg))
        {
            session_id = std::get<ResultMessage>(msg).session_id();
            std::cout << "\n========================================\n";
            std::cout << "[ClientMultiTurnSession-Turn1]\n";
            std::cout << "Session ID: " << session_id << "\n";
            std::cout << "Resume: claude --resume " << session_id << "\n";
            std::cout << "========================================\n";
        }
        if (is_assistant_message(msg))
        {
            std::cout << "Response: " << get_text_content(std::get<AssistantMessage>(msg).content)
                      << "\n";
        }
    }
    ASSERT_FALSE(session_id.empty());

    // Turn 2: Query about context (pass session_id to continue)
    std::cout << "\n=== Turn 2 (ClaudeClient) ===\n";
    client.send_query("What is my pet's name?", session_id);
    std::string response;
    for (const auto& msg : client.receive_messages())
    {
        if (is_assistant_message(msg))
        {
            response = get_text_content(std::get<AssistantMessage>(msg).content);
            std::cout << "Response: " << response << "\n";
        }
    }

    client.disconnect();

    // Response should mention "Max" if session was continued correctly
    std::cout << "Final response: " << response << "\n";
}

// Test 8: Multi-turn using default session_id (Python parity)
// No manual session_id capture/passing needed - uses "default" automatically
TEST_F(SessionIntegrationTest, AutomaticSessionContinuity)
{
    SKIP_IN_CI();

    ClaudeOptions opts;
    opts.permission_mode = "bypassPermissions";

    ClaudeClient client(opts);
    client.connect();

    // Turn 1: Establish context - NO session_id passed (uses "default")
    std::cout << "=== Turn 1 (Auto Session) ===\n";
    client.send_query("My favorite fruit is mango. Just say 'Noted'.");
    std::string response1;
    for (const auto& msg : client.receive_messages())
    {
        if (is_assistant_message(msg))
        {
            response1 = get_text_content(std::get<AssistantMessage>(msg).content);
            std::cout << "Response: " << response1 << "\n";
        }
    }

    // Turn 2: Query about context - STILL no session_id passed
    // Uses the same "default" session_id automatically
    std::cout << "\n=== Turn 2 (Auto Session) ===\n";
    client.send_query("What is my favorite fruit?");
    std::string response2;
    for (const auto& msg : client.receive_messages())
    {
        if (is_assistant_message(msg))
        {
            response2 = get_text_content(std::get<AssistantMessage>(msg).content);
            std::cout << "Response: " << response2 << "\n";
        }
    }

    // Turn 3: Another follow-up - still no manual session_id
    std::cout << "\n=== Turn 3 (Auto Session) ===\n";
    client.send_query("And what color is that fruit typically?");
    std::string response3;
    for (const auto& msg : client.receive_messages())
    {
        if (is_assistant_message(msg))
        {
            response3 = get_text_content(std::get<AssistantMessage>(msg).content);
            std::cout << "Response: " << response3 << "\n";
        }
    }

    client.disconnect();

    // Response should reference "mango" if auto-session works
    std::cout << "\n=== Verification ===\n";
    std::cout << "Turn 2: " << response2 << "\n";

    bool context_maintained = (response2.find("mango") != std::string::npos ||
                               response2.find("Mango") != std::string::npos);

    EXPECT_TRUE(context_maintained)
        << "Automatic session continuity should maintain context across turns. "
        << "Response was: " << response2;
}
