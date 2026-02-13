#include <claude/types.hpp>
#include <gtest/gtest.h>

using namespace claude;

TEST(TypesTest, TextBlockBasic)
{
    TextBlock block;
    block.text = "Hello, world!";

    EXPECT_EQ(block.type, "text");
    EXPECT_EQ(block.text, "Hello, world!");
}

TEST(TypesTest, ThinkingBlockBasic)
{
    ThinkingBlock block;
    block.thinking = "Analyzing the problem...";

    EXPECT_EQ(block.type, "thinking");
    EXPECT_EQ(block.thinking, "Analyzing the problem...");
}

TEST(TypesTest, ToolUseBlockBasic)
{
    ToolUseBlock block;
    block.id = "tool_123";
    block.name = "read_file";
    block.input = claude::json{{"path", "/test/file.txt"}};

    EXPECT_EQ(block.type, "tool_use");
    EXPECT_EQ(block.id, "tool_123");
    EXPECT_EQ(block.name, "read_file");
    EXPECT_EQ(block.input["path"], "/test/file.txt");
}

TEST(TypesTest, ContentBlockVariant)
{
    std::vector<ContentBlock> content;

    TextBlock text;
    text.text = "Hello";
    content.push_back(text);

    ThinkingBlock thinking;
    thinking.thinking = "Analyzing...";
    content.push_back(thinking);

    ToolUseBlock tool;
    tool.id = "t1";
    tool.name = "test_tool";
    tool.input = claude::json{{"arg", "value"}};
    content.push_back(tool);

    EXPECT_EQ(content.size(), 3);
    EXPECT_TRUE(std::holds_alternative<TextBlock>(content[0]));
    EXPECT_TRUE(std::holds_alternative<ThinkingBlock>(content[1]));
    EXPECT_TRUE(std::holds_alternative<ToolUseBlock>(content[2]));
}

TEST(TypesTest, GetTextContent)
{
    std::vector<ContentBlock> content;

    TextBlock text1;
    text1.text = "Hello ";
    content.push_back(text1);

    ThinkingBlock thinking;
    thinking.thinking = "...";
    content.push_back(thinking);

    TextBlock text2;
    text2.text = "World";
    content.push_back(text2);

    std::string text = get_text_content(content);
    EXPECT_EQ(text, "Hello World");
}

TEST(TypesTest, AssistantMessage)
{
    AssistantMessage msg;

    TextBlock block;
    block.text = "Hello from assistant";
    msg.content.push_back(block);

    EXPECT_EQ(msg.type, "assistant");
    EXPECT_EQ(msg.role, "assistant");
    EXPECT_EQ(msg.content.size(), 1);
}

TEST(TypesTest, UserMessage)
{
    UserMessage msg;

    TextBlock block;
    block.text = "Hello from user";
    msg.content.push_back(block);

    EXPECT_EQ(msg.type, "user");
    EXPECT_EQ(msg.role, "user");
    EXPECT_EQ(msg.content.size(), 1);
    EXPECT_FALSE(msg.uuid.has_value());
    EXPECT_FALSE(msg.parent_tool_use_id.has_value());
}

TEST(TypesTest, SystemMessage)
{
    SystemMessage msg;
    msg.content = "System notification";

    EXPECT_EQ(msg.type, "system");
    EXPECT_EQ(msg.content, "System notification");
}

TEST(TypesTest, ResultMessage)
{
    ResultMessage msg;
    msg.result.session_id = "session_123";
    msg.result.conversation_id = "conv_456";
    msg.result.usage.input_tokens = 100;
    msg.result.usage.output_tokens = 50;
    msg.result.cost.total = 0.01;

    EXPECT_EQ(msg.type, "result");
    EXPECT_EQ(msg.result.session_id, "session_123");
    EXPECT_EQ(msg.result.conversation_id, "conv_456");
    EXPECT_EQ(msg.result.usage.input_tokens, 100);
    EXPECT_EQ(msg.result.usage.output_tokens, 50);
    EXPECT_DOUBLE_EQ(msg.result.cost.total, 0.01);
}

