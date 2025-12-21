#include "../../src/internal/subprocess/process.hpp"

#include <atomic>
#include <claude/client.hpp>
#include <cstdlib>
#include <gtest/gtest.h>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

using namespace claude;

// ============================================================================
// EXTRA: Multithreading Tests
// ============================================================================
//
// These tests verify that multiple ClaudeClient instances can run concurrently
// in separate threads without interference.
//
// IMPORTANT NOTES:
// - Each ClaudeClient spawns its own Claude CLI subprocess
// - Running many concurrent CLI instances is resource-intensive
// - Tests may be slower than typical unit tests due to subprocess overhead
// - Some tests are DISABLED by default due to long runtime
//
// TEST CATEGORIES:
// 1. MultipleClientsSequential    - Baseline: clients created one after another
// 2. MultipleClientsParallel      - Main test: 3 concurrent client threads
// 3. DISABLED_ConcurrentQueries   - Stress test: 5 concurrent threads (disabled)
// 4. ClientLifetimeInThread       - Verifies client creation/destruction in thread
//
// To run disabled tests:
//   ./test_claude --gtest_also_run_disabled_tests --gtest_filter="MultithreadingTest.*"
// ============================================================================

namespace
{

std::optional<std::string> cli_skip_reason()
{
    if (std::getenv("CLAUDE_AGENT_SDK_REQUIRE_EXPLICIT_CLI") != nullptr &&
        (std::getenv("CLAUDE_CLI_PATH") == nullptr || std::getenv("CLAUDE_CLI_PATH")[0] == '\0'))
    {
        return std::string(
            "CLAUDE_AGENT_SDK_REQUIRE_EXPLICIT_CLI is set; set CLAUDE_CLI_PATH to run this test.");
    }

    if (const char* env_cli = std::getenv("CLAUDE_CLI_PATH");
        env_cli != nullptr && env_cli[0] != '\0')
        return std::nullopt;

    if (!claude::subprocess::find_executable("claude").has_value())
        return std::string("Claude CLI not found in PATH; install it or set CLAUDE_CLI_PATH.");

    return std::nullopt;
}

ClaudeOptions make_thread_client_options()
{
    ClaudeOptions opts;
    opts.permission_mode = "bypassPermissions";

    if (const char* env_cli = std::getenv("CLAUDE_CLI_PATH");
        env_cli != nullptr && env_cli[0] != '\0')
        opts.cli_path = env_cli;
    else if (auto found = claude::subprocess::find_executable("claude"))
        opts.cli_path = *found;

    return opts;
}

// Thread-safe result collector
struct ThreadResult
{
    int thread_id;
    bool success;
    std::string error_message;
};

class ThreadResultCollector
{
  public:
    void add_result(int thread_id, bool success, const std::string& error = "")
    {
        std::lock_guard<std::mutex> lock(mutex_);
        results_.push_back({thread_id, success, error});
    }

    std::vector<ThreadResult> get_results() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return results_;
    }

    int count_successful() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        int count = 0;
        for (const auto& result : results_)
            if (result.success)
                ++count;
        return count;
    }

  private:
    mutable std::mutex mutex_;
    std::vector<ThreadResult> results_;
};

// Worker function that each thread will execute
void worker_thread(int thread_id, ThreadResultCollector& collector)
{
    try
    {
        ClaudeClient client(make_thread_client_options());
        client.connect();

        // Each thread asks a different simple question
        std::string query;
        switch (thread_id % 5)
        {
        case 0:
            query = "What is 2+2? Answer with just the number.";
            break;
        case 1:
            query = "What is 3*3? Answer with just the number.";
            break;
        case 2:
            query = "What is 10-5? Answer with just the number.";
            break;
        case 3:
            query = "What is 8/2? Answer with just the number.";
            break;
        case 4:
            query = "What is 5+3? Answer with just the number.";
            break;
        }

        client.send_query(query);

        // Receive response
        auto messages = client.receive_response();

        // Verify we got a response
        bool found_assistant = false;
        bool found_result = false;

        for (const auto& msg : messages)
        {
            if (is_assistant_message(msg))
            {
                found_assistant = true;
                const auto& assistant = std::get<AssistantMessage>(msg);
                std::string text = get_text_content(assistant.content);
                if (text.empty())
                    throw std::runtime_error("Empty assistant response");
            }
            if (is_result_message(msg))
                found_result = true;
        }

        if (!found_assistant || !found_result)
            throw std::runtime_error("Incomplete response");

        client.disconnect();

        // Report success
        collector.add_result(thread_id, true);
    }
    catch (const std::exception& e)
    {
        collector.add_result(thread_id, false, e.what());
    }
    catch (...)
    {
        collector.add_result(thread_id, false, "Unknown exception");
    }
}

} // anonymous namespace

