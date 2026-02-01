/**
 * Example of loading filesystem-based agents via setting_sources.
 *
 * This example demonstrates how to load agents defined in .claude/agents/ files
 * using the setting_sources option. This is different from inline AgentDefinition
 * objects - these agents are loaded from markdown files on disk.
 *
 * Port of: claude-agent-sdk-python/examples/filesystem_agents.py
 *
 * Usage:
 * ./filesystem_agents - Run the example
 */

#include <claude/claude.hpp>
#include <iostream>
#include <string>
#include <vector>

std::vector<std::string> extract_agents(const claude::SystemMessage& msg)
{
    std::vector<std::string> result;

    // SystemMessage has raw_json; init data is in raw_json["data"]
    if (msg.subtype == "init" && !msg.raw_json.empty() && msg.raw_json.contains("data") &&
        msg.raw_json["data"].contains("agents"))
    {
        const auto& agents = msg.raw_json["data"]["agents"];
        for (const auto& a : agents)
        {
            if (a.is_string())
            {
                result.push_back(a.get<std::string>());
            }
            else if (a.is_object() && a.contains("name"))
            {
                result.push_back(a["name"].get<std::string>());
            }
        }
    }

    return result;
}

int main()
{
    std::cout << "=== Filesystem Agents Example ===\n";
    std::cout << "Testing: setting_sources=['project'] with .claude/agents/ files\n\n";

    claude::ClaudeOptions opts;
    opts.setting_sources = std::vector<std::string>{"project"};
    // Use current working directory (assumes run from a directory with .claude/agents/)

    std::vector<std::string> message_types;
    std::vector<std::string> agents_found;

    try
    {
        auto result = claude::query("Say hello in exactly 3 words", opts);

        for (const auto& msg : result)
        {
            if (claude::is_system_message(msg))
            {
                message_types.push_back("SystemMessage");
                const auto& sys_msg = std::get<claude::SystemMessage>(msg);

                if (sys_msg.subtype == "init")
                {
                    agents_found = extract_agents(sys_msg);
                    std::cout << "Init message received. Agents loaded: [";
                    for (size_t i = 0; i < agents_found.size(); ++i)
                    {
                        std::cout << agents_found[i];
                        if (i + 1 < agents_found.size())
                            std::cout << ", ";
                    }
                    std::cout << "]\n";
                }
            }
            else if (claude::is_assistant_message(msg))
            {
                message_types.push_back("AssistantMessage");
                const auto& assistant = std::get<claude::AssistantMessage>(msg);
                std::cout << "Assistant: " << claude::get_text_content(assistant.content) << "\n";
            }
            else if (claude::is_result_message(msg))
            {
                message_types.push_back("ResultMessage");
                const auto& result_msg = std::get<claude::ResultMessage>(msg);
                std::cout << "Result: subtype=" << result_msg.subtype
                          << ", cost=$" << result_msg.total_cost_usd() << "\n";
            }
            else if (std::holds_alternative<claude::UserMessage>(msg))
            {
                message_types.push_back("UserMessage");
            }
        }
    }
    catch (const claude::CLINotFoundError& e)
    {
        std::cerr << "Error: Claude CLI not found - " << e.what() << "\n";
        std::cerr << "Please install: npm install -g @anthropic-ai/claude-code\n";
        return 1;
    }
    catch (const claude::ClaudeError& e)
    {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    std::cout << "\n=== Summary ===\n";
    std::cout << "Message types received: [";
    for (size_t i = 0; i < message_types.size(); ++i)
    {
        std::cout << message_types[i];
        if (i + 1 < message_types.size())
            std::cout << ", ";
    }
    std::cout << "]\n";
    std::cout << "Total messages: " << message_types.size() << "\n\n";

    // Validate the results
    bool has_init = false;
    bool has_assistant = false;
    bool has_result = false;

    for (const auto& t : message_types)
    {
        if (t == "SystemMessage")
            has_init = true;
        if (t == "AssistantMessage")
            has_assistant = true;
        if (t == "ResultMessage")
            has_result = true;
    }

    if (has_init && has_assistant && has_result)
    {
        std::cout << "SUCCESS: Received full response (init, assistant, result)\n";
    }
    else
    {
        std::cout << "FAILURE: Did not receive full response\n";
        std::cout << "  - Init: " << (has_init ? "yes" : "no") << "\n";
        std::cout << "  - Assistant: " << (has_assistant ? "yes" : "no") << "\n";
        std::cout << "  - Result: " << (has_result ? "yes" : "no") << "\n";
    }

    // Check for any loaded agents
    if (!agents_found.empty())
    {
        std::cout << "SUCCESS: Agents loaded from filesystem: ";
        for (const auto& a : agents_found)
            std::cout << a << " ";
        std::cout << "\n";
    }
    else
    {
        std::cout << "INFO: No filesystem agents loaded (may not exist in .claude/agents/)\n";
    }

    return 0;
}
