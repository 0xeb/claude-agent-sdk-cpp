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
