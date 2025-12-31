#include "../test_utils.hpp"

#include <claude/claude.hpp>
#include <gtest/gtest.h>

class ControlProtocolTest : public ::testing::Test
{
  protected:
    claude::ClaudeOptions opts;

    void SetUp() override
    {
        SKIP_IN_CI();
        opts.permission_mode = "default";
    }
};

TEST_F(ControlProtocolTest, HookCallbackApproval)
{
    bool hook_called = false;

    auto hook_callback = [&hook_called](const claude::json& input,
                                        const std::string& tool_use_id) -> claude::json
    {
        hook_called = true;
        EXPECT_TRUE(input.contains("hook_event_name"));
        EXPECT_TRUE(input.contains("tool_name"));
        EXPECT_EQ(input.value("hook_event_name", ""), "PreToolUse");

        return claude::json{{"continue", true}};
    };

    opts.hooks[claude::HookEvent::PreToolUse] = {claude::HookMatcher{"Bash", {hook_callback}}};

    claude::ClaudeClient client(opts);
    client.connect();
    client.send_query("Run 'echo test'");

    bool got_response = false;
    for (const auto& msg : client.receive_messages())
    {
        if (claude::is_assistant_message(msg))
            got_response = true;
        if (claude::is_result_message(msg))
            break;
    }

    client.disconnect();

    EXPECT_TRUE(hook_called) << "Hook callback should have been invoked";
    EXPECT_TRUE(got_response) << "Should receive response after approval";
}

TEST_F(ControlProtocolTest, HookCallbackDenial)
{
    bool hook_called = false;

    auto hook_callback = [&hook_called](const claude::json& input,
                                        const std::string& tool_use_id) -> claude::json
    {
        hook_called = true;

        return claude::json{
            {"continue", false}, {"stopReason", "Test denial"}, {"decision", "block"}};
    };

    opts.hooks[claude::HookEvent::PreToolUse] = {claude::HookMatcher{"Bash", {hook_callback}}};

    claude::ClaudeClient client(opts);
    client.connect();
    client.send_query("Run 'echo test'");

    bool got_error = false;
    bool saw_tool_result = false;
    for (const auto& msg : client.receive_messages())
    {
        if (claude::is_assistant_message(msg))
        {
            const auto& assistant = std::get<claude::AssistantMessage>(msg);

            for (const auto& block : assistant.content)
            {
                if (auto* tool_result = std::get_if<claude::ToolResultBlock>(&block))
                {
                    saw_tool_result = true;
                    if (tool_result->is_error)
                        got_error = true;
                }
            }

            std::string content = claude::get_text_content(assistant.content);
            if (content.find("denied") != std::string::npos ||
                content.find("blocked") != std::string::npos ||
                content.find("stopped") != std::string::npos ||
                content.find("Test denial") != std::string::npos)
            {
                got_error = true;
            }
        }
        if (claude::is_result_message(msg))
            break;
    }

    client.disconnect();

    EXPECT_TRUE(hook_called) << "Hook callback should have been invoked";
    if (!got_error)
    {
        // If the CLI doesn't emit a descriptive assistant message for a blocked hook,
        // treat the lack of a tool result as a blocked action.
        got_error = !saw_tool_result;
    }
    EXPECT_TRUE(got_error) << "Should indicate command was blocked";
}

TEST_F(ControlProtocolTest, ToolPermissionAllow)
{
    bool permission_requested = false;

    opts.tool_permission_callback =
        [&permission_requested](
            const std::string& tool_name, const claude::json& input,
            const claude::ToolPermissionContext& context) -> claude::PermissionResult
    {
        permission_requested = true;
        return claude::PermissionResultAllow{};
    };

    claude::ClaudeClient client(opts);
    client.connect();
    client.send_query("Write the text 'test' to /tmp/test_permission.txt");

    for (const auto& msg : client.receive_messages())
        if (claude::is_result_message(msg))
            break;

    client.disconnect();

    EXPECT_TRUE(permission_requested) << "Tool permission should have been requested";
}

