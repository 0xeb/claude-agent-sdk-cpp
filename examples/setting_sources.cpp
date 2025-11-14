/**
 * @file setting_sources.cpp
 * @brief Example demonstrating setting sources control
 *
 * This example shows how to use the setting_sources option to control which
 * settings are loaded, including custom slash commands, agents, and other
 * configurations.
 *
 * Setting sources determine where Claude Code loads configurations from:
 * - "user": Global user settings (~/.claude/)
 * - "project": Project-level settings (.claude/ in project)
 * - "local": Local gitignored settings (.claude-local/)
 *
 * IMPORTANT: When setting_sources is not provided (empty), NO settings are loaded
 * by default. This creates an isolated environment. To load settings, explicitly
 * specify which sources to use.
 *
 * Usage:
 *   ./setting_sources default        - Run default behavior example
 *   ./setting_sources user_only      - Run user-only example
 *   ./setting_sources project_and_user - Run project + user example
 *   ./setting_sources all            - Run all examples
 */

#include <claude/claude.hpp>
#include <iostream>
#include <string>
#include <vector>

using namespace claude;

// Extract slash commands from system message
std::vector<std::string> extract_slash_commands(const SystemMessage& msg)
{
    std::vector<std::string> commands;
    // SystemMessage has: type, content (string), subtype, raw_json
    // Parse raw_json for data fields
    if (msg.subtype == "init" && !msg.raw_json.empty() && msg.raw_json.contains("data"))
    {
        auto data = msg.raw_json["data"];
        if (data.contains("slash_commands") && data["slash_commands"].is_array())
        {
            for (const auto& cmd : data["slash_commands"])
            {
                if (cmd.is_string())
                {
                    commands.push_back(cmd.get<std::string>());
                }
            }
        }
    }
    return commands;
}

void example_default()
{
    std::cout << "=== Default Behavior Example ===\n";
    std::cout << "Setting sources: None (default)\n";
    std::cout << "Expected: No custom slash commands will be available\n\n";

    ClaudeOptions opts;
    opts.permission_mode = "bypassPermissions";
    // No setting_sources set - empty means no settings loaded

    try
    {
        ClaudeClient client(opts);
        client.connect();
        client.send_query("What is 2 + 2?");

        for (const auto& msg : client.receive_messages())
        {
            if (is_system_message(msg))
            {
                const auto& sys_msg = std::get<SystemMessage>(msg);
                auto commands = extract_slash_commands(sys_msg);

                std::cout << "Available slash commands: [";
                for (size_t i = 0; i < commands.size(); ++i)
                {
                    if (i > 0) std::cout << ", ";
                    std::cout << commands[i];
                }
                std::cout << "]\n";

                bool has_commit = std::find(commands.begin(), commands.end(), "commit") != commands.end();
                if (has_commit)
                {
                    std::cout << "❌ /commit is available (unexpected)\n";
                }
                else
                {
                    std::cout << "✓ /commit is NOT available (expected - no settings loaded)\n";
                }
                break;
            }
            else if (is_result_message(msg))
            {
                break;
            }
        }

        client.disconnect();
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << "\n";
    }

    std::cout << "\n";
}

void example_user_only()
{
    std::cout << "=== User Settings Only Example ===\n";
    std::cout << "Setting sources: ['user']\n";
    std::cout << "Expected: Project slash commands (like /commit) will NOT be available\n\n";

    ClaudeOptions opts;
    opts.permission_mode = "bypassPermissions";
    opts.setting_sources = {"user"};

    try
    {
        ClaudeClient client(opts);
        client.connect();
        client.send_query("What is 2 + 2?");

        for (const auto& msg : client.receive_messages())
        {
            if (is_system_message(msg))
            {
                const auto& sys_msg = std::get<SystemMessage>(msg);
                auto commands = extract_slash_commands(sys_msg);

                std::cout << "Available slash commands: [";
                for (size_t i = 0; i < commands.size(); ++i)
                {
                    if (i > 0) std::cout << ", ";
                    std::cout << commands[i];
                }
                std::cout << "]\n";

                bool has_commit = std::find(commands.begin(), commands.end(), "commit") != commands.end();
                if (has_commit)
                {
                    std::cout << "❌ /commit is available (unexpected)\n";
                }
                else
                {
                    std::cout << "✓ /commit is NOT available (expected)\n";
                }
                break;
            }
            else if (is_result_message(msg))
            {
                break;
            }
        }

        client.disconnect();
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << "\n";
    }

    std::cout << "\n";
}

void example_project_and_user()
{
    std::cout << "=== Project + User Settings Example ===\n";
    std::cout << "Setting sources: ['user', 'project']\n";
    std::cout << "Expected: Project slash commands (like /commit) WILL be available\n\n";

    ClaudeOptions opts;
    opts.permission_mode = "bypassPermissions";
    opts.setting_sources = {"user", "project"};

    try
    {
        ClaudeClient client(opts);
        client.connect();
        client.send_query("What is 2 + 2?");

        for (const auto& msg : client.receive_messages())
        {
            if (is_system_message(msg))
            {
                const auto& sys_msg = std::get<SystemMessage>(msg);
                auto commands = extract_slash_commands(sys_msg);

                std::cout << "Available slash commands: [";
                for (size_t i = 0; i < commands.size(); ++i)
                {
                    if (i > 0) std::cout << ", ";
                    std::cout << commands[i];
                }
                std::cout << "]\n";

                bool has_commit = std::find(commands.begin(), commands.end(), "commit") != commands.end();
                if (has_commit)
                {
                    std::cout << "✓ /commit is available (expected)\n";
                }
                else
                {
                    std::cout << "❌ /commit is NOT available (unexpected)\n";
                }
                break;
            }
            else if (is_result_message(msg))
            {
                break;
            }
        }

        client.disconnect();
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << "\n";
    }

    std::cout << "\n";
}

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        std::cout << "Usage: setting_sources <example_name>\n\n";
        std::cout << "Available examples:\n";
        std::cout << "  all                - Run all examples\n";
        std::cout << "  default            - Default behavior\n";
        std::cout << "  user_only          - User settings only\n";
        std::cout << "  project_and_user   - Project + user settings\n";
        return 1;
    }

    std::string example = argv[1];

    std::cout << "Starting Claude SDK Setting Sources Examples...\n";
    std::cout << std::string(50, '=') << "\n\n";

    if (example == "all")
    {
        example_default();
        std::cout << std::string(50, '-') << "\n\n";
        example_user_only();
        std::cout << std::string(50, '-') << "\n\n";
        example_project_and_user();
    }
    else if (example == "default")
    {
        example_default();
    }
    else if (example == "user_only")
    {
        example_user_only();
    }
    else if (example == "project_and_user")
    {
        example_project_and_user();
    }
    else
    {
        std::cerr << "Error: Unknown example '" << example << "'\n\n";
        std::cout << "Available examples:\n";
        std::cout << "  all, default, user_only, project_and_user\n";
        return 1;
    }

    return 0;
}

