#include "../../src/internal/message_parser.hpp"

#include <claude/errors.hpp>
#include <gtest/gtest.h>

using namespace claude;
using namespace claude::protocol;

TEST(ParserTest, ParseSimpleTextMessage)
{
    std::string json =
        R"({"type":"assistant","role":"assistant","content":[{"type":"text","text":"Hello"}]})";

    Message msg = MessageParser::parse_message(json);

    ASSERT_TRUE(is_assistant_message(msg));
    auto& assistant = std::get<AssistantMessage>(msg);
    EXPECT_EQ(assistant.content.size(), 1);

    auto* text = std::get_if<TextBlock>(&assistant.content[0]);
    ASSERT_NE(text, nullptr);
    EXPECT_EQ(text->text, "Hello");
}

TEST(ParserTest, ParseThinkingBlock)
{
    std::string json =
        R"({"type":"assistant","role":"assistant","content":[{"type":"thinking","thinking":"Let me think..."}]})";

    Message msg = MessageParser::parse_message(json);

    ASSERT_TRUE(is_assistant_message(msg));
    auto& assistant = std::get<AssistantMessage>(msg);
    EXPECT_EQ(assistant.content.size(), 1);

    auto* thinking = std::get_if<ThinkingBlock>(&assistant.content[0]);
    ASSERT_NE(thinking, nullptr);
    EXPECT_EQ(thinking->thinking, "Let me think...");
}

TEST(ParserTest, ParseToolUseBlock)
{
    std::string json = R"({
        "type":"assistant",
        "role":"assistant",
        "content":[{
            "type":"tool_use",
            "id":"tool_123",
            "name":"read_file",
            "input":{"path":"/test.txt"}
        }]
    })";

    Message msg = MessageParser::parse_message(json);

    ASSERT_TRUE(is_assistant_message(msg));
    auto& assistant = std::get<AssistantMessage>(msg);
    EXPECT_EQ(assistant.content.size(), 1);

    auto* tool_use = std::get_if<ToolUseBlock>(&assistant.content[0]);
    ASSERT_NE(tool_use, nullptr);
    EXPECT_EQ(tool_use->id, "tool_123");
    EXPECT_EQ(tool_use->name, "read_file");
    EXPECT_EQ(tool_use->input["path"], "/test.txt");
}

TEST(ParserTest, ParseMultipleContentBlocks)
{
    std::string json = R"({
        "type":"assistant",
        "role":"assistant",
        "content":[
            {"type":"text","text":"First"},
            {"type":"thinking","thinking":"Thought"},
            {"type":"text","text":"Second"}
        ]
    })";

    Message msg = MessageParser::parse_message(json);

    ASSERT_TRUE(is_assistant_message(msg));
    auto& assistant = std::get<AssistantMessage>(msg);
    EXPECT_EQ(assistant.content.size(), 3);

    EXPECT_TRUE(std::holds_alternative<TextBlock>(assistant.content[0]));
    EXPECT_TRUE(std::holds_alternative<ThinkingBlock>(assistant.content[1]));
    EXPECT_TRUE(std::holds_alternative<TextBlock>(assistant.content[2]));
}

TEST(ParserTest, ParseNestedCLIAssistantMessage)
{
    std::string json = R"({
        "type":"assistant",
        "message":{
            "model":"claude-sonnet-4-5-20250929",
            "id":"msg_123",
            "type":"message",
            "role":"assistant",
            "content":[{"type":"text","text":"4"}],
            "stop_reason":null,
            "usage":{"input_tokens":3,"output_tokens":5}
        },
        "session_id":"session123"
    })";

    Message msg = MessageParser::parse_message(json);

    ASSERT_TRUE(is_assistant_message(msg));
    auto& assistant = std::get<AssistantMessage>(msg);
    EXPECT_EQ(assistant.content.size(), 1);

    auto* text = std::get_if<TextBlock>(&assistant.content[0]);
    ASSERT_NE(text, nullptr);
    EXPECT_EQ(text->text, "4");
}

TEST(ParserTest, ParseResultMessage)
{
    std::string json = R"({
        "type":"result",
        "session_id":"session123",
        "conversation_id":"conv456",
        "usage":{"input_tokens":100,"output_tokens":50,"cache_creation_input_tokens":10,"cache_read_input_tokens":5},
        "total_cost_usd":0.01
    })";

    Message msg = MessageParser::parse_message(json);

    ASSERT_TRUE(is_result_message(msg));
    auto& result = std::get<ResultMessage>(msg);
    EXPECT_EQ(result.result.session_id, "session123");
    EXPECT_EQ(result.result.conversation_id, "conv456");
    EXPECT_EQ(result.result.usage.input_tokens, 100);
    EXPECT_EQ(result.result.usage.output_tokens, 50);
    EXPECT_EQ(result.result.usage.cache_creation_input_tokens, 10);
    EXPECT_EQ(result.result.usage.cache_read_input_tokens, 5);
    EXPECT_DOUBLE_EQ(result.result.cost.total, 0.01);
}

