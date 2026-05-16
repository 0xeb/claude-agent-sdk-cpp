// Tests for Python parity v0.2.82 types (Phase 2A, tasks T1-T23).
// See kb/sync/review_result.md for task definitions.

#include <claude/sessions/session_store.hpp>
#include <claude/types.hpp>
#include <gtest/gtest.h>

using namespace claude;

// ============================================================================
// T1: PermissionMode values
// ============================================================================
TEST(TypesV0282, PermissionModeValues)
{
    EXPECT_STREQ(PermissionMode::Default, "default");
    EXPECT_STREQ(PermissionMode::AcceptEdits, "acceptEdits");
    EXPECT_STREQ(PermissionMode::Plan, "plan");
    EXPECT_STREQ(PermissionMode::BypassPermissions, "bypassPermissions");
    EXPECT_STREQ(PermissionMode::DontAsk, "dontAsk");
    EXPECT_STREQ(PermissionMode::Auto, "auto");
}

// ============================================================================
// T2: EffortLevel.xhigh
// ============================================================================
TEST(TypesV0282, EffortXHigh)
{
    EXPECT_STREQ(Effort::XHigh, "xhigh");
}

// ============================================================================
// T3: SystemPromptFile
// ============================================================================
TEST(TypesV0282, SystemPromptFileRoundTrip)
{
    SystemPromptFile f;
    f.path = "/etc/prompts/coding.md";
    auto j = f.to_json();
    EXPECT_EQ(j["type"], "file");
    EXPECT_EQ(j["path"], "/etc/prompts/coding.md");

    auto p = SystemPromptFile::from_json(j);
    EXPECT_EQ(p.type, "file");
    EXPECT_EQ(p.path, "/etc/prompts/coding.md");
}

// ============================================================================
// T4: SystemPromptPreset.exclude_dynamic_sections
// ============================================================================
TEST(TypesV0282, SystemPromptPresetExcludeDynamic)
{
    SystemPromptPreset p;
    p.append = "Always be brief.";
    p.exclude_dynamic_sections = true;
    auto j = p.to_json();
    EXPECT_EQ(j["type"], "preset");
    EXPECT_EQ(j["preset"], "claude_code");
    EXPECT_EQ(j["append"], "Always be brief.");
    EXPECT_EQ(j["exclude_dynamic_sections"], true);

    auto back = SystemPromptPreset::from_json(j);
    ASSERT_TRUE(back.append.has_value());
    EXPECT_EQ(*back.append, "Always be brief.");
    ASSERT_TRUE(back.exclude_dynamic_sections.has_value());
    EXPECT_TRUE(*back.exclude_dynamic_sections);
}

TEST(TypesV0282, SystemPromptPresetMinimal)
{
    SystemPromptPreset p;
    auto j = p.to_json();
    EXPECT_FALSE(j.contains("append"));
    EXPECT_FALSE(j.contains("exclude_dynamic_sections"));
}

// ============================================================================
// T5: TaskBudget
// ============================================================================
TEST(TypesV0282, TaskBudgetRoundTrip)
{
    TaskBudget b{50000};
    auto j = b.to_json();
    EXPECT_EQ(j["total"], 50000);
    auto back = TaskBudget::from_json(j);
    EXPECT_EQ(back.total, 50000);
}

// ============================================================================
// T6: AgentDefinition expansion (9 new fields)
// ============================================================================
TEST(TypesV0282, AgentDefinitionExpandedFields)
{
    AgentDefinition a;
    a.description = "Code reviewer agent";
    a.prompt = "Review code for bugs";
    a.disallowedTools = std::vector<std::string>{"Bash"};
    a.skills = std::vector<std::string>{"code-review"};
    a.memory = "project";
    a.mcpServers = std::vector<json>{json("my-server"), json{{"name", "x"}, {"config", "y"}}};
    a.initialPrompt = "Hello";
    a.maxTurns = 5;
    a.background = true;
    a.effort = AgentEffort{std::string{"xhigh"}};
    a.permissionMode = PermissionMode::AcceptEdits;

    ASSERT_TRUE(a.disallowedTools.has_value());
    EXPECT_EQ((*a.disallowedTools)[0], "Bash");
    ASSERT_TRUE(a.skills.has_value());
    EXPECT_EQ((*a.skills)[0], "code-review");
    EXPECT_EQ(*a.memory, "project");
    ASSERT_TRUE(a.mcpServers.has_value());
    EXPECT_EQ((*a.mcpServers).size(), 2u);
    EXPECT_EQ(*a.initialPrompt, "Hello");
    EXPECT_EQ(*a.maxTurns, 5);
    EXPECT_TRUE(*a.background);
    ASSERT_TRUE(a.effort.has_value());
    EXPECT_EQ(std::get<std::string>(*a.effort), "xhigh");
    EXPECT_EQ(*a.permissionMode, "acceptEdits");
}