TEST(TypesTest, ResultMessageConvenienceAccessors)
{
    ResultMessage msg;
    msg.subtype = "error";
    msg.result.cost.total = 1.23;

    EXPECT_TRUE(msg.is_error());
    EXPECT_DOUBLE_EQ(msg.total_cost_usd(), 1.23);
}

TEST(TypesTest, StreamEvent)
{
    StreamEvent event;
    event.event = "content_block_delta";
    event.index = 0;
    event.data = claude::json{{"delta", "text"}};

    EXPECT_EQ(event.type, "stream");
    EXPECT_EQ(event.event, "content_block_delta");
    EXPECT_EQ(event.index, 0);
}

TEST(TypesTest, StreamEventEventDict)
{
    StreamEvent event;
    event.event = "tool_use_delta";
    event.index = 2;
    event.data = claude::json{{"delta", claude::json{{"id", "x"}}}};

    auto payload = event.event_dict();
    ASSERT_TRUE(payload.is_object());
    EXPECT_EQ(payload["type"], "tool_use_delta");
    EXPECT_EQ(payload["index"], 2);
    EXPECT_EQ(payload["delta"]["id"], "x");
}

TEST(TypesTest, MessageVariant)
{
    AssistantMessage assistant;
    Message msg = assistant;

    EXPECT_TRUE(is_assistant_message(msg));
    EXPECT_FALSE(is_result_message(msg));
    EXPECT_FALSE(is_system_message(msg));
    EXPECT_FALSE(is_stream_event(msg));
}

TEST(TypesTest, MessageVariantResult)
{
    ResultMessage result;
    Message msg = result;

    EXPECT_FALSE(is_assistant_message(msg));
    EXPECT_TRUE(is_result_message(msg));
    EXPECT_FALSE(is_system_message(msg));
    EXPECT_FALSE(is_stream_event(msg));
}

TEST(TypesTest, UsageInfo)
{
    UsageInfo usage;
    usage.input_tokens = 100;
    usage.output_tokens = 50;
    usage.cache_creation_input_tokens = 20;
    usage.cache_read_input_tokens = 30;

    EXPECT_EQ(usage.input_tokens, 100);
    EXPECT_EQ(usage.output_tokens, 50);
    EXPECT_EQ(usage.cache_creation_input_tokens, 20);
    EXPECT_EQ(usage.cache_read_input_tokens, 30);
}

TEST(TypesTest, PostToolUseFailureHookInputParsing)
{
    json payload = {{"session_id", "sess-1"},
                    {"transcript_path", "C:/tmp/transcript.jsonl"},
                    {"cwd", "C:/work"},
                    {"permission_mode", "default"},
                    {"hook_event_name", HookEvent::PostToolUseFailure},
                    {"tool_name", "Bash"},
                    {"tool_input", json{{"command", "exit 1"}}},
                    {"tool_use_id", "tool_use_123"},
                    {"error", "exit code 1"},
                    {"is_interrupt", true}};

    auto parsed = PostToolUseFailureHookInput::from_json(payload);

    EXPECT_EQ(parsed.session_id, "sess-1");
    EXPECT_EQ(parsed.transcript_path, "C:/tmp/transcript.jsonl");
    EXPECT_EQ(parsed.cwd, "C:/work");
    ASSERT_TRUE(parsed.permission_mode.has_value());
    EXPECT_EQ(*parsed.permission_mode, "default");
    EXPECT_EQ(parsed.hook_event_name, HookEvent::PostToolUseFailure);
    EXPECT_EQ(parsed.tool_name, "Bash");
    EXPECT_EQ(parsed.tool_input.value("command", ""), "exit 1");
    EXPECT_EQ(parsed.tool_use_id, "tool_use_123");
    EXPECT_EQ(parsed.error, "exit code 1");
    ASSERT_TRUE(parsed.is_interrupt.has_value());
    EXPECT_TRUE(*parsed.is_interrupt);
}