TEST(ParserTest, ParseResultMessageWithDurations)
{
    std::string json = R"({
        "type":"result",
        "subtype":"success",
        "session_id":"s1",
        "conversation_id":"c1",
        "duration_ms":1234,
        "duration_api_ms":789,
        "num_turns":3,
        "usage":{},
        "total_cost_usd":0.0
    })";

    Message msg = MessageParser::parse_message(json);

    ASSERT_TRUE(is_result_message(msg));
    auto& result = std::get<ResultMessage>(msg);
    EXPECT_EQ(result.subtype, std::string("success"));
    EXPECT_EQ(result.duration_ms_value(), 1234);
    EXPECT_EQ(result.duration_api_ms_value(), 789);
    EXPECT_EQ(result.num_turns_value(), 3);
}

TEST(ParserTest, ParseResultMessageSubtypeOnly)
{
    std::string json = R"({
        "type":"result",
        "subtype":"error",
        "session_id":"s2"
    })";

    Message msg = MessageParser::parse_message(json);

    ASSERT_TRUE(is_result_message(msg));
    auto& result = std::get<ResultMessage>(msg);
    EXPECT_EQ(result.subtype, std::string("error"));
}

TEST(ParserTest, ParseSystemMessage)
{
    std::string json = R"({"type":"system","content":"System notification"})";

    Message msg = MessageParser::parse_message(json);

    ASSERT_TRUE(is_system_message(msg));
    auto& system = std::get<SystemMessage>(msg);
    EXPECT_EQ(system.content, "System notification");
}

TEST(ParserTest, ParseSystemMessageSubtype)
{
    std::string json = R"({"type":"system","subtype":"init","content":{"commands":[]}})";

    Message msg = MessageParser::parse_message(json);

    ASSERT_TRUE(is_system_message(msg));
    auto& system = std::get<SystemMessage>(msg);
    EXPECT_EQ(system.subtype, "init");
}

TEST(ParserTest, ParseStreamEvent)
{
    std::string json = R"({"type":"stream","event":"content_block_delta","index":0})";

    Message msg = MessageParser::parse_message(json);

    ASSERT_TRUE(is_stream_event(msg));
    auto& stream = std::get<StreamEvent>(msg);
    EXPECT_EQ(stream.event, "content_block_delta");
    EXPECT_EQ(stream.index, 0);
}

TEST(ParserTest, ParseUserMessage)
{
    std::string json =
        R"({"type":"user","role":"user","content":[{"type":"text","text":"User message"}]})";

    Message msg = MessageParser::parse_message(json);

    ASSERT_TRUE(std::holds_alternative<UserMessage>(msg));
    auto& user = std::get<UserMessage>(msg);
    EXPECT_EQ(user.content.size(), 1);
    EXPECT_FALSE(user.uuid.has_value());
    EXPECT_FALSE(user.parent_tool_use_id.has_value());

    auto* text = std::get_if<TextBlock>(&user.content[0]);
    ASSERT_NE(text, nullptr);
    EXPECT_EQ(text->text, "User message");
}

TEST(ParserTest, ParseUserMessageWithUuid)
{
    std::string json =
        R"({"type":"user","uuid":"msg-abc123-def456","parent_tool_use_id":"tool_123","message":{"content":[{"type":"text","text":"Hello"}]}})";

    Message msg = MessageParser::parse_message(json);

    ASSERT_TRUE(std::holds_alternative<UserMessage>(msg));
    auto& user = std::get<UserMessage>(msg);
    ASSERT_TRUE(user.uuid.has_value());
    EXPECT_EQ(*user.uuid, "msg-abc123-def456");
    ASSERT_TRUE(user.parent_tool_use_id.has_value());
    EXPECT_EQ(*user.parent_tool_use_id, "tool_123");

    EXPECT_EQ(user.content.size(), 1);
    auto* text = std::get_if<TextBlock>(&user.content[0]);
    ASSERT_NE(text, nullptr);
    EXPECT_EQ(text->text, "Hello");
}