TEST(TypesV0282, AgentEffortIntegerVariant)
{
    AgentDefinition a;
    a.description = "x";
    a.prompt = "y";
    a.effort = AgentEffort{16000};
    ASSERT_TRUE(a.effort.has_value());
    EXPECT_EQ(std::get<int>(*a.effort), 16000);
}

// ============================================================================
// T7: PermissionUpdate::from_json
// ============================================================================
TEST(TypesV0282, PermissionUpdateFromJsonRules)
{
    json j = {
        {"type", "addRules"},
        {"behavior", "allow"},
        {"destination", "session"},
        {"rules", json::array({json{{"toolName", "Bash"}, {"ruleContent", "echo *"}},
                               json{{"toolName", "Read"}, {"ruleContent", nullptr}}})}};
    auto u = PermissionUpdate::from_json(j);
    EXPECT_EQ(u.type, "addRules");
    EXPECT_EQ(*u.behavior, "allow");
    EXPECT_EQ(*u.destination, "session");
    ASSERT_TRUE(u.rules.has_value());
    EXPECT_EQ((*u.rules).size(), 2u);
    EXPECT_EQ((*u.rules)[0].tool_name, "Bash");
    ASSERT_TRUE((*u.rules)[0].rule_content.has_value());
    EXPECT_EQ(*(*u.rules)[0].rule_content, "echo *");
    EXPECT_FALSE((*u.rules)[1].rule_content.has_value());
}

TEST(TypesV0282, PermissionUpdateFromJsonSetMode)
{
    json j = {{"type", "setMode"}, {"mode", "plan"}};
    auto u = PermissionUpdate::from_json(j);
    EXPECT_EQ(u.type, "setMode");
    EXPECT_EQ(*u.mode, "plan");
}

TEST(TypesV0282, PermissionUpdateRoundTrip)
{
    PermissionUpdate u;
    u.type = "addDirectories";
    u.directories = std::vector<std::string>{"/tmp", "/var"};
    auto j = u.to_json();
    auto back = PermissionUpdate::from_json(j);
    EXPECT_EQ(back.type, "addDirectories");
    ASSERT_TRUE(back.directories.has_value());
    EXPECT_EQ((*back.directories).size(), 2u);
}

// ============================================================================
// T8: ToolPermissionContext expansion (7 new fields)
// ============================================================================
TEST(TypesV0282, ToolPermissionContextExpansion)
{
    ToolPermissionContext ctx;
    ctx.tool_use_id = "tu_42";
    ctx.agent_id = "agent_42";
    ctx.blocked_path = "/etc/passwd";
    ctx.decision_reason = "hook said ask";
    ctx.title = "Claude wants to read /etc/passwd";
    ctx.display_name = "Read file";
    ctx.description = "Read a sensitive file";

    EXPECT_EQ(*ctx.tool_use_id, "tu_42");
    EXPECT_EQ(*ctx.agent_id, "agent_42");
    EXPECT_EQ(*ctx.blocked_path, "/etc/passwd");
    EXPECT_EQ(*ctx.decision_reason, "hook said ask");
    EXPECT_EQ(*ctx.title, "Claude wants to read /etc/passwd");
    EXPECT_EQ(*ctx.display_name, "Read file");
    EXPECT_EQ(*ctx.description, "Read a sensitive file");
}

// ============================================================================
// T9: defer decision + updatedToolOutput
// ============================================================================
TEST(TypesV0282, PreToolUseDeferDecision)
{
    PreToolUseHookOutput out;
    out.permissionDecision = "defer";
    auto j = out.to_json();
    EXPECT_EQ(j["permissionDecision"], "defer");
}