TEST(TypesTest, PostToolUseFailureHookOutputToJson)
{
    PostToolUseFailureHookOutput output;
    output.additionalContext = "Tool failed; retry with different arguments.";

    auto j = output.to_json();
    EXPECT_EQ(j["hookEventName"], HookEvent::PostToolUseFailure);
    ASSERT_TRUE(j.contains("additionalContext"));
    EXPECT_EQ(j["additionalContext"], "Tool failed; retry with different arguments.");
}

TEST(TypesTest, CostInfo)
{
    CostInfo cost;
    cost.total = 0.05;
    cost.input = 0.02;
    cost.output = 0.03;

    EXPECT_DOUBLE_EQ(cost.total, 0.05);
    EXPECT_DOUBLE_EQ(cost.input, 0.02);
    EXPECT_DOUBLE_EQ(cost.output, 0.03);
}

TEST(TypesTest, ClaudeOptions)
{
    ClaudeOptions opts;
    opts.model = "claude-3-5-sonnet-20241022";
    opts.system_prompt = "You are a helpful assistant";
    opts.permission_mode = "default";
    opts.max_turns = 10;
    opts.working_directory = "/home/user";
    opts.environment["KEY"] = "value";
    opts.include_partial_messages = true;

    EXPECT_EQ(opts.model, "claude-3-5-sonnet-20241022");
    EXPECT_EQ(opts.system_prompt, "You are a helpful assistant");
    EXPECT_EQ(opts.permission_mode, "default");
    EXPECT_EQ(opts.max_turns.value(), 10);
    EXPECT_EQ(opts.working_directory.value(), "/home/user");
    EXPECT_EQ(opts.environment.at("KEY"), "value");
    EXPECT_TRUE(opts.include_partial_messages);
}

TEST(TypesTest, ClaudeOptionsSystemPromptAppend)
{
    // Test system_prompt_append field (Task 2: System prompt preset append)
    ClaudeOptions opts;
    opts.system_prompt_append = "Always end your response with a fun fact.";

    EXPECT_EQ(opts.system_prompt_append, "Always end your response with a fun fact.");
    EXPECT_TRUE(opts.system_prompt.empty()); // system_prompt and append are mutually exclusive

    // Test that both fields can be set independently (append takes precedence in transport)
    opts.system_prompt = "Custom prompt";
    EXPECT_EQ(opts.system_prompt, "Custom prompt");
    EXPECT_EQ(opts.system_prompt_append, "Always end your response with a fun fact.");
}

// ============================================================================
// v0.1.35 Parity Tests: ThinkingConfig, EffortLevel, Hook Events
// ============================================================================

TEST(TypesTest, ThinkingConfigAdaptive)
{
    ThinkingConfigAdaptive config;
    EXPECT_EQ(config.type, "adaptive");

    // Can hold in variant
    ThinkingConfig tc = config;
    EXPECT_TRUE(std::holds_alternative<ThinkingConfigAdaptive>(tc));
}

TEST(TypesTest, ThinkingConfigEnabled)
{
    ThinkingConfigEnabled config(8000);
    EXPECT_EQ(config.type, "enabled");
    EXPECT_EQ(config.budget_tokens, 8000);

    ThinkingConfig tc = config;
    EXPECT_TRUE(std::holds_alternative<ThinkingConfigEnabled>(tc));
    EXPECT_EQ(std::get<ThinkingConfigEnabled>(tc).budget_tokens, 8000);
}

TEST(TypesTest, ThinkingConfigDisabled)
{
    ThinkingConfigDisabled config;
    EXPECT_EQ(config.type, "disabled");

    ThinkingConfig tc = config;
    EXPECT_TRUE(std::holds_alternative<ThinkingConfigDisabled>(tc));
}

TEST(TypesTest, EffortConstants)
{
    EXPECT_STREQ(Effort::Low, "low");
    EXPECT_STREQ(Effort::Medium, "medium");
    EXPECT_STREQ(Effort::High, "high");
    EXPECT_STREQ(Effort::Max, "max");
}

