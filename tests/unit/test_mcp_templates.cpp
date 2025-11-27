#include <claude/mcp.hpp>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

using namespace claude::mcp;
using json = nlohmann::json;

// ============================================================================
// Type Traits Tests
// ============================================================================

TEST(McpTypeTraits, PrimitiveTypes)
{
    // Test bool
    auto bool_schema = TypeToSchema<bool>::get();
    EXPECT_EQ(bool_schema["type"], "boolean");

    // Test int
    auto int_schema = TypeToSchema<int>::get();
    EXPECT_EQ(int_schema["type"], "integer");

    // Test double
    auto double_schema = TypeToSchema<double>::get();
    EXPECT_EQ(double_schema["type"], "number");

    // Test string
    auto string_schema = TypeToSchema<std::string>::get();
    EXPECT_EQ(string_schema["type"], "string");

    // Test json
    auto json_schema = TypeToSchema<json>::get();
    EXPECT_EQ(json_schema["type"], "object");
}

TEST(McpTypeTraits, ArrayTypes)
{
    auto array_schema = TypeToSchema<std::vector<int>>::get();
    EXPECT_EQ(array_schema["type"], "array");
    EXPECT_EQ(array_schema["items"]["type"], "integer");

    auto nested_array_schema = TypeToSchema<std::vector<std::vector<double>>>::get();
    EXPECT_EQ(nested_array_schema["type"], "array");
    EXPECT_EQ(nested_array_schema["items"]["type"], "array");
    EXPECT_EQ(nested_array_schema["items"]["items"]["type"], "number");
}

TEST(McpTypeTraits, MapTypes)
{
    auto map_schema = TypeToSchema<std::map<std::string, int>>::get();
    EXPECT_EQ(map_schema["type"], "object");
    EXPECT_EQ(map_schema["additionalProperties"]["type"], "integer");
}

TEST(McpTypeTraits, FunctionTraitsLambda)
{
    auto lambda = [](int a, double b) -> std::string { return "test"; };
    using Traits = FunctionTraits<decltype(lambda)>;

    EXPECT_EQ(Traits::arity, 2);
    EXPECT_TRUE((std::is_same_v<Traits::ReturnType, std::string>));
    EXPECT_TRUE((std::is_same_v<Traits::ArgType<0>, int>));
    EXPECT_TRUE((std::is_same_v<Traits::ArgType<1>, double>));
}

TEST(McpTypeTraits, FunctionTraitsFunctionPointer)
{
    using Func = bool (*)(float, float, float);
    using Traits = FunctionTraits<Func>;

    EXPECT_EQ(Traits::arity, 3);
    EXPECT_TRUE((std::is_same_v<Traits::ReturnType, bool>));
    EXPECT_TRUE((std::is_same_v<Traits::ArgType<0>, float>));
}

// ============================================================================
// Tool Creation Tests
// ============================================================================

TEST(McpTool, BasicToolCreation)
{
    auto add =
        make_tool("add", "Add two numbers", [](double a, double b) -> double { return a + b; });

    EXPECT_EQ(add.name(), "add");
    EXPECT_EQ(add.description(), "Add two numbers");

    // Verify input schema
    auto input_schema = add.input_schema();
    EXPECT_EQ(input_schema["type"], "object");
    EXPECT_EQ(input_schema["properties"]["arg0"]["type"], "number");
    EXPECT_EQ(input_schema["properties"]["arg1"]["type"], "number");
    EXPECT_TRUE(input_schema["required"].is_array());
    EXPECT_EQ(input_schema["required"].size(), 2);

    // Verify output schema
    auto output_schema = add.output_schema();
    EXPECT_EQ(output_schema["type"], "number");
}

TEST(McpTool, ToolWithStrings)
{
    auto greet = make_tool("greet", "Greet a user",
                           [](std::string name) -> std::string { return "Hello, " + name + "!"; });

    EXPECT_EQ(greet.name(), "greet");

    auto input_schema = greet.input_schema();
    EXPECT_EQ(input_schema["properties"]["arg0"]["type"], "string");

    auto output_schema = greet.output_schema();
    EXPECT_EQ(output_schema["type"], "string");
}

TEST(McpTool, ToolWithMultipleTypes)
{
    auto mixed = make_tool("mixed", "Mixed types", [](int x, std::string s, bool b) -> double
                           { return x * (b ? 2.0 : 1.0); });

    auto input_schema = mixed.input_schema();
    EXPECT_EQ(input_schema["properties"]["arg0"]["type"], "integer");
    EXPECT_EQ(input_schema["properties"]["arg1"]["type"], "string");
    EXPECT_EQ(input_schema["properties"]["arg2"]["type"], "boolean");
}

TEST(McpTool, ToolWithVoidReturn)
{
    auto noop = make_tool("noop", "No operation", []() -> void {});

    // Should work without errors
    EXPECT_EQ(noop.name(), "noop");
}

