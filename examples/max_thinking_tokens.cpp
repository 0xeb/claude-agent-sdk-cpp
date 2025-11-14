// Demonstrates limiting thinking tokens for models that support "thinking"
#include <claude/claude.hpp>
#include <iostream>

using namespace claude;

int main()
{
    std::cout << "Claude SDK version: " << version_string() << "\n\n";

    ClaudeOptions opts;
    // Use a thinking-capable model alias or full name as available in your CLI
    // Adjust if your local CLI uses a different alias
    opts.model = "claude-sonnet-4.1-thinking";
    opts.max_thinking_tokens = 2048; // cap thinking tokens

    try
    {
        auto result = query(
            "Solve: You have 12 apples, you give 5 to Alice, 3 to Bob, then buy 7 more. "
            "Explain step by step, but be concise.",
            opts);

        for (const auto& msg : result)
        {
            if (is_assistant_message(msg))
            {
                const auto& a = std::get<AssistantMessage>(msg);
                std::cout << get_text_content(a.content) << "\n";
            }
            else if (is_result_message(msg))
            {
                const auto& r = std::get<ResultMessage>(msg);
                std::cout << "\nStatus: " << r.subtype << "\n";
            }
        }
    }
    catch (const CLINotFoundError& e)
    {
        std::cerr << "Claude CLI not found: " << e.what() << "\n";
        std::cerr << "Install with: npm install -g @anthropic-ai/claude-code\n";
        return 1;
    }
    catch (const ClaudeError& e)
    {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}