TEST(ParserTest, ParseMultipleLinesNewline)
{
    MessageParser parser;

    std::string data =
        R"({"type":"assistant","role":"assistant","content":[{"type":"text","text":"Line1"}]})"
        "\n"
        R"({"type":"assistant","role":"assistant","content":[{"type":"text","text":"Line2"}]})"
        "\n";

    auto messages = parser.add_data(data);

    EXPECT_EQ(messages.size(), 2);

    ASSERT_TRUE(is_assistant_message(messages[0]));
    auto& msg1 = std::get<AssistantMessage>(messages[0]);
    auto* text1 = std::get_if<TextBlock>(&msg1.content[0]);
    EXPECT_EQ(text1->text, "Line1");

    ASSERT_TRUE(is_assistant_message(messages[1]));
    auto& msg2 = std::get<AssistantMessage>(messages[1]);
    auto* text2 = std::get_if<TextBlock>(&msg2.content[0]);
    EXPECT_EQ(text2->text, "Line2");
}

TEST(ParserTest, PartialJSON)
{
    MessageParser parser;

    // Send half a message
    std::string part1 = R"({"type":"assistant","role":"assistant",)";
    auto messages1 = parser.add_data(part1);
    EXPECT_EQ(messages1.size(), 0);
    EXPECT_TRUE(parser.has_buffered_data());

    // Send rest
    std::string part2 = R"("content":[{"type":"text","text":"Hello"}]})"
                        "\n";
    auto messages2 = parser.add_data(part2);
    EXPECT_EQ(messages2.size(), 1);

    ASSERT_TRUE(is_assistant_message(messages2[0]));
    auto& msg = std::get<AssistantMessage>(messages2[0]);
    auto* text = std::get_if<TextBlock>(&msg.content[0]);
    EXPECT_EQ(text->text, "Hello");
}

TEST(ParserTest, PartialLine)
{
    MessageParser parser;

    // Send message without newline
    std::string part1 =
        R"({"type":"assistant","role":"assistant","content":[{"type":"text","text":"Hello"}]})";
    auto messages1 = parser.add_data(part1);
    EXPECT_EQ(messages1.size(), 0);
    EXPECT_TRUE(parser.has_buffered_data());

    // Send newline
    std::string part2 = "\n";
    auto messages2 = parser.add_data(part2);
    EXPECT_EQ(messages2.size(), 1);
}

TEST(ParserTest, BufferSizeLimit)
{
    MessageParser parser(100); // Small limit

    std::string large_data(200, 'x');

    EXPECT_THROW(parser.add_data(large_data), JSONDecodeError);
}

TEST(ParserTest, InvalidJSON)
{
    std::string invalid = "not json";

    EXPECT_THROW(MessageParser::parse_message(invalid), JSONDecodeError);
}

TEST(ParserTest, UnknownMessageType)
{
    std::string json = R"({"type":"unknown_type"})";

    // Now correctly throws MessageParseError for unknown message types
    EXPECT_THROW(MessageParser::parse_message(json), MessageParseError);
}

TEST(ParserTest, MissingTypeField)
{
    std::string json = R"({"content":"test"})";

    EXPECT_THROW(MessageParser::parse_message(json), JSONDecodeError);
}

TEST(ParserTest, EmptyLines)
{
    MessageParser parser;

    std::string data = "\n\n"
                       R"({"type":"system","content":"test"})"
                       "\n\n";

    auto messages = parser.add_data(data);

    EXPECT_EQ(messages.size(), 1);
    EXPECT_TRUE(is_system_message(messages[0]));
}

TEST(ParserTest, ClearBuffer)
{
    MessageParser parser;

    parser.add_data("partial data");
    EXPECT_TRUE(parser.has_buffered_data());

    parser.clear_buffer();
    EXPECT_FALSE(parser.has_buffered_data());
}

TEST(ParserTest, ResultMessageWithMissingFields)
{
    std::string json = R"({
        "type":"result",
        "session_id":"session123"
    })";

    Message msg = MessageParser::parse_message(json);

    ASSERT_TRUE(is_result_message(msg));
    auto& result = std::get<ResultMessage>(msg);
    EXPECT_EQ(result.result.session_id, "session123");
    EXPECT_EQ(result.result.conversation_id, "");
    EXPECT_EQ(result.result.usage.input_tokens, 0);
    EXPECT_DOUBLE_EQ(result.result.cost.total, 0.0);
}

// Task 4: StreamEvent field parsing tests

TEST(ParserTest, StreamEventWithNestedUuidAndSessionId)
{
    std::string json = R"({
        "type":"stream",
        "event":"content_block_delta",
        "index":0,
        "data":{
            "uuid":"evt_abc123",
            "session_id":"sess_xyz789",
            "delta":{"text":"Hello"}
        }
    })";

    Message msg = MessageParser::parse_message(json);

    ASSERT_TRUE(is_stream_event(msg));
    auto& stream = std::get<StreamEvent>(msg);
    EXPECT_EQ(stream.event, "content_block_delta");
    EXPECT_EQ(stream.index, 0);
    EXPECT_EQ(stream.uuid, "evt_abc123");
    EXPECT_EQ(stream.session_id, "sess_xyz789");
    EXPECT_FALSE(stream.parent_tool_use_id.has_value());
}