TEST(TypesV0282, PostToolUseUpdatedToolOutput)
{
    PostToolUseHookOutput out;
    out.updatedToolOutput = json{{"stdout", "REDACTED"}};
    auto j = out.to_json();
    ASSERT_TRUE(j.contains("updatedToolOutput"));
    EXPECT_EQ(j["updatedToolOutput"]["stdout"], "REDACTED");
    EXPECT_FALSE(j.contains("updatedMCPToolOutput"));
}

// ============================================================================
// T10: _SubagentContextMixin fields on hook inputs
// ============================================================================
TEST(TypesV0282, PreToolUseAgentAttribution)
{
    json p = {{"session_id", "s"}, {"transcript_path", "t"}, {"cwd", "c"},
              {"hook_event_name", HookEvent::PreToolUse},
              {"tool_name", "Bash"}, {"tool_input", json::object()},
              {"tool_use_id", "tu1"}, {"agent_id", "a1"}, {"agent_type", "code-reviewer"}};
    auto i = PreToolUseHookInput::from_json(p);
    ASSERT_TRUE(i.agent_id.has_value());
    EXPECT_EQ(*i.agent_id, "a1");
    EXPECT_EQ(*i.agent_type, "code-reviewer");
}

TEST(TypesV0282, PostToolUseAgentAttribution)
{
    json p = {{"session_id", "s"}, {"transcript_path", "t"}, {"cwd", "c"},
              {"hook_event_name", HookEvent::PostToolUse},
              {"tool_name", "Bash"}, {"tool_input", json::object()},
              {"tool_response", "ok"}, {"tool_use_id", "tu1"},
              {"agent_id", "a1"}};
    auto i = PostToolUseHookInput::from_json(p);
    ASSERT_TRUE(i.agent_id.has_value());
    EXPECT_EQ(*i.agent_id, "a1");
}

TEST(TypesV0282, PostToolUseFailureAgentAttribution)
{
    json p = {{"session_id", "s"}, {"transcript_path", "t"}, {"cwd", "c"},
              {"hook_event_name", HookEvent::PostToolUseFailure},
              {"tool_name", "Bash"}, {"tool_input", json::object()},
              {"tool_use_id", "tu1"}, {"error", "boom"},
              {"agent_id", "a1"}, {"agent_type", "general"}};
    auto i = PostToolUseFailureHookInput::from_json(p);
    EXPECT_EQ(*i.agent_id, "a1");
    EXPECT_EQ(*i.agent_type, "general");
}

TEST(TypesV0282, PermissionRequestAgentAttribution)
{
    json p = {{"session_id", "s"}, {"transcript_path", "t"}, {"cwd", "c"},
              {"hook_event_name", HookEvent::PermissionRequest},
              {"tool_name", "Bash"}, {"tool_input", json::object()},
              {"agent_id", "a1"}};
    auto i = PermissionRequestHookInput::from_json(p);
    EXPECT_EQ(*i.agent_id, "a1");
}

// ============================================================================
// T12: ServerToolUseBlock + ServerToolResultBlock + ServerToolName
// ============================================================================
TEST(TypesV0282, ServerToolNameConstants)
{
    EXPECT_STREQ(ServerToolName::Advisor, "advisor");
    EXPECT_STREQ(ServerToolName::WebSearch, "web_search");
    EXPECT_STREQ(ServerToolName::WebFetch, "web_fetch");
    EXPECT_STREQ(ServerToolName::CodeExecution, "code_execution");
    EXPECT_STREQ(ServerToolName::BashCodeExecution, "bash_code_execution");
    EXPECT_STREQ(ServerToolName::TextEditorCodeExecution, "text_editor_code_execution");
    EXPECT_STREQ(ServerToolName::ToolSearchToolRegex, "tool_search_tool_regex");
    EXPECT_STREQ(ServerToolName::ToolSearchToolBm25, "tool_search_tool_bm25");
}

TEST(TypesV0282, ServerToolBlocksInContentVariant)
{
    ServerToolUseBlock stu;
    stu.id = "srv_1";
    stu.name = ServerToolName::WebSearch;
    stu.input = json{{"query", "claude code"}};

    ServerToolResultBlock str;
    str.tool_use_id = "srv_1";
    str.content = json{{"results", json::array()}};

    std::vector<ContentBlock> blocks;
    blocks.push_back(stu);
    blocks.push_back(str);
    EXPECT_TRUE(std::holds_alternative<ServerToolUseBlock>(blocks[0]));
    EXPECT_TRUE(std::holds_alternative<ServerToolResultBlock>(blocks[1]));
    EXPECT_EQ(std::get<ServerToolUseBlock>(blocks[0]).name, "web_search");
}

