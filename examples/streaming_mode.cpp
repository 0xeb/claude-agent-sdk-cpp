#include <claude/claude.hpp>
#include <iostream>
#include <string>
#include <vector>

int main()
{
    try
    {
        std::cout << "Claude Streaming Client Example\n";
        std::cout << "================================\n\n";

        // Configure client options
        claude::ClaudeOptions opts;
        opts.permission_mode = "bypassPermissions";

        // Create client
        claude::ClaudeClient client(opts);

        std::cout << "Connecting to Claude Code CLI...\n";
        client.connect();
        std::cout << "Connected!\n";
        std::cout << "Claude CLI Process ID: " << client.get_pid() << "\n\n";

        // Array of queries
        std::vector<std::string> queries = {
            "Hello! Please introduce yourself briefly in 2-3 sentences.",
            "What's 17 * 23? Just give me the answer.",
            "What was the math problem I just asked you about?"};

        // Process each query
        for (size_t i = 0; i < queries.size(); ++i)
        {
            const auto& query = queries[i];
            bool is_first = (i == 0);

            std::cout << "You: " << query << "\n\n";
            client.send_query(query);

            std::cout << "Claude: ";
            std::cout.flush();

            // Stream messages as they arrive
            for (const auto& msg : client.receive_messages())
            {
                if (claude::is_assistant_message(msg))
                {
                    const auto& assistant = std::get<claude::AssistantMessage>(msg);
                    std::string text = claude::get_text_content(assistant.content);
                    std::cout << text;
                    std::cout.flush();
                }
                else if (claude::is_result_message(msg))
                {
                    std::cout << "\n\n";

                    // Show detailed usage info on first query only
                    if (is_first)
                    {
                        const auto& result = std::get<claude::ResultMessage>(msg);
                        std::cout << "[Session: " << result.result.session_id << "]\n";
                        std::cout << "[Tokens - Input: " << result.result.usage.input_tokens
                                  << ", Output: " << result.result.usage.output_tokens << "]\n";
                        std::cout << "[Cost: $" << result.result.cost.total << "]\n";
                    }
                    else
                    {
                        const auto& result = std::get<claude::ResultMessage>(msg);
                        std::cout << "[Tokens - Input: " << result.result.usage.input_tokens
                                  << ", Output: " << result.result.usage.output_tokens << "]\n";
                    }
                    break;
                }
            }

            std::cout << "\n";
        }

        // ============================================================
        // Cleanup
        // ============================================================
        std::cout << "Disconnecting...\n";
        client.disconnect();
        std::cout << "Done!\n";
    }
    catch (const claude::ClaudeError& e)
    {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Unexpected error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
