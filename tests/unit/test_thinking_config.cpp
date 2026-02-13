/**
 * @file test_thinking_config.cpp
 * @brief Unit tests for ThinkingConfig CLI flag resolution and EffortLevel
 *
 * Tests the ThinkingConfig → --max-thinking-tokens CLI flag generation
 * and --effort flag resolution without requiring a live CLI.
 * Mirrors the resolution logic in subprocess_transport.cpp and subprocess_cli.cpp.
 */

#include <claude/types.hpp>
#include <gtest/gtest.h>
#include <optional>
#include <variant>

using namespace claude;

// Helper that mimics the ThinkingConfig resolution logic from subprocess_transport.cpp
// This allows testing the flag generation without accessing private build_command()
static std::optional<int> resolve_max_thinking_tokens(const ClaudeOptions& opts)
{
    std::optional<int> resolved = opts.max_thinking_tokens;
    if (opts.thinking.has_value())
    {
        std::visit(
            [&resolved](const auto& config)
            {
                using T = std::decay_t<decltype(config)>;
                if constexpr (std::is_same_v<T, ThinkingConfigAdaptive>)
                {
                    if (!resolved.has_value())
                        resolved = 32000;
                }
                else if constexpr (std::is_same_v<T, ThinkingConfigEnabled>)
                {
                    resolved = config.budget_tokens;
                }
                else if constexpr (std::is_same_v<T, ThinkingConfigDisabled>)
                {
                    resolved = 0;
                }
            },
            *opts.thinking);
    }
    return resolved;
}

// ============================================================================
// ThinkingConfig Resolution Tests
// ============================================================================

TEST(ThinkingConfigResolution, AdaptiveWithNoExistingTokens)
{
    ClaudeOptions opts;
    opts.thinking = ThinkingConfigAdaptive{};
    // max_thinking_tokens not set

    auto resolved = resolve_max_thinking_tokens(opts);
    ASSERT_TRUE(resolved.has_value());
    EXPECT_EQ(*resolved, 32000); // Default for adaptive
}

TEST(ThinkingConfigResolution, AdaptiveWithExistingTokens)
{
    ClaudeOptions opts;
    opts.max_thinking_tokens = 16000;
    opts.thinking = ThinkingConfigAdaptive{};

    auto resolved = resolve_max_thinking_tokens(opts);
    ASSERT_TRUE(resolved.has_value());
    EXPECT_EQ(*resolved, 16000); // Keeps existing value
}

TEST(ThinkingConfigResolution, EnabledOverridesExisting)
{
    ClaudeOptions opts;
    opts.max_thinking_tokens = 16000; // Will be overridden
    opts.thinking = ThinkingConfigEnabled(8000);

    auto resolved = resolve_max_thinking_tokens(opts);
    ASSERT_TRUE(resolved.has_value());
    EXPECT_EQ(*resolved, 8000); // Enabled budget takes precedence
}

TEST(ThinkingConfigResolution, EnabledWithoutExisting)
{
    ClaudeOptions opts;
    opts.thinking = ThinkingConfigEnabled(4096);

    auto resolved = resolve_max_thinking_tokens(opts);
    ASSERT_TRUE(resolved.has_value());
    EXPECT_EQ(*resolved, 4096);
}

TEST(ThinkingConfigResolution, DisabledSetsZero)
{
    ClaudeOptions opts;
    opts.max_thinking_tokens = 16000; // Will be overridden
    opts.thinking = ThinkingConfigDisabled{};

    auto resolved = resolve_max_thinking_tokens(opts);
    ASSERT_TRUE(resolved.has_value());
    EXPECT_EQ(*resolved, 0); // Disabled = 0
}

TEST(ThinkingConfigResolution, NoThinkingConfig)
{
    ClaudeOptions opts;
    // Neither thinking nor max_thinking_tokens set

    auto resolved = resolve_max_thinking_tokens(opts);
    EXPECT_FALSE(resolved.has_value()); // Nothing to resolve
}

TEST(ThinkingConfigResolution, OnlyMaxThinkingTokens)
{
    ClaudeOptions opts;
    opts.max_thinking_tokens = 2048;
    // thinking not set

    auto resolved = resolve_max_thinking_tokens(opts);
    ASSERT_TRUE(resolved.has_value());
    EXPECT_EQ(*resolved, 2048); // Pass through directly
}