// ============================================================================
// T13: AssistantMessage extensions
// ============================================================================
TEST(TypesV0282, AssistantMessageExtensions)
{
    AssistantMessage m;
    m.model = "claude-opus-4-7";
    m.usage = json{{"input_tokens", 10}, {"output_tokens", 20}};
    m.message_id = "msg_01";
    m.stop_reason = "end_turn";
    m.session_id = "sess_1";
    m.uuid = "uuid_1";
    m.parent_tool_use_id = "ptu_1";

    EXPECT_EQ(*m.message_id, "msg_01");
    EXPECT_EQ(*m.stop_reason, "end_turn");
    EXPECT_EQ(*m.session_id, "sess_1");
    EXPECT_EQ(*m.uuid, "uuid_1");
    EXPECT_EQ(*m.parent_tool_use_id, "ptu_1");
    ASSERT_TRUE(m.usage.has_value());
    EXPECT_EQ((*m.usage)["input_tokens"], 10);
}

// ============================================================================
// T14: ResultMessage extensions
// ============================================================================
TEST(TypesV0282, ResultMessageExtensions)
{
    ResultMessage r;
    r.stop_reason = "max_turns";
    r.errors = std::vector<std::string>{"network blip"};
    r.api_error_status = 429;
    r.uuid = "result_uuid";

    DeferredToolUse d;
    d.id = "dtu_1";
    d.name = "Bash";
    d.input = json{{"cmd", "ls"}};
    r.deferred_tool_use = d;

    EXPECT_EQ(*r.stop_reason, "max_turns");
    ASSERT_TRUE(r.errors.has_value());
    EXPECT_EQ((*r.errors)[0], "network blip");
    EXPECT_EQ(*r.api_error_status, 429);
    EXPECT_EQ(*r.uuid, "result_uuid");
    ASSERT_TRUE(r.deferred_tool_use.has_value());
    EXPECT_EQ(r.deferred_tool_use->id, "dtu_1");
}

// ============================================================================
// T15: DeferredToolUse round-trip
// ============================================================================
TEST(TypesV0282, DeferredToolUseRoundTrip)
{
    DeferredToolUse d;
    d.id = "tu_1";
    d.name = "Read";
    d.input = json{{"path", "x.txt"}};
    auto j = d.to_json();
    EXPECT_EQ(j["id"], "tu_1");
    EXPECT_EQ(j["name"], "Read");
    EXPECT_EQ(j["input"]["path"], "x.txt");
    auto back = DeferredToolUse::from_json(j);
    EXPECT_EQ(back.id, "tu_1");
    EXPECT_EQ(back.name, "Read");
}

// ============================================================================
// T16: Task* messages
// ============================================================================
TEST(TypesV0282, TaskStartedMessageFields)
{
    TaskStartedMessage m;
    m.subtype = "task_started";
    m.task_id = "task-1";
    m.description = "do work";
    m.uuid = "u1";
    m.session_id = "s1";
    m.tool_use_id = "tu1";
    m.task_type = "general";
    EXPECT_EQ(m.task_id, "task-1");
    // Subclass relationship: TaskStartedMessage is-a SystemMessage
    SystemMessage* base = &m;
    EXPECT_EQ(base->subtype, "task_started");
}

TEST(TypesV0282, TaskUsageRoundTrip)
{
    TaskUsage u{1000, 5, 250};
    auto j = u.to_json();
    EXPECT_EQ(j["total_tokens"], 1000);
    EXPECT_EQ(j["tool_uses"], 5);
    EXPECT_EQ(j["duration_ms"], 250);
    auto back = TaskUsage::from_json(j);
    EXPECT_EQ(back.total_tokens, 1000);
}

TEST(TypesV0282, TaskNotificationStatusConstants)
{
    EXPECT_STREQ(TaskNotificationStatus::Completed, "completed");
    EXPECT_STREQ(TaskNotificationStatus::Failed, "failed");
    EXPECT_STREQ(TaskNotificationStatus::Stopped, "stopped");
}

