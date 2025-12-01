#include <claude/types.hpp>
#include <gtest/gtest.h>

using namespace claude;

// Helper function that mimics agents_to_json from subprocess_cli.cpp
// This allows us to test the serialization logic without accessing private members
static json agents_to_json(const std::map<std::string, AgentDefinition>& agents)
{
    json agents_json = json::object();
    for (const auto& [name, def] : agents)
    {
        json agent_obj = json::object();
        agent_obj["description"] = def.description;
        agent_obj["prompt"] = def.prompt;
        if (def.tools.has_value())
            agent_obj["tools"] = *def.tools;
        if (def.model.has_value())
            agent_obj["model"] = *def.model;
        agents_json[name] = agent_obj;
    }
    return agents_json;
}

// Test 1: Minimal agent with only required fields
TEST(AgentDefinitionTest, MinimalAgent)
{
    AgentDefinition agent;
    agent.description = "A simple agent";
    agent.prompt = "You are a helpful assistant.";

    std::map<std::string, AgentDefinition> agents;
    agents["simple"] = agent;

    auto j = agents_to_json(agents);

    ASSERT_TRUE(j.contains("simple"));
    auto simple = j["simple"];
    EXPECT_EQ(simple["description"].get<std::string>(), "A simple agent");
    EXPECT_EQ(simple["prompt"].get<std::string>(), "You are a helpful assistant.");
    EXPECT_FALSE(simple.contains("tools"));
    EXPECT_FALSE(simple.contains("model"));
}

// Test 2: Agent with tools (code reviewer example)
TEST(AgentDefinitionTest, AgentWithTools)
{
    AgentDefinition code_reviewer;
    code_reviewer.description = "Reviews code for best practices and potential issues";
    code_reviewer.prompt = "You are a code reviewer. Analyze code for bugs, performance issues, "
                           "security vulnerabilities, and adherence to best practices.";
    code_reviewer.tools = std::vector<std::string>{"Read", "Grep"};

    std::map<std::string, AgentDefinition> agents;
    agents["code-reviewer"] = code_reviewer;

    auto j = agents_to_json(agents);

    ASSERT_TRUE(j.contains("code-reviewer"));
    auto reviewer = j["code-reviewer"];
    EXPECT_EQ(reviewer["description"].get<std::string>(),
              "Reviews code for best practices and potential issues");
    ASSERT_TRUE(reviewer.contains("tools"));
    auto tools = reviewer["tools"].get<std::vector<std::string>>();
    EXPECT_EQ(tools.size(), 2u);
    EXPECT_EQ(tools[0], "Read");
    EXPECT_EQ(tools[1], "Grep");
}

// Test 3: Agent with model specified
TEST(AgentDefinitionTest, AgentWithModel)
{
    AgentDefinition doc_writer;
    doc_writer.description = "Writes comprehensive documentation";
    doc_writer.prompt = "You are a technical documentation expert.";
    doc_writer.tools = std::vector<std::string>{"Read", "Write", "Edit"};
    doc_writer.model = "sonnet";

    std::map<std::string, AgentDefinition> agents;
    agents["doc-writer"] = doc_writer;

    auto j = agents_to_json(agents);

    auto writer = j["doc-writer"];
    EXPECT_EQ(writer["model"].get<std::string>(), "sonnet");
    auto tools = writer["tools"].get<std::vector<std::string>>();
    EXPECT_EQ(tools.size(), 3u);
}

// Test 4: Multiple agents (mirrors agents.cpp multiple_agents_example)
TEST(AgentDefinitionTest, MultipleAgents)
{
    AgentDefinition analyzer;
    analyzer.description = "Analyzes code structure and patterns";
    analyzer.prompt =
        "You are a code analyzer. Examine code structure, patterns, and architecture.";
    analyzer.tools = std::vector<std::string>{"Read", "Grep", "Glob"};
    // analyzer.model left as nullopt to inherit from parent

    AgentDefinition tester;
    tester.description = "Creates and runs tests";
    tester.prompt = "You are a testing expert. Write comprehensive tests and ensure code quality.";
    tester.tools = std::vector<std::string>{"Read", "Write", "Bash"};
    tester.model = "sonnet";

    std::map<std::string, AgentDefinition> agents;
    agents["analyzer"] = analyzer;
    agents["tester"] = tester;

    auto j = agents_to_json(agents);

    EXPECT_EQ(j.size(), 2u);

    // Verify analyzer (no model)
    ASSERT_TRUE(j.contains("analyzer"));
    auto analyzer_j = j["analyzer"];
    EXPECT_FALSE(analyzer_j.contains("model"));
    auto analyzer_tools = analyzer_j["tools"].get<std::vector<std::string>>();
    EXPECT_EQ(analyzer_tools.size(), 3u);

    // Verify tester (has model)
    ASSERT_TRUE(j.contains("tester"));
    auto tester_j = j["tester"];
    EXPECT_EQ(tester_j["model"].get<std::string>(), "sonnet");
    auto tester_tools = tester_j["tools"].get<std::vector<std::string>>();
    EXPECT_EQ(tester_tools.size(), 3u);
}