TEST(TypesTest, ClaudeOptionsThinkingAndEffort)
{
    ClaudeOptions opts;
    EXPECT_FALSE(opts.thinking.has_value());
    EXPECT_FALSE(opts.effort.has_value());

    opts.thinking = ThinkingConfigAdaptive{};
    opts.effort = Effort::High;

    EXPECT_TRUE(opts.thinking.has_value());
    EXPECT_TRUE(std::holds_alternative<ThinkingConfigAdaptive>(*opts.thinking));
    EXPECT_TRUE(opts.effort.has_value());
    EXPECT_EQ(*opts.effort, "high");
}

TEST(TypesTest, HookEventAllValues)
{
    // 10 hook events in v0.1.35
    EXPECT_STREQ(HookEvent::PreToolUse, "PreToolUse");
    EXPECT_STREQ(HookEvent::PostToolUse, "PostToolUse");
    EXPECT_STREQ(HookEvent::PostToolUseFailure, "PostToolUseFailure");
    EXPECT_STREQ(HookEvent::UserPromptSubmit, "UserPromptSubmit");
    EXPECT_STREQ(HookEvent::Stop, "Stop");
    EXPECT_STREQ(HookEvent::SubagentStop, "SubagentStop");
    EXPECT_STREQ(HookEvent::PreCompact, "PreCompact");
    EXPECT_STREQ(HookEvent::Notification, "Notification");
    EXPECT_STREQ(HookEvent::SubagentStart, "SubagentStart");
    EXPECT_STREQ(HookEvent::PermissionRequest, "PermissionRequest");
}

TEST(TypesTest, PreToolUseHookInputParsing)
{
    json payload = {{"session_id", "sess-1"},
                    {"transcript_path", "/tmp/t.jsonl"},
                    {"cwd", "/work"},
                    {"hook_event_name", HookEvent::PreToolUse},
                    {"tool_name", "Write"},
                    {"tool_input", json{{"path", "test.txt"}}},
                    {"tool_use_id", "tu_001"}};

    auto parsed = PreToolUseHookInput::from_json(payload);
    EXPECT_EQ(parsed.session_id, "sess-1");
    EXPECT_EQ(parsed.hook_event_name, HookEvent::PreToolUse);
    EXPECT_EQ(parsed.tool_name, "Write");
    EXPECT_EQ(parsed.tool_use_id, "tu_001");
    EXPECT_EQ(parsed.tool_input["path"], "test.txt");
}

TEST(TypesTest, PostToolUseHookInputParsing)
{
    json payload = {{"session_id", "sess-2"},
                    {"transcript_path", "/tmp/t.jsonl"},
                    {"cwd", "/work"},
                    {"hook_event_name", HookEvent::PostToolUse},
                    {"tool_name", "Read"},
                    {"tool_input", json{{"path", "file.txt"}}},
                    {"tool_response", "file contents here"},
                    {"tool_use_id", "tu_002"}};

    auto parsed = PostToolUseHookInput::from_json(payload);
    EXPECT_EQ(parsed.hook_event_name, HookEvent::PostToolUse);
    EXPECT_EQ(parsed.tool_name, "Read");
    EXPECT_EQ(parsed.tool_use_id, "tu_002");
    EXPECT_EQ(parsed.tool_response, "file contents here");
}

TEST(TypesTest, NotificationHookInputParsing)
{
    json payload = {{"session_id", "sess-3"},
                    {"transcript_path", "/tmp/t.jsonl"},
                    {"cwd", "/work"},
                    {"hook_event_name", HookEvent::Notification},
                    {"message", "Task completed"},
                    {"title", "Info"},
                    {"notification_type", "info"}};

    auto parsed = NotificationHookInput::from_json(payload);
    EXPECT_EQ(parsed.hook_event_name, HookEvent::Notification);
    EXPECT_EQ(parsed.message, "Task completed");
    ASSERT_TRUE(parsed.title.has_value());
    EXPECT_EQ(*parsed.title, "Info");
    EXPECT_EQ(parsed.notification_type, "info");
}