TEST(TypesV0282, TaskProgressAndNotification)
{
    TaskProgressMessage p;
    p.subtype = "task_progress";
    p.task_id = "t1";
    p.usage = TaskUsage{100, 2, 50};
    p.uuid = "u";
    p.session_id = "s";
    p.last_tool_name = "Read";
    EXPECT_EQ(p.usage.total_tokens, 100);

    TaskNotificationMessage n;
    n.subtype = "task_notification";
    n.task_id = "t1";
    n.status = TaskNotificationStatus::Completed;
    n.output_file = "out.txt";
    n.summary = "done";
    n.uuid = "u";
    n.session_id = "s";
    n.usage = TaskUsage{200, 4, 100};
    EXPECT_EQ(n.status, "completed");
    ASSERT_TRUE(n.usage.has_value());
    EXPECT_EQ(n.usage->total_tokens, 200);
}

// ============================================================================
// T17: MirrorErrorMessage
// ============================================================================
TEST(TypesV0282, MirrorErrorMessageFields)
{
    MirrorErrorMessage m;
    m.subtype = "mirror_error";
    SessionKey k;
    k.project_key = "proj";
    k.session_id = "sid";
    m.key = k;
    m.error = "S3 timeout";
    EXPECT_EQ(m.error, "S3 timeout");
    ASSERT_TRUE(m.key.has_value());
    EXPECT_EQ(m.key->project_key, "proj");
}

TEST(TypesV0282, SessionKeyRoundTrip)
{
    SessionKey k;
    k.project_key = "proj";
    k.session_id = "sid";
    k.subpath = "subagents/agent-1";
    auto j = k.to_json();
    EXPECT_EQ(j["project_key"], "proj");
    EXPECT_EQ(j["subpath"], "subagents/agent-1");
    auto back = SessionKey::from_json(j);
    EXPECT_EQ(back.session_id, "sid");
    ASSERT_TRUE(back.subpath.has_value());
}

// ============================================================================
// T18: RateLimit types + Message variant
// ============================================================================
TEST(TypesV0282, RateLimitStatusAndTypeConstants)
{
    EXPECT_STREQ(RateLimitStatus::Allowed, "allowed");
    EXPECT_STREQ(RateLimitStatus::AllowedWarning, "allowed_warning");
    EXPECT_STREQ(RateLimitStatus::Rejected, "rejected");
    EXPECT_STREQ(RateLimitType::FiveHour, "five_hour");
    EXPECT_STREQ(RateLimitType::SevenDay, "seven_day");
    EXPECT_STREQ(RateLimitType::SevenDayOpus, "seven_day_opus");
    EXPECT_STREQ(RateLimitType::SevenDaySonnet, "seven_day_sonnet");
    EXPECT_STREQ(RateLimitType::Overage, "overage");
}

TEST(TypesV0282, RateLimitInfoRoundTrip)
{
    RateLimitInfo i;
    i.status = RateLimitStatus::AllowedWarning;
    i.resets_at = 1700000000;
    i.rate_limit_type = RateLimitType::FiveHour;
    i.utilization = 0.85;
    auto j = i.to_json();
    EXPECT_EQ(j["status"], "allowed_warning");
    auto back = RateLimitInfo::from_json(j);
    EXPECT_EQ(back.status, "allowed_warning");
    EXPECT_EQ(*back.resets_at, 1700000000);
    EXPECT_DOUBLE_EQ(*back.utilization, 0.85);
}

TEST(TypesV0282, RateLimitEventInMessageVariant)
{
    RateLimitEvent e;
    e.uuid = "u";
    e.session_id = "s";
    e.rate_limit_info.status = RateLimitStatus::Allowed;
    Message m = e;
    EXPECT_TRUE(std::holds_alternative<RateLimitEvent>(m));
}

// ============================================================================
// T19: HookEventMessage
// ============================================================================
TEST(TypesV0282, HookEventMessageFields)
{
    HookEventMessage m;
    m.subtype = "hook_started";
    m.hook_event_name = "PreToolUse";
    m.session_id = "s1";
    m.uuid = "u1";
    EXPECT_EQ(m.hook_event_name, "PreToolUse");
    // Subclass: HookEventMessage is-a SystemMessage
    SystemMessage* base = &m;
    EXPECT_EQ(base->subtype, "hook_started");
}

// ============================================================================
// T20: MCP status types
// ============================================================================
TEST(TypesV0282, McpServerConnectionStatusConstants)
{
    EXPECT_STREQ(McpServerConnectionStatus::Connected, "connected");
    EXPECT_STREQ(McpServerConnectionStatus::Failed, "failed");
    EXPECT_STREQ(McpServerConnectionStatus::NeedsAuth, "needs-auth");
    EXPECT_STREQ(McpServerConnectionStatus::Pending, "pending");
    EXPECT_STREQ(McpServerConnectionStatus::Disabled, "disabled");
}