TEST(McpTool, ToolInvocation)
{
    auto add =
        make_tool("add", "Add two numbers", [](double a, double b) -> double { return a + b; });

    json args = {{"arg0", 5.5}, {"arg1", 3.2}};
    json result = add.invoke(args);

    // Result should be MCP-formatted
    EXPECT_TRUE(result.contains("content"));
    EXPECT_TRUE(result["content"].is_array());
    EXPECT_EQ(result["content"][0]["type"], "text");

    // Extract and verify the result
    std::string text = result["content"][0]["text"];
    double value = std::stod(text);
    EXPECT_NEAR(value, 8.7, 0.001);
}

TEST(McpTool, ToolInvocationString)
{
    auto greet = make_tool("greet", "Greet a user",
                           [](std::string name) -> std::string { return "Hello, " + name + "!"; });

    json args = {{"arg0", "Alice"}};
    json result = greet.invoke(args);

    EXPECT_TRUE(result.contains("content"));
    std::string text = result["content"][0]["text"];
    EXPECT_EQ(text, "Hello, Alice!");
}

TEST(McpTool, ToolInvocationVoid)
{
    int counter = 0;
    auto increment =
        make_tool("increment", "Increment counter", [&counter]() -> void { counter++; });

    json args = json::object();
    json result = increment.invoke(args);

    EXPECT_EQ(counter, 1);
    EXPECT_TRUE(result.contains("content"));
    EXPECT_EQ(result["content"][0]["text"], "Success");
}

TEST(McpTool, CustomParameterNames)
{
    auto calc = make_tool(
        "calc", "Calculate", [](double x, double y) -> double { return x * y; },
        std::vector<std::string>{"multiplier", "multiplicand"});

    auto input_schema = calc.input_schema();
    EXPECT_TRUE(input_schema["properties"].contains("multiplier"));
    EXPECT_TRUE(input_schema["properties"].contains("multiplicand"));

    // Invoke with custom names
    json args = {{"multiplier", 3.0}, {"multiplicand", 7.0}};
    json result = calc.invoke(args);

    std::string text = result["content"][0]["text"];
    double value = std::stod(text);
    EXPECT_NEAR(value, 21.0, 0.001);
}

// ============================================================================
// Server Creation Tests
// ============================================================================

TEST(McpServer, BasicServerCreation)
{
    auto add =
        make_tool("add", "Add two numbers", [](double a, double b) -> double { return a + b; });

    auto multiply = make_tool("multiply", "Multiply two numbers",
                              [](double a, double b) -> double { return a * b; });

    auto server = create_server("calculator", "1.0.0", add, multiply);

    // Server should be a callable function
    EXPECT_TRUE(server);
}

TEST(McpServer, InitializeRequest)
{
    auto add = make_tool("add", "Add", [](double a, double b) -> double { return a + b; });
    auto server = create_server("calc", "1.0.0", add);

    json request = {{"jsonrpc", "2.0"}, {"id", 1}, {"method", "initialize"}};

    json response = server(request);

    EXPECT_EQ(response["jsonrpc"], "2.0");
    EXPECT_EQ(response["id"], 1);
    EXPECT_TRUE(response.contains("result"));
    EXPECT_EQ(response["result"]["serverInfo"]["name"], "calc");
    EXPECT_EQ(response["result"]["serverInfo"]["version"], "1.0.0");
    EXPECT_EQ(response["result"]["protocolVersion"], "2024-11-05");
}

TEST(McpServer, ToolsListRequest)
{
    auto add =
        make_tool("add", "Add two numbers", [](double a, double b) -> double { return a + b; });

    auto greet = make_tool("greet", "Greet user",
                           [](std::string name) -> std::string { return "Hi " + name; });

    auto server = create_server("myserver", "1.0.0", add, greet);

    json request = {{"jsonrpc", "2.0"}, {"id", 2}, {"method", "tools/list"}};

    json response = server(request);

    EXPECT_EQ(response["jsonrpc"], "2.0");
    EXPECT_EQ(response["id"], 2);
    EXPECT_TRUE(response["result"]["tools"].is_array());
    EXPECT_EQ(response["result"]["tools"].size(), 2);

    // Verify tools are listed
    bool found_add = false;
    bool found_greet = false;
    for (const auto& tool : response["result"]["tools"])
    {
        if (tool["name"] == "add")
        {
            found_add = true;
            EXPECT_EQ(tool["description"], "Add two numbers");
        }
        if (tool["name"] == "greet")
        {
            found_greet = true;
            EXPECT_EQ(tool["description"], "Greet user");
        }
    }
    EXPECT_TRUE(found_add);
    EXPECT_TRUE(found_greet);
}

