// Example demonstrating how to use plugins with Claude Code SDK.
//
// Plugins allow you to extend Claude Code with custom commands, agents, skills,
// and hooks. This example shows how to load a local plugin and verify it's
// loaded by checking the system message.
//
// The demo plugin is located in examples/plugins/demo-plugin/ and provides
// a custom /greet command.

#include <claude/claude.hpp>
#include <filesystem>
#include <iostream>

void plugin_example()
{
    std::cout << "=== Plugin Example ===\n\n";

    // Get the path to the demo plugin
    // In production, you can use any path to your plugin directory
    std::filesystem::path plugin_path = std::filesystem::current_path() / "plugins" / "demo-plugin";

    // Configure Claude with plugin
    claude::ClaudeOptions opts;
    opts.plugins = {
        claude::SdkPluginConfig{
            "local",  // type
            plugin_path.string()  // path
        }
    };
    opts.max_turns = 1;  // Limit to one turn for quick demo

    std::cout << "Loading plugin from: " << plugin_path << "\n\n";

    bool found_plugins = false;

    // Use query() to send a simple message and observe system initialization
    for (const auto& message : claude::query("Hello!", opts))
    {
        if (claude::is_system_message(message))
        {
            const auto& sys_msg = std::get<claude::SystemMessage>(message);

            if (sys_msg.subtype == "init")
            {
                std::cout << "System initialized!\n";

                // Check for plugins in the system message
                if (sys_msg.raw_json.contains("plugins") && sys_msg.raw_json["plugins"].is_array())
                {
                    const auto& plugins_data = sys_msg.raw_json["plugins"];

                    if (!plugins_data.empty())
                    {
                        std::cout << "Plugins loaded:\n";
                        for (const auto& plugin : plugins_data)
                        {
                            std::string name = plugin.value("name", "unknown");
                            std::string path = plugin.value("path", "unknown");
                            std::cout << "  - " << name << " (path: " << path << ")\n";
                        }
                        found_plugins = true;
                    }
                }
                else
                {
                    std::cout << "Note: Plugin was passed via CLI but may not appear in system message.\n";
                    std::cout << "Plugin path configured: " << plugin_path << "\n";
                    found_plugins = true;
                }
            }
        }
    }

    if (found_plugins)
    {
        std::cout << "\nPlugin successfully configured!\n\n";
        std::cout << "Try running: claude /greet\n";
        std::cout << "to test the custom command provided by the demo plugin.\n";
    }
}

int main()
{
    try
    {
        plugin_example();
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}
