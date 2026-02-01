/**
 * Example demonstrating the tools option and verifying tools in system message.
 *
 * Port of: claude-agent-sdk-python/examples/tools_option.py
 *
 * Usage:
 * ./tools_option - Run the example
 */

#include <claude/claude.hpp>
#include <iostream>
#include <string>

void tools_array_example()
{
    std::cout << "=== Tools Array Example ===\n";
    std::cout << "Setting tools=['Read', 'Glob', 'Grep']\n\n";

    claude::ClaudeOptions opts;
    opts.tools = std::vector<std::string>{"Read", "Glob", "Grep"};
    opts.max_turns = 1;

    try
    {
        auto result = claude::query("What tools do you have available? Just list them briefly.", opts);

        for (const auto& msg : result)
        {
            if (claude::is_system_message(msg))
            {
                const auto& sys_msg = std::get<claude::SystemMessage>(msg);
                // SystemMessage has raw_json; init data is in raw_json["data"]
                if (sys_msg.subtype == "init" && !sys_msg.raw_json.empty() &&
                    sys_msg.raw_json.contains("data") && sys_msg.raw_json["data"].contains("tools"))
                {
                    auto tools = sys_msg.raw_json["data"]["tools"];
                    std::cout << "Tools from system message: " << tools.dump() << "\n\n";
                }
            }
            else if (claude::is_assistant_message(msg))
            {
                const auto& assistant = std::get<claude::AssistantMessage>(msg);
                std::cout << "Claude: " << claude::get_text_content(assistant.content) << "\n";
            }
            else if (claude::is_result_message(msg))
            {
                const auto& result_msg = std::get<claude::ResultMessage>(msg);
                if (result_msg.total_cost_usd() > 0)
                    std::cout << "\nCost: $" << result_msg.total_cost_usd() << "\n";
            }
        }
    }
    catch (const claude::CLINotFoundError& e)
    {
        std::cerr << "Error: Claude CLI not found - " << e.what() << "\n";
        std::cerr << "Please install: npm install -g @anthropic-ai/claude-code\n";
        return;
    }
    catch (const claude::ClaudeError& e)
    {
        std::cerr << "Error: " << e.what() << "\n";
        return;
    }

    std::cout << "\n";
}

void tools_empty_array_example()
{
    std::cout << "=== Tools Empty Array Example ===\n";
    std::cout << "Setting tools=[] (disables all built-in tools)\n\n";

    claude::ClaudeOptions opts;
    opts.tools = std::vector<std::string>{}; // Empty array disables built-in tools
    opts.max_turns = 1;

    try
    {
        auto result = claude::query("What tools do you have available? Just list them briefly.", opts);

        for (const auto& msg : result)
        {
            if (claude::is_system_message(msg))
            {
                const auto& sys_msg = std::get<claude::SystemMessage>(msg);
                if (sys_msg.subtype == "init" && !sys_msg.raw_json.empty() &&
                    sys_msg.raw_json.contains("data") && sys_msg.raw_json["data"].contains("tools"))
                {
                    auto tools = sys_msg.raw_json["data"]["tools"];
                    std::cout << "Tools from system message: " << tools.dump() << "\n\n";
                }
            }
            else if (claude::is_assistant_message(msg))
            {
                const auto& assistant = std::get<claude::AssistantMessage>(msg);
                std::cout << "Claude: " << claude::get_text_content(assistant.content) << "\n";
            }
            else if (claude::is_result_message(msg))
            {
                const auto& result_msg = std::get<claude::ResultMessage>(msg);
                if (result_msg.total_cost_usd() > 0)
                    std::cout << "\nCost: $" << result_msg.total_cost_usd() << "\n";
            }
        }
    }
    catch (const claude::CLINotFoundError& e)
    {
        std::cerr << "Error: Claude CLI not found - " << e.what() << "\n";
        return;
    }
    catch (const claude::ClaudeError& e)
    {
        std::cerr << "Error: " << e.what() << "\n";
        return;
    }

    std::cout << "\n";
}

void tools_preset_example()
{
    std::cout << "=== Tools Preset Example ===\n";
    std::cout << "Setting tools_preset='claude_code' (all default Claude Code tools)\n\n";

    claude::ClaudeOptions opts;
    opts.tools_preset = "claude_code";
    opts.max_turns = 1;

    try
    {
        auto result = claude::query("What tools do you have available? Just list them briefly.", opts);

        for (const auto& msg : result)
        {
            if (claude::is_system_message(msg))
            {
                const auto& sys_msg = std::get<claude::SystemMessage>(msg);
                if (sys_msg.subtype == "init" && !sys_msg.raw_json.empty() &&
                    sys_msg.raw_json.contains("data") && sys_msg.raw_json["data"].contains("tools"))
                {
                    auto tools = sys_msg.raw_json["data"]["tools"];
                    std::cout << "Tools from system message (" << tools.size() << " tools): ";
                    // Print first 5 tools
                    size_t count = 0;
                    for (const auto& t : tools)
                    {
                        if (count++ >= 5)
                        {
                            std::cout << "...";
                            break;
                        }
                        std::cout << t.get<std::string>();
                        if (count < tools.size() && count < 5)
                            std::cout << ", ";
                    }
                    std::cout << "\n\n";
                }
            }
            else if (claude::is_assistant_message(msg))
            {
                const auto& assistant = std::get<claude::AssistantMessage>(msg);
                std::cout << "Claude: " << claude::get_text_content(assistant.content) << "\n";
            }
            else if (claude::is_result_message(msg))
            {
                const auto& result_msg = std::get<claude::ResultMessage>(msg);
                if (result_msg.total_cost_usd() > 0)
                    std::cout << "\nCost: $" << result_msg.total_cost_usd() << "\n";
            }
        }
    }
    catch (const claude::CLINotFoundError& e)
    {
        std::cerr << "Error: Claude CLI not found - " << e.what() << "\n";
        return;
    }
    catch (const claude::ClaudeError& e)
    {
        std::cerr << "Error: " << e.what() << "\n";
        return;
    }

    std::cout << "\n";
}

int main()
{
    std::cout << "Claude SDK version: " << claude::version_string() << "\n\n";
    std::cout << "This example demonstrates the tools option for controlling available tools.\n";
    std::cout << "- tools=[...] limits to specific tools\n";
    std::cout << "- tools=[] disables all built-in tools\n";
    std::cout << "- tools_preset='claude_code' enables all default tools\n\n";

    tools_array_example();
    tools_empty_array_example();
    tools_preset_example();

    return 0;
}
