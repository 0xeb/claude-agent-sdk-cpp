/**
 * Example of using custom agents with Claude Code SDK.
 *
 * This example demonstrates how to define and use custom agents with specific
 * tools, prompts, and models.
 *
 * Port of: claude-agent-sdk-python/examples/agents.py
 *
 * Usage:
 * ./agents - Run the example
 */

#include <claude/claude.hpp>
#include <iostream>
#include <string>

void code_reviewer_example()
{
    std::cout << "=== Code Reviewer Agent Example ===\n";

    claude::ClaudeOptions opts;
    opts.permission_mode = "bypassPermissions";

    // Define a custom code reviewer agent
    claude::AgentDefinition code_reviewer;
    code_reviewer.description = "Reviews code for best practices and potential issues";
    code_reviewer.prompt = "You are a code reviewer. Analyze code for bugs, performance issues, "
                           "security vulnerabilities, and adherence to best practices. "
                           "Provide constructive feedback.";
    code_reviewer.tools = std::vector<std::string>{"Read", "Grep"};
    code_reviewer.model = "sonnet";
    opts.agents["code-reviewer"] = code_reviewer;

    try
    {
        auto result = claude::query(
            "Use the code-reviewer agent to briefly describe what types are available "
            "in this SDK. Be concise - just list 3-5 main types.",
            opts);

        for (const auto& msg : result)
        {
            if (claude::is_assistant_message(msg))
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

void documentation_writer_example()
{
    std::cout << "=== Documentation Writer Agent Example ===\n";

    claude::ClaudeOptions opts;
    opts.permission_mode = "bypassPermissions";

    // Define a documentation writer agent
    claude::AgentDefinition doc_writer;
    doc_writer.description = "Writes comprehensive documentation";
    doc_writer.prompt = "You are a technical documentation expert. Write clear, comprehensive "
                        "documentation with examples. Focus on clarity and completeness.";
    doc_writer.tools = std::vector<std::string>{"Read", "Write", "Edit"};
    doc_writer.model = "sonnet";
    opts.agents["doc-writer"] = doc_writer;

    try
    {
        auto result = claude::query("Use the doc-writer agent to explain what AgentDefinition is "
                                    "used for in 2-3 sentences.",
                                    opts);

        for (const auto& msg : result)
        {
            if (claude::is_assistant_message(msg))
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

void multiple_agents_example()
{
    std::cout << "=== Multiple Agents Example ===\n";

    claude::ClaudeOptions opts;
    opts.permission_mode = "bypassPermissions";

    // Define multiple custom agents
    claude::AgentDefinition analyzer;
    analyzer.description = "Analyzes code structure and patterns";
    analyzer.prompt =
        "You are a code analyzer. Examine code structure, patterns, and architecture.";
    analyzer.tools = std::vector<std::string>{"Read", "Grep", "Glob"};
    // analyzer.model is left as nullopt to inherit from parent
    opts.agents["analyzer"] = analyzer;

    claude::AgentDefinition tester;
    tester.description = "Creates and runs tests";
    tester.prompt = "You are a testing expert. Write comprehensive tests and ensure code quality.";
    tester.tools = std::vector<std::string>{"Read", "Write", "Bash"};
    tester.model = "sonnet";
    opts.agents["tester"] = tester;

    // Also set setting_sources
    opts.setting_sources = std::vector<std::string>{"user", "project"};

    try
    {
        auto result = claude::query(
            "Use the analyzer agent to find how many C++ example files exist in this project. "
            "Just give me the count.",
            opts);

        for (const auto& msg : result)
        {
            if (claude::is_assistant_message(msg))
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
    std::cout << "This example demonstrates custom agent definitions.\n";
    std::cout << "Each agent has specific tools, prompts, and optionally a model.\n\n";

    code_reviewer_example();
    documentation_writer_example();
    multiple_agents_example();

    return 0;
}
