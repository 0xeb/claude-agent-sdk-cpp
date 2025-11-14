#include <chrono>
#include <claude/client.hpp>
#include <gtest/gtest.h>
#include <thread>

using namespace claude;

// Integration tests for ClaudeClient (DISABLED by default, enable manually)

TEST(ClientIntegrationTest, DISABLED_ConnectDisconnect)
{
    ClaudeOptions opts;
    opts.permission_mode = "bypassPermissions";

    ClaudeClient client(opts);

    EXPECT_FALSE(client.is_connected());

    client.connect();
    EXPECT_TRUE(client.is_connected());

    client.disconnect();
    EXPECT_FALSE(client.is_connected());
}

TEST(ClientIntegrationTest, DISABLED_SingleQuery)
{
    ClaudeOptions opts;
    opts.permission_mode = "bypassPermissions";

    ClaudeClient client(opts);
    client.connect();

    client.send_query("What is 2+2? Answer with just the number.");

    auto messages = client.receive_response();

    EXPECT_GT(messages.size(), 0);

    bool found_assistant = false;
    for (const auto& msg : messages)
    {
        if (is_assistant_message(msg))
        {
            found_assistant = true;
            const auto& assistant = std::get<AssistantMessage>(msg);
            std::string text = get_text_content(assistant.content);
            EXPECT_FALSE(text.empty());
        }
    }

    EXPECT_TRUE(found_assistant);

    client.disconnect();
}

TEST(ClientIntegrationTest, DISABLED_MultipleQueries)
{
    ClaudeOptions opts;
    opts.permission_mode = "bypassPermissions";

    ClaudeClient client(opts);
    client.connect();

    // First query
    client.send_query("What is 2+2? Answer with just the number.");
    auto response1 = client.receive_response();
    EXPECT_GT(response1.size(), 0);

    bool found_result1 = false;
    for (const auto& msg : response1)
    {
        if (is_result_message(msg))
        {
            found_result1 = true;
        }
    }
    EXPECT_TRUE(found_result1);

    // Second query (same session - should remember context)
    client.send_query("What did I just ask you about?");
    auto response2 = client.receive_response();
    EXPECT_GT(response2.size(), 0);

    bool found_assistant2 = false;
    for (const auto& msg : response2)
    {
        if (is_assistant_message(msg))
        {
            found_assistant2 = true;
        }
    }
    EXPECT_TRUE(found_assistant2);

    client.disconnect();
}

TEST(ClientIntegrationTest, DISABLED_StreamingMessages)
{
    ClaudeOptions opts;
    opts.permission_mode = "bypassPermissions";

    ClaudeClient client(opts);
    client.connect();

    client.send_query("Count from 1 to 3.");

    auto stream = client.receive_messages();

    int count = 0;
    bool found_result = false;
    for (const auto& msg : stream)
    {
        ++count;

        if (is_result_message(msg))
        {
            found_result = true;
            break; // End of response
        }
    }

    EXPECT_GT(count, 0);
    EXPECT_TRUE(found_result);

    client.disconnect();
}

TEST(ClientIntegrationTest, DISABLED_MessageStreamIterator)
{
    ClaudeOptions opts;
    opts.permission_mode = "bypassPermissions";

    ClaudeClient client(opts);
    client.connect();

    client.send_query("Say 'Hello'");

    auto stream = client.receive_messages();
    auto it = stream.begin();
    auto end = stream.end();

    EXPECT_NE(it, end);

    int msg_count = 0;
    while (it != end)
    {
        ++msg_count;
        const auto& msg = *it;

        if (is_result_message(msg))
        {
            break;
        }

        ++it;
    }

    EXPECT_GT(msg_count, 0);

    client.disconnect();
}

TEST(ClientIntegrationTest, DISABLED_MoveConstructor)
{
    ClaudeOptions opts;
    opts.permission_mode = "bypassPermissions";

    ClaudeClient client1(opts);
    client1.connect();
    EXPECT_TRUE(client1.is_connected());

    // Move construct
    ClaudeClient client2(std::move(client1));
    EXPECT_TRUE(client2.is_connected());

    client2.disconnect();
}

TEST(ClientIntegrationTest, DISABLED_MoveAssignment)
{
    ClaudeOptions opts;
    opts.permission_mode = "bypassPermissions";

    ClaudeClient client1(opts);
    client1.connect();
    EXPECT_TRUE(client1.is_connected());

    // Move assign
    ClaudeClient client2(opts);
    client2 = std::move(client1);
    EXPECT_TRUE(client2.is_connected());

    client2.disconnect();
}

TEST(ClientIntegrationTest, DISABLED_ReceiveResponseConvenience)
{
    ClaudeOptions opts;
    opts.permission_mode = "bypassPermissions";

    ClaudeClient client(opts);
    client.connect();

    client.send_query("What is 10 * 5? Answer with just the number.");

    // Use convenience method
    auto messages = client.receive_response();

    EXPECT_GT(messages.size(), 0);

    bool found_assistant = false;
    bool found_result = false;

    for (const auto& msg : messages)
    {
        if (is_assistant_message(msg))
        {
            found_assistant = true;
        }
        if (is_result_message(msg))
        {
            found_result = true;
        }
    }

    EXPECT_TRUE(found_assistant);
    EXPECT_TRUE(found_result);

    client.disconnect();
}
