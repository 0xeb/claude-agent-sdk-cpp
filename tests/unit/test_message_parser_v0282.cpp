/// Phase 4A (T48): message parser updates for v0.2.82 — server_tool_use /
/// advisor_tool_result blocks, unknown-type tolerance, rate_limit_event,
/// system-message subtype dispatch, per-turn usage on AssistantMessage.
#include "../../src/internal/message_parser.hpp"

#include <claude/errors.hpp>
#include <gtest/gtest.h>

using namespace claude;
using namespace claude::protocol;

TEST(ParserV0282, ServerToolUseBlock)
{
    std::string json = R"({
        "type":"assistant",
        "message":{"role":"assistant","model":"claude","content":[
            {"type":"server_tool_use","id":"srv_1","name":"web_search","input":{"q":"hi"}}
        ]}
    })";
    Message msg = MessageParser::parse_message(json);
    ASSERT_TRUE(is_assistant_message(msg));
    auto& a = std::get<AssistantMessage>(msg);
    ASSERT_EQ(a.content.size(), 1u);
    auto* blk = std::get_if<ServerToolUseBlock>(&a.content[0]);
    ASSERT_NE(blk, nullptr);
    EXPECT_EQ(blk->id, "srv_1");
    EXPECT_EQ(blk->name, "web_search");
}

TEST(ParserV0282, AdvisorToolResultBlock)
{
    std::string json = R"({
        "type":"assistant",
        "message":{"role":"assistant","model":"claude","content":[
            {"type":"advisor_tool_result","tool_use_id":"srv_1","content":{"ok":true}}
        ]}
    })";
    Message msg = MessageParser::parse_message(json);
    auto& a = std::get<AssistantMessage>(msg);
    ASSERT_EQ(a.content.size(), 1u);
    auto* blk = std::get_if<ServerToolResultBlock>(&a.content[0]);
    ASSERT_NE(blk, nullptr);
    EXPECT_EQ(blk->tool_use_id, "srv_1");
    EXPECT_TRUE(blk->content.is_object());
    EXPECT_TRUE(blk->content.value("ok", false));
}

TEST(ParserV0282, AssistantPreservesUsage)
{
    std::string json = R"({
        "type":"assistant",
        "session_id":"sess1",
        "uuid":"u1",
        "message":{"role":"assistant","model":"claude","id":"msg_x","stop_reason":"end_turn",
            "usage":{"input_tokens":12,"output_tokens":5},
            "content":[{"type":"text","text":"hi"}]}
    })";
    Message msg = MessageParser::parse_message(json);
    auto& a = std::get<AssistantMessage>(msg);
    ASSERT_TRUE(a.usage.has_value());
    EXPECT_EQ(a.usage->value("input_tokens", 0), 12);
    ASSERT_TRUE(a.message_id.has_value());
    EXPECT_EQ(*a.message_id, "msg_x");
    ASSERT_TRUE(a.stop_reason.has_value());
    EXPECT_EQ(*a.stop_reason, "end_turn");
    ASSERT_TRUE(a.session_id.has_value());
    EXPECT_EQ(*a.session_id, "sess1");
}

TEST(ParserV0282, RateLimitEventParsed)
{
    std::string json = R"({
        "type":"rate_limit_event",
        "uuid":"u1","session_id":"s1",
        "rate_limit_info":{
            "status":"allowed_warning","resetsAt":1234567,"utilization":0.7,
            "rateLimitType":"five_hour"
        }
    })";
    Message msg = MessageParser::parse_message(json);
    ASSERT_TRUE(std::holds_alternative<RateLimitEvent>(msg));
    auto& ev = std::get<RateLimitEvent>(msg);
    EXPECT_EQ(ev.uuid, "u1");
    EXPECT_EQ(ev.session_id, "s1");
    EXPECT_EQ(ev.rate_limit_info.status, "allowed_warning");
    ASSERT_TRUE(ev.rate_limit_info.utilization.has_value());
    EXPECT_NEAR(*ev.rate_limit_info.utilization, 0.7, 1e-6);
    ASSERT_TRUE(ev.rate_limit_info.rate_limit_type.has_value());
    EXPECT_EQ(*ev.rate_limit_info.rate_limit_type, "five_hour");
}