TEST(TypesTest, SubagentStartHookInputParsing)
{
    json payload = {{"session_id", "sess-4"},
                    {"transcript_path", "/tmp/t.jsonl"},
                    {"cwd", "/work"},
                    {"hook_event_name", HookEvent::SubagentStart},
                    {"agent_id", "agent-123"},
                    {"agent_type", "plan"}};

    auto parsed = SubagentStartHookInput::from_json(payload);
    EXPECT_EQ(parsed.hook_event_name, HookEvent::SubagentStart);
    EXPECT_EQ(parsed.agent_id, "agent-123");
    EXPECT_EQ(parsed.agent_type, "plan");
}

TEST(TypesTest, SubagentStopHookInputParsing)
{
    json payload = {{"session_id", "sess-5"},
                    {"transcript_path", "/tmp/t.jsonl"},
                    {"cwd", "/work"},
                    {"hook_event_name", HookEvent::SubagentStop},
                    {"stop_hook_active", true},
                    {"agent_id", "agent-456"},
                    {"agent_transcript_path", "/tmp/agent.jsonl"},
                    {"agent_type", "explore"}};

    auto parsed = SubagentStopHookInput::from_json(payload);
    EXPECT_EQ(parsed.hook_event_name, HookEvent::SubagentStop);
    EXPECT_TRUE(parsed.stop_hook_active);
    EXPECT_EQ(parsed.agent_id, "agent-456");
    EXPECT_EQ(parsed.agent_transcript_path, "/tmp/agent.jsonl");
    EXPECT_EQ(parsed.agent_type, "explore");
}

TEST(TypesTest, PermissionRequestHookInputParsing)
{
    json suggestions = json::array({json{{"type", "addRules"}, {"behavior", "allow"}}});
    json payload = {{"session_id", "sess-6"},
                    {"transcript_path", "/tmp/t.jsonl"},
                    {"cwd", "/work"},
                    {"hook_event_name", HookEvent::PermissionRequest},
                    {"tool_name", "Bash"},
                    {"tool_input", json{{"command", "ls"}}},
                    {"permission_suggestions", suggestions}};

    auto parsed = PermissionRequestHookInput::from_json(payload);
    EXPECT_EQ(parsed.hook_event_name, HookEvent::PermissionRequest);
    EXPECT_EQ(parsed.tool_name, "Bash");
    ASSERT_TRUE(parsed.permission_suggestions.has_value());
    EXPECT_TRUE(parsed.permission_suggestions->is_array());
}

TEST(TypesTest, PreToolUseHookOutputToJson)
{
    PreToolUseHookOutput output;
    output.permissionDecision = "deny";
    output.permissionDecisionReason = "not allowed";
    output.additionalContext = "blocked by policy";

    auto j = output.to_json();
    EXPECT_EQ(j["hookEventName"], HookEvent::PreToolUse);
    EXPECT_EQ(j["permissionDecision"], "deny");
    EXPECT_EQ(j["permissionDecisionReason"], "not allowed");
    EXPECT_EQ(j["additionalContext"], "blocked by policy");
    EXPECT_FALSE(j.contains("updatedInput")); // not set
}

TEST(TypesTest, PostToolUseHookOutputToJson)
{
    PostToolUseHookOutput output;
    output.additionalContext = "tool succeeded";
    output.updatedMCPToolOutput = json{{"modified", true}};

    auto j = output.to_json();
    EXPECT_EQ(j["hookEventName"], HookEvent::PostToolUse);
    EXPECT_EQ(j["additionalContext"], "tool succeeded");
    EXPECT_EQ(j["updatedMCPToolOutput"]["modified"], true);
}

TEST(TypesTest, PermissionRequestHookOutputToJson)
{
    PermissionRequestHookOutput output;
    output.decision = json{{"behavior", "allow"}, {"updatedInput", json::object()}};

    auto j = output.to_json();
    EXPECT_EQ(j["hookEventName"], HookEvent::PermissionRequest);
    EXPECT_EQ(j["decision"]["behavior"], "allow");
}