TEST(ThinkingConfigResolution, ThinkingTakesPrecedenceOverDeprecated)
{
    // Enabled config should always win over deprecated max_thinking_tokens
    ClaudeOptions opts;
    opts.max_thinking_tokens = 99999;
    opts.thinking = ThinkingConfigEnabled(1024);

    auto resolved = resolve_max_thinking_tokens(opts);
    ASSERT_TRUE(resolved.has_value());
    EXPECT_EQ(*resolved, 1024); // ThinkingConfig wins
}

// ============================================================================
// EffortLevel Tests
// ============================================================================

TEST(EffortLevelTest, OptionsAcceptsEffort)
{
    ClaudeOptions opts;
    EXPECT_FALSE(opts.effort.has_value());

    opts.effort = Effort::Low;
    EXPECT_EQ(*opts.effort, "low");

    opts.effort = Effort::Medium;
    EXPECT_EQ(*opts.effort, "medium");

    opts.effort = Effort::High;
    EXPECT_EQ(*opts.effort, "high");

    opts.effort = Effort::Max;
    EXPECT_EQ(*opts.effort, "max");
}

TEST(EffortLevelTest, EffortAndThinkingCombined)
{
    ClaudeOptions opts;
    opts.thinking = ThinkingConfigAdaptive{};
    opts.effort = Effort::High;

    // Both can coexist
    EXPECT_TRUE(opts.thinking.has_value());
    EXPECT_TRUE(opts.effort.has_value());

    auto resolved = resolve_max_thinking_tokens(opts);
    ASSERT_TRUE(resolved.has_value());
    EXPECT_EQ(*resolved, 32000); // Adaptive default
    EXPECT_EQ(*opts.effort, "high");
}

// ============================================================================
// Agents via Initialize (not CLI args) Verification
// ============================================================================

TEST(AgentsViaInitialize, AgentsJsonForInitializeRequest)
{
    // This test verifies that agents are serialized correctly for the
    // initialize control request (matching Python SDK v0.1.35 behavior)
    ClaudeOptions opts;

    AgentDefinition agent;
    agent.description = "Test agent";
    agent.prompt = "You are a test agent.";
    agent.tools = std::vector<std::string>{"Read", "Write"};
    agent.model = "sonnet";
    opts.agents["test-agent"] = agent;

    // Build agents config as done in client.cpp initialize()
    json agents_config = json::object();
    for (const auto& [name, def] : opts.agents)
    {
        json agent_obj = json::object();
        agent_obj["description"] = def.description;
        agent_obj["prompt"] = def.prompt;
        if (def.tools.has_value())
            agent_obj["tools"] = *def.tools;
        if (def.model.has_value())
            agent_obj["model"] = *def.model;
        agents_config[name] = agent_obj;
    }

    // Build the initialize request data (matching client.cpp)
    json request_data = {{"hooks", nullptr}, {"agents", agents_config}};

    // Verify structure
    ASSERT_TRUE(request_data.contains("agents"));
    ASSERT_TRUE(request_data["agents"].is_object());
    ASSERT_TRUE(request_data["agents"].contains("test-agent"));

    auto& agent_json = request_data["agents"]["test-agent"];
    EXPECT_EQ(agent_json["description"], "Test agent");
    EXPECT_EQ(agent_json["prompt"], "You are a test agent.");
    EXPECT_EQ(agent_json["model"], "sonnet");
    EXPECT_EQ(agent_json["tools"].size(), 2);
}

TEST(AgentsViaInitialize, EmptyAgentsProducesNull)
{
    ClaudeOptions opts;
    // No agents set

    json agents_config = json(nullptr);
    if (!opts.agents.empty())
    {
        agents_config = json::object();
        for (const auto& [name, def] : opts.agents)
        {
            json agent_obj = json::object();
            agent_obj["description"] = def.description;
            agent_obj["prompt"] = def.prompt;
            agents_config[name] = agent_obj;
        }
    }

    json request_data = {{"hooks", nullptr}, {"agents", agents_config}};
    EXPECT_TRUE(request_data["agents"].is_null());
}
