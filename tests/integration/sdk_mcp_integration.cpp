#include <gtest/gtest.h>
#include <claude/client.hpp>
#include <claude/mcp.hpp>
#include <thread>
#include <chrono>

using namespace claude;
using json = nlohmann::json;

// Refactored per Python SDK parity: MCP routing tests removed (used
// MockTransport + connect() pattern not in Python SDK). MCP functionality
// tested via pure unit tests below and E2E tests with real CLI.

// Label: conformance
TEST(SdkMcpIntegration, ToolCreation)
{
    using namespace claude::mcp;

    auto echo = make_tool("echo", "Echo input", [](std::string input) -> std::string {
        return input;
    }, std::vector<std::string>{"input"});

    auto server = create_server("test", "1.0", echo);

    json call_req = {
        {"jsonrpc", "2.0"},
        {"id", 1},
        {"method", "tools/call"},
        {"params", {{"name", "echo"}, {"arguments", {{"input", "test"}}}}}
    };

    auto response = server(call_req);

    ASSERT_TRUE(response.contains("result"));
    ASSERT_TRUE(response["result"].contains("content"));
    ASSERT_TRUE(response["result"]["content"].is_array());
    ASSERT_FALSE(response["result"]["content"].empty());
    EXPECT_EQ(response["result"]["content"][0]["text"], "test");
}

// Label: conformance
TEST(SdkMcpIntegration, ServerCreation)
{
    using namespace claude::mcp;

    auto server = create_server("test-server", "2.0.0");

    json list_req = {{"jsonrpc", "2.0"}, {"id", 1}, {"method", "tools/list"}};
    auto response = server(list_req);

    ASSERT_TRUE(response.contains("result"));
    ASSERT_TRUE(response["result"].contains("tools"));
    ASSERT_TRUE(response["result"]["tools"].is_array());
    EXPECT_EQ(response["result"]["tools"].size(), 0);
}

// Label: conformance
TEST(SdkMcpIntegration, ImageContentSupport)
{
    using namespace claude::mcp;

    // Sample base64 1x1 PNG (minimal valid PNG)
    std::string png_b64 = "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAIAAACQd1PeAAAADElEQVQI12P4//8/AAX+Av7czFnnAAAAAElFTkSuQmCC";

    auto generate_chart = make_tool(
        "generate_chart",
        "Generate chart",
        [png_b64](std::string title) -> json {
            return json{
                {"content", json::array({
                    {{"type", "text"}, {"text", "Generated chart: " + title}},
                    {{"type", "image"}, {"data", png_b64}, {"mimeType", "image/png"}}
                })}
            };
        },
        std::vector<std::string>{"title"});

    auto server = create_server("image-test", "1.0", generate_chart);

    json call_req = {
        {"jsonrpc", "2.0"},
        {"id", 1},
        {"method", "tools/call"},
        {"params", {{"name", "generate_chart"}, {"arguments", {{"title", "Sales Report"}}}}}
    };

    auto response = server(call_req);

    ASSERT_TRUE(response.contains("result"));
    auto content = response["result"]["content"];
    ASSERT_TRUE(content.is_array());
    ASSERT_EQ(content.size(), 2);

    EXPECT_EQ(content[0]["type"], "text");
    EXPECT_EQ(content[0]["text"], "Generated chart: Sales Report");

    EXPECT_EQ(content[1]["type"], "image");
    EXPECT_EQ(content[1]["data"], png_b64);
    EXPECT_EQ(content[1]["mimeType"], "image/png");
}

// Label: conformance
TEST(SdkMcpIntegration, MixedServers)
{
    using namespace claude::mcp;

    auto sdk_tool = make_tool("sdk_tool", "SDK tool", []() -> std::string {
        return "from SDK";
    });
    auto sdk_server = create_server("sdk-server", "1.0", sdk_tool);

    ClaudeOptions opts;
    opts.sdk_mcp_handlers["sdk"] = sdk_server;

    // Note: C++ doesn't have external MCP server config in ClaudeOptions yet
    // (Python has mcp_servers map), so we just verify SDK server is present

    ASSERT_EQ(opts.sdk_mcp_handlers.size(), 1);
    ASSERT_TRUE(opts.sdk_mcp_handlers.find("sdk") != opts.sdk_mcp_handlers.end());

    json list_req = {{"jsonrpc", "2.0"}, {"id", 1}, {"method", "tools/list"}};
    auto response = sdk_server(list_req);

    ASSERT_TRUE(response.contains("result"));
    ASSERT_TRUE(response["result"].contains("tools"));
    auto tools = response["result"]["tools"];
    ASSERT_EQ(tools.size(), 1);
    EXPECT_EQ(tools[0]["name"], "sdk_tool");
}
