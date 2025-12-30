/**
 * @file test_session_storage.cpp
 * @brief Tests for claude::ext::SessionWrapper
 */

#include "../test_utils.hpp"

#include <claude/ext/session_storage.hpp>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>

namespace fs = std::filesystem;

namespace
{

// Test fixture
class SessionWrapperTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        // Use unique test storage directory
        test_storage_dir_ = ".test_claude_sessions_" + std::to_string(rand());

        // Clean up any existing test directory
        if (fs::exists(test_storage_dir_))
            fs::remove_all(test_storage_dir_);
    }

    void TearDown() override
    {
        // Clean up test directory
        if (fs::exists(test_storage_dir_))
            fs::remove_all(test_storage_dir_);
    }

    std::string test_storage_dir_;
};

// ============================================================================
// Construction and Basic Access Tests
// ============================================================================

TEST_F(SessionWrapperTest, Construction)
{
    claude::ClaudeOptions opts;
    opts.permission_mode = "bypassPermissions";

    EXPECT_NO_THROW({ claude::ext::SessionWrapper wrapper(opts, test_storage_dir_); });
}

TEST_F(SessionWrapperTest, OperatorArrowAccess)
{
    claude::ClaudeOptions opts;
    opts.permission_mode = "bypassPermissions";

    claude::ext::SessionWrapper wrapper(opts, test_storage_dir_);

    // Should be able to access client via operator->
    EXPECT_NO_THROW({
        // Note: Can't actually test connect() without Claude CLI installed
        // Just verify we can access the pointer
        auto* client = wrapper.operator->();
        EXPECT_NE(client, nullptr);
    });
}

TEST_F(SessionWrapperTest, OperatorDereferenceAccess)
{
    claude::ClaudeOptions opts;
    opts.permission_mode = "bypassPermissions";

    claude::ext::SessionWrapper wrapper(opts, test_storage_dir_);

    // Should be able to dereference to client
    EXPECT_NO_THROW({
        claude::ClaudeClient& client = *wrapper;
        // Just verify it compiles and returns a reference
        (void)client;
    });
}

TEST_F(SessionWrapperTest, StorageDirectoryCreated)
{
    claude::ClaudeOptions opts;
    claude::ext::SessionWrapper wrapper(opts, test_storage_dir_);

    // Storage directory should be created
    EXPECT_TRUE(fs::exists(test_storage_dir_));
    EXPECT_TRUE(fs::is_directory(test_storage_dir_));
}

// ============================================================================
// Session Management Tests
// ============================================================================

TEST_F(SessionWrapperTest, SessionIdInitiallyEmpty)
{
    claude::ClaudeOptions opts;
    claude::ext::SessionWrapper wrapper(opts, test_storage_dir_);

    EXPECT_TRUE(wrapper.session_id().empty());
}

TEST_F(SessionWrapperTest, MessagesInitiallyEmpty)
{
    claude::ClaudeOptions opts;
    claude::ext::SessionWrapper wrapper(opts, test_storage_dir_);

    EXPECT_TRUE(wrapper.messages().empty());
}

TEST_F(SessionWrapperTest, StorageDirectory)
{
    claude::ClaudeOptions opts;
    claude::ext::SessionWrapper wrapper(opts, test_storage_dir_);

    EXPECT_EQ(wrapper.storage_directory(), test_storage_dir_);
}

TEST_F(SessionWrapperTest, ListSessionsEmpty)
{
    claude::ClaudeOptions opts;
    claude::ext::SessionWrapper wrapper(opts, test_storage_dir_);

    auto sessions = wrapper.list_sessions();
    EXPECT_TRUE(sessions.empty());
}

// ============================================================================
// File Operations Tests
// ============================================================================

TEST_F(SessionWrapperTest, SaveAndLoadHistory)
{
    claude::ClaudeOptions opts;
    claude::ext::SessionWrapper wrapper(opts, test_storage_dir_);

    // Create a fake session file manually
    std::string session_id = "test_session_123";
    std::string filename = test_storage_dir_ + "/" + session_id + ".json";

    // Create minimal session JSON
    nlohmann::json session_data;
    session_data["session_id"] = session_id;
    session_data["timestamp"] = "2025-01-18 12:00:00";
    session_data["message_count"] = 1;
    session_data["messages"] = nlohmann::json::array();

    // Add a test message
    nlohmann::json msg;
    msg["type"] = "user";
    msg["content"] = nlohmann::json::array();
    msg["content"].push_back({{"type", "text"}, {"text", "Hello"}});
    session_data["messages"].push_back(msg);

    // Write file
    std::ofstream file(filename);
    file << session_data.dump(2);
    file.close();

    // Load history
    auto messages = wrapper.load_history(session_id);

    EXPECT_EQ(messages.size(), 1);
    EXPECT_EQ(wrapper.session_id(), session_id);
    EXPECT_EQ(wrapper.messages().size(), 1);
}

