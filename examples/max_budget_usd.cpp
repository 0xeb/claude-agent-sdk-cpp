// Demonstrates using max_budget_usd to control API cost
#include <claude/claude.hpp>
#include <iostream>
#include <string>

using namespace claude;

static void run_query(const std::string& title, const std::string& prompt, ClaudeOptions opts)
{
    std::cout << "=== " << title << " ===\n";
    try
    {
        auto result = query(prompt, opts);
        std::string text;
        for (const auto& msg : result)
        {
            if (is_assistant_message(msg))
            {
                const auto& a = std::get<AssistantMessage>(msg);
                text = get_text_content(a.content);
            }
            else if (is_result_message(msg))
            {
                const auto& r = std::get<ResultMessage>(msg);
                std::cout << "Status: " << r.subtype << "\n";
                // Cost fields are optional; print if non-zero
                double total_cost = r.result.cost.total;
                if (total_cost > 0.0)
                {
                    std::cout << "Total cost: $" << total_cost << "\n";
                }
            }
        }
        if (!text.empty())
        {
            std::cout << "Claude: " << text << "\n";
        }
        std::cout << "\n";
    }
    catch (const CLINotFoundError& e)
    {
        std::cerr << "Claude CLI not found: " << e.what() << "\n";
        std::cerr << "Install with: npm install -g @anthropic-ai/claude-code\n";
    }
    catch (const ClaudeError& e)
    {
        std::cerr << "Error: " << e.what() << "\n";
    }
}

int main()
{
    std::cout << "Claude SDK version: " << version_string() << "\n\n";

    // No explicit budget
    {
        ClaudeOptions opts;
        opts.model = "claude-sonnet-4-5";
        run_query("Without Budget Limit", "What is 2 + 2?", opts);
    }

    // Reasonable budget (should not be exceeded for a simple query)
    {
        ClaudeOptions opts;
        opts.model = "claude-sonnet-4-5";
        opts.max_budget_usd = 0.10; // $0.10
        run_query("With Reasonable Budget ($0.10)", "What is 2 + 2?", opts);
    }

    // Very tight budget that may be exceeded
    {
        ClaudeOptions opts;
        opts.model = "claude-sonnet-4-5";
        opts.max_budget_usd = 0.0001; // likely exceeded quickly
        run_query("With Tight Budget ($0.0001)", "Summarize the repository structure", opts);
        std::cout << "Note: Budget enforcement is checked after API calls;\n"
                     "final cost can slightly exceed the target.\n\n";
    }

    return 0;
}

