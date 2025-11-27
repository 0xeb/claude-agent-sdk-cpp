#include <claude/claude.hpp>
#include <iostream>
#include <set>

int main()
{
    // Allow only specific tools
    std::set<std::string> allowed_tools = {"Read", "Glob", "Grep"};

    claude::ClaudeOptions opts;
    opts.permission_mode = "default";

    opts.tool_permission_callback =
        [&allowed_tools](const std::string& tool_name, const claude::json& input,
                         const claude::ToolPermissionContext& context) -> claude::PermissionResult
    {
        bool allowed = allowed_tools.count(tool_name) > 0;

        std::cout << "[TOOL] " << tool_name << (allowed ? " [ALLOWED]" : " [DENIED]") << "\n";

        if (allowed)
        {
            return claude::PermissionResultAllow{};
        }
        else
        {
            return claude::PermissionResultDeny{"deny", "Tool '" + tool_name +
                                                            "' is not in the allowed list"};
        }
    };

    try
    {
        claude::ClaudeClient client(opts);
        client.connect();

        std::cout << "Tool Permissions Example\n";
        std::cout << "Allowed tools: Read, Glob, Grep\n";
        std::cout << "All other tools will be denied\n\n";

        client.send_query("Search for all .cpp files, read one, "
                          "and then try to write a new file");

        for (const auto& msg : client.receive_messages())
        {
            if (claude::is_assistant_message(msg))
            {
                const auto& assistant = std::get<claude::AssistantMessage>(msg);
                std::cout << claude::get_text_content(assistant.content) << std::flush;
            }
            else if (claude::is_result_message(msg))
            {
                std::cout << "\n";
                break;
            }
        }

        client.disconnect();
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
