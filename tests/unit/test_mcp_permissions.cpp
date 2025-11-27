#include <claude/mcp.hpp>
#include <gtest/gtest.h>

using namespace claude;
using json = nlohmann::json;

// Refactored per Python SDK parity guidelines: unit tests invoke MCP API
// directly (no MockTransport + connect()). Permission enforcement tested
// via E2E tests with real CLI. See kb/sync2/agent-testing.md for rationale.

TEST(McpPermissionsTest, ToolPermissionLogic)
{
    using namespace claude::mcp;

    auto calc_tool =
        make_tool("calculate", "Calculate", [](double a, double b) -> double { return a + b; });

    auto server = create_server("test", "1.0", calc_tool);

    json list_req = {{"jsonrpc", "2.0"}, {"id", 1}, {"method", "tools/list"}};
    auto list_response = server(list_req);

    ASSERT_TRUE(list_response.contains("result"));
    ASSERT_TRUE(list_response["result"].contains("tools"));
    auto tools = list_response["result"]["tools"];
    ASSERT_EQ(tools.size(), 1);
    EXPECT_EQ(tools[0]["name"], "calculate");
}
