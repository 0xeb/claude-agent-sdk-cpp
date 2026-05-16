/// Phase 4A (T72): query.cpp suppresses redundant ProcessError after an
/// error result message and surfaces actionable error text. Unit-level tests
/// here just verify the behavior is wired up at the ResultMessage type level;
/// integration coverage requires a live CLI.
#include <claude/errors.hpp>
#include <claude/query.hpp>
#include <claude/types.hpp>
#include <gtest/gtest.h>

using namespace claude;

TEST(QueryV0282, EmptyPromptThrowsClaudeError)
{
    EXPECT_THROW(query("", ClaudeOptions{}), ClaudeError);
}

TEST(QueryV0282, ResultMessageActionableErrorFields)
{
    /// Validate that error-result fields needed for actionable error text
    /// round-trip through the type system. query.cpp uses these to build the
    /// suppressed-ProcessError replacement message (Python parity 9aafd84).
    ResultMessage r;
    r.subtype = "error";
    r.api_error_status = 429;
    r.errors = std::vector<std::string>{"rate_limit_error"};
    EXPECT_TRUE(r.is_error());
    ASSERT_TRUE(r.api_error_status.has_value());
    EXPECT_EQ(*r.api_error_status, 429);
    ASSERT_TRUE(r.errors.has_value());
    EXPECT_EQ(r.errors->front(), "rate_limit_error");
}