TEST(MultithreadingTest, MultipleClientsSequential)
{
    if (auto reason = cli_skip_reason())
        GTEST_SKIP() << *reason;

    // First test: Create multiple clients sequentially (not in threads)
    // This verifies basic multi-client support
    const int num_clients = 3;
    ThreadResultCollector collector;

    for (int i = 0; i < num_clients; ++i)
        worker_thread(i, collector);

    EXPECT_EQ(collector.count_successful(), num_clients);

    auto results = collector.get_results();
    for (const auto& result : results)
    {
        EXPECT_TRUE(result.success)
            << "Thread " << result.thread_id << " failed: " << result.error_message;
    }
}

TEST(MultithreadingTest, DISABLED_MultipleClientsParallel)
{
    if (auto reason = cli_skip_reason())
        GTEST_SKIP() << *reason;

    // Main test: Create multiple clients in parallel threads
    // Note: Each client spawns a Claude CLI subprocess, so this is resource-intensive
    const int num_threads = 3;
    ThreadResultCollector collector;
    std::vector<std::thread> threads;

    // Launch threads
    for (int i = 0; i < num_threads; ++i)
        threads.emplace_back(worker_thread, i, std::ref(collector));

    // Wait for all threads to complete
    for (auto& thread : threads)
        thread.join();

    // Verify all threads succeeded
    EXPECT_EQ(collector.count_successful(), num_threads);

    auto results = collector.get_results();
    EXPECT_EQ(results.size(), num_threads);

    for (const auto& result : results)
    {
        EXPECT_TRUE(result.success)
            << "Thread " << result.thread_id << " failed: " << result.error_message;
    }
}

TEST(MultithreadingTest, DISABLED_ConcurrentQueriesStressTest)
{
    if (auto reason = cli_skip_reason())
        GTEST_SKIP() << *reason;

    // Stress test: More threads doing shorter operations
    // DISABLED by default due to long runtime (~2-5 minutes)
    // Enable with: --gtest_also_run_disabled_tests
    const int num_threads = 5;
    ThreadResultCollector collector;
    std::vector<std::thread> threads;
    std::atomic<int> active_clients{0};
    std::atomic<int> peak_clients{0};

    auto stress_worker = [&](int thread_id)
    {
        try
        {
            ClaudeClient client(make_thread_client_options());
            client.connect();

            // Track active clients
            int current = ++active_clients;
            int prev_peak = peak_clients.load();
            while (current > prev_peak && !peak_clients.compare_exchange_weak(prev_peak, current))
            {
                // Retry if another thread updated peak_clients
            }

            // Simple query
            client.send_query("Reply with just 'OK'");
            auto messages = client.receive_response();

            bool found_response = false;
            for (const auto& msg : messages)
            {
                if (is_assistant_message(msg) || is_result_message(msg))
                {
                    found_response = true;
                    break;
                }
            }

            if (!found_response)
                throw std::runtime_error("No response received");

            client.disconnect();
            --active_clients;

            collector.add_result(thread_id, true);
        }
        catch (const std::exception& e)
        {
            --active_clients;
            collector.add_result(thread_id, false, e.what());
        }
    };

    // Launch all threads at once
    for (int i = 0; i < num_threads; ++i)
        threads.emplace_back(stress_worker, i);

    // Wait for completion
    for (auto& thread : threads)
        thread.join();

    // Report results
    auto results = collector.get_results();
    int successful = collector.count_successful();

    // We expect at least most threads to succeed (some might fail due to system limits)
    EXPECT_GE(successful, num_threads * 0.8) << "At least 80% of threads should succeed";
    EXPECT_GT(peak_clients.load(), 1) << "Multiple clients should have been active concurrently";

    // Log any failures
    for (const auto& result : results)
    {
        if (!result.success)
        {
            std::cerr << "Thread " << result.thread_id << " failed: " << result.error_message
                      << std::endl;
        }
    }
}

TEST(MultithreadingTest, ClientLifetimeInThread)
{
    if (auto reason = cli_skip_reason())
        GTEST_SKIP() << *reason;

    // Test client creation, usage, and destruction within a thread
    std::atomic<bool> test_passed{false};
    std::string error_message;

    std::thread worker(
        [&]()
        {
            try
            {
                ClaudeOptions opts;
                opts.permission_mode = "bypassPermissions";

                // Create client in thread scope
                ClaudeClient client(opts);
                client.connect();

                client.send_query("Say 'Hello'");
                auto messages = client.receive_response();

                bool found_message = false;
                for (const auto& msg : messages)
                {
                    if (is_assistant_message(msg))
                    {
                        found_message = true;
                        break;
                    }
                }

                if (!found_message)
                    throw std::runtime_error("No assistant message found");

                client.disconnect();
                test_passed = true;

                // Client destructor will be called here
            }
            catch (const std::exception& e)
            {
                error_message = e.what();
            }
        });

    worker.join();

    EXPECT_TRUE(test_passed) << "Error: " << error_message;
}
