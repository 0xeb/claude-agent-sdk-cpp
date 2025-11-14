#include <gtest/gtest.h>
#include <claude/mcp.hpp>

using namespace claude;
using json = nlohmann::json;

// Refactored per Python SDK parity guidelines: unit tests invoke handlers
// directly without client lifecycle (no MockTransport + connect()).
// See kb/sync2/agent-testing.md for rationale.

TEST(McpMessageRoutingTest, HandlerInvocationDirect) {
    using namespace claude::mcp;

    auto add_tool = make_tool("add", "Add numbers", [](double a, double b) -> double {
        return a + b;
    });

    auto server = create_server("calc", "1.0", add_tool);

    json call_req = {
        {"jsonrpc", "2.0"},
        {"id", 1},
        {"method", "tools/call"},
        {"params", {{"name", "add"}, {"arguments", {{"arg0", 2.0}, {"arg1", 3.0}}}}}
    };

    auto response = server(call_req);

    EXPECT_EQ(response["jsonrpc"], "2.0");
    EXPECT_EQ(response["id"], 1);
    ASSERT_TRUE(response.contains("result"));
    ASSERT_TRUE(response["result"].contains("content"));

    auto content = response["result"]["content"];
    ASSERT_TRUE(content.is_array());
    ASSERT_FALSE(content.empty());
    EXPECT_EQ(content[0]["type"], "text");
    EXPECT_EQ(content[0]["text"], "5");
}
