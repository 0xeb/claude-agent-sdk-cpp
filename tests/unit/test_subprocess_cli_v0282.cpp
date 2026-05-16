/// Phase 4A (T50, T52, T54, T70): verify new CLI flag emission in the
/// subprocess transport build_command(). Drives build_command() directly
/// without spawning a CLI process.
#include "../../src/internal/transport/subprocess_transport.hpp"

#include <claude/types.hpp>
#include <gtest/gtest.h>

using namespace claude;

namespace
{
bool contains(const std::vector<std::string>& args, const std::string& a)
{
    for (const auto& x : args)
        if (x == a)
            return true;
    return false;
}

// Returns the value that follows the first occurrence of flag in args
// (empty string if flag is last or absent).
std::string value_after(const std::vector<std::string>& args, const std::string& flag)
{
    for (size_t i = 0; i + 1 < args.size(); ++i)
        if (args[i] == flag)
            return args[i + 1];
    return "";
}
} // namespace

TEST(SubprocessCliV0282, EmitsSessionIdFlag)
{
    ClaudeOptions opts;
    opts.session_id = "abc-123";
    internal::SubprocessTransport t(opts);
    auto args = t.build_command();
    EXPECT_TRUE(contains(args, "--session-id"));
    EXPECT_EQ(value_after(args, "--session-id"), "abc-123");
}

TEST(SubprocessCliV0282, OmitsSessionIdWhenUnset)
{
    ClaudeOptions opts;
    internal::SubprocessTransport t(opts);
    auto args = t.build_command();
    EXPECT_FALSE(contains(args, "--session-id"));
}

TEST(SubprocessCliV0282, EmitsTaskBudgetFlag)
{
    ClaudeOptions opts;
    opts.task_budget = TaskBudget{10000};
    internal::SubprocessTransport t(opts);
    auto args = t.build_command();
    EXPECT_TRUE(contains(args, "--task-budget"));
    EXPECT_EQ(value_after(args, "--task-budget"), "10000");
}

TEST(SubprocessCliV0282, EmitsStrictMcpConfigFlag)
{
    ClaudeOptions opts;
    opts.strict_mcp_config = true;
    internal::SubprocessTransport t(opts);
    auto args = t.build_command();
    EXPECT_TRUE(contains(args, "--strict-mcp-config"));
}

TEST(SubprocessCliV0282, EmitsIncludeHookEventsFlag)
{
    ClaudeOptions opts;
    opts.include_hook_events = true;
    internal::SubprocessTransport t(opts);
    auto args = t.build_command();
    EXPECT_TRUE(contains(args, "--include-hook-events"));
}

TEST(SubprocessCliV0282, EmitsSystemPromptFileFlag)
{
    ClaudeOptions opts;
    SystemPromptFile spf;
    spf.path = "/tmp/prompt.txt";
    opts.system_prompt_file = spf;
    internal::SubprocessTransport t(opts);
    auto args = t.build_command();
    EXPECT_TRUE(contains(args, "--system-prompt-file"));
    EXPECT_EQ(value_after(args, "--system-prompt-file"), "/tmp/prompt.txt");
    // Should not also emit --system-prompt
    EXPECT_FALSE(contains(args, "--system-prompt"));
}

TEST(SubprocessCliV0282, EmitsSystemPromptPresetAppend)
{
    ClaudeOptions opts;
    SystemPromptPreset preset;
    preset.append = "additional instructions";
    opts.system_prompt_preset = preset;
    internal::SubprocessTransport t(opts);
    auto args = t.build_command();
    EXPECT_TRUE(contains(args, "--append-system-prompt"));
    EXPECT_EQ(value_after(args, "--append-system-prompt"), "additional instructions");
}

TEST(SubprocessCliV0282, ThinkingAdaptiveEmitsThinkingFlag)
{
    ClaudeOptions opts;
    opts.thinking = ThinkingConfigAdaptive{};
    internal::SubprocessTransport t(opts);
    auto args = t.build_command();
    EXPECT_TRUE(contains(args, "--thinking"));
    EXPECT_EQ(value_after(args, "--thinking"), "adaptive");
    // Adaptive must NOT emit --max-thinking-tokens (Python commit 6617b9e)
    EXPECT_FALSE(contains(args, "--max-thinking-tokens"));
}

TEST(SubprocessCliV0282, ThinkingDisabledEmitsThinkingFlag)
{
    ClaudeOptions opts;
    opts.thinking = ThinkingConfigDisabled{};
    internal::SubprocessTransport t(opts);
    auto args = t.build_command();
    EXPECT_EQ(value_after(args, "--thinking"), "disabled");
    EXPECT_FALSE(contains(args, "--max-thinking-tokens"));
}

TEST(SubprocessCliV0282, ThinkingEnabledEmitsMaxTokens)
{
    ClaudeOptions opts;
    opts.thinking = ThinkingConfigEnabled{8000};
    internal::SubprocessTransport t(opts);
    auto args = t.build_command();
    EXPECT_TRUE(contains(args, "--max-thinking-tokens"));
    EXPECT_EQ(value_after(args, "--max-thinking-tokens"), "8000");
}

TEST(SubprocessCliV0282, ThinkingDisplayForwarded)
{
    ClaudeOptions opts;
    ThinkingConfigAdaptive adaptive;
    adaptive.display = "summary";
    opts.thinking = adaptive;
    internal::SubprocessTransport t(opts);
    auto args = t.build_command();
    EXPECT_TRUE(contains(args, "--thinking-display"));
    EXPECT_EQ(value_after(args, "--thinking-display"), "summary");
}

TEST(SubprocessCliV0282, SkillsAllInjectsSkillTool)
{
    ClaudeOptions opts;
    opts.skills = std::string("all");
    internal::SubprocessTransport t(opts);
    auto args = t.build_command();
    // Skill should be in --allowedTools
    std::string allowed = value_after(args, "--allowedTools");
    EXPECT_NE(allowed.find("Skill"), std::string::npos);
    // setting-sources should default to user,project
    std::string srcs = value_after(args, "--setting-sources");
    EXPECT_NE(srcs.find("user"), std::string::npos);
    EXPECT_NE(srcs.find("project"), std::string::npos);
}

TEST(SubprocessCliV0282, SkillsListInjectsSkillNames)
{
    ClaudeOptions opts;
    opts.skills = std::vector<std::string>{"git", "docker"};
    internal::SubprocessTransport t(opts);
    auto args = t.build_command();
    std::string allowed = value_after(args, "--allowedTools");
    EXPECT_NE(allowed.find("Skill(git)"), std::string::npos);
    EXPECT_NE(allowed.find("Skill(docker)"), std::string::npos);
}

TEST(SubprocessCliV0282, SettingSourcesEmptyEmitsEmptyFlag)
{
    /// Python parity (commit e621929): empty list disables filesystem settings.
    /// C++ port emits "--setting-sources" "" which CLI accepts as empty.
    ClaudeOptions opts;
    internal::SubprocessTransport t(opts);
    auto args = t.build_command();
    EXPECT_TRUE(contains(args, "--setting-sources"));
    EXPECT_EQ(value_after(args, "--setting-sources"), "");
}

TEST(SubprocessCliV0282, SettingSourcesNonEmpty)
{
    ClaudeOptions opts;
    opts.setting_sources = {"user", "local"};
    internal::SubprocessTransport t(opts);
    auto args = t.build_command();
    EXPECT_EQ(value_after(args, "--setting-sources"), "user,local");
}