TEST(TypesV0282, McpToolInfoRoundTrip)
{
    McpToolInfo t;
    t.name = "search";
    t.description = "Search docs";
    McpToolAnnotations a;
    a.readOnly = true;
    a.openWorld = false;
    t.annotations = a;

    auto j = t.to_json();
    EXPECT_EQ(j["name"], "search");
    EXPECT_EQ(j["annotations"]["readOnly"], true);
    EXPECT_EQ(j["annotations"]["openWorld"], false);

    auto back = McpToolInfo::from_json(j);
    EXPECT_EQ(back.name, "search");
    ASSERT_TRUE(back.annotations.has_value());
    EXPECT_TRUE(*back.annotations->readOnly);
}

TEST(TypesV0282, McpServerStatusRoundTrip)
{
    McpServerStatus s;
    s.name = "fs";
    s.status = McpServerConnectionStatus::Connected;
    s.serverInfo = McpServerInfo{"fs-server", "1.2.3"};
    McpToolInfo t;
    t.name = "ls";
    s.tools = std::vector<McpToolInfo>{t};
    s.scope = "project";

    auto j = s.to_json();
    EXPECT_EQ(j["name"], "fs");
    EXPECT_EQ(j["status"], "connected");
    EXPECT_EQ(j["serverInfo"]["version"], "1.2.3");
    EXPECT_EQ(j["tools"][0]["name"], "ls");

    auto back = McpServerStatus::from_json(j);
    EXPECT_EQ(back.name, "fs");
    ASSERT_TRUE(back.serverInfo.has_value());
    EXPECT_EQ(back.serverInfo->version, "1.2.3");
    ASSERT_TRUE(back.tools.has_value());
    EXPECT_EQ((*back.tools)[0].name, "ls");
}

TEST(TypesV0282, McpStatusResponseRoundTrip)
{
    McpStatusResponse r;
    McpServerStatus s;
    s.name = "fs";
    s.status = "connected";
    r.mcpServers.push_back(s);
    auto j = r.to_json();
    EXPECT_EQ(j["mcpServers"][0]["name"], "fs");
    auto back = McpStatusResponse::from_json(j);
    EXPECT_EQ(back.mcpServers.size(), 1u);
}

TEST(TypesV0282, McpSdkServerConfigStatusAndProxy)
{
    McpSdkServerConfigStatus sdk;
    sdk.name = "x";
    auto j = sdk.to_json();
    EXPECT_EQ(j["type"], "sdk");
    EXPECT_EQ(j["name"], "x");

    McpClaudeAIProxyServerConfig p;
    p.url = "https://claude.ai";
    p.id = "proxy-1";
    auto jp = p.to_json();
    EXPECT_EQ(jp["type"], "claudeai-proxy");
    EXPECT_EQ(jp["url"], "https://claude.ai");
    EXPECT_EQ(jp["id"], "proxy-1");
}

// ============================================================================
// T21: ContextUsage*
// ============================================================================
TEST(TypesV0282, ContextUsageCategoryAndResponse)
{
    json j = {
        {"categories", json::array({json{{"name", "system"}, {"tokens", 100}, {"color", "red"}}})},
        {"totalTokens", 5000},
        {"maxTokens", 200000},
        {"rawMaxTokens", 200000},
        {"percentage", 2.5},
        {"model", "claude-opus-4-7"},
        {"isAutoCompactEnabled", true},
        {"memoryFiles", json::array()},
        {"mcpTools", json::array()},
        {"agents", json::array()},
        {"gridRows", json::array()},
        {"autoCompactThreshold", 150000}};
    auto r = ContextUsageResponse::from_json(j);
    EXPECT_EQ(r.totalTokens, 5000);
    EXPECT_EQ(r.maxTokens, 200000);
    EXPECT_DOUBLE_EQ(r.percentage, 2.5);
    EXPECT_EQ(r.model, "claude-opus-4-7");
    EXPECT_TRUE(r.isAutoCompactEnabled);
    ASSERT_EQ(r.categories.size(), 1u);
    EXPECT_EQ(r.categories[0].name, "system");
    EXPECT_EQ(r.categories[0].tokens, 100);
    ASSERT_TRUE(r.autoCompactThreshold.has_value());
    EXPECT_EQ(*r.autoCompactThreshold, 150000);
}