TEST_F(SessionWrapperTest, LoadHistoryNonExistentFile)
{
    claude::ClaudeOptions opts;
    claude::ext::SessionWrapper wrapper(opts, test_storage_dir_);

    EXPECT_THROW({ wrapper.load_history("nonexistent_session"); }, std::runtime_error);
}

TEST_F(SessionWrapperTest, ListSessionsWithFiles)
{
    claude::ClaudeOptions opts;
    claude::ext::SessionWrapper wrapper(opts, test_storage_dir_);

    // Create dummy session files
    std::vector<std::string> session_ids = {"session_001", "session_002", "session_003"};

    for (const auto& id : session_ids)
    {
        std::string filename = test_storage_dir_ + "/" + id + ".json";
        nlohmann::json data;
        data["session_id"] = id;
        data["messages"] = nlohmann::json::array();

        std::ofstream file(filename);
        file << data.dump();
        file.close();
    }

    auto sessions = wrapper.list_sessions();
    EXPECT_EQ(sessions.size(), 3);

    // Verify all session IDs are present
    for (const auto& id : session_ids)
        EXPECT_NE(std::find(sessions.begin(), sessions.end(), id), sessions.end());
}

// ============================================================================
// Move Semantics Tests
// ============================================================================

TEST_F(SessionWrapperTest, MoveConstruction)
{
    claude::ClaudeOptions opts;
    claude::ext::SessionWrapper wrapper1(opts, test_storage_dir_);

    // Move construct
    claude::ext::SessionWrapper wrapper2(std::move(wrapper1));

    EXPECT_EQ(wrapper2.storage_directory(), test_storage_dir_);
}

TEST_F(SessionWrapperTest, MoveAssignment)
{
    claude::ClaudeOptions opts;
    claude::ext::SessionWrapper wrapper1(opts, test_storage_dir_);
    claude::ext::SessionWrapper wrapper2(opts, test_storage_dir_ + "_2");

    // Move assign
    wrapper2 = std::move(wrapper1);

    EXPECT_EQ(wrapper2.storage_directory(), test_storage_dir_);
}

} // anonymous namespace

// ============================================================================
// Integration Tests (Require Claude CLI)
// Skipped in CI, enabled locally
// ============================================================================

TEST_F(SessionWrapperTest, ConnectAndDisconnect)
{
    SKIP_IN_CI();

    claude::ClaudeOptions opts;
    opts.permission_mode = "bypassPermissions";
    opts.model = "claude-sonnet-4-5";

    claude::ext::SessionWrapper wrapper(opts, test_storage_dir_);

    EXPECT_NO_THROW({
        wrapper.connect();
        EXPECT_TRUE(wrapper.is_connected());

        wrapper.disconnect();
        EXPECT_FALSE(wrapper.is_connected());
    });
}

TEST_F(SessionWrapperTest, SendQueryAndReceiveMessages)
{
    SKIP_IN_CI();

    claude::ClaudeOptions opts;
    opts.permission_mode = "bypassPermissions";
    opts.model = "claude-sonnet-4-5";
    opts.max_turns = 1;

    claude::ext::SessionWrapper wrapper(opts, test_storage_dir_);
    wrapper.connect();

    wrapper->send_query("What is 2+2?");

    size_t message_count = 0;
    for (const auto& msg : wrapper.receive_messages())
    {
        message_count++;

        if (claude::is_result_message(msg))
        {
            // Session ID should be captured
            EXPECT_FALSE(wrapper.session_id().empty());
            break;
        }
    }

    EXPECT_GT(message_count, 0);
    EXPECT_GT(wrapper.messages().size(), 0);

    wrapper.disconnect();

    // Session should be saved
    auto sessions = wrapper.list_sessions();
    EXPECT_FALSE(sessions.empty());
}
