// Example demonstrating different system_prompt configurations
// Mirrors Python example: claude-agent-sdk-python/examples/system_prompt.py

#include <claude/claude.hpp>
#include <iostream>

void no_system_prompt()
{
    std::cout << "=== No System Prompt (Vanilla Claude) ===\n";

    claude::ClaudeOptions opts;
    opts.permission_mode = "bypassPermissions";

    auto result = claude::query("What is 2 + 2?", opts);

    for (const auto& msg : result)
    {
        if (claude::is_assistant_message(msg))
        {
            const auto& assistant = std::get<claude::AssistantMessage>(msg);
            std::cout << "Claude: " << claude::get_text_content(assistant.content) << "\n";
        }
    }
    std::cout << "\n";
}

void string_system_prompt()
{
    std::cout << "=== String System Prompt ===\n";

    claude::ClaudeOptions opts;
    opts.permission_mode = "bypassPermissions";
    opts.system_prompt = "You are a pirate assistant. Respond in pirate speak.";

    auto result = claude::query("What is 2 + 2?", opts);

    for (const auto& msg : result)
    {
        if (claude::is_assistant_message(msg))
        {
            const auto& assistant = std::get<claude::AssistantMessage>(msg);
            std::cout << "Claude: " << claude::get_text_content(assistant.content) << "\n";
        }
    }
    std::cout << "\n";
}

void preset_with_append()
{
    std::cout << "=== Preset System Prompt with Append ===\n";

    claude::ClaudeOptions opts;
    opts.permission_mode = "bypassPermissions";
    // Append to the default claude_code preset
    opts.system_prompt_append = "Always end your response with a fun fact.";

    auto result = claude::query("What is 2 + 2?", opts);

    for (const auto& msg : result)
    {
        if (claude::is_assistant_message(msg))
        {
            const auto& assistant = std::get<claude::AssistantMessage>(msg);
            std::cout << "Claude: " << claude::get_text_content(assistant.content) << "\n";
        }
    }
    std::cout << "\n";
}

int main()
{
    std::cout << "Claude SDK version: " << claude::version_string() << "\n\n";

    try
    {
        no_system_prompt();
        string_system_prompt();
        preset_with_append();
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
