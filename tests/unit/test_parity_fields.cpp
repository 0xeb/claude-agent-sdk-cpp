#include <gtest/gtest.h>
#include <claude/types.hpp>

using namespace claude;

TEST(ParityFields, FallbackModelField)
{
    ClaudeOptions opts;
    // Field should be default-constructible (empty string)
    EXPECT_TRUE(opts.fallback_model.empty());

    // Field should be settable
    opts.fallback_model = "claude-sonnet-4-5";
    EXPECT_EQ(opts.fallback_model, "claude-sonnet-4-5");
    EXPECT_FALSE(opts.fallback_model.empty());
}

TEST(ParityFields, ResultMessageIsErrorDefaultFalse)
{
    ResultMessage msg;
    EXPECT_FALSE(msg.is_error());
    msg.subtype = "success";
    EXPECT_FALSE(msg.is_error());
    msg.subtype = "error";
    EXPECT_TRUE(msg.is_error());
}

TEST(ParityFields, StreamEventEventDictNormalization)
{
    StreamEvent e;
    e.event = "content_block_delta";
    e.index = 3;
    e.data = claude::json{{"delta", {{"type", "text_delta"}, {"text", "xy"}}}};

    auto d = e.event_dict();
    ASSERT_TRUE(d.is_object());
    EXPECT_EQ(d["type"], "content_block_delta");
    EXPECT_EQ(d["index"], 3);
    EXPECT_EQ(d["delta"]["text"], "xy");
}