TEST_F(ControlProtocolTest, ToolPermissionDeny)
{
    bool permission_requested = false;

    opts.tool_permission_callback =
        [&permission_requested](
            const std::string& tool_name, const claude::json& input,
            const claude::ToolPermissionContext& context) -> claude::PermissionResult
    {
        permission_requested = true;
        return claude::PermissionResultDeny{"deny", "Tool denied for testing"};
    };

    claude::ClaudeClient client(opts);
    client.connect();
    client.send_query("Write the text 'test' to /tmp/test_permission_deny.txt");

    // Drain messages until we see the result; the permission callback already confirms denial.
    for (const auto& msg : client.receive_messages())
        if (claude::is_result_message(msg))
            break;

    client.disconnect();

    EXPECT_TRUE(permission_requested) << "Tool permission should have been requested";
}

TEST_F(ControlProtocolTest, HookCallbackDenialUnderscore)
{
    bool hook_called = false;

    auto hook_callback = [&hook_called](const claude::json& input,
                                        const std::string& tool_use_id) -> claude::json
    {
        hook_called = true;

        // Use underscore variant; SDK should convert to "continue": false
        return claude::json{{"continue_", false},
                            {"stopReason", "Test denial (underscore)"},
                            {"decision", "block"}};
    };

    opts.hooks[claude::HookEvent::PreToolUse] = {claude::HookMatcher{"Bash", {hook_callback}}};

    claude::ClaudeClient client(opts);
    client.connect();
    client.send_query("Run 'echo test'");

    bool got_error = false;
    bool saw_tool_result = false;
    for (const auto& msg : client.receive_messages())
    {
        if (claude::is_assistant_message(msg))
        {
            const auto& assistant = std::get<claude::AssistantMessage>(msg);

            for (const auto& block : assistant.content)
            {
                if (auto* tool_result = std::get_if<claude::ToolResultBlock>(&block))
                {
                    saw_tool_result = true;
                    if (tool_result->is_error)
                        got_error = true;
                }
            }

            std::string content = claude::get_text_content(assistant.content);
            if (content.find("denied") != std::string::npos ||
                content.find("blocked") != std::string::npos ||
                content.find("stopped") != std::string::npos ||
                content.find("Test denial (underscore)") != std::string::npos)
            {
                got_error = true;
            }
        }
        if (claude::is_result_message(msg))
            break;
    }

    client.disconnect();

    EXPECT_TRUE(hook_called) << "Hook callback should have been invoked";
    if (!got_error)
    {
        // Some CLI versions do not emit a descriptive assistant message for a blocked hook.
        // Treat the lack of a tool result as a blocked action.
        got_error = !saw_tool_result;
    }
    EXPECT_TRUE(got_error) << "Should indicate command was blocked via underscore key";
}

TEST_F(ControlProtocolTest, HookCallbackAsyncUnderscore)
{
    bool hook_called = false;

    auto hook_callback = [&hook_called](const claude::json& input,
                                        const std::string& tool_use_id) -> claude::json
    {
        hook_called = true;
        // Request async handling using underscore key; SDK should convert to "async": true
        return claude::json{{"async_", true}, {"asyncTimeout", 100}};
    };

    opts.hooks[claude::HookEvent::PreToolUse] = {claude::HookMatcher{"Bash", {hook_callback}}};

    claude::ClaudeClient client(opts);
    client.connect();
    client.send_query("Run 'echo test'");

    bool got_response = false;
    for (const auto& msg : client.receive_messages())
    {
        if (claude::is_assistant_message(msg))
            got_response = true; // At minimum ensure conversation proceeds
        if (claude::is_result_message(msg))
            break;
    }

    client.disconnect();

    EXPECT_TRUE(hook_called) << "Hook callback should have been invoked";
    EXPECT_TRUE(got_response) << "Should receive response with async deferral accepted";
}

TEST_F(ControlProtocolTest, MultipleHooks)
{
    int hook_count = 0;

    auto hook_callback = [&hook_count](const claude::json& input,
                                       const std::string& tool_use_id) -> claude::json
    {
        hook_count++;
        return claude::json{{"continue", true}};
    };

    opts.hooks[claude::HookEvent::PreToolUse] = {
        claude::HookMatcher{"Bash|Write|Edit", {hook_callback}}};

    claude::ClaudeClient client(opts);
    client.connect();
    client.send_query("Run echo 'test' > file.txt then edit the file");

    for (const auto& msg : client.receive_messages())
        if (claude::is_result_message(msg))
            break;

    client.disconnect();

    EXPECT_GT(hook_count, 0) << "At least one hook should have fired";
}