TEST(McpServer, ToolCallRequest)
{
    auto add =
        make_tool("add", "Add two numbers", [](double a, double b) -> double { return a + b; });

    auto server = create_server("calc", "1.0.0", add);

    json request = {{"jsonrpc", "2.0"},
                    {"id", 3},
                    {"method", "tools/call"},
                    {"params", {{"name", "add"}, {"arguments", {{"arg0", 10.0}, {"arg1", 20.0}}}}}};

    json response = server(request);

    EXPECT_EQ(response["jsonrpc"], "2.0");
    EXPECT_EQ(response["id"], 3);
    EXPECT_TRUE(response.contains("result"));

    // Result should contain MCP-formatted content
    EXPECT_TRUE(response["result"].contains("content"));
}

TEST(McpServer, ErrorHandlingInvalidMethod)
{
    auto add = make_tool("add", "Add", [](double a, double b) -> double { return a + b; });
    auto server = create_server("calc", "1.0.0", add);

    json request = {{"jsonrpc", "2.0"}, {"id", 4}, {"method", "invalid/method"}};

    json response = server(request);

    EXPECT_TRUE(response.contains("error"));
    EXPECT_EQ(response["error"]["code"], -32601);
}

TEST(McpServer, ErrorHandlingToolNotFound)
{
    auto add = make_tool("add", "Add", [](double a, double b) -> double { return a + b; });
    auto server = create_server("calc", "1.0.0", add);

    json request = {{"jsonrpc", "2.0"},
                    {"id", 5},
                    {"method", "tools/call"},
                    {"params", {{"name", "nonexistent"}, {"arguments", json::object()}}}};

    json response = server(request);

    EXPECT_TRUE(response.contains("error"));
}

// ============================================================================
// Server Builder Tests
// ============================================================================

TEST(McpServer, ServerBuilder)
{
    auto builder = server("myserver", "2.0.0");

    builder.add_tool(make_tool("add", "Add", [](double a, double b) -> double { return a + b; }));

    builder.add_tool(
        make_tool("subtract", "Subtract", [](double a, double b) -> double { return a - b; }));

    EXPECT_EQ(builder.tool_count(), 2);

    auto handler = builder.build();

    // Test the built handler
    json request = {{"jsonrpc", "2.0"}, {"id", 1}, {"method", "initialize"}};
    json response = handler(request);

    EXPECT_EQ(response["result"]["serverInfo"]["name"], "myserver");
    EXPECT_EQ(response["result"]["serverInfo"]["version"], "2.0.0");
}

// ============================================================================
// Integration Tests
// ============================================================================

TEST(McpIntegration, EndToEndWorkflow)
{
    // Create tools
    auto add = make_tool("add", "Add numbers", [](double a, double b) -> double { return a + b; });

    auto multiply = make_tool("multiply", "Multiply numbers",
                              [](double a, double b) -> double { return a * b; });

    auto greet = make_tool("greet", "Greet user",
                           [](std::string name) -> std::string { return "Hello, " + name + "!"; });

    // Create server
    auto server_handler = create_server("testserver", "1.0.0", add, multiply, greet);

    // Initialize
    json init_request = {{"jsonrpc", "2.0"}, {"id", 1}, {"method", "initialize"}};
    json init_response = server_handler(init_request);
    EXPECT_EQ(init_response["result"]["serverInfo"]["name"], "testserver");

    // List tools
    json list_request = {{"jsonrpc", "2.0"}, {"id", 2}, {"method", "tools/list"}};
    json list_response = server_handler(list_request);
    EXPECT_EQ(list_response["result"]["tools"].size(), 3);

    // Call add tool
    json add_request = {
        {"jsonrpc", "2.0"},
        {"id", 3},
        {"method", "tools/call"},
        {"params", {{"name", "add"}, {"arguments", {{"arg0", 5.0}, {"arg1", 7.0}}}}}};
    json add_response = server_handler(add_request);
    EXPECT_FALSE(add_response.contains("error"));

    // Call greet tool
    json greet_request = {{"jsonrpc", "2.0"},
                          {"id", 4},
                          {"method", "tools/call"},
                          {"params", {{"name", "greet"}, {"arguments", {{"arg0", "World"}}}}}};
    json greet_response = server_handler(greet_request);
    EXPECT_FALSE(greet_response.contains("error"));
    EXPECT_EQ(greet_response["result"]["content"][0]["text"], "Hello, World!");
}

TEST(McpIntegration, ComplexTypes)
{
    auto array_sum = make_tool("array_sum", "Sum array of numbers",
                               [](std::vector<double> numbers) -> double
                               {
                                   double sum = 0.0;
                                   for (double n : numbers)
                                       sum += n;
                                   return sum;
                               });

    auto server_handler = create_server("arrayserver", "1.0.0", array_sum);

    json request = {{"jsonrpc", "2.0"},
                    {"id", 1},
                    {"method", "tools/call"},
                    {"params",
                     {{"name", "array_sum"},
                      {"arguments", {{"arg0", json::array({1.0, 2.0, 3.0, 4.0, 5.0})}}}}}};

    json response = server_handler(request);

    EXPECT_FALSE(response.contains("error"));
    std::string text = response["result"]["content"][0]["text"];
    double value = std::stod(text);
    EXPECT_NEAR(value, 15.0, 0.001);
}
