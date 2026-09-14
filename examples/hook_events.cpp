// Mirrors: reference/claude-agent-sdk-python hooks streaming via
//          ``ClaudeAgentOptions(include_hook_events=True)`` (Python commit
//          c1182a4). Iterates the assistant stream and prints every
//          ``HookEventMessage`` the CLI surfaces.
//
// Requires the claude CLI; when missing, this example prints a usage hint and
// exits 0 so it remains buildable everywhere.

#include <claude/claude.hpp>

#include <iostream>
#include <variant>

int main()
{
    std::cout << "Claude SDK " << claude::version_string() << " - hook events stream\n\n";

    claude::ClaudeOptions opts;
    opts.permission_mode = "bypassPermissions";
    opts.include_hook_events = true;
    opts.model = "claude-sonnet-4-5";

    try
    {
        auto result = claude::query("List two files in the current directory.", opts);

        int hook_event_count = 0;
        for (const auto& msg : result)
        {
            if (claude::is_system_message(msg))
            {
                const auto& sys = std::get<claude::SystemMessage>(msg);
                if (sys.subtype == "hook_event" || sys.subtype == "hook_started" ||
                    sys.subtype == "hook_response")
                {
                    ++hook_event_count;
                    std::cout << "[hook] " << sys.subtype << ": " << sys.content << "\n";
                }
            }
            else if (claude::is_assistant_message(msg))
            {
                const auto& assistant = std::get<claude::AssistantMessage>(msg);
                std::cout << "[assistant] "
                          << claude::get_text_content(assistant.content) << "\n";
            }
        }
        std::cout << "\nTotal hook events seen: " << hook_event_count << "\n";
    }
    catch (const claude::CLINotFoundError&)
    {
        std::cout << "claude CLI not detected — set CLAUDE_CLI_PATH or install "
                     "@anthropic-ai/claude-code and re-run.\n";
        return 0;
    }
    catch (const claude::ClaudeError& e)
    {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
