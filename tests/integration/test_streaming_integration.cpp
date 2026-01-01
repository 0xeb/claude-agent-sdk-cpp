#include "../test_utils.hpp"

#include <claude/client.hpp>
#include <claude/errors.hpp>
#include <claude/query.hpp>
#include <claude/types.hpp>
#include <gtest/gtest.h>
#include <iostream>
#include <vector>

using namespace claude;

// Integration tests for ClaudeClient streaming and message handling
// Skipped in CI (live API tests), enabled locally

class StreamingIntegrationTest : public ::testing::Test
{
  protected:
    void print_session_info(const std::string& session_id, const std::string& test_name)
    {
        std::cout << "\n========================================\n";
        std::cout << "[" << test_name << "]\n";
        std::cout << "Session ID: " << session_id << "\n";
        std::cout << "Resume: claude --resume " << session_id << "\n";
        std::cout << "========================================\n";
    }
};

// Test 1: ClaudeClient basic connect/query/disconnect
TEST_F(StreamingIntegrationTest, ClientBasicUsage)
{
    SKIP_IN_CI();

    ClaudeOptions opts;
    opts.permission_mode = "bypassPermissions";

    ClaudeClient client(opts);
    client.connect();
    EXPECT_TRUE(client.is_connected());

    client.send_query("Say 'Hello' and nothing else.");

    std::string session_id;
    std::string response;

    // Use streaming iterator
    for (const auto& msg : client.receive_messages())
    {
        if (is_result_message(msg))
            session_id = std::get<ResultMessage>(msg).session_id();
        if (is_assistant_message(msg))
            response = get_text_content(std::get<AssistantMessage>(msg).content);
    }

    print_session_info(session_id, "ClientBasicUsage");
    std::cout << "Response: " << response << "\n";

    client.disconnect();
    EXPECT_FALSE(client.is_connected());
    EXPECT_FALSE(session_id.empty());
}

// Test 2: ClaudeClient multi-turn conversation
TEST_F(StreamingIntegrationTest, ClientMultiTurn)
{
    SKIP_IN_CI();

    ClaudeOptions opts;
    opts.permission_mode = "bypassPermissions";

    ClaudeClient client(opts);
    client.connect();

    std::string session_id;

    // Turn 1: Establish context
    client.send_query("My favorite number is 42. Just say 'Noted'.");
    for (const auto& msg : client.receive_messages())
        if (is_result_message(msg))
            session_id = std::get<ResultMessage>(msg).session_id();
    print_session_info(session_id, "ClientMultiTurn-Turn1");
    ASSERT_FALSE(session_id.empty());

    // Turn 2: Ask about context (pass session_id)
    client.send_query("What is my favorite number?", session_id);
    std::string response;
    for (const auto& msg : client.receive_messages())
        if (is_assistant_message(msg))
            response = get_text_content(std::get<AssistantMessage>(msg).content);
    std::cout << "Turn 2 Response: " << response << "\n";

    client.disconnect();
}

// Test 3: ClaudeClient receive_response() blocking API
TEST_F(StreamingIntegrationTest, ClientReceiveResponse)
{
    SKIP_IN_CI();

    ClaudeOptions opts;
    opts.permission_mode = "bypassPermissions";

    ClaudeClient client(opts);
    client.connect();

    client.send_query("What is 2+2? Answer with just the number.");

    // Use blocking receive_response() instead of streaming
    std::vector<Message> messages = client.receive_response();

    std::string session_id;
    std::string response;
    for (const auto& msg : messages)
    {
        if (is_result_message(msg))
            session_id = std::get<ResultMessage>(msg).session_id();
        if (is_assistant_message(msg))
            response = get_text_content(std::get<AssistantMessage>(msg).content);
    }

    print_session_info(session_id, "ClientReceiveResponse");
    std::cout << "Response: " << response << "\n";

    client.disconnect();
    EXPECT_FALSE(session_id.empty());
}

// Test 4: Compare query() vs ClaudeClient (both should work)
TEST_F(StreamingIntegrationTest, QueryVsClient)
{
    SKIP_IN_CI();

    ClaudeOptions opts;
    opts.permission_mode = "bypassPermissions";

    // Method 1: Simple query() function
    QueryResult result = query("Say 'one'.", opts);
    std::string session1;
    for (const auto& msg : result)
        if (is_result_message(msg))
            session1 = std::get<ResultMessage>(msg).session_id();
    print_session_info(session1, "QueryVsClient-query()");

    // Method 2: ClaudeClient
    ClaudeClient client(opts);
    client.connect();
    client.send_query("Say 'two'.");
    std::string session2;
    for (const auto& msg : client.receive_messages())
        if (is_result_message(msg))
            session2 = std::get<ResultMessage>(msg).session_id();
    client.disconnect();
    print_session_info(session2, "QueryVsClient-ClaudeClient");

    EXPECT_FALSE(session1.empty());
    EXPECT_FALSE(session2.empty());
    EXPECT_NE(session1, session2) << "Different sessions";
}

// Test 5: Multi-turn WITHOUT manual session_id passing (Python parity)
// Verifies that using default session_id="default" maintains conversation context
TEST_F(StreamingIntegrationTest, ClientMultiTurnAutoSession)
{
    SKIP_IN_CI();

    ClaudeOptions opts;
    opts.permission_mode = "bypassPermissions";

    ClaudeClient client(opts);
    client.connect();

    // Turn 1: Establish context - no session_id passed (uses default)
    client.send_query("Remember this secret code: ALPHA-7. Just say 'Remembered'.");
    std::string response1;
    for (const auto& msg : client.receive_messages())
        if (is_assistant_message(msg))
            response1 = get_text_content(std::get<AssistantMessage>(msg).content);
    std::cout << "Turn 1 Response: " << response1 << "\n";

    // Turn 2: Ask about context - STILL no session_id passed (uses default)
    // This should work because both queries use session_id="default"
    client.send_query("What was the secret code I told you?");
    std::string response2;
    for (const auto& msg : client.receive_messages())
        if (is_assistant_message(msg))
            response2 = get_text_content(std::get<AssistantMessage>(msg).content);
    std::cout << "Turn 2 Response: " << response2 << "\n";

    client.disconnect();

    // Response should contain "ALPHA-7" if session context was maintained
    EXPECT_TRUE(response2.find("ALPHA") != std::string::npos ||
                response2.find("7") != std::string::npos)
        << "Multi-turn context should be maintained without manual session_id. "
        << "Response was: " << response2;
}

// Test 6: ClaudeClient get_pid() and server info
TEST_F(StreamingIntegrationTest, ClientProcessInfo)
{
    SKIP_IN_CI();

    ClaudeOptions opts;
    opts.permission_mode = "bypassPermissions";

    ClaudeClient client(opts);

    EXPECT_EQ(client.get_pid(), 0) << "No PID before connect";

    client.connect();

    long pid = client.get_pid();
    std::cout << "CLI Process ID: " << pid << "\n";
    EXPECT_GT(pid, 0) << "Should have valid PID after connect";

    auto server_info = client.get_server_info();
    if (server_info)
        std::cout << "Server info available\n";

    client.send_query("Say 'test'.");
    client.receive_response();
    client.disconnect();
}
