#include <chrono>
#include <claude/claude.hpp>
#include <iostream>
#include <vector>

constexpr bool TIMING = true;
constexpr bool DEBUG = true;
constexpr bool VERBOSE = false;
constexpr bool DUMP_JSON = false; // Enable to see raw JSON responses

int main()
{
    std::cout << "Claude SDK version: " << claude::version_string() << "\n\n";

    claude::ClaudeOptions opts;
    opts.permission_mode = "bypassPermissions";
    opts.model = "claude-sonnet-4-5";

    std::vector<std::string> queries = {"What is 2+2? Be very brief.", "Name a primary color.",
                                        "What year comes after 2024?",
                                        "Is water wet? One word answer."};

    std::vector<double> timings;

    std::cout << "Running " << queries.size() << " queries...\n\n";

    for (size_t i = 0; i < queries.size(); ++i)
    {
        std::cout << "Query " << (i + 1) << ": " << queries[i] << "\n";

        auto start = std::chrono::high_resolution_clock::now();

        claude::QueryResult result;
        try
        {
            result = claude::query(queries[i], opts);
        }
        catch (const claude::CLINotFoundError& e)
        {
            std::cerr << "Error: Claude CLI not found - " << e.what() << "\n";
            std::cerr << "Please install: npm install -g @anthropic-ai/claude-code\n";
            return 1;
        }
        catch (const claude::ProcessError& e)
        {
            std::cerr << "Error: CLI process failed - " << e.what() << "\n";
            return 1;
        }
        catch (const claude::JSONDecodeError& e)
        {
            std::cerr << "Error: JSON parsing failed - " << e.what() << "\n";
            return 1;
        }
        catch (const claude::ClaudeError& e)
        {
            std::cerr << "Error: " << e.what() << "\n";
            return 1;
        }

        std::string response;
        int message_count = 0;

        for (const auto& msg : result)
        {
            message_count++;

            if (VERBOSE)
            {
                std::cout << "  [Message " << message_count << "] ";
                if (claude::is_assistant_message(msg))
                    std::cout << "AssistantMessage\n";
                else if (claude::is_result_message(msg))
                    std::cout << "ResultMessage\n";
                else if (claude::is_system_message(msg))
                    std::cout << "SystemMessage\n";
                else
                    std::cout << "UnknownMessage\n";
            }

            if (claude::is_assistant_message(msg))
            {
                const auto& assistant = std::get<claude::AssistantMessage>(msg);
                response = claude::get_text_content(assistant.content);

                if (DUMP_JSON)
                {
                    std::cout << "  [RAW JSON]:\n";
                    std::cout << claude::dump_raw_json(assistant) << "\n";
                }
            }
            else if (claude::is_result_message(msg) && DUMP_JSON)
            {
                const auto& result_msg = std::get<claude::ResultMessage>(msg);
                std::cout << "  [RESULT RAW JSON]:\n";
                std::cout << claude::dump_raw_json(result_msg) << "\n";
            }
            else if (claude::is_system_message(msg) && (VERBOSE || DUMP_JSON))
            {
                const auto& system = std::get<claude::SystemMessage>(msg);
                if (DUMP_JSON)
                {
                    std::cout << "  [SYSTEM RAW JSON]:\n";
                    std::cout << claude::dump_raw_json(system) << "\n";
                }
                else if (VERBOSE && !system.content.empty())
                {
                    std::cout << "  System: " << system.content << "\n";
                }
            }
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        timings.push_back(duration.count());

        std::cout << "Response: " << response << "\n";

        if (TIMING)
            std::cout << "Time: " << duration.count() << " ms";

        if (DEBUG)
            std::cout << " (" << message_count << " messages)";

        std::cout << "\n\n";
    }

    if (TIMING)
    {
        std::cout << "=== Timing Summary ===\n";
        double total = 0;
        for (size_t i = 0; i < timings.size(); ++i)
        {
            std::cout << "Query " << (i + 1) << ": " << timings[i] << " ms\n";
            total += timings[i];
        }
        double average = total / timings.size();
        std::cout << "\nTotal time: " << total << " ms\n";
        std::cout << "Average time per query: " << average << " ms\n";
    }

    return 0;
}
