/**
 * @file error_handling.cpp
 * @brief Comprehensive error handling example
 *
 * Demonstrates:
 * - Catching all SDK exception types
 * - Providing helpful error messages
 * - Graceful degradation
 * - Recovery strategies
 *
 * This example shows best practices for production error handling.
 */

#include <claude/claude.hpp>
#include <iostream>
#include <string>

// Helper function to demonstrate error handling in different scenarios
void demonstrate_error_handling(const std::string& scenario)
{
    std::cout << "\n" << std::string(60, '=') << "\n";
    std::cout << "Scenario: " << scenario << "\n";
    std::cout << std::string(60, '=') << "\n\n";
}

// Example 1: Basic error handling
void example_basic_error_handling()
{
    demonstrate_error_handling("Basic Error Handling");

    try
    {
        claude::ClaudeOptions opts;
        opts.permission_mode = "bypassPermissions";

        auto messages = claude::query("Hello, Claude!", opts);

        for (const auto& msg : messages)
        {
            if (claude::is_assistant_message(msg))
            {
                const auto& assistant = std::get<claude::AssistantMessage>(msg);
                std::cout << claude::get_text_content(assistant.content) << "\n";
            }
        }

        std::cout << "\n✓ Query succeeded\n";
    }
    catch (const claude::CLINotFoundError& e)
    {
        std::cerr << "\n✗ CLI Not Found Error:\n";
        std::cerr << "  " << e.what() << "\n";
        std::cerr << "\nSolution:\n";
        std::cerr << "  npm install -g @anthropic-ai/claude-code\n";
        std::cerr << "  Then verify: claude -v\n";
    }
    catch (const claude::ProcessError& e)
    {
        std::cerr << "\n✗ Process Error:\n";
        std::cerr << "  " << e.what() << "\n";
        std::cerr << "  Exit code: " << e.exit_code() << "\n";
        std::cerr << "\nPossible causes:\n";
        std::cerr << "  - CLI crashed\n";
        std::cerr << "  - Invalid arguments\n";
        std::cerr << "  - Permission issues\n";
    }
    catch (const claude::JSONDecodeError& e)
    {
        std::cerr << "\n✗ JSON Decode Error:\n";
        std::cerr << "  " << e.what() << "\n";
        std::cerr << "\nPossible causes:\n";
        std::cerr << "  - CLI version mismatch\n";
        std::cerr << "  - Corrupted output\n";
        std::cerr << "  - Binary data in text stream\n";
    }
    catch (const claude::CLIConnectionError& e)
    {
        std::cerr << "\n✗ CLI Connection Error:\n";
        std::cerr << "  " << e.what() << "\n";
        std::cerr << "\nPossible causes:\n";
        std::cerr << "  - CLI not responding\n";
        std::cerr << "  - Timeout\n";
        std::cerr << "  - Network issues\n";
    }
    catch (const claude::ClaudeError& e)
    {
        std::cerr << "\n✗ Claude SDK Error:\n";
        std::cerr << "  " << e.what() << "\n";
    }
    catch (const std::exception& e)
    {
        std::cerr << "\n✗ Unexpected Error:\n";
        std::cerr << "  " << e.what() << "\n";
    }
}

// Example 2: Client with cleanup guarantee
void example_client_with_cleanup()
{
    demonstrate_error_handling("Client with Guaranteed Cleanup");

    claude::ClaudeClient client(claude::ClaudeOptions{});
    bool connected = false;

    try
    {
        claude::ClaudeOptions opts;
        opts.permission_mode = "bypassPermissions";

        claude::ClaudeClient client(opts);
        client.connect();
        connected = true;
        std::cout << "✓ Connected to Claude\n";

        // Send query
        client.send_query("Tell me a short joke");

        // Receive response
        for (const auto& msg : client.receive_messages())
        {
            if (claude::is_assistant_message(msg))
            {
                const auto& assistant = std::get<claude::AssistantMessage>(msg);
                std::cout << "\n" << claude::get_text_content(assistant.content) << "\n";
            }
            if (claude::is_result_message(msg))
                break;
        }

        // Clean disconnect
        client.disconnect();
        connected = false;
        std::cout << "\n✓ Disconnected cleanly\n";
    }
    catch (const claude::ClaudeError& e)
    {
        std::cerr << "\n✗ Error: " << e.what() << "\n";

        // Ensure cleanup even on error
        if (connected)
        {
            std::cerr << "  Attempting cleanup...\n";
            try
            {
                client.disconnect();
                std::cerr << "  ✓ Cleanup successful\n";
            }
            catch (...)
            {
                std::cerr << "  ✗ Cleanup failed\n";
            }
        }
    }
}

// Example 3: Retry logic
bool query_with_retry(const std::string& prompt, int max_retries = 3)
{
    for (int attempt = 1; attempt <= max_retries; attempt++)
    {
        try
        {
            std::cout << "Attempt " << attempt << "/" << max_retries << "...\n";

            claude::ClaudeOptions opts;
            opts.permission_mode = "bypassPermissions";

            auto messages = claude::query(prompt, opts);

            for (const auto& msg : messages)
            {
                if (claude::is_assistant_message(msg))
                {
                    const auto& assistant = std::get<claude::AssistantMessage>(msg);
                    std::cout << claude::get_text_content(assistant.content) << "\n";
                }
            }

            std::cout << "✓ Success!\n";
            return true;
        }
        catch (const claude::ProcessError& e)
        {
            std::cerr << "✗ Process error (attempt " << attempt << "): " << e.what() << "\n";
            if (attempt < max_retries)
            {
                std::cerr << "  Retrying in 2 seconds...\n";
                std::this_thread::sleep_for(std::chrono::seconds(2));
            }
        }
        catch (const claude::ClaudeError& e)
        {
            std::cerr << "✗ Fatal error: " << e.what() << "\n";
            return false; // Don't retry on fatal errors
        }
    }

    std::cerr << "✗ All retry attempts failed\n";
    return false;
}

void example_retry_logic()
{
    demonstrate_error_handling("Retry Logic");
    query_with_retry("What is 2+2?", 3);
}

// Example 4: Error logging
class ErrorLogger
{
  public:
    static void log_error(const std::string& context, const std::exception& e)
    {
        std::cerr << "[" << get_timestamp() << "] ERROR in " << context << ":\n";
        std::cerr << "  " << e.what() << "\n";

        // In production, write to file or logging system
    }

    static std::string get_timestamp()
    {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        char buffer[26];
        ctime_s(buffer, sizeof(buffer), &time_t);
        std::string timestamp(buffer);
        timestamp.pop_back(); // Remove newline
        return timestamp;
    }
};

void example_error_logging()
{
    demonstrate_error_handling("Error Logging");

    try
    {
        claude::ClaudeOptions opts;
        opts.permission_mode = "bypassPermissions";

        auto messages = claude::query("Test query", opts);
        std::cout << "✓ Query succeeded\n";
    }
    catch (const claude::ClaudeError& e)
    {
        ErrorLogger::log_error("query execution", e);
    }
}

// Main entry point
int main()
{
    std::cout << "Claude SDK Error Handling Examples\n";
    std::cout << std::string(60, '=') << "\n";

    // Run all examples
    example_basic_error_handling();
    example_client_with_cleanup();
    example_retry_logic();
    example_error_logging();

    std::cout << "\n" << std::string(60, '=') << "\n";
    std::cout << "All examples completed\n";

    return 0;
}