// ============================================================================
// T22: SandboxNetworkConfig additions
// ============================================================================
TEST(TypesV0282, SandboxNetworkConfigDomainFields)
{
    SandboxNetworkConfig nc;
    nc.allowedDomains = std::vector<std::string>{"github.com", "anthropic.com"};
    nc.deniedDomains = std::vector<std::string>{"example.com"};
    nc.allowManagedDomainsOnly = true;
    nc.allowMachLookup = std::vector<std::string>{"com.apple.*"};
    nc.allowLocalBinding = true;

    ASSERT_TRUE(nc.allowedDomains.has_value());
    EXPECT_EQ((*nc.allowedDomains).size(), 2u);
    ASSERT_TRUE(nc.deniedDomains.has_value());
    EXPECT_EQ((*nc.deniedDomains)[0], "example.com");
    EXPECT_TRUE(*nc.allowManagedDomainsOnly);
    ASSERT_TRUE(nc.allowMachLookup.has_value());
    EXPECT_EQ((*nc.allowMachLookup)[0], "com.apple.*");
}

// ============================================================================
// T23: ClaudeOptions new fields
// ============================================================================
TEST(TypesV0282, ClaudeOptionsNewFields)
{
    ClaudeOptions opts;
    opts.session_id = "11111111-1111-1111-1111-111111111111";
    opts.task_budget = TaskBudget{100000};
    opts.skills = std::vector<std::string>{"code-review"};
    opts.strict_mcp_config = true;
    opts.include_hook_events = true;
    opts.session_store_flush = SessionStoreFlushMode::Eager;
    opts.system_prompt_file = SystemPromptFile{"file", "/tmp/prompt.md"};
    opts.system_prompt_preset = SystemPromptPreset{};

    EXPECT_EQ(*opts.session_id, "11111111-1111-1111-1111-111111111111");
    ASSERT_TRUE(opts.task_budget.has_value());
    EXPECT_EQ(opts.task_budget->total, 100000);
    ASSERT_TRUE(opts.skills.has_value());
    EXPECT_TRUE(std::holds_alternative<std::vector<std::string>>(*opts.skills));
    EXPECT_TRUE(opts.strict_mcp_config);
    EXPECT_TRUE(opts.include_hook_events);
    EXPECT_EQ(opts.session_store_flush, "eager");
    EXPECT_EQ(opts.system_prompt_file->path, "/tmp/prompt.md");
    EXPECT_TRUE(opts.system_prompt_preset.has_value());
}

TEST(TypesV0282, ClaudeOptionsSkillsAllVariant)
{
    ClaudeOptions opts;
    opts.skills = std::string{"all"};
    ASSERT_TRUE(opts.skills.has_value());
    EXPECT_TRUE(std::holds_alternative<std::string>(*opts.skills));
    EXPECT_EQ(std::get<std::string>(*opts.skills), "all");
}

TEST(TypesV0282, SessionStoreFlushModeConstants)
{
    EXPECT_STREQ(SessionStoreFlushMode::Batched, "batched");
    EXPECT_STREQ(SessionStoreFlushMode::Eager, "eager");
}

// ============================================================================
// SessionStore abstract interface (T23 stub)
// ============================================================================
namespace
{
class StubStore : public SessionStore
{
  public:
    void append(const SessionKey&, const std::vector<SessionStoreEntry>&) override
    {
        ++appends;
    }
    std::optional<std::vector<SessionStoreEntry>> load(const SessionKey&) override
    {
        return std::nullopt;
    }
    int appends = 0;
};
} // namespace

TEST(TypesV0282, SessionStoreInterfaceUsable)
{
    auto store = std::make_shared<StubStore>();
    SessionKey k;
    k.project_key = "p";
    k.session_id = "s";
    store->append(k, std::vector<SessionStoreEntry>{json{{"type", "user"}}});
    EXPECT_EQ(store->appends, 1);
    EXPECT_FALSE(store->load(k).has_value());

    EXPECT_THROW(store->list_sessions("p"), std::logic_error);
    EXPECT_THROW(store->delete_session(k), std::logic_error);

    ClaudeOptions opts;
    opts.session_store = store;
    EXPECT_NE(opts.session_store, nullptr);
}
