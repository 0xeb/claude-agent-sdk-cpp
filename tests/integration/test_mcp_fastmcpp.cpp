#include <gtest/gtest.h>
#include <claude/claude.hpp>

// Integration test for cppclient + fastmcpp MCP tool creation
// This test verifies that fast mcpp's make_mcp_handler() can be used
// with cppclient's ClaudeOptions::sdk_mcp_handlers.

// NOTE: This test requires fastmcpp to be available. It will be
// conditionally compiled based on whether fastmcpp is found.

#ifdef FASTMCPP_AVAILABLE
#include <fastmcpp/tools/manager.hpp>
#include <fastmcpp/server/server.hpp>
#include <fastmcpp/mcp/handler.hpp>

TEST(FastmcppIntegration, CreateHandlerFromToolManager)
{
    using Json = nlohmann::json;

    // Create a simple calculator tool using fastmcpp
    fastmcpp::tools::ToolManager tm;
    fastmcpp::tools::Tool add{
        "add",
        Json{{"type", "object"},
             {"properties", Json{{"a", Json{{"type", "number"}}}, {"b", Json{{"type", "number"}}}}},
             {"required", Json::array({"a", "b"})}},
        Json{{"type", "number"}},
        [](const Json& in) {
            return in.at("a").get<double>() + in.at("b").get<double>();
        }
    };
    tm.register_tool(add);

    // Build MCP handler
    auto handler = fastmcpp::mcp::make_mcp_handler("calc", "1.0.0", tm);

    // Verify handler responds to initialize
    Json init_req = {{"jsonrpc", "2.0"}, {"id", 1}, {"method", "initialize"}};
    Json init_resp = handler(init_req);
    ASSERT_TRUE(init_resp.contains("result"));
    ASSERT_TRUE(init_resp["result"].contains("serverInfo"));
    EXPECT_EQ(init_resp["result"]["serverInfo"]["name"], "calc");

    // Verify handler responds to tools/list
    Json list_req = {{"jsonrpc", "2.0"}, {"id", 2}, {"method", "tools/list"}};
    Json list_resp = handler(list_req);
    ASSERT_TRUE(list_resp.contains("result"));
    ASSERT_TRUE(list_resp["result"].contains("tools"));
    ASSERT_EQ(list_resp["result"]["tools"].size(), 1);
    EXPECT_EQ(list_resp["result"]["tools"][0]["name"], "add");

    // Verify handler responds to tools/call
    Json call_req = {{"jsonrpc", "2.0"},
                     {"id", 3},
                     {"method", "tools/call"},
                     {"params", Json{{"name", "add"}, {"arguments", Json{{"a", 5}, {"b", 7}}}}}};
    Json call_resp = handler(call_req);
    ASSERT_TRUE(call_resp.contains("result"));
    ASSERT_TRUE(call_resp["result"].contains("content"));
    // The handler wraps the result in a content array
    EXPECT_GT(call_resp["result"]["content"].size(), 0);
}

TEST(FastmcppIntegration, UseHandlerWithClaudeClient)
{
    using namespace claude;
    using Json = nlohmann::json;

    // Create a simple calculator tool
    fastmcpp::tools::ToolManager tm;
    fastmcpp::tools::Tool add{
        "add",
        Json{{"type", "object"},
             {"properties", Json{{"a", Json{{"type", "number"}}}, {"b", Json{{"type", "number"}}}}},
             {"required", Json::array({"a", "b"})}},
        Json{{"type", "number"}},
        [](const Json& in) {
            return in.at("a").get<double>() + in.at("b").get<double>();
        }
    };
    tm.register_tool(add);

    // Create server for routing
    auto srv = std::make_shared<fastmcpp::server::Server>();
    srv->route("add", [&tm](const Json& input) {
        auto result = tm.invoke("add", input);
        return Json{{"content", Json::array({Json{{"type", "text"}, {"text", std::string("Sum: ") + result.dump()}}})}};
    });

    // Build MCP handler
    auto handler = fastmcpp::mcp::make_mcp_handler("calc", "1.0.0", *srv, tm);

    // Register with cppclient
    ClaudeOptions opts;
    opts.permission_mode = "bypassPermissions";
    opts.allowed_tools = {"add"};
    opts.sdk_mcp_handlers["calc"] = handler;

    // Create client and send a query
    ClaudeClient client(opts);
    client.connect();

    client.send_query("Use the calc tool to add 10 and 20. Just give me the result.");

    bool found_result = false;
    for (const auto& msg : client.receive_messages())
    {
        if (is_result_message(msg))
        {
            found_result = true;
            break;
        }
    }

    EXPECT_TRUE(found_result);

    client.disconnect();
}

#else
// Placeholder test when fastmcpp is not available
TEST(FastmcppIntegration, SkippedWithoutFastmcpp)
{
    GTEST_SKIP() << "fastmcpp not available - skipping integration tests";
}
#endif
