#include <claude/types.hpp>
#include <gtest/gtest.h>

using namespace claude;

// Helper function that mimics sandbox_to_json from subprocess_cli.cpp
// This allows us to test the serialization logic without accessing private members
static json sandbox_to_json(const SandboxSettings& sandbox)
{
    json j;

    if (sandbox.enabled.has_value())
        j["enabled"] = *sandbox.enabled;
    if (sandbox.autoAllowBashIfSandboxed.has_value())
        j["autoAllowBashIfSandboxed"] = *sandbox.autoAllowBashIfSandboxed;
    if (sandbox.excludedCommands.has_value())
        j["excludedCommands"] = *sandbox.excludedCommands;
    if (sandbox.allowUnsandboxedCommands.has_value())
        j["allowUnsandboxedCommands"] = *sandbox.allowUnsandboxedCommands;

    if (sandbox.network.has_value())
    {
        json net;
        if (sandbox.network->allowUnixSockets.has_value())
            net["allowUnixSockets"] = *sandbox.network->allowUnixSockets;
        if (sandbox.network->allowAllUnixSockets.has_value())
            net["allowAllUnixSockets"] = *sandbox.network->allowAllUnixSockets;
        if (sandbox.network->allowLocalBinding.has_value())
            net["allowLocalBinding"] = *sandbox.network->allowLocalBinding;
        if (sandbox.network->httpProxyPort.has_value())
            net["httpProxyPort"] = *sandbox.network->httpProxyPort;
        if (sandbox.network->socksProxyPort.has_value())
            net["socksProxyPort"] = *sandbox.network->socksProxyPort;
        if (!net.empty())
            j["network"] = net;
    }

    if (sandbox.ignoreViolations.has_value())
    {
        json ignore;
        if (sandbox.ignoreViolations->file.has_value())
            ignore["file"] = *sandbox.ignoreViolations->file;
        if (sandbox.ignoreViolations->network.has_value())
            ignore["network"] = *sandbox.ignoreViolations->network;
        if (!ignore.empty())
            j["ignoreViolations"] = ignore;
    }

    if (sandbox.enableWeakerNestedSandbox.has_value())
        j["enableWeakerNestedSandbox"] = *sandbox.enableWeakerNestedSandbox;

    return j;
}

// Test 1: Sandbox with only enabled=true (minimal config)
TEST(SandboxTest, MinimalSandboxEnabled)
{
    SandboxSettings sandbox;
    sandbox.enabled = true;

    auto j = sandbox_to_json(sandbox);

    EXPECT_TRUE(j.contains("enabled"));
    EXPECT_TRUE(j["enabled"].get<bool>());
    EXPECT_EQ(j.size(), 1u); // Only "enabled" key
}

// Test 2: Sandbox with autoAllowBashIfSandboxed
TEST(SandboxTest, SandboxWithAutoAllow)
{
    SandboxSettings sandbox;
    sandbox.enabled = true;
    sandbox.autoAllowBashIfSandboxed = true;

    auto j = sandbox_to_json(sandbox);

    EXPECT_TRUE(j["enabled"].get<bool>());
    EXPECT_TRUE(j["autoAllowBashIfSandboxed"].get<bool>());
}

// Test 3: Sandbox with excludedCommands
TEST(SandboxTest, SandboxWithExcludedCommands)
{
    SandboxSettings sandbox;
    sandbox.enabled = true;
    sandbox.excludedCommands = std::vector<std::string>{"git", "docker"};

    auto j = sandbox_to_json(sandbox);

    EXPECT_TRUE(j["enabled"].get<bool>());
    ASSERT_TRUE(j.contains("excludedCommands"));
    auto cmds = j["excludedCommands"].get<std::vector<std::string>>();
    EXPECT_EQ(cmds.size(), 2u);
    EXPECT_EQ(cmds[0], "git");
    EXPECT_EQ(cmds[1], "docker");
}

// Test 4: Sandbox with network configuration (matching Python test)
TEST(SandboxTest, SandboxWithNetworkConfig)
{
    SandboxSettings sandbox;
    sandbox.enabled = true;

    SandboxNetworkConfig network;
    network.allowLocalBinding = true;
    network.allowUnixSockets = std::vector<std::string>{"/var/run/docker.sock"};
    sandbox.network = network;

    auto j = sandbox_to_json(sandbox);

    EXPECT_TRUE(j["enabled"].get<bool>());
    ASSERT_TRUE(j.contains("network"));

    auto net = j["network"];
    EXPECT_TRUE(net["allowLocalBinding"].get<bool>());
    auto sockets = net["allowUnixSockets"].get<std::vector<std::string>>();
    EXPECT_EQ(sockets.size(), 1u);
    EXPECT_EQ(sockets[0], "/var/run/docker.sock");
}

