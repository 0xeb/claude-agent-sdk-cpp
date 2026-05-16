/// Phase 4A (T42, T44): test new ClaudeClient methods. Since we don't have a
/// live CLI in unit tests, these primarily verify that the methods exist with
/// the right signatures and that they correctly throw CLIConnectionError when
/// not connected.
#include <claude/client.hpp>
#include <claude/errors.hpp>
#include <claude/types.hpp>
#include <gtest/gtest.h>

using namespace claude;

TEST(ClientV0282, GetContextUsageThrowsWhenNotConnected)
{
    ClaudeClient client{ClaudeOptions{}};
    EXPECT_THROW(client.get_context_usage(), CLIConnectionError);
}

TEST(ClientV0282, ReconnectMcpServerThrowsWhenNotConnected)
{
    ClaudeClient client{ClaudeOptions{}};
    EXPECT_THROW(client.reconnect_mcp_server("my-server"), CLIConnectionError);
}

TEST(ClientV0282, ToggleMcpServerThrowsWhenNotConnected)
{
    ClaudeClient client{ClaudeOptions{}};
    EXPECT_THROW(client.toggle_mcp_server("my-server", true), CLIConnectionError);
    EXPECT_THROW(client.toggle_mcp_server("my-server", false), CLIConnectionError);
}

TEST(ClientV0282, StopTaskThrowsWhenNotConnected)
{
    ClaudeClient client{ClaudeOptions{}};
    EXPECT_THROW(client.stop_task("task-1"), CLIConnectionError);
}

TEST(ClientV0282, ContextUsageResponseFromJson)
{
    json j = R"({
        "categories":[{"name":"system","tokens":100,"color":"red"}],
        "totalTokens":1000,"maxTokens":200000,"rawMaxTokens":200000,
        "percentage":0.5,"model":"claude","isAutoCompactEnabled":true
    })"_json;
    auto r = ContextUsageResponse::from_json(j);
    EXPECT_EQ(r.totalTokens, 1000);
    EXPECT_EQ(r.maxTokens, 200000);
    EXPECT_NEAR(r.percentage, 0.5, 1e-9);
    EXPECT_EQ(r.model, "claude");
    EXPECT_TRUE(r.isAutoCompactEnabled);
    ASSERT_EQ(r.categories.size(), 1u);
    EXPECT_EQ(r.categories[0].name, "system");
    EXPECT_EQ(r.categories[0].tokens, 100);
}
