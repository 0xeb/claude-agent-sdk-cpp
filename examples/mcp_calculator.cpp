/**
 * @file mcp_calculator.cpp
 * @brief Example: Calculator MCP Server
 *
 * This example demonstrates how to create an in-process MCP server with
 * calculator tools using the Claude Code C++ SDK.
 *
 * Unlike external MCP servers that require separate processes, this server
 * runs directly within your C++ application, providing better performance
 * and simpler deployment.
 *
 * This is a direct port of examples/mcp_calculator.py from the Python SDK.
 */

#include <claude/claude.hpp>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

using namespace claude;
using namespace claude::mcp;

// Define calculator tools using make_tool()

auto add_numbers()
{
    return make_tool("add", "Add two numbers",
                     [](double a, double b) -> std::string {
                         double result = a + b;
                         return std::to_string(a) + " + " + std::to_string(b) + " = " +
                                std::to_string(result);
                     },
                     std::vector<std::string>{"a", "b"});
}

auto subtract_numbers()
{
    return make_tool("subtract", "Subtract one number from another",
                     [](double a, double b) -> std::string {
                         double result = a - b;
                         return std::to_string(a) + " - " + std::to_string(b) + " = " +
                                std::to_string(result);
                     },
                     std::vector<std::string>{"a", "b"});
}

auto multiply_numbers()
{
    return make_tool("multiply", "Multiply two numbers",
                     [](double a, double b) -> std::string {
                         double result = a * b;
                         return std::to_string(a) + " × " + std::to_string(b) + " = " +
                                std::to_string(result);
                     },
                     std::vector<std::string>{"a", "b"});
}

auto divide_numbers()
{
    return make_tool(
        "divide", "Divide one number by another",
        [](double a, double b) -> std::string {
            if (b == 0.0)
            {
                throw std::runtime_error("Error: Division by zero is not allowed");
            }
            double result = a / b;
            return std::to_string(a) + " ÷ " + std::to_string(b) + " = " +
                   std::to_string(result);
        },
        std::vector<std::string>{"a", "b"});
}

auto square_root()
{
    return make_tool(
        "sqrt", "Calculate square root",
        [](double n) -> std::string {
            if (n < 0)
            {
                throw std::runtime_error("Error: Cannot calculate square root of negative "
                                         "number " +
                                         std::to_string(n));
            }
            double result = std::sqrt(n);
            return "√" + std::to_string(n) + " = " + std::to_string(result);
        },
        std::vector<std::string>{"n"});
}

auto power()
{
    return make_tool("power", "Raise a number to a power",
                     [](double base, double exponent) -> std::string {
                         double result = std::pow(base, exponent);
                         return std::to_string(base) + "^" + std::to_string(exponent) + " = " +
                                std::to_string(result);
                     },
                     std::vector<std::string>{"base", "exponent"});
}

// Display message content in a clean format
void display_message(const Message& msg)
{
    if (is_assistant_message(msg))
    {
        const auto& assistant = std::get<AssistantMessage>(msg);
        for (const auto& block : assistant.content)
        {
            if (std::holds_alternative<TextBlock>(block))
            {
                const auto& text = std::get<TextBlock>(block);
                std::cout << "Claude: " << text.text << "\n";
            }
            else if (std::holds_alternative<ToolUseBlock>(block))
            {
                const auto& tool_use = std::get<ToolUseBlock>(block);
                std::cout << "Using tool: " << tool_use.name << "\n";
                if (!tool_use.input.is_null())
                {
                    std::cout << "  Input: " << tool_use.input.dump() << "\n";
                }
            }
        }
    }
    else if (is_result_message(msg))
    {
        const auto& result = std::get<ResultMessage>(msg);
        std::cout << "Result ended\n";
        double cost = result.total_cost_usd();
        if (cost > 0.0)
        {
            std::cout << "Cost: $" << std::fixed << std::setprecision(6) << cost << "\n";
        }
    }
    // Ignore system messages
}

int main()
{
    std::cout << "=== Calculator MCP Server Example ===\n\n";

    // Create the calculator server with all tools
    auto calculator = create_server("calculator", "2.0.0", add_numbers(), subtract_numbers(),
                                    multiply_numbers(), divide_numbers(), square_root(), power());

    std::cout << "Created MCP server 'calculator' v2.0.0 with 6 tools\n\n";

    // Configure Claude to use the calculator server with allowed tools
    // Pre-approve all calculator MCP tools so they can be used without permission prompts
    ClaudeOptions options;
    options.permission_mode = "bypassPermissions"; // Bypass all permission prompts
    options.sdk_mcp_handlers["calc"] = calculator;
    options.allowed_tools = {"mcp__calc__add",    "mcp__calc__subtract", "mcp__calc__multiply",
                             "mcp__calc__divide", "mcp__calc__sqrt",     "mcp__calc__power"};

    // Example prompts to demonstrate calculator usage
    std::vector<std::string> prompts = {
        "List your tools",
        "Calculate 15 + 27",
        "What is 100 divided by 7?",
        "Calculate the square root of 144",
        "What is 2 raised to the power of 8?",
        "Calculate (12 + 8) * 3 - 10", // Complex calculation
    };

    try
    {
        for (const auto& prompt : prompts)
        {
            std::cout << "\n" << std::string(50, '=') << "\n";
            std::cout << "Prompt: " << prompt << "\n";
            std::cout << std::string(50, '=') << "\n";

            // Create a new client for each query (matching Python's async with pattern)
            ClaudeClient client(options);
            client.connect();

            client.send_query(prompt);

            for (const auto& message : client.receive_messages())
            {
                display_message(message);

                // Break after result message
                if (is_result_message(message))
                {
                    break;
                }
            }

            client.disconnect();
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    std::cout << "\n=== Example Complete ===\n";

    return 0;
}