// Test 5: Sandbox with full network configuration (matching Python test_sandbox_network_config)
TEST(SandboxTest, SandboxFullNetworkConfig)
{
    SandboxSettings sandbox;
    sandbox.enabled = true;

    SandboxNetworkConfig network;
    network.allowUnixSockets = std::vector<std::string>{"/tmp/ssh-agent.sock"};
    network.allowAllUnixSockets = false;
    network.allowLocalBinding = true;
    network.httpProxyPort = 8080;
    network.socksProxyPort = 8081;
    sandbox.network = network;

    auto j = sandbox_to_json(sandbox);

    auto net = j["network"];
    EXPECT_EQ(net["allowUnixSockets"].get<std::vector<std::string>>()[0], "/tmp/ssh-agent.sock");
    EXPECT_FALSE(net["allowAllUnixSockets"].get<bool>());
    EXPECT_TRUE(net["allowLocalBinding"].get<bool>());
    EXPECT_EQ(net["httpProxyPort"].get<int>(), 8080);
    EXPECT_EQ(net["socksProxyPort"].get<int>(), 8081);
}

// Test 6: Sandbox with ignoreViolations
TEST(SandboxTest, SandboxWithIgnoreViolations)
{
    SandboxSettings sandbox;
    sandbox.enabled = true;

    SandboxIgnoreViolations ignore;
    ignore.file = std::vector<std::string>{"/tmp/*", "/var/log/*"};
    ignore.network = std::vector<std::string>{"localhost", "127.0.0.1"};
    sandbox.ignoreViolations = ignore;

    auto j = sandbox_to_json(sandbox);

    ASSERT_TRUE(j.contains("ignoreViolations"));
    auto ignoreJ = j["ignoreViolations"];

    auto files = ignoreJ["file"].get<std::vector<std::string>>();
    EXPECT_EQ(files.size(), 2u);
    EXPECT_EQ(files[0], "/tmp/*");
    EXPECT_EQ(files[1], "/var/log/*");

    auto networks = ignoreJ["network"].get<std::vector<std::string>>();
    EXPECT_EQ(networks.size(), 2u);
    EXPECT_EQ(networks[0], "localhost");
    EXPECT_EQ(networks[1], "127.0.0.1");
}

// Test 7: Sandbox with enableWeakerNestedSandbox (Linux-only feature)
TEST(SandboxTest, SandboxWithWeakerNestedSandbox)
{
    SandboxSettings sandbox;
    sandbox.enabled = true;
    sandbox.enableWeakerNestedSandbox = true;

    auto j = sandbox_to_json(sandbox);

    EXPECT_TRUE(j["enabled"].get<bool>());
    EXPECT_TRUE(j["enableWeakerNestedSandbox"].get<bool>());
}

// Test 8: Sandbox can be added to ClaudeOptions
TEST(SandboxTest, SandboxInClaudeOptions)
{
    ClaudeOptions opts;
    opts.model = "claude-sonnet-4-20250514";

    SandboxSettings sandbox;
    sandbox.enabled = true;
    sandbox.autoAllowBashIfSandboxed = true;
    opts.sandbox = sandbox;

    // Verify sandbox is set
    ASSERT_TRUE(opts.sandbox.has_value());
    EXPECT_TRUE(opts.sandbox->enabled.value());
    EXPECT_TRUE(opts.sandbox->autoAllowBashIfSandboxed.value());
}

// Test 9: Empty sandbox produces empty JSON
TEST(SandboxTest, EmptySandboxProducesEmptyJson)
{
    SandboxSettings sandbox;
    // No fields set

    auto j = sandbox_to_json(sandbox);

    EXPECT_TRUE(j.empty());
}

// Test 10: Sandbox with allowUnsandboxedCommands
TEST(SandboxTest, SandboxWithAllowUnsandboxedCommands)
{
    SandboxSettings sandbox;
    sandbox.enabled = true;
    sandbox.allowUnsandboxedCommands = false; // Force all commands to be sandboxed

    auto j = sandbox_to_json(sandbox);

    EXPECT_TRUE(j["enabled"].get<bool>());
    EXPECT_FALSE(j["allowUnsandboxedCommands"].get<bool>());
}
