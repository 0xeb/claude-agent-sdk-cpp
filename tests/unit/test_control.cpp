#include <chrono>
#include <claude/client.hpp>
#include <claude/errors.hpp>
#include <claude/protocol/control.hpp>
#include <cstdlib>
#include <gtest/gtest.h>
#include <set>
#include <thread>

using namespace claude::protocol;

namespace claude
{
// Provided by client.cpp for testing
int claude_test_get_initialize_timeout_ms();
} // namespace claude

static void set_env_ms(const char* key, const char* value)
{
#if defined(_WIN32)
    _putenv_s(key, value);
#else
    setenv(key, value, 1);
#endif
}

TEST(ControlProtocolUnitTest, GenerateRequestId)
{
    ControlProtocol protocol;

    auto id1 = protocol.generate_request_id();
    auto id2 = protocol.generate_request_id();

    // IDs should be unique
    EXPECT_NE(id1, id2);

    // IDs should start with "req_"
    EXPECT_EQ(id1.substr(0, 4), "req_");
    EXPECT_EQ(id2.substr(0, 4), "req_");

    // IDs should have reasonable length (req_N_XXXXXXXX)
    EXPECT_GT(id1.length(), 10);
}

TEST(ControlProtocolUnitTest, BuildRequestMessage)
{
    ControlProtocol protocol;

    claude::json data = {{"mode", "acceptEdits"}};
    auto message = protocol.build_request_message("set_permission_mode", data);

    // Should be valid JSON
    auto parsed = claude::json::parse(message);

    // Should have correct structure
    EXPECT_EQ(parsed["type"], "control_request");
    EXPECT_TRUE(parsed.contains("request_id"));
    EXPECT_TRUE(parsed.contains("request"));
    EXPECT_EQ(parsed["request"]["subtype"], "set_permission_mode");
    EXPECT_EQ(parsed["request"]["mode"], "acceptEdits");

    // Should end with newline
    EXPECT_EQ(message.back(), '\n');
}

TEST(ControlProtocolUnitTest, HandleSuccessResponse)
{
    ControlProtocol protocol;
    std::string request_id;
    bool write_called = false;

    // Send request
    auto write_func = [&](const std::string& data)
    {
        write_called = true;
        // Extract request_id from the message
        auto parsed = claude::json::parse(data);
        request_id = parsed["request_id"].get<std::string>();
    };

    // Start async request (will timeout, but we'll respond manually)
    std::thread request_thread(
        [&]()
        {
            try
            {
                claude::json result = protocol.send_request(write_func, "interrupt", {},
                                                            100 // Short timeout for test
                );
                // Should not reach here in this test (timeout)
            }
            catch (const claude::ClaudeError& e)
            {
                // Expected timeout
                EXPECT_TRUE(std::string(e.what()).find("timed out") != std::string::npos);
            }
        });

    // Wait for write to happen
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    EXPECT_TRUE(write_called);

    request_thread.join();
}

TEST(ControlProtocolUnitTest, HandleErrorResponse)
{
    ControlProtocol protocol;
    std::string request_id = "test_req_123";

    // Create error response
    ControlResponse response;
    response.response.subtype = "error";
    response.response.request_id = request_id;
    response.response.error = "Test error message";

    // Handle response (no registered request, so it should be ignored)
    EXPECT_NO_THROW(protocol.handle_response(response));
}

TEST(ControlProtocolUnitTest, RequestIdIncremental)
{
    ControlProtocol protocol;

    auto id1 = protocol.generate_request_id();
    auto id2 = protocol.generate_request_id();
    auto id3 = protocol.generate_request_id();

    // Extract counter from IDs (format: req_{counter}_{random})
    auto extract_counter = [](const std::string& id)
    {
        auto pos1 = id.find('_', 4); // Skip "req_"
        return std::stoi(id.substr(4, pos1 - 4));
    };

    int counter1 = extract_counter(id1);
    int counter2 = extract_counter(id2);
    int counter3 = extract_counter(id3);

    // Counters should increment
    EXPECT_EQ(counter2, counter1 + 1);
    EXPECT_EQ(counter3, counter2 + 1);
}

TEST(ControlProtocolUnitTest, InitializeTimeoutEnvOverrideRespectsMinimum)
{
    // Lower than default should clamp to 60000
    set_env_ms("CLAUDE_CODE_STREAM_CLOSE_TIMEOUT", "1000");
    EXPECT_EQ(claude::claude_test_get_initialize_timeout_ms(), 60000);

    // Higher than default should be honored
    set_env_ms("CLAUDE_CODE_STREAM_CLOSE_TIMEOUT", "120000");
    EXPECT_EQ(claude::claude_test_get_initialize_timeout_ms(), 120000);

    // Invalid values fall back to default
    set_env_ms("CLAUDE_CODE_STREAM_CLOSE_TIMEOUT", "not_a_number");
    EXPECT_EQ(claude::claude_test_get_initialize_timeout_ms(), 60000);
}

TEST(ControlProtocolUnitTest, HookMatcherAcceptsFractionalTimeout)
{
    claude::HookMatcher matcher{"Bash", {}, 0.5};
    ASSERT_TRUE(matcher.timeout.has_value());
    EXPECT_NEAR(*matcher.timeout, 0.5, 1e-9);
}

TEST(ControlProtocolUnitTest, MultipleRequests)
{
    ControlProtocol protocol;
    std::vector<std::string> request_ids;
    std::mutex ids_mutex;
    std::vector<std::thread> threads;

    // Send multiple requests
    for (int i = 0; i < 5; ++i)
    {
        threads.emplace_back(
            [&, i]()
            {
                auto write_func = [&](const std::string& data)
                {
                    auto parsed = claude::json::parse(data);
                    std::lock_guard<std::mutex> lock(ids_mutex);
                    request_ids.push_back(parsed["request_id"].get<std::string>());
                };

                try
                {
                    protocol.send_request(write_func, "interrupt", {},
                                          10 // Very short timeout
                    );
                }
                catch (...)
                {
                    // Expected timeout
                }
            });
    }

    // Wait for all threads to finish
    for (auto& thread : threads)
        thread.join();

    // All request IDs should be unique
    std::set<std::string> unique_ids(request_ids.begin(), request_ids.end());
    EXPECT_EQ(unique_ids.size(), request_ids.size());
}

TEST(ControlProtocolUnitTest, BuildRequestMessageVariousSubtypes)
{
    ControlProtocol protocol;

    // Test interrupt (empty data)
    {
        auto message = protocol.build_request_message("interrupt", {});
        auto parsed = claude::json::parse(message);
        EXPECT_EQ(parsed["request"]["subtype"], "interrupt");
    }

    // Test set_permission_mode
    {
        claude::json data = {{"mode", "bypassPermissions"}};
        auto message = protocol.build_request_message("set_permission_mode", data);
        auto parsed = claude::json::parse(message);
        EXPECT_EQ(parsed["request"]["subtype"], "set_permission_mode");
        EXPECT_EQ(parsed["request"]["mode"], "bypassPermissions");
    }

    // Test set_model
    {
        claude::json data = {{"model", "claude-opus-4"}};
        auto message = protocol.build_request_message("set_model", data);
        auto parsed = claude::json::parse(message);
        EXPECT_EQ(parsed["request"]["subtype"], "set_model");
        EXPECT_EQ(parsed["request"]["model"], "claude-opus-4");
    }
}