TEST(ParserTest, StreamEventWithTopLevelUuidAndSessionId)
{
    std::string json = R"({
        "type":"stream",
        "event":"message_start",
        "index":0,
        "uuid":"msg_uuid_456",
        "session_id":"sess_top_123"
    })";

    Message msg = MessageParser::parse_message(json);

    ASSERT_TRUE(is_stream_event(msg));
    auto& stream = std::get<StreamEvent>(msg);
    EXPECT_EQ(stream.event, "message_start");
    EXPECT_EQ(stream.uuid, "msg_uuid_456");
    EXPECT_EQ(stream.session_id, "sess_top_123");
}

TEST(ParserTest, StreamEventWithParentToolUseId)
{
    std::string json = R"({
        "type":"stream",
        "event":"content_block_start",
        "index":0,
        "data":{
            "uuid":"tool_event_123",
            "parent_tool_use_id":"tool_use_parent_789"
        }
    })";

    Message msg = MessageParser::parse_message(json);

    ASSERT_TRUE(is_stream_event(msg));
    auto& stream = std::get<StreamEvent>(msg);
    EXPECT_EQ(stream.event, "content_block_start");
    EXPECT_EQ(stream.uuid, "tool_event_123");
    ASSERT_TRUE(stream.parent_tool_use_id.has_value());
    EXPECT_EQ(*stream.parent_tool_use_id, "tool_use_parent_789");
}

TEST(ParserTest, StreamEventWithNullParentToolUseId)
{
    std::string json = R"({
        "type":"stream",
        "event":"content_block_delta",
        "index":0,
        "data":{
            "parent_tool_use_id":null
        }
    })";

    Message msg = MessageParser::parse_message(json);

    ASSERT_TRUE(is_stream_event(msg));
    auto& stream = std::get<StreamEvent>(msg);
    EXPECT_FALSE(stream.parent_tool_use_id.has_value());
}

TEST(ParserTest, StreamEventWithAllFields)
{
    std::string json = R"({
        "type":"stream",
        "event":"tool_use_delta",
        "index":1,
        "uuid":"comprehensive_uuid",
        "session_id":"comprehensive_session",
        "parent_tool_use_id":"parent_tool_123",
        "data":{
            "delta":{"partial_json":"{\"key\":"}
        }
    })";

    Message msg = MessageParser::parse_message(json);

    ASSERT_TRUE(is_stream_event(msg));
    auto& stream = std::get<StreamEvent>(msg);
    EXPECT_EQ(stream.event, "tool_use_delta");
    EXPECT_EQ(stream.index, 1);
    EXPECT_EQ(stream.uuid, "comprehensive_uuid");
    EXPECT_EQ(stream.session_id, "comprehensive_session");
    ASSERT_TRUE(stream.parent_tool_use_id.has_value());
    EXPECT_EQ(*stream.parent_tool_use_id, "parent_tool_123");
    EXPECT_TRUE(stream.data.contains("delta"));
}

TEST(ParserTest, StreamEventWithMissingOptionalFields)
{
    std::string json = R"({
        "type":"stream",
        "event":"message_stop",
        "index":0
    })";

    Message msg = MessageParser::parse_message(json);

    ASSERT_TRUE(is_stream_event(msg));
    auto& stream = std::get<StreamEvent>(msg);
    EXPECT_EQ(stream.event, "message_stop");
    EXPECT_TRUE(stream.uuid.empty());
    EXPECT_TRUE(stream.session_id.empty());
    EXPECT_FALSE(stream.parent_tool_use_id.has_value());
}

TEST(ParserTest, StreamEventPrioritizesTopLevelOverData)
{
    std::string json = R"({
        "type":"stream",
        "event":"content_block_delta",
        "index":0,
        "uuid":"top_level_uuid",
        "session_id":"top_level_session",
        "data":{
            "uuid":"nested_uuid",
            "session_id":"nested_session",
            "delta":{"text":"Test"}
        }
    })";

    Message msg = MessageParser::parse_message(json);

    ASSERT_TRUE(is_stream_event(msg));
    auto& stream = std::get<StreamEvent>(msg);
    // Parser extracts from data first, then top-level fields override
    // Top-level has priority (makes sense for CLI to override)
    EXPECT_EQ(stream.uuid, "top_level_uuid");
    EXPECT_EQ(stream.session_id, "top_level_session");
}
