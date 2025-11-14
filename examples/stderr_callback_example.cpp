// Example demonstrating stderr callback for observing CLI debug output
// Matches Python SDK stderr callback feature

#include <claude/claude.hpp>
#include <iostream>
#include <vector>

int main()
{
    std::cout << "Claude SDK version: " << claude::version_string() << "\n\n";

    // Store stderr output for demonstration
    std::vector<std::string> stderr_lines;

    // Configure options with stderr callback
    claude::ClaudeOptions opts;
    opts.permission_mode = "bypassPermissions";
    opts.stderr_callback = [&stderr_lines](const std::string& line) {
        // Callback is invoked for each line written to stderr by the CLI
        stderr_lines.push_back(line);
        std::cerr << "[STDERR] " << line << "\n";
    };

    // Optional: Enable debug output to see more stderr activity
    // opts.extra_args["debug-to-stderr"] = "";

    std::cout << "Running query with stderr callback enabled...\n\n";

    try
    {
        auto result = claude::query("What is 2+2? Be brief.", opts);

        for (const auto& msg : result)
        {
            if (claude::is_assistant_message(msg))
            {
                const auto& assistant = std::get<claude::AssistantMessage>(msg);
                std::cout << "Claude: " << claude::get_text_content(assistant.content) << "\n";
            }
        }

        std::cout << "\n=== Stderr Lines Captured ===" << "\n";
        if (stderr_lines.empty())
        {
            std::cout << "(No stderr output from CLI)\n";
        }
        else
        {
            std::cout << "Captured " << stderr_lines.size() << " stderr line(s):\n";
            for (size_t i = 0; i < stderr_lines.size(); ++i)
            {
                std::cout << "  [" << (i + 1) << "] " << stderr_lines[i] << "\n";
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

    return 0;
}