TEST(ParserV0282, UnknownTopLevelTypeIsSkipped)
{
    // parse_message itself throws MessageParseError on unknown types, but
    // add_data() catches and skips for forward compatibility.
    MessageParser parser;
    std::string line =
        R"({"type":"future_thing","payload":"unknown"})" "\n";
    auto msgs = parser.add_data(line);
    EXPECT_TRUE(msgs.empty());
}

TEST(ParserV0282, NonJSONLineIsSkipped)
{
    MessageParser parser;
    // Mixed lines: non-JSON warning, then a real assistant message
    std::string data =
        "[SandboxDebug] some warning text\n"
        R"({"type":"assistant","message":{"role":"assistant","model":"x","content":[{"type":"text","text":"ok"}]}})"
        "\n";
    auto msgs = parser.add_data(data);
    ASSERT_EQ(msgs.size(), 1u);
    EXPECT_TRUE(is_assistant_message(msgs[0]));
}

TEST(ParserV0282, TaskStartedSubtypeDispatch)
{
    std::string json = R"({
        "type":"system","subtype":"task_started",
        "task_id":"t1","description":"work","uuid":"u","session_id":"s"
    })";
    Message msg = MessageParser::parse_message(json);
    ASSERT_TRUE(is_system_message(msg));
    auto& s = std::get<SystemMessage>(msg);
    EXPECT_EQ(s.subtype, "task_started");
    // Raw JSON preserves all the task_id/description fields
    EXPECT_EQ(s.raw_json.value("task_id", ""), "t1");
    EXPECT_EQ(s.raw_json.value("description", ""), "work");
}

TEST(ParserV0282, HookStartedSubtypeDispatch)
{
    std::string json = R"({
        "type":"system","subtype":"hook_started",
        "hook_event":"PreToolUse","session_id":"s","uuid":"u"
    })";
    Message msg = MessageParser::parse_message(json);
    ASSERT_TRUE(is_system_message(msg));
    auto& s = std::get<SystemMessage>(msg);
    EXPECT_EQ(s.subtype, "hook_started");
    EXPECT_EQ(s.raw_json.value("hook_event", ""), "PreToolUse");
}

TEST(ParserV0282, ResultMessageNewFields)
{
    std::string json = R"({
        "type":"result","subtype":"error","session_id":"s1",
        "duration_ms":100,"duration_api_ms":50,"num_turns":1,
        "stop_reason":"api_error","api_error_status":429,
        "errors":["rate_limit_error","retry_after=10"],
        "deferred_tool_use":{"id":"d1","name":"tool","input":{"x":1}},
        "uuid":"r1"
    })";
    Message msg = MessageParser::parse_message(json);
    ASSERT_TRUE(is_result_message(msg));
    auto& r = std::get<ResultMessage>(msg);
    EXPECT_TRUE(r.is_error());
    ASSERT_TRUE(r.stop_reason.has_value());
    EXPECT_EQ(*r.stop_reason, "api_error");
    ASSERT_TRUE(r.api_error_status.has_value());
    EXPECT_EQ(*r.api_error_status, 429);
    ASSERT_TRUE(r.errors.has_value());
    EXPECT_EQ(r.errors->size(), 2u);
    ASSERT_TRUE(r.deferred_tool_use.has_value());
    EXPECT_EQ(r.deferred_tool_use->id, "d1");
    EXPECT_EQ(r.deferred_tool_use->name, "tool");
    ASSERT_TRUE(r.uuid.has_value());
    EXPECT_EQ(*r.uuid, "r1");
}