// Test 5: Agent can be added to ClaudeOptions
TEST(AgentDefinitionTest, AgentInClaudeOptions)
{
    ClaudeOptions opts;
    opts.model = "claude-sonnet-4-20250514";

    AgentDefinition agent;
    agent.description = "Test agent";
    agent.prompt = "You are a test agent.";
    agent.tools = std::vector<std::string>{"Read"};
    agent.model = "haiku";

    opts.agents["test-agent"] = agent;

    ASSERT_EQ(opts.agents.size(), 1u);
    ASSERT_TRUE(opts.agents.count("test-agent") > 0);
    EXPECT_EQ(opts.agents["test-agent"].description, "Test agent");
    EXPECT_TRUE(opts.agents["test-agent"].model.has_value());
    EXPECT_EQ(*opts.agents["test-agent"].model, "haiku");
}

// Test 6: Empty agents map produces empty JSON
TEST(AgentDefinitionTest, EmptyAgentsProducesEmptyJson)
{
    std::map<std::string, AgentDefinition> agents;

    auto j = agents_to_json(agents);

    EXPECT_TRUE(j.empty());
}

// Test 7: Agent with empty tools list (valid but unusual)
TEST(AgentDefinitionTest, AgentWithEmptyToolsList)
{
    AgentDefinition agent;
    agent.description = "Agent with no tools";
    agent.prompt = "You have no tools available.";
    agent.tools = std::vector<std::string>{}; // Explicitly empty

    std::map<std::string, AgentDefinition> agents;
    agents["no-tools"] = agent;

    auto j = agents_to_json(agents);

    auto no_tools = j["no-tools"];
    ASSERT_TRUE(no_tools.contains("tools"));
    auto tools = no_tools["tools"].get<std::vector<std::string>>();
    EXPECT_TRUE(tools.empty());
}

// Test 8: Agent with all supported model values
TEST(AgentDefinitionTest, AgentModelValues)
{
    std::vector<std::string> valid_models = {"sonnet", "haiku", "opus"};

    for (const auto& model : valid_models)
    {
        AgentDefinition agent;
        agent.description = "Agent with " + model + " model";
        agent.prompt = "Test prompt";
        agent.model = model;

        std::map<std::string, AgentDefinition> agents;
        agents["test"] = agent;

        auto j = agents_to_json(agents);
        EXPECT_EQ(j["test"]["model"].get<std::string>(), model);
    }
}

// Test 9: Agent names with special characters
TEST(AgentDefinitionTest, AgentNamesWithSpecialCharacters)
{
    AgentDefinition agent;
    agent.description = "Test";
    agent.prompt = "Test";

    std::map<std::string, AgentDefinition> agents;
    agents["my-agent"] = agent;
    agents["my_agent"] = agent;
    agents["myAgent123"] = agent;

    auto j = agents_to_json(agents);

    EXPECT_TRUE(j.contains("my-agent"));
    EXPECT_TRUE(j.contains("my_agent"));
    EXPECT_TRUE(j.contains("myAgent123"));
    EXPECT_EQ(j.size(), 3u);
}

// Test 10: JSON round-trip consistency
TEST(AgentDefinitionTest, JsonRoundTripConsistency)
{
    AgentDefinition original;
    original.description = "A comprehensive agent";
    original.prompt = "You are helpful.";
    original.tools = std::vector<std::string>{"Read", "Write", "Edit", "Bash"};
    original.model = "sonnet";

    std::map<std::string, AgentDefinition> agents;
    agents["comprehensive"] = original;

    auto j = agents_to_json(agents);
    std::string json_str = j.dump();

    // Parse back
    auto j2 = json::parse(json_str);

    EXPECT_EQ(j2["comprehensive"]["description"].get<std::string>(), original.description);
    EXPECT_EQ(j2["comprehensive"]["prompt"].get<std::string>(), original.prompt);
    EXPECT_EQ(j2["comprehensive"]["model"].get<std::string>(), *original.model);
    EXPECT_EQ(j2["comprehensive"]["tools"].get<std::vector<std::string>>(), *original.tools);
}
