// Python SDK v0.2.152 additions.
//
// Concentrates on the three things the parity review flagged as easy to port
// incorrectly, because none of them would fail visibly without a live CLI.

#include <claude/errors.hpp>
#include <claude/types.hpp>
#include <gtest/gtest.h>

using namespace claude;

// TERMINAL_TASK_STATUSES spans two vocabularies on purpose: task_notification
// reports the CLI's mapped "stopped", task_updated reports the raw "killed". A
// task stopped via TaskStop may only ever report "killed", so checking one
// vocabulary leaves it tracked as active forever.
TEST(V02152ParityTest, TerminalTaskStatusSpansBothVocabularies)
{
    EXPECT_TRUE(is_terminal_task_status("completed"));
    EXPECT_TRUE(is_terminal_task_status("failed"));
    EXPECT_TRUE(is_terminal_task_status("stopped")); // task_notification form
    EXPECT_TRUE(is_terminal_task_status("killed"));  // task_updated form

    EXPECT_FALSE(is_terminal_task_status("pending"));
    EXPECT_FALSE(is_terminal_task_status("running"));
    EXPECT_FALSE(is_terminal_task_status("paused"));
    EXPECT_FALSE(is_terminal_task_status(""));
    EXPECT_FALSE(is_terminal_task_status("Completed")); // case-sensitive wire values
}

// ModelUsage is passed through verbatim from the CLI's modelUsage field, so its
// keys are camelCase even though this SDK is otherwise snake_case. Renaming
// them to match house style would silently stop parsing.
TEST(V02152ParityTest, ModelUsageUsesCamelCaseWireKeys)
{
    ModelUsage usage;
    usage.input_tokens = 11;
    usage.output_tokens = 22;
    usage.cache_read_input_tokens = 33;
    usage.cache_creation_input_tokens = 44;
    usage.web_search_requests = 5;
    usage.cost_usd = 1.25;
    usage.context_window = 200000;
    usage.max_output_tokens = 8192;

    const json j = usage;
    EXPECT_EQ(j["inputTokens"], 11);
    EXPECT_EQ(j["outputTokens"], 22);
    EXPECT_EQ(j["cacheReadInputTokens"], 33);
    EXPECT_EQ(j["cacheCreationInputTokens"], 44);
    EXPECT_EQ(j["webSearchRequests"], 5);
    EXPECT_DOUBLE_EQ(j["costUSD"].get<double>(), 1.25);
    EXPECT_EQ(j["contextWindow"], 200000);
    EXPECT_EQ(j["maxOutputTokens"], 8192);
    // No snake_case aliases: a reader relying on them would be reading nothing.
    EXPECT_FALSE(j.contains("input_tokens"));
    EXPECT_FALSE(j.contains("cost_usd"));

    const auto round_tripped = j.get<ModelUsage>();
    EXPECT_EQ(round_tripped.input_tokens, 11);
    EXPECT_EQ(round_tripped.cache_read_input_tokens, 33);
    EXPECT_DOUBLE_EQ(round_tripped.cost_usd, 1.25);
    EXPECT_EQ(round_tripped.max_output_tokens, 8192);
}

TEST(V02152ParityTest, ModelUsageToleratesMissingFields)
{
    // A partial payload must not throw -- the CLI is free to omit fields.
    const auto usage = json{{"inputTokens", 7}}.get<ModelUsage>();
    EXPECT_EQ(usage.input_tokens, 7);
    EXPECT_EQ(usage.output_tokens, 0);
    EXPECT_DOUBLE_EQ(usage.cost_usd, 0.0);
}

// MessageOriginKind is an open set: upstream documents that newer CLIs may emit
// kinds not listed, and unrecognized ones are "not human". Modelling kind as an
// enum would turn a forward-compatible field into a parse failure.
TEST(V02152ParityTest, MessageOriginKindIsOpenAndUnknownIsNotHuman)
{
    MessageOrigin human;
    human.kind = MessageOriginKind::Human;
    EXPECT_TRUE(human.is_human());

    MessageOrigin peer;
    peer.kind = MessageOriginKind::Peer;
    EXPECT_FALSE(peer.is_human());

    // A kind this SDK has never heard of: accepted, and not human.
    MessageOrigin future;
    future.kind = "some-kind-from-a-newer-cli";
    EXPECT_FALSE(future.is_human());
    EXPECT_EQ(future.kind, "some-kind-from-a-newer-cli");
}

// normalize_result_errors keeps the structured errors and the exception text in
// agreement: a bare string is tolerated, blanks and non-strings are dropped.
TEST(V02152ParityTest, NormalizeResultErrors)
{
    EXPECT_EQ(normalize_result_errors(json::array({"boom", "bang"})),
              (std::vector<std::string>{"boom", "bang"}));

    // Older or buggy emitters send a bare string rather than a list.
    EXPECT_EQ(normalize_result_errors(json("solo")), (std::vector<std::string>{"solo"}));

    // Blanks, whitespace-only entries and non-strings are dropped, and survivors
    // are trimmed.
    EXPECT_EQ(normalize_result_errors(json::array({"  padded  ", "", "   ", 42, nullptr, "kept"})),
              (std::vector<std::string>{"padded", "kept"}));

    EXPECT_TRUE(normalize_result_errors(json::object()).empty());
    EXPECT_TRUE(normalize_result_errors(json(nullptr)).empty());
}

// ResultError subclasses ProcessError so existing handlers keep working.
TEST(V02152ParityTest, ResultErrorIsCatchableAsProcessError)
{
    const auto thrower = []
    {
        throw ResultError("run failed", 1, "error_max_turns", std::string("api_error"),
                          {"overloaded"});
    };

    try
    {
        thrower();
        FAIL() << "expected ResultError";
    }
    catch (const ProcessError& e)
    {
        // Caught through the base, as an existing handler would.
        EXPECT_EQ(e.exit_code(), 1);

        const auto* result = dynamic_cast<const ResultError*>(&e);
        ASSERT_NE(result, nullptr) << "ResultError must remain a ProcessError";
        EXPECT_EQ(result->subtype(), "error_max_turns");
        ASSERT_TRUE(result->terminal_reason().has_value());
        EXPECT_EQ(*result->terminal_reason(), "api_error");
        EXPECT_EQ(result->errors(), (std::vector<std::string>{"overloaded"}));
    }
}

TEST(V02152ParityTest, TaskUpdatedMessageIsASystemMessage)
{
    TaskUpdatedMessage msg;
    msg.subtype = "task_updated";
    msg.task_id = "task-1";
    msg.patch = json{{"status", "killed"}};
    msg.status = TaskUpdatedStatus::Killed;

    // Existing code that handles SystemMessage must keep matching.
    const SystemMessage& base = msg;
    EXPECT_EQ(base.type, "system");
    EXPECT_EQ(base.subtype, "task_updated");

    ASSERT_TRUE(msg.status.has_value());
    EXPECT_TRUE(is_terminal_task_status(*msg.status));
    EXPECT_EQ(msg.patch["status"], "killed");
}

TEST(V02152ParityTest, ConversationResetMessageShape)
{
    ConversationResetMessage msg;
    msg.new_conversation_id = "conv-2";
    msg.uuid = "u-1";
    msg.session_id = "sess-old";

    EXPECT_EQ(msg.type, "conversation_reset");
    // The new conversation id is not the next session id -- distinct fields, and
    // conflating them would mis-key a UI transcript.
    EXPECT_NE(msg.new_conversation_id, msg.session_id);
}
