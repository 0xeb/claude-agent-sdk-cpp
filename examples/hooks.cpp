#include <claude/claude.hpp>
#include <iostream>

int main()
{
    claude::ClaudeOptions opts;
    opts.permission_mode = "default";

    // Register hook for PreToolUse events
    // Callback receives hook input (JSON) and tool_use_id (string)
    // Returns JSON output following HookJSONOutput schema
    auto pre_tool_callback = [](const claude::json& input,
                                const std::string& tool_use_id) -> claude::json
    {
        std::string hook_event = input.value("hook_event_name", "");
        std::string tool_name = input.value("tool_name", "");

        std::cout << "[HOOK] " << hook_event << " - Tool: " << tool_name;
        if (!tool_use_id.empty())
            std::cout << " (ID: " << tool_use_id << ")";
        std::cout << "\n";

        // Return JSON output: allow hook to continue
        // See: https://docs.anthropic.com/en/docs/claude-code/hooks#advanced%3A-json-output
        return claude::json{
            {"continue", true} // Continue execution
        };
    };

    // Configure hooks using HookMatcher structure
    opts.hooks[claude::HookEvent::PreToolUse] = {claude::HookMatcher{
        "Bash|Write|Edit",  // Matcher pattern for Bash, Write, and Edit tools
        {pre_tool_callback} // List of callbacks
    }};

    // Auto-approve all tool permissions
    opts.tool_permission_callback =
        [](const std::string& tool_name, const claude::json& input,
           const claude::ToolPermissionContext& context) -> claude::PermissionResult
    {
        std::cout << "[TOOL PERMISSION] " << tool_name << " [APPROVED]\n";
        return claude::PermissionResultAllow{};
    };

    try
    {
        claude::ClaudeClient client(opts);
        client.connect();

        std::cout << "Streaming with Hooks Example\n\n";

        // Multi-turn conversation
        std::vector<std::string> queries = {"What's the current date? Use bash to find out.",
                                            "Create a file called test.txt with 'Hello World'",
                                            "Read the file and tell me what it says"};

        for (const auto& query : queries)
        {
            std::cout << ">>> " << query << "\n\n";

            client.send_query(query);

            for (const auto& msg : client.receive_messages())
            {
                if (claude::is_assistant_message(msg))
                {
                    const auto& assistant = std::get<claude::AssistantMessage>(msg);
                    std::cout << claude::get_text_content(assistant.content) << std::flush;
                }
                else if (claude::is_result_message(msg))
                {
                    std::cout << "\n\n";
                    break;
                }
            }
        }

        client.disconnect();
        std::cout << "Done!\n";
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
