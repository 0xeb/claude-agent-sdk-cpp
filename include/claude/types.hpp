#ifndef CLAUDE_TYPES_HPP
#define CLAUDE_TYPES_HPP

#include <functional>
#include <map>
#include <memory>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <variant>
#include <vector>

// Include control protocol types (needed for std::variant)
#include <claude/protocol/control.hpp>

namespace claude
{

// JSON type alias - allows swapping implementation later if needed
using json = nlohmann::json;

// Optional MCP in-process handler type. This avoids a hard dependency on
// fastmcpp while allowing callers to route SDK MCP messages.
using McpRequestHandler = std::function<json(const json&)>;

// ============================================================================
// Permission Mode Constants (matches Python SDK v0.2.82)
// ============================================================================

/// Permission mode values. Python: PermissionMode literal
/// Added "dontAsk" (commit e30c742) and "auto" (commit 841ee87).
namespace PermissionMode
{
constexpr const char* Default = "default";
constexpr const char* AcceptEdits = "acceptEdits";
constexpr const char* Plan = "plan";
constexpr const char* BypassPermissions = "bypassPermissions";
constexpr const char* DontAsk = "dontAsk";
constexpr const char* Auto = "auto";
} // namespace PermissionMode

// ============================================================================
// Agent and System Prompt Types (matches Python SDK)
// ============================================================================

/// System prompt preset (TypedDict in Python).
/// Python: SystemPromptPreset (commit 3bf8fd5 added exclude_dynamic_sections).
struct SystemPromptPreset
{
    std::string type = "preset";
    std::string preset = "claude_code";
    std::optional<std::string> append = std::nullopt;
    /// Strip per-user dynamic sections from the system prompt for cross-user
    /// prompt caching. (Python commit 3bf8fd5.)
    std::optional<bool> exclude_dynamic_sections = std::nullopt;

    json to_json() const
    {
        json out = {{"type", type}, {"preset", preset}};
        if (append.has_value())
            out["append"] = *append;
        if (exclude_dynamic_sections.has_value())
            out["exclude_dynamic_sections"] = *exclude_dynamic_sections;
        return out;
    }

    static SystemPromptPreset from_json(const json& j)
    {
        SystemPromptPreset p;
        p.type = j.value("type", "preset");
        p.preset = j.value("preset", "claude_code");
        if (j.contains("append") && !j.at("append").is_null())
            p.append = j.at("append").get<std::string>();
        if (j.contains("exclude_dynamic_sections") && !j.at("exclude_dynamic_sections").is_null())
            p.exclude_dynamic_sections = j.at("exclude_dynamic_sections").get<bool>();
        return p;
    }
};

/// System prompt loaded from a file. Python: SystemPromptFile (commit 139b815).
struct SystemPromptFile
{
    std::string type = "file";
    std::string path;

    json to_json() const
    {
        return json{{"type", type}, {"path", path}};
    }

    static SystemPromptFile from_json(const json& j)
    {
        SystemPromptFile f;
        f.type = j.value("type", "file");
        f.path = j.value("path", "");
        return f;
    }
};

/// API-side task budget in tokens. Python: TaskBudget (commit 2e60cec).
/// Sent as output_config.task_budget with the task-budgets-2026-03-13 beta header.
struct TaskBudget
{
    int total = 0;

    json to_json() const
    {
        return json{{"total", total}};
    }

    static TaskBudget from_json(const json& j)
    {
        TaskBudget b;
        b.total = j.value("total", 0);
        return b;
    }
};

/// Effort level alternatives: enum string or integer.
/// Python: EffortLevel | int | None on AgentDefinition.effort (commit 7c6902b).
using AgentEffort = std::variant<std::string, int>;

/// Agent definition for custom agents.
/// Python: AgentDefinition (commits 028d591, fad1b84, 7c6902b expanded fields).
struct AgentDefinition
{
    std::string description;                                      // Required
    std::string prompt;                                           // Required
    std::optional<std::vector<std::string>> tools = std::nullopt; // Optional tools list
    std::optional<std::string> model = std::nullopt; // Optional: e.g., "claude-sonnet-4-5",
                                                     // "claude-opus-4", "claude-haiku-4", "inherit"

    // ----- Added for Python parity v0.2.82 -----
    /// Tools the model is prevented from using (commit 028d591).
    std::optional<std::vector<std::string>> disallowedTools = std::nullopt;
    /// Skills allowed in this agent (commit fad1b84).
    std::optional<std::vector<std::string>> skills = std::nullopt;
    /// Memory scope: "user", "project", or "local" (commit fad1b84).
    std::optional<std::string> memory = std::nullopt;
    /// MCP servers; each entry is a server name string or inline {name: config} object.
    /// Python: list[str | dict[str, Any]]. (Commit fad1b84.)
    std::optional<std::vector<json>> mcpServers = std::nullopt;
    /// Initial user prompt to seed the agent (commit 028d591).
    std::optional<std::string> initialPrompt = std::nullopt;
    /// Maximum conversation turns (commit 028d591).
    std::optional<int> maxTurns = std::nullopt;
    /// Run the agent in background mode (commit 7c6902b).
    std::optional<bool> background = std::nullopt;
    /// Effort level or integer budget (commit 7c6902b).
    std::optional<AgentEffort> effort = std::nullopt;
    /// Permission mode override for this agent (commit 7c6902b).
    std::optional<std::string> permissionMode = std::nullopt;
};

/// Plugin configuration for Claude Code plugins (matches Python SDK v0.1.5)
struct SdkPluginConfig
{
    std::string type; // Currently only "local" is supported
    std::string path; // Path to plugin directory

    /// Convert to JSON format
    json to_json() const
    {
        return json{{"type", type}, {"path", path}};
    }

    /// Create from JSON
    static SdkPluginConfig from_json(const json& j)
    {
        return SdkPluginConfig{j.at("type").get<std::string>(), j.at("path").get<std::string>()};
    }
};

// ============================================================================
// Permission Update Types (matches Python SDK)
// ============================================================================

/// Permission update destination options
namespace PermissionUpdateDestination
{
constexpr const char* UserSettings = "userSettings";
constexpr const char* ProjectSettings = "projectSettings";
constexpr const char* LocalSettings = "localSettings";
constexpr const char* Session = "session";
} // namespace PermissionUpdateDestination

/// Permission behavior options
namespace PermissionBehavior
{
constexpr const char* Allow = "allow";
constexpr const char* Deny = "deny";
constexpr const char* Ask = "ask";
} // namespace PermissionBehavior

/// Permission rule value
struct PermissionRuleValue
{
    std::string tool_name;                                  // Required
    std::optional<std::string> rule_content = std::nullopt; // Optional
};

/// Permission update configuration
struct PermissionUpdate
{
    std::string type; // "addRules", "replaceRules", "removeRules", "setMode", "addDirectories",
                      // "removeDirectories"
    std::optional<std::vector<PermissionRuleValue>> rules = std::nullopt;
    std::optional<std::string> behavior = std::nullopt; // PermissionBehavior value
    std::optional<std::string> mode = std::nullopt;     // PermissionMode value
    std::optional<std::vector<std::string>> directories = std::nullopt;
    std::optional<std::string> destination = std::nullopt; // PermissionUpdateDestination value

    /// Convert to JSON format matching TypeScript control protocol
    json to_json() const
    {
        json result = {{"type", type}};

        if (destination.has_value())
            result["destination"] = *destination;

        // Handle different type variants
        if (type == "addRules" || type == "replaceRules" || type == "removeRules")
        {
            if (rules.has_value())
            {
                json rules_array = json::array();
                for (const auto& rule : *rules)
                {
                    json rule_obj = {{"toolName", rule.tool_name}};

                    // Safely handle optional rule_content (null if not present)
                    if (rule.rule_content.has_value())
                        rule_obj["ruleContent"] = *rule.rule_content;
                    else
                        rule_obj["ruleContent"] = nullptr;

                    rules_array.push_back(rule_obj);
                }
                result["rules"] = rules_array;
            }
            if (behavior.has_value())
                result["behavior"] = *behavior;
        }
        else if (type == "setMode")
        {
            if (mode.has_value())
                result["mode"] = *mode;
        }
        else if (type == "addDirectories" || type == "removeDirectories")
        {
            if (directories.has_value())
                result["directories"] = *directories;
        }

        return result;
    }

    /// Construct from control-protocol dict format (inverse of to_json).
    /// Python: PermissionUpdate.from_dict (commit 6597529).
    static PermissionUpdate from_json(const json& j)
    {
        PermissionUpdate u;
        u.type = j.value("type", "");
        if (j.contains("rules") && j.at("rules").is_array())
        {
            std::vector<PermissionRuleValue> rules;
            for (const auto& r : j.at("rules"))
            {
                PermissionRuleValue rv;
                rv.tool_name = r.value("toolName", "");
                if (r.contains("ruleContent") && !r.at("ruleContent").is_null())
                    rv.rule_content = r.at("ruleContent").get<std::string>();
                rules.push_back(rv);
            }
            u.rules = std::move(rules);
        }
        if (j.contains("behavior") && !j.at("behavior").is_null())
            u.behavior = j.at("behavior").get<std::string>();
        if (j.contains("mode") && !j.at("mode").is_null())
            u.mode = j.at("mode").get<std::string>();
        if (j.contains("directories") && j.at("directories").is_array())
            u.directories = j.at("directories").get<std::vector<std::string>>();
        if (j.contains("destination") && !j.at("destination").is_null())
            u.destination = j.at("destination").get<std::string>();
        return u;
    }
};

// ============================================================================
// Tool Permission Context and Result Types (matches Python SDK)
// ============================================================================

/// Context information for tool permission callbacks.
/// Python: ToolPermissionContext (commits 3caf665, fe0cff3 added the
/// tool_use_id / agent_id / display fields).
struct ToolPermissionContext
{
    std::vector<PermissionUpdate> suggestions; // Permission suggestions from CLI
    /// Unique identifier for this tool call (commit fe0cff3).
    std::optional<std::string> tool_use_id = std::nullopt;
    /// Sub-agent identifier if running inside a sub-agent (commit fe0cff3).
    std::optional<std::string> agent_id = std::nullopt;
    /// File path that triggered the permission request (commit 3caf665).
    std::optional<std::string> blocked_path = std::nullopt;
    /// Why this permission request was triggered (commit 3caf665).
    std::optional<std::string> decision_reason = std::nullopt;
    /// Full permission prompt sentence (commit 3caf665).
    std::optional<std::string> title = std::nullopt;
    /// Short noun phrase for the tool action (commit 3caf665).
    std::optional<std::string> display_name = std::nullopt;
    /// Human-readable subtitle for the permission UI (commit 3caf665).
    std::optional<std::string> description = std::nullopt;
};

/// Permission result: Allow
struct PermissionResultAllow
{
    std::string behavior = "allow";
    std::optional<json> updated_input = std::nullopt;
    std::optional<std::vector<PermissionUpdate>> updated_permissions = std::nullopt;
};

/// Permission result: Deny
struct PermissionResultDeny
{
    std::string behavior = "deny";
    std::string message = "";
    bool interrupt = false;
};

/// Permission result variant (Allow or Deny)
using PermissionResult = std::variant<PermissionResultAllow, PermissionResultDeny>;

// ============================================================================
// Hook Event Types (matches Python SDK)
// ============================================================================

/// Supported hook event types (matches Python SDK v0.1.35)
namespace HookEvent
{
constexpr const char* PreToolUse = "PreToolUse";
constexpr const char* PostToolUse = "PostToolUse";
constexpr const char* PostToolUseFailure = "PostToolUseFailure";
constexpr const char* UserPromptSubmit = "UserPromptSubmit";
constexpr const char* Stop = "Stop";
constexpr const char* SubagentStop = "SubagentStop";
constexpr const char* PreCompact = "PreCompact";
constexpr const char* Notification = "Notification";
constexpr const char* SubagentStart = "SubagentStart";
constexpr const char* PermissionRequest = "PermissionRequest";
} // namespace HookEvent

// ============================================================================
// Callback Function Types (matches Python SDK)
// ============================================================================

/// Callback invoked when a registered hook is triggered.
/// Receives hook input data and tool_use_id (may be null).
/// @param input Hook-specific input data (strongly typed based on hook event)
/// @param tool_use_id Optional tool use identifier (for PreToolUse/PostToolUse hooks)
/// @return JSON output following HookJSONOutput schema (see Claude Code docs)
using HookCallback = std::function<json(const json& input, const std::string& tool_use_id)>;

/// Callback invoked when tool permission is requested.
/// @param tool_name Tool name (e.g., "Read", "Write", "Bash")
/// @param input Tool-specific arguments
/// @param context Permission context with suggestions from CLI
/// @return PermissionResult (allow with optional updates, or deny with optional message)
using ToolPermissionCallback = std::function<PermissionResult(
    const std::string& tool_name, const json& input, const ToolPermissionContext& context)>;

/// Callback invoked when the CLI process writes to stderr.
/// Useful for debugging and observing CLI warnings/errors.
/// @param line Single line of stderr output (without trailing newline)
using StderrCallback = std::function<void(const std::string& line)>;

// --------------------------------------------------------------------------
// Hook convenience helpers (new in Python 0.1.26)
// --------------------------------------------------------------------------

/// Input data for PreToolUse hook callbacks.
/// Mirrors `PreToolUseHookInput` from Python SDK.
struct PreToolUseHookInput
{
    std::string session_id;
    std::string transcript_path;
    std::string cwd;
    std::optional<std::string> permission_mode = std::nullopt;
    std::string hook_event_name = HookEvent::PreToolUse;
    std::string tool_name;
    json tool_input;
    std::string tool_use_id;
    /// Sub-agent attribution (Python _SubagentContextMixin, commit 2f1fd38).
    std::optional<std::string> agent_id = std::nullopt;
    std::optional<std::string> agent_type = std::nullopt;

    static PreToolUseHookInput from_json(const json& j)
    {
        PreToolUseHookInput input;
        input.session_id = j.value("session_id", "");
        input.transcript_path = j.value("transcript_path", "");
        input.cwd = j.value("cwd", "");
        if (j.contains("permission_mode") && !j.at("permission_mode").is_null())
            input.permission_mode = j.at("permission_mode").get<std::string>();
        input.hook_event_name = j.value("hook_event_name", HookEvent::PreToolUse);
        input.tool_name = j.value("tool_name", "");
        input.tool_input = j.value("tool_input", json::object());
        input.tool_use_id = j.value("tool_use_id", "");
        if (j.contains("agent_id") && !j.at("agent_id").is_null())
            input.agent_id = j.at("agent_id").get<std::string>();
        if (j.contains("agent_type") && !j.at("agent_type").is_null())
            input.agent_type = j.at("agent_type").get<std::string>();
        return input;
    }
};

/// Input data for PostToolUse hook callbacks.
/// Mirrors `PostToolUseHookInput` from Python SDK.
struct PostToolUseHookInput
{
    std::string session_id;
    std::string transcript_path;
    std::string cwd;
    std::optional<std::string> permission_mode = std::nullopt;
    std::string hook_event_name = HookEvent::PostToolUse;
    std::string tool_name;
    json tool_input;
    json tool_response;
    std::string tool_use_id;
    /// Sub-agent attribution (Python _SubagentContextMixin, commit 2f1fd38).
    std::optional<std::string> agent_id = std::nullopt;
    std::optional<std::string> agent_type = std::nullopt;

    static PostToolUseHookInput from_json(const json& j)
    {
        PostToolUseHookInput input;
        input.session_id = j.value("session_id", "");
        input.transcript_path = j.value("transcript_path", "");
        input.cwd = j.value("cwd", "");
        if (j.contains("permission_mode") && !j.at("permission_mode").is_null())
            input.permission_mode = j.at("permission_mode").get<std::string>();
        input.hook_event_name = j.value("hook_event_name", HookEvent::PostToolUse);
        input.tool_name = j.value("tool_name", "");
        input.tool_input = j.value("tool_input", json::object());
        input.tool_response = j.value("tool_response", json());
        input.tool_use_id = j.value("tool_use_id", "");
        if (j.contains("agent_id") && !j.at("agent_id").is_null())
            input.agent_id = j.at("agent_id").get<std::string>();
        if (j.contains("agent_type") && !j.at("agent_type").is_null())
            input.agent_type = j.at("agent_type").get<std::string>();
        return input;
    }
};

/// Input data for PostToolUseFailure hook callbacks.
/// Mirrors `PostToolUseFailureHookInput` from Python SDK.
struct PostToolUseFailureHookInput
{
    std::string session_id;
    std::string transcript_path;
    std::string cwd;
    std::optional<std::string> permission_mode = std::nullopt;
    std::string hook_event_name = HookEvent::PostToolUseFailure;
    std::string tool_name;
    json tool_input;
    std::string tool_use_id;
    std::string error;
    std::optional<bool> is_interrupt = std::nullopt;
    /// Sub-agent attribution (Python _SubagentContextMixin, commit 2f1fd38).
    std::optional<std::string> agent_id = std::nullopt;
    std::optional<std::string> agent_type = std::nullopt;

    static PostToolUseFailureHookInput from_json(const json& j)
    {
        PostToolUseFailureHookInput input;
        input.session_id = j.value("session_id", "");
        input.transcript_path = j.value("transcript_path", "");
        input.cwd = j.value("cwd", "");
        if (j.contains("permission_mode") && !j.at("permission_mode").is_null())
            input.permission_mode = j.at("permission_mode").get<std::string>();
        input.hook_event_name = j.value("hook_event_name", HookEvent::PostToolUseFailure);
        input.tool_name = j.value("tool_name", "");
        input.tool_input = j.value("tool_input", json::object());
        input.tool_use_id = j.value("tool_use_id", "");
        input.error = j.value("error", "");
        if (j.contains("is_interrupt") && !j.at("is_interrupt").is_null())
            input.is_interrupt = j.at("is_interrupt").get<bool>();
        if (j.contains("agent_id") && !j.at("agent_id").is_null())
            input.agent_id = j.at("agent_id").get<std::string>();
        if (j.contains("agent_type") && !j.at("agent_type").is_null())
            input.agent_type = j.at("agent_type").get<std::string>();
        return input;
    }
};

/// Input data for Notification hook callbacks.
/// Mirrors `NotificationHookInput` from Python SDK.
struct NotificationHookInput
{
    std::string session_id;
    std::string transcript_path;
    std::string cwd;
    std::optional<std::string> permission_mode = std::nullopt;
    std::string hook_event_name = HookEvent::Notification;
    std::string message;
    std::optional<std::string> title = std::nullopt;
    std::string notification_type;

    static NotificationHookInput from_json(const json& j)
    {
        NotificationHookInput input;
        input.session_id = j.value("session_id", "");
        input.transcript_path = j.value("transcript_path", "");
        input.cwd = j.value("cwd", "");
        if (j.contains("permission_mode") && !j.at("permission_mode").is_null())
            input.permission_mode = j.at("permission_mode").get<std::string>();
        input.hook_event_name = j.value("hook_event_name", HookEvent::Notification);
        input.message = j.value("message", "");
        if (j.contains("title") && !j.at("title").is_null())
            input.title = j.at("title").get<std::string>();
        input.notification_type = j.value("notification_type", "");
        return input;
    }
};

/// Input data for SubagentStart hook callbacks.
/// Mirrors `SubagentStartHookInput` from Python SDK.
struct SubagentStartHookInput
{
    std::string session_id;
    std::string transcript_path;
    std::string cwd;
    std::optional<std::string> permission_mode = std::nullopt;
    std::string hook_event_name = HookEvent::SubagentStart;
    std::string agent_id;
    std::string agent_type;

    static SubagentStartHookInput from_json(const json& j)
    {
        SubagentStartHookInput input;
        input.session_id = j.value("session_id", "");
        input.transcript_path = j.value("transcript_path", "");
        input.cwd = j.value("cwd", "");
        if (j.contains("permission_mode") && !j.at("permission_mode").is_null())
            input.permission_mode = j.at("permission_mode").get<std::string>();
        input.hook_event_name = j.value("hook_event_name", HookEvent::SubagentStart);
        input.agent_id = j.value("agent_id", "");
        input.agent_type = j.value("agent_type", "");
        return input;
    }
};

/// Input data for SubagentStop hook callbacks.
/// Mirrors `SubagentStopHookInput` from Python SDK.
struct SubagentStopHookInput
{
    std::string session_id;
    std::string transcript_path;
    std::string cwd;
    std::optional<std::string> permission_mode = std::nullopt;
    std::string hook_event_name = HookEvent::SubagentStop;
    bool stop_hook_active = false;
    std::string agent_id;
    std::string agent_transcript_path;
    std::string agent_type;

    static SubagentStopHookInput from_json(const json& j)
    {
        SubagentStopHookInput input;
        input.session_id = j.value("session_id", "");
        input.transcript_path = j.value("transcript_path", "");
        input.cwd = j.value("cwd", "");
        if (j.contains("permission_mode") && !j.at("permission_mode").is_null())
            input.permission_mode = j.at("permission_mode").get<std::string>();
        input.hook_event_name = j.value("hook_event_name", HookEvent::SubagentStop);
        input.stop_hook_active = j.value("stop_hook_active", false);
        input.agent_id = j.value("agent_id", "");
        input.agent_transcript_path = j.value("agent_transcript_path", "");
        input.agent_type = j.value("agent_type", "");
        return input;
    }
};

/// Input data for PermissionRequest hook callbacks.
/// Mirrors `PermissionRequestHookInput` from Python SDK.
struct PermissionRequestHookInput
{
    std::string session_id;
    std::string transcript_path;
    std::string cwd;
    std::optional<std::string> permission_mode = std::nullopt;
    std::string hook_event_name = HookEvent::PermissionRequest;
    std::string tool_name;
    json tool_input;
    std::optional<json> permission_suggestions = std::nullopt;
    /// Sub-agent attribution (Python _SubagentContextMixin, commit 2f1fd38).
    std::optional<std::string> agent_id = std::nullopt;
    std::optional<std::string> agent_type = std::nullopt;

    static PermissionRequestHookInput from_json(const json& j)
    {
        PermissionRequestHookInput input;
        input.session_id = j.value("session_id", "");
        input.transcript_path = j.value("transcript_path", "");
        input.cwd = j.value("cwd", "");
        if (j.contains("permission_mode") && !j.at("permission_mode").is_null())
            input.permission_mode = j.at("permission_mode").get<std::string>();
        input.hook_event_name = j.value("hook_event_name", HookEvent::PermissionRequest);
        input.tool_name = j.value("tool_name", "");
        input.tool_input = j.value("tool_input", json::object());
        if (j.contains("permission_suggestions") && !j.at("permission_suggestions").is_null())
            input.permission_suggestions = j.at("permission_suggestions");
        if (j.contains("agent_id") && !j.at("agent_id").is_null())
            input.agent_id = j.at("agent_id").get<std::string>();
        if (j.contains("agent_type") && !j.at("agent_type").is_null())
            input.agent_type = j.at("agent_type").get<std::string>();
        return input;
    }
};

// ============================================================================
// Hook-Specific Output Types (matches Python SDK v0.1.35)
// ============================================================================

/// Hook-specific output for PreToolUse callbacks.
/// permissionDecision accepts: "allow", "deny", "ask", "defer" (defer added in
/// Python commit f5a1b67).
struct PreToolUseHookOutput
{
    std::string hookEventName = HookEvent::PreToolUse;
    std::optional<std::string> permissionDecision = std::nullopt; // "allow", "deny", "ask", "defer"
    std::optional<std::string> permissionDecisionReason = std::nullopt;
    std::optional<json> updatedInput = std::nullopt;
    std::optional<std::string> additionalContext = std::nullopt;

    json to_json() const
    {
        json out = {{"hookEventName", hookEventName}};
        if (permissionDecision.has_value())
            out["permissionDecision"] = *permissionDecision;
        if (permissionDecisionReason.has_value())
            out["permissionDecisionReason"] = *permissionDecisionReason;
        if (updatedInput.has_value())
            out["updatedInput"] = *updatedInput;
        if (additionalContext.has_value())
            out["additionalContext"] = *additionalContext;
        return out;
    }
};

/// Hook-specific output for PostToolUse callbacks.
/// updatedToolOutput added in Python commit b0b652f — replaces the tool output
/// for both built-in and MCP tools, superseding updatedMCPToolOutput.
struct PostToolUseHookOutput
{
    std::string hookEventName = HookEvent::PostToolUse;
    std::optional<std::string> additionalContext = std::nullopt;
    /// Replaces tool output before it is sent to the model (all tools).
    std::optional<json> updatedToolOutput = std::nullopt;
    /// Replaces MCP tool output only. Prefer updatedToolOutput.
    std::optional<json> updatedMCPToolOutput = std::nullopt;

    json to_json() const
    {
        json out = {{"hookEventName", hookEventName}};
        if (additionalContext.has_value())
            out["additionalContext"] = *additionalContext;
        if (updatedToolOutput.has_value())
            out["updatedToolOutput"] = *updatedToolOutput;
        if (updatedMCPToolOutput.has_value())
            out["updatedMCPToolOutput"] = *updatedMCPToolOutput;
        return out;
    }
};

/// Hook-specific output for PostToolUseFailure callbacks.
struct PostToolUseFailureHookOutput
{
    std::string hookEventName = HookEvent::PostToolUseFailure;
    std::optional<std::string> additionalContext = std::nullopt;

    json to_json() const
    {
        json out = {{"hookEventName", hookEventName}};
        if (additionalContext.has_value())
            out["additionalContext"] = *additionalContext;
        return out;
    }
};

/// Hook-specific output for Notification callbacks.
struct NotificationHookOutput
{
    std::string hookEventName = HookEvent::Notification;
    std::optional<std::string> additionalContext = std::nullopt;

    json to_json() const
    {
        json out = {{"hookEventName", hookEventName}};
        if (additionalContext.has_value())
            out["additionalContext"] = *additionalContext;
        return out;
    }
};

/// Hook-specific output for SubagentStart callbacks.
struct SubagentStartHookOutput
{
    std::string hookEventName = HookEvent::SubagentStart;
    std::optional<std::string> additionalContext = std::nullopt;

    json to_json() const
    {
        json out = {{"hookEventName", hookEventName}};
        if (additionalContext.has_value())
            out["additionalContext"] = *additionalContext;
        return out;
    }
};

/// Hook-specific output for PermissionRequest callbacks.
struct PermissionRequestHookOutput
{
    std::string hookEventName = HookEvent::PermissionRequest;
    json decision; // Required decision object

    json to_json() const
    {
        json out = {{"hookEventName", hookEventName}, {"decision", decision}};
        return out;
    }
};

// ============================================================================
// Hook Configuration (matches Python SDK HookMatcher)
// ============================================================================

/// Hook matcher configuration
struct HookMatcher
{
    /// Glob pattern for matching tools/actions (e.g., "Bash", "Write|Edit")
    /// See: https://docs.anthropic.com/en/docs/claude-code/hooks#structure
    std::optional<std::string> matcher;

    /// List of callback functions to invoke when hook matches
    std::vector<HookCallback> hooks;

    /// Timeout in seconds for hook execution (default: 60). Accepts fractional seconds.
    std::optional<double> timeout;

    HookMatcher() = default;
    HookMatcher(std::optional<std::string> m, std::vector<HookCallback> h,
                std::optional<double> t = std::nullopt)
        : matcher(std::move(m)), hooks(std::move(h)), timeout(t)
    {
    }
};

// Content block types
struct TextBlock
{
    std::string type = "text";
    std::string text;
};

struct ThinkingBlock
{
    std::string type = "thinking";
    std::string thinking;
    std::string signature; // Cryptographic signature for thinking block integrity
};

struct ToolUseBlock
{
    std::string type = "tool_use";
    std::string id;
    std::string name;
    json input;
};

struct ToolResultBlock
{
    std::string type = "tool_result";
    std::string tool_use_id;
    json content; // Can be string, array of content blocks, or null
    bool is_error = false;
};

// ============================================================================
// Server-side tool blocks (Python commit 6ab97b4)
// ============================================================================

/// Server-side tool names. Python: ServerToolName literal.
namespace ServerToolName
{
constexpr const char* Advisor = "advisor";
constexpr const char* WebSearch = "web_search";
constexpr const char* WebFetch = "web_fetch";
constexpr const char* CodeExecution = "code_execution";
constexpr const char* BashCodeExecution = "bash_code_execution";
constexpr const char* TextEditorCodeExecution = "text_editor_code_execution";
constexpr const char* ToolSearchToolRegex = "tool_search_tool_regex";
constexpr const char* ToolSearchToolBm25 = "tool_search_tool_bm25";
} // namespace ServerToolName

/// Server-side tool use block (advisor, web_search, web_fetch, etc.). The API
/// executes these on the model's behalf; the caller never returns a result.
struct ServerToolUseBlock
{
    std::string type = "server_tool_use";
    std::string id;
    std::string name; // One of ServerToolName::*
    json input;
};

/// Result block returned for a server-side tool call. Mirrors ToolResultBlock.
struct ServerToolResultBlock
{
    std::string type = "server_tool_result";
    std::string tool_use_id;
    json content;
};

// Content block variant - includes server-side tool blocks (Python commit 6ab97b4)
using ContentBlock = std::variant<TextBlock, ThinkingBlock, ToolUseBlock, ToolResultBlock,
                                  ServerToolUseBlock, ServerToolResultBlock>;

// Assistant message error types
enum class AssistantMessageError
{
    AuthenticationFailed,
    BillingError,
    RateLimit,
    InvalidRequest,
    ServerError,
    Unknown
};

// Message types
struct UserMessage
{
    std::string type = "user";
    std::string role = "user";
    std::vector<ContentBlock> content;
    std::optional<std::string> uuid;               // v0.1.18: message UUID for rewind/checkpointing
    std::optional<std::string> parent_tool_use_id; // v0.1.18: tool-use linkage (if present)
    std::optional<json> tool_use_result;           // v0.1.26: tool execution result metadata
    json raw_json; // Original JSON from CLI (optional, for debugging)
};

struct AssistantMessage
{
    std::string type = "assistant";
    std::string role = "assistant";
    std::vector<ContentBlock> content;
    std::string model; // Model used for this assistant message (e.g., "claude-sonnet-4-5")
    std::optional<AssistantMessageError> error; // Error type if message contains an error
    /// Parent tool use ID for nested sub-agent messages.
    std::optional<std::string> parent_tool_use_id = std::nullopt;
    /// API usage block (Python commit fc82420).
    std::optional<json> usage = std::nullopt;
    /// Message ID preserved across CLI -> SDK boundary (Python commit 24b9b68).
    std::optional<std::string> message_id = std::nullopt;
    std::optional<std::string> stop_reason = std::nullopt;
    std::optional<std::string> session_id = std::nullopt;
    std::optional<std::string> uuid = std::nullopt;
    json raw_json;                              // Original JSON from CLI (optional, for debugging)
};

struct SystemMessage
{
    std::string type = "system";
    std::string content;
    std::string subtype;
    json raw_json; // Original JSON from CLI (optional, for debugging)
};

struct UsageInfo
{
    int input_tokens = 0;
    int output_tokens = 0;
    int cache_creation_input_tokens = 0;
    int cache_read_input_tokens = 0;
};

struct CostInfo
{
    double total = 0.0;
    double input = 0.0;
    double output = 0.0;
};

// ============================================================================
// Deferred Tool Use (Python commit f5a1b67)
// ============================================================================

/// A tool use that was deferred by a PreToolUse hook returning "defer".
/// Carried on ResultMessage.deferred_tool_use so the caller can inspect and
/// decide whether to resume.
struct DeferredToolUse
{
    std::string id;
    std::string name;
    json input;

    json to_json() const
    {
        return json{{"id", id}, {"name", name}, {"input", input}};
    }

    static DeferredToolUse from_json(const json& j)
    {
        DeferredToolUse d;
        d.id = j.value("id", "");
        d.name = j.value("name", "");
        d.input = j.value("input", json::object());
        return d;
    }
};

struct ResultMessage
{
    std::string type = "result";
    std::string subtype; // "success" | "error"
    struct Result
    {
        std::string session_id;
        std::string conversation_id;
        UsageInfo usage;
        CostInfo cost;
    } result;
    int duration_ms = 0;
    int duration_api_ms = 0;
    int num_turns = 0;
    std::optional<json> structured_output; // Structured output from JSON schema
    /// Stop reason (Python commit 7219299).
    std::optional<std::string> stop_reason = std::nullopt;
    /// Tool use deferred by a PreToolUse hook (Python commit f5a1b67).
    std::optional<DeferredToolUse> deferred_tool_use = std::nullopt;
    /// Error strings collected during execution (Python commit f9fc8e0).
    std::optional<std::vector<std::string>> errors = std::nullopt;
    /// HTTP status code of failing API call when is_error=true (Python commit b80d244).
    std::optional<int> api_error_status = std::nullopt;
    /// Result message UUID (Python commit 24b9b68).
    std::optional<std::string> uuid = std::nullopt;
    json raw_json;                         // Original JSON from CLI (optional, for debugging)

    // Convenience accessors (allows both nested and flat access)
    const std::string& session_id() const
    {
        return result.session_id;
    }
    const std::string& conversation_id() const
    {
        return result.conversation_id;
    }
    double total_cost_usd() const
    {
        return result.cost.total;
    }
    bool is_error() const
    {
        return subtype == "error";
    }
    const UsageInfo& usage() const
    {
        return result.usage;
    }
    int duration_ms_value() const
    {
        return duration_ms;
    }
    int duration_api_ms_value() const
    {
        return duration_api_ms;
    }
    int num_turns_value() const
    {
        return num_turns;
    }
};

struct StreamEvent
{
    std::string type = "stream";
    std::string event; // e.g., "content_block_delta", "tool_use_delta"
    int index = 0;     // Optional index when provided by CLI

    // Optional identifiers commonly provided by CLI
    std::string uuid;                              // Optional identifier (empty if absent)
    std::string session_id;                        // Optional session id (empty if absent)
    std::optional<std::string> parent_tool_use_id; // Optional parent tool use id

    json data;     // Event-specific data (nested event object or flat payload)
    json raw_json; // Original JSON from CLI (optional, for debugging)

    // Returns reconstructed event payload
    json event_payload() const
    {
        // Reconstruct Python-like event dict from C++ split representation
        json payload = data.is_object() ? data : json::object();
        payload["type"] = event; // Event name like "content_block_delta"
        if (index != 0)
            payload["index"] = index;
        return payload;
    }

    // Alias for readability (Python-style naming)
    json event_dict() const
    {
        return event_payload();
    }
};

// ============================================================================
// Task System Messages (Python commit 9af27d7)
// ============================================================================

/// Usage statistics carried in task_progress / task_notification messages.
struct TaskUsage
{
    int total_tokens = 0;
    int tool_uses = 0;
    int duration_ms = 0;

    json to_json() const
    {
        return json{{"total_tokens", total_tokens},
                    {"tool_uses", tool_uses},
                    {"duration_ms", duration_ms}};
    }

    static TaskUsage from_json(const json& j)
    {
        TaskUsage u;
        u.total_tokens = j.value("total_tokens", 0);
        u.tool_uses = j.value("tool_uses", 0);
        u.duration_ms = j.value("duration_ms", 0);
        return u;
    }
};

/// Possible status values for a task_notification message.
namespace TaskNotificationStatus
{
constexpr const char* Completed = "completed";
constexpr const char* Failed = "failed";
constexpr const char* Stopped = "stopped";
} // namespace TaskNotificationStatus

/// System message emitted when a task starts. Subclass of SystemMessage:
/// existing isinstance/holds_alternative checks against SystemMessage still
/// match because the base SystemMessage carries the raw payload. In C++ we
/// model the subclass relationship via inheritance.
struct TaskStartedMessage : SystemMessage
{
    std::string task_id;
    std::string description;
    std::string uuid;
    std::string session_id;
    std::optional<std::string> tool_use_id = std::nullopt;
    std::optional<std::string> task_type = std::nullopt;
};

/// System message emitted while a task is in progress.
struct TaskProgressMessage : SystemMessage
{
    std::string task_id;
    std::string description;
    TaskUsage usage;
    std::string uuid;
    std::string session_id;
    std::optional<std::string> tool_use_id = std::nullopt;
    std::optional<std::string> last_tool_name = std::nullopt;
};

/// System message emitted when a task completes, fails, or is stopped.
struct TaskNotificationMessage : SystemMessage
{
    std::string task_id;
    std::string status; // One of TaskNotificationStatus::*
    std::string output_file;
    std::string summary;
    std::string uuid;
    std::string session_id;
    std::optional<std::string> tool_use_id = std::nullopt;
    std::optional<TaskUsage> usage = std::nullopt;
};

// ============================================================================
// Session Store Mirror Error (Python commit 6e3d54f)
// ============================================================================

/// Identifies a session transcript in a SessionStore.
/// Python: SessionKey (TypedDict). Defined here as a plain struct so it can be
/// referenced from MirrorErrorMessage and the (Phase 3A) SessionStore interface.
struct SessionKey
{
    std::string project_key;
    std::string session_id;
    std::optional<std::string> subpath = std::nullopt;

    json to_json() const
    {
        json out = {{"project_key", project_key}, {"session_id", session_id}};
        if (subpath.has_value())
            out["subpath"] = *subpath;
        return out;
    }

    static SessionKey from_json(const json& j)
    {
        SessionKey k;
        k.project_key = j.value("project_key", "");
        k.session_id = j.value("session_id", "");
        if (j.contains("subpath") && !j.at("subpath").is_null())
            k.subpath = j.at("subpath").get<std::string>();
        return k;
    }
};

/// System message emitted when a SessionStore.append call fails. Non-fatal:
/// the local transcript is already durable, only the mirrored copy is missing
/// the failed batch. Subclass of SystemMessage.
struct MirrorErrorMessage : SystemMessage
{
    std::optional<SessionKey> key = std::nullopt;
    std::string error;
};

// ============================================================================
// Hook Event Message (Python commit c1182a4)
// ============================================================================

/// Hook event emitted by the CLI when include_hook_events is enabled.
/// subtype is "hook_started" or "hook_response"; data carries the full payload.
struct HookEventMessage : SystemMessage
{
    std::string hook_event_name; // e.g. "PreToolUse", "PostToolUse", "Stop"
    std::optional<std::string> session_id = std::nullopt;
    std::optional<std::string> uuid = std::nullopt;
};

// ============================================================================
// Rate Limit Types (Python commit 2d5c3cb)
// ============================================================================

/// Rate limit status values.
namespace RateLimitStatus
{
constexpr const char* Allowed = "allowed";
constexpr const char* AllowedWarning = "allowed_warning";
constexpr const char* Rejected = "rejected";
} // namespace RateLimitStatus

/// Rate limit window types.
namespace RateLimitType
{
constexpr const char* FiveHour = "five_hour";
constexpr const char* SevenDay = "seven_day";
constexpr const char* SevenDayOpus = "seven_day_opus";
constexpr const char* SevenDaySonnet = "seven_day_sonnet";
constexpr const char* Overage = "overage";
} // namespace RateLimitType

/// Rate limit status emitted by the CLI when rate limit state changes.
struct RateLimitInfo
{
    std::string status; // One of RateLimitStatus::*
    std::optional<int64_t> resets_at = std::nullopt;
    std::optional<std::string> rate_limit_type = std::nullopt;
    std::optional<double> utilization = std::nullopt;
    std::optional<std::string> overage_status = std::nullopt;
    std::optional<int64_t> overage_resets_at = std::nullopt;
    std::optional<std::string> overage_disabled_reason = std::nullopt;
    json raw; // Full raw dict from the CLI

    json to_json() const
    {
        json out = {{"status", status}};
        if (resets_at.has_value())
            out["resets_at"] = *resets_at;
        if (rate_limit_type.has_value())
            out["rate_limit_type"] = *rate_limit_type;
        if (utilization.has_value())
            out["utilization"] = *utilization;
        if (overage_status.has_value())
            out["overage_status"] = *overage_status;
        if (overage_resets_at.has_value())
            out["overage_resets_at"] = *overage_resets_at;
        if (overage_disabled_reason.has_value())
            out["overage_disabled_reason"] = *overage_disabled_reason;
        if (!raw.is_null())
            out["raw"] = raw;
        return out;
    }

    static RateLimitInfo from_json(const json& j)
    {
        RateLimitInfo r;
        r.status = j.value("status", "");
        if (j.contains("resets_at") && !j.at("resets_at").is_null())
            r.resets_at = j.at("resets_at").get<int64_t>();
        if (j.contains("rate_limit_type") && !j.at("rate_limit_type").is_null())
            r.rate_limit_type = j.at("rate_limit_type").get<std::string>();
        if (j.contains("utilization") && !j.at("utilization").is_null())
            r.utilization = j.at("utilization").get<double>();
        if (j.contains("overage_status") && !j.at("overage_status").is_null())
            r.overage_status = j.at("overage_status").get<std::string>();
        if (j.contains("overage_resets_at") && !j.at("overage_resets_at").is_null())
            r.overage_resets_at = j.at("overage_resets_at").get<int64_t>();
        if (j.contains("overage_disabled_reason") && !j.at("overage_disabled_reason").is_null())
            r.overage_disabled_reason = j.at("overage_disabled_reason").get<std::string>();
        if (j.contains("raw"))
            r.raw = j.at("raw");
        return r;
    }
};

/// Rate limit event emitted when rate limit info changes.
struct RateLimitEvent
{
    std::string type = "rate_limit";
    RateLimitInfo rate_limit_info;
    std::string uuid;
    std::string session_id;
    json raw_json;
};

// Main message variant (includes protocol types and rate-limit event v0.2.82).
using Message = std::variant<UserMessage, AssistantMessage, SystemMessage, ResultMessage,
                             StreamEvent, RateLimitEvent, protocol::ControlRequest,
                             protocol::ControlResponse>;

// Sandbox configuration types (v0.1.10+)
// Controls how Claude Code sandboxes bash commands for filesystem and network isolation.
// NOTE: Sandbox only works on macOS/Linux. Settings are no-op on Windows.

/// Violations to ignore when sandboxing
struct SandboxIgnoreViolations
{
    std::optional<std::vector<std::string>>
        file; // File paths for which violations should be ignored
    std::optional<std::vector<std::string>>
        network; // Network hosts for which violations should be ignored
};

/// Network configuration for sandbox.
/// Python: SandboxNetworkConfig (commit 92a4615 added domain allowlist fields).
struct SandboxNetworkConfig
{
    /// Domain names sandboxed processes can access (commit 92a4615).
    std::optional<std::vector<std::string>> allowedDomains;
    /// Domains always blocked, even if matched by allowedDomains (commit 92a4615).
    std::optional<std::vector<std::string>> deniedDomains;
    /// When true (managed settings), only managed-settings allowedDomains apply (commit 92a4615).
    std::optional<bool> allowManagedDomainsOnly;
    std::optional<std::vector<std::string>>
        allowUnixSockets;                    // Unix socket paths accessible in sandbox
    std::optional<bool> allowAllUnixSockets; // Allow all Unix sockets (less secure)
    std::optional<bool> allowLocalBinding;   // Allow binding to localhost ports (macOS only)
    /// macOS only: XPC/Mach service names to allow (supports trailing wildcard).
    std::optional<std::vector<std::string>> allowMachLookup;
    std::optional<int> httpProxyPort;        // HTTP proxy port if bringing your own proxy
    std::optional<int> socksProxyPort;       // SOCKS5 proxy port if bringing your own proxy
};

/// Sandbox settings configuration
/// This controls how Claude Code sandboxes bash commands for filesystem and network isolation.
///
/// **Important:** Filesystem and network restrictions are configured via permission rules,
/// not via these sandbox settings:
/// - Filesystem read restrictions: Use Read deny rules
/// - Filesystem write restrictions: Use Edit allow/deny rules
/// - Network restrictions: Use WebFetch allow/deny rules
///
/// Example:
/// ```cpp
/// SandboxSettings sandbox;
/// sandbox.enabled = true;
/// sandbox.autoAllowBashIfSandboxed = true;
/// sandbox.excludedCommands = {"docker"};
/// sandbox.network = SandboxNetworkConfig{
///     .allowLocalBinding = true
/// };
/// opts.sandbox = sandbox;
/// ```
struct SandboxSettings
{
    /// Enable bash sandboxing (macOS/Linux only). Default: False
    std::optional<bool> enabled;

    /// Auto-approve bash commands when sandboxed. Default: True
    std::optional<bool> autoAllowBashIfSandboxed;

    /// Commands that should run outside the sandbox (e.g., ["git", "docker"])
    std::optional<std::vector<std::string>> excludedCommands;

    /// Allow commands to bypass sandbox via dangerouslyDisableSandbox.
    /// When False, all commands must run sandboxed (or be in excludedCommands). Default: True
    std::optional<bool> allowUnsandboxedCommands;

    /// Network configuration for sandbox
    std::optional<SandboxNetworkConfig> network;

    /// Violations to ignore
    std::optional<SandboxIgnoreViolations> ignoreViolations;

    /// Enable weaker sandbox for unprivileged Docker environments (Linux only).
    /// Reduces security. Default: False
    std::optional<bool> enableWeakerNestedSandbox;
};

// ============================================================================
// ThinkingConfig Types (matches Python SDK v0.1.35)
// ============================================================================

/// Effort level for Claude's responses.
/// Python: EffortLevel — "low" | "medium" | "high" | "xhigh" | "max".
/// xhigh added in commit 04a39ac (Opus 4.7+).
namespace Effort
{
constexpr const char* Low = "low";
constexpr const char* Medium = "medium";
constexpr const char* High = "high";
constexpr const char* XHigh = "xhigh";
constexpr const char* Max = "max";
} // namespace Effort

/// ThinkingConfig variant: adaptive thinking
struct ThinkingConfigAdaptive
{
    std::string type = "adaptive";
    /// Optional display mode forwarded via --thinking-display (Python commit 32f09c1).
    std::optional<std::string> display = std::nullopt;
};

/// ThinkingConfig variant: enabled with explicit budget
struct ThinkingConfigEnabled
{
    std::string type = "enabled";
    int budget_tokens;
    /// Optional display mode forwarded via --thinking-display (Python commit 32f09c1).
    std::optional<std::string> display = std::nullopt;
    explicit ThinkingConfigEnabled(int budget) : budget_tokens(budget) {}
};

/// ThinkingConfig variant: thinking disabled
struct ThinkingConfigDisabled
{
    std::string type = "disabled";
};

/// ThinkingConfig discriminated union
using ThinkingConfig = std::variant<ThinkingConfigAdaptive, ThinkingConfigEnabled, ThinkingConfigDisabled>;

// ============================================================================
// MCP Status Types (Python commit 28f9b4b)
// Returned by ClaudeSDKClient.get_mcp_status(). Wire-format uses camelCase.
// ============================================================================

/// Connection status values for an MCP server.
namespace McpServerConnectionStatus
{
constexpr const char* Connected = "connected";
constexpr const char* Failed = "failed";
constexpr const char* NeedsAuth = "needs-auth";
constexpr const char* Pending = "pending";
constexpr const char* Disabled = "disabled";
} // namespace McpServerConnectionStatus

/// Tool annotations as returned in MCP server status.
struct McpToolAnnotations
{
    std::optional<bool> readOnly = std::nullopt;
    std::optional<bool> destructive = std::nullopt;
    std::optional<bool> openWorld = std::nullopt;

    json to_json() const
    {
        json out = json::object();
        if (readOnly.has_value()) out["readOnly"] = *readOnly;
        if (destructive.has_value()) out["destructive"] = *destructive;
        if (openWorld.has_value()) out["openWorld"] = *openWorld;
        return out;
    }
    static McpToolAnnotations from_json(const json& j)
    {
        McpToolAnnotations a;
        if (j.contains("readOnly") && !j.at("readOnly").is_null())
            a.readOnly = j.at("readOnly").get<bool>();
        if (j.contains("destructive") && !j.at("destructive").is_null())
            a.destructive = j.at("destructive").get<bool>();
        if (j.contains("openWorld") && !j.at("openWorld").is_null())
            a.openWorld = j.at("openWorld").get<bool>();
        return a;
    }
};

struct McpToolInfo
{
    std::string name;
    std::optional<std::string> description = std::nullopt;
    std::optional<McpToolAnnotations> annotations = std::nullopt;

    json to_json() const
    {
        json out = {{"name", name}};
        if (description.has_value()) out["description"] = *description;
        if (annotations.has_value()) out["annotations"] = annotations->to_json();
        return out;
    }
    static McpToolInfo from_json(const json& j)
    {
        McpToolInfo t;
        t.name = j.value("name", "");
        if (j.contains("description") && !j.at("description").is_null())
            t.description = j.at("description").get<std::string>();
        if (j.contains("annotations") && j.at("annotations").is_object())
            t.annotations = McpToolAnnotations::from_json(j.at("annotations"));
        return t;
    }
};

/// Server info from MCP initialize handshake (when connected).
struct McpServerInfo
{
    std::string name;
    std::string version;

    json to_json() const
    {
        return json{{"name", name}, {"version", version}};
    }
    static McpServerInfo from_json(const json& j)
    {
        return McpServerInfo{j.value("name", ""), j.value("version", "")};
    }
};

/// SDK MCP server config as returned in status responses (no instance field).
struct McpSdkServerConfigStatus
{
    std::string type = "sdk";
    std::string name;

    json to_json() const { return json{{"type", type}, {"name", name}}; }
    static McpSdkServerConfigStatus from_json(const json& j)
    {
        McpSdkServerConfigStatus c;
        c.type = j.value("type", "sdk");
        c.name = j.value("name", "");
        return c;
    }
};

/// Claude.ai proxy MCP server config (output-only).
struct McpClaudeAIProxyServerConfig
{
    std::string type = "claudeai-proxy";
    std::string url;
    std::string id;

    json to_json() const { return json{{"type", type}, {"url", url}, {"id", id}}; }
    static McpClaudeAIProxyServerConfig from_json(const json& j)
    {
        McpClaudeAIProxyServerConfig c;
        c.type = j.value("type", "claudeai-proxy");
        c.url = j.value("url", "");
        c.id = j.value("id", "");
        return c;
    }
};

/// Server config in status responses. Modeled as opaque json since the union
/// (stdio/sse/http/sdk/claudeai-proxy) is large and rarely consumed.
using McpServerStatusConfig = json;

/// Status information for an MCP server connection.
struct McpServerStatus
{
    std::string name;
    std::string status; // One of McpServerConnectionStatus::*
    std::optional<McpServerInfo> serverInfo = std::nullopt;
    std::optional<std::string> error = std::nullopt;
    std::optional<McpServerStatusConfig> config = std::nullopt;
    std::optional<std::string> scope = std::nullopt;
    std::optional<std::vector<McpToolInfo>> tools = std::nullopt;

    json to_json() const
    {
        json out = {{"name", name}, {"status", status}};
        if (serverInfo.has_value()) out["serverInfo"] = serverInfo->to_json();
        if (error.has_value()) out["error"] = *error;
        if (config.has_value()) out["config"] = *config;
        if (scope.has_value()) out["scope"] = *scope;
        if (tools.has_value()) {
            json arr = json::array();
            for (const auto& t : *tools) arr.push_back(t.to_json());
            out["tools"] = arr;
        }
        return out;
    }
    static McpServerStatus from_json(const json& j)
    {
        McpServerStatus s;
        s.name = j.value("name", "");
        s.status = j.value("status", "");
        if (j.contains("serverInfo") && j.at("serverInfo").is_object())
            s.serverInfo = McpServerInfo::from_json(j.at("serverInfo"));
        if (j.contains("error") && !j.at("error").is_null())
            s.error = j.at("error").get<std::string>();
        if (j.contains("config"))
            s.config = j.at("config");
        if (j.contains("scope") && !j.at("scope").is_null())
            s.scope = j.at("scope").get<std::string>();
        if (j.contains("tools") && j.at("tools").is_array())
        {
            std::vector<McpToolInfo> tools;
            for (const auto& t : j.at("tools"))
                tools.push_back(McpToolInfo::from_json(t));
            s.tools = std::move(tools);
        }
        return s;
    }
};

/// Wrapper response from get_mcp_status().
struct McpStatusResponse
{
    std::vector<McpServerStatus> mcpServers;

    json to_json() const
    {
        json arr = json::array();
        for (const auto& s : mcpServers) arr.push_back(s.to_json());
        return json{{"mcpServers", arr}};
    }
    static McpStatusResponse from_json(const json& j)
    {
        McpStatusResponse r;
        if (j.contains("mcpServers") && j.at("mcpServers").is_array())
            for (const auto& s : j.at("mcpServers"))
                r.mcpServers.push_back(McpServerStatus::from_json(s));
        return r;
    }
};

// ============================================================================
// Context Usage Types (Python commit ac900bd)
// ============================================================================

/// A single context usage category (system prompt, tools, messages, etc.).
struct ContextUsageCategory
{
    std::string name;
    int tokens = 0;
    std::string color;
    std::optional<bool> isDeferred = std::nullopt;

    json to_json() const
    {
        json out = {{"name", name}, {"tokens", tokens}, {"color", color}};
        if (isDeferred.has_value()) out["isDeferred"] = *isDeferred;
        return out;
    }
    static ContextUsageCategory from_json(const json& j)
    {
        ContextUsageCategory c;
        c.name = j.value("name", "");
        c.tokens = j.value("tokens", 0);
        c.color = j.value("color", "");
        if (j.contains("isDeferred") && !j.at("isDeferred").is_null())
            c.isDeferred = j.at("isDeferred").get<bool>();
        return c;
    }
};

/// Response from ClaudeSDKClient.get_context_usage().
struct ContextUsageResponse
{
    std::vector<ContextUsageCategory> categories;
    int totalTokens = 0;
    int maxTokens = 0;
    int rawMaxTokens = 0;
    double percentage = 0.0;
    std::string model;
    bool isAutoCompactEnabled = false;
    std::vector<json> memoryFiles;
    std::vector<json> mcpTools;
    std::vector<json> agents;
    std::vector<std::vector<json>> gridRows;
    std::optional<int> autoCompactThreshold = std::nullopt;
    std::optional<std::vector<json>> deferredBuiltinTools = std::nullopt;
    std::optional<std::vector<json>> systemTools = std::nullopt;
    std::optional<std::vector<json>> systemPromptSections = std::nullopt;
    std::optional<json> slashCommands = std::nullopt;
    std::optional<json> skills = std::nullopt;
    std::optional<json> messageBreakdown = std::nullopt;
    std::optional<json> apiUsage = std::nullopt;

    static ContextUsageResponse from_json(const json& j)
    {
        ContextUsageResponse r;
        if (j.contains("categories") && j.at("categories").is_array())
            for (const auto& c : j.at("categories"))
                r.categories.push_back(ContextUsageCategory::from_json(c));
        r.totalTokens = j.value("totalTokens", 0);
        r.maxTokens = j.value("maxTokens", 0);
        r.rawMaxTokens = j.value("rawMaxTokens", 0);
        r.percentage = j.value("percentage", 0.0);
        r.model = j.value("model", "");
        r.isAutoCompactEnabled = j.value("isAutoCompactEnabled", false);
        if (j.contains("memoryFiles") && j.at("memoryFiles").is_array())
            r.memoryFiles = j.at("memoryFiles").get<std::vector<json>>();
        if (j.contains("mcpTools") && j.at("mcpTools").is_array())
            r.mcpTools = j.at("mcpTools").get<std::vector<json>>();
        if (j.contains("agents") && j.at("agents").is_array())
            r.agents = j.at("agents").get<std::vector<json>>();
        if (j.contains("gridRows") && j.at("gridRows").is_array())
            for (const auto& row : j.at("gridRows"))
                r.gridRows.push_back(row.get<std::vector<json>>());
        if (j.contains("autoCompactThreshold") && !j.at("autoCompactThreshold").is_null())
            r.autoCompactThreshold = j.at("autoCompactThreshold").get<int>();
        if (j.contains("deferredBuiltinTools") && j.at("deferredBuiltinTools").is_array())
            r.deferredBuiltinTools = j.at("deferredBuiltinTools").get<std::vector<json>>();
        if (j.contains("systemTools") && j.at("systemTools").is_array())
            r.systemTools = j.at("systemTools").get<std::vector<json>>();
        if (j.contains("systemPromptSections") && j.at("systemPromptSections").is_array())
            r.systemPromptSections = j.at("systemPromptSections").get<std::vector<json>>();
        if (j.contains("slashCommands")) r.slashCommands = j.at("slashCommands");
        if (j.contains("skills")) r.skills = j.at("skills");
        if (j.contains("messageBreakdown")) r.messageBreakdown = j.at("messageBreakdown");
        if (j.contains("apiUsage")) r.apiUsage = j.at("apiUsage");
        return r;
    }
};

// ============================================================================
// Session Store flush mode (Python commit 0a69e94)
// ============================================================================
namespace SessionStoreFlushMode
{
constexpr const char* Batched = "batched";
constexpr const char* Eager = "eager";
} // namespace SessionStoreFlushMode

// Forward declaration for ClaudeOptions::session_store (interface in claude/sessions/session_store.hpp)
class SessionStore;

// Configuration options
struct ClaudeOptions
{
    std::string model;
    std::string fallback_model; // Secondary model to use if primary model fails or is unavailable
    std::string system_prompt;
    std::string system_prompt_append; // Append to default claude_code preset (mutually exclusive
                                      // with system_prompt)

    // Base tools configuration (matches Python ClaudeAgentOptions.tools)
    // - When unset, CLI default tool behavior is used.
    // - When set to a list:
    //     * Empty list => disable all built-in tools ("--tools" with empty value).
    //     * Non-empty list => comma-separated list passed via "--tools".
    // - When tools_preset is set to "claude_code", CLI "default" toolset is used.
    std::optional<std::vector<std::string>> tools; // Optional base tools list
    std::optional<std::string> tools_preset;       // Optional preset name (e.g., "claude_code")

    std::vector<std::string> allowed_tools;
    std::vector<std::string> disallowed_tools;
    std::string permission_mode; // "default", "acceptEdits", "plan", "bypassPermissions"
    std::optional<int> max_turns;
    std::optional<double> max_budget_usd; // v0.1.6: limit total cost in USD
    std::optional<std::string> working_directory;
    std::map<std::string, std::string> environment;
    bool inherit_environment =
        true; // If false, do not inherit parent environment when spawning CLI
    // Optional explicit path to the Claude Code CLI executable.
    // If empty, SDK searches PATH as usual.
    std::string cli_path;
    // When true (or when CLAUDE_AGENT_SDK_REQUIRE_EXPLICIT_CLI is set), only explicit CLI paths
    // are allowed (cli_path or CLAUDE_CLI_PATH). PATH/home discovery is disabled.
    bool require_explicit_cli = false;

    // Security: Enforce version check (v0.1.11+)
    // When true (default), version check failures cause hard errors instead of warnings.
    // Set to false only for testing/development.
    bool enforce_version_check = true;

    // Security: Environment variable sanitization (v0.1.11+)
    // When true (default), only forward environment variables in allowed_env_vars plus
    // essential system variables (PATH, HOME, TEMP, etc.). Prevents credential leakage.
    // When false, inherits full parent environment (legacy behavior, not recommended).
    bool sanitize_environment = true;

    // Security: Allowed environment variables for CLI subprocess (v0.1.11+)
    // When sanitize_environment is true, only these variables (plus essential system vars)
    // are forwarded to the spawned CLI process. Empty list = only essential vars.
    // Example: {"NODE_ENV", "DEBUG"}
    std::vector<std::string> allowed_env_vars;

    // Security: Message parsing limits (v0.1.11+)
    // Maximum size for message parser internal buffer. Prevents unbounded memory growth
    // from malformed or very long message lines. Default: 10MB
    size_t max_message_buffer_size = 10 * 1024 * 1024;

    // Security: Maximum number of messages to accumulate in a single read operation (v0.1.11+)
    // Prevents memory exhaustion from CLI outputting excessive messages. Default: 10000
    size_t max_messages_per_read = 10000;

    // Security: Maximum total bytes to read from CLI stdout in one operation (v0.1.11+)
    // Prevents unbounded reads from malicious/broken CLI. Default: 100MB
    size_t max_total_read_bytes = 100 * 1024 * 1024;

    // SDK beta features (matches Python SdkBeta / ClaudeAgentOptions.betas)
    // Values are forwarded to the CLI via the --betas flag as a comma-separated list.
    std::vector<std::string> betas;

    // Plugin configurations
    // List of plugins to load. Each plugin is passed to CLI via --plugin-dir flag.
    std::vector<SdkPluginConfig> plugins;

    bool include_partial_messages = false;
    std::string permission_prompt_tool_name;  // Tool to prompt for permissions
    std::string mcp_config;                   // MCP server configuration (JSON string or path)
    std::vector<std::string> add_dirs;        // Additional directories to add
    std::string settings;                     // Settings file path or JSON string
    std::optional<SandboxSettings> sandbox;   // Sandbox configuration (v0.1.10+, macOS/Linux only)
    std::string resume;                       // Resume session ID
    std::vector<std::string> setting_sources; // Setting sources
    bool continue_conversation = false;       // Continue previous conversation
    bool fork_session = false;                // Fork the session
    std::optional<int> max_thinking_tokens;          // v0.1.6: limit thinking tokens (deprecated, use thinking)
    std::optional<ThinkingConfig> thinking;            // v0.1.35: ThinkingConfig (takes precedence over max_thinking_tokens)
    std::optional<std::string> effort;                 // v0.1.35: Effort level ("low", "medium", "high", "max")
    std::optional<json> output_format;        // v0.1.8: Structured output format (JSON schema)
    bool enable_file_checkpointing = false;   // v0.1.15: enable file checkpointing/rewind_files

    // Control protocol hooks and callbacks
    /// Hook configurations organized by event type
    /// Example:
    /// ```cpp
    /// opts.hooks[HookEvent::PreToolUse] = {
    ///     HookMatcher{
    ///         "Bash",  // matcher pattern
    ///         {my_hook_callback}  // list of callbacks
    ///     }
    /// };
    /// ```
    std::map<std::string, std::vector<HookMatcher>> hooks;

    /// Callback invoked when tool permission is requested.
    /// If not set, all tools are automatically allowed (like bypassPermissions mode).
    /// Note: Executes on the message reader thread - blocking operations will delay message
    /// processing.
    std::optional<ToolPermissionCallback> tool_permission_callback;

    /// Callback invoked when CLI writes to stderr.
    /// Useful for observing debug output, warnings, and errors from the CLI process.
    /// Note: Executes on a background thread - ensure callback is thread-safe.
    std::optional<StderrCallback> stderr_callback;

    /// Maximum buffer size for JSON message parsing (in bytes).
    /// Default: 1MB (1024 * 1024 bytes)
    /// If a single JSON message exceeds this size, parsing will fail with an error.
    std::optional<size_t> max_buffer_size;

    // Advanced CLI options
    /// Agent definitions for custom agents
    /// Maps agent name -> AgentDefinition
    std::map<std::string, AgentDefinition> agents;

    /// Arbitrary CLI flags to pass through
    /// Maps flag name -> value (or empty string for boolean flags)
    /// Example: {"--custom-flag", "value"}, {"--enable-feature", ""}
    std::map<std::string, std::string> extra_args;

    // In-process MCP servers (SDK) — optional
    // Map of server name -> handler that accepts an MCP JSON-RPC message object
    // and returns a JSON-RPC response object (result or error).
    std::map<std::string, McpRequestHandler> sdk_mcp_handlers;

    // ========================================================================
    // Python parity v0.2.82 additions (Task T23)
    // ========================================================================

    /// Use a specific session ID for the conversation. Must be a valid UUID.
    /// Python: ClaudeAgentOptions.session_id (commit 5656d20).
    std::optional<std::string> session_id = std::nullopt;

    /// API-side task budget in tokens.
    /// Python: ClaudeAgentOptions.task_budget (commit 2e60cec).
    std::optional<TaskBudget> task_budget = std::nullopt;

    /// Skills to enable for the main session. std::nullopt = SDK auto-defaults
    /// (CLI defaults still apply). variant alternatives: "all" (std::string) or
    /// list of skill names.
    /// Python: ClaudeAgentOptions.skills (commit 1c26bd3).
    std::optional<std::variant<std::string, std::vector<std::string>>> skills = std::nullopt;

    /// Only use MCP servers passed in mcp_config; ignore CLI defaults.
    /// Python: ClaudeAgentOptions.strict_mcp_config (commit 32bcc4e).
    bool strict_mcp_config = false;

    /// Include hook lifecycle events in the message stream.
    /// Python: ClaudeAgentOptions.include_hook_events (commit c1182a4).
    bool include_hook_events = false;

    /// Optional external session store adapter (mirror transcripts).
    /// Implementations: claude/sessions/session_store.hpp (Phase 3A).
    /// Python: ClaudeAgentOptions.session_store (commit 6e3d54f).
    std::shared_ptr<SessionStore> session_store = nullptr;

    /// When to flush mirrored transcript entries to session_store.
    /// One of SessionStoreFlushMode::Batched / Eager. Default: "batched".
    /// Python: ClaudeAgentOptions.session_store_flush (commit 0a69e94).
    std::string session_store_flush = SessionStoreFlushMode::Batched;

    /// System prompt loaded from a file (Python commit 139b815).
    std::optional<SystemPromptFile> system_prompt_file = std::nullopt;

    /// System prompt preset (with optional exclude_dynamic_sections).
    std::optional<SystemPromptPreset> system_prompt_preset = std::nullopt;
};

// Helper functions for type checking
inline bool is_assistant_message(const Message& msg)
{
    return std::holds_alternative<AssistantMessage>(msg);
}

inline bool is_result_message(const Message& msg)
{
    return std::holds_alternative<ResultMessage>(msg);
}

inline bool is_system_message(const Message& msg)
{
    return std::holds_alternative<SystemMessage>(msg);
}

inline bool is_stream_event(const Message& msg)
{
    return std::holds_alternative<StreamEvent>(msg);
}

inline bool is_control_request(const Message& msg)
{
    return std::holds_alternative<protocol::ControlRequest>(msg);
}

inline bool is_control_response(const Message& msg)
{
    return std::holds_alternative<protocol::ControlResponse>(msg);
}

// Helper to get text from content blocks
std::string get_text_content(const std::vector<ContentBlock>& content);

// Helper to dump raw JSON from messages (for debugging)
template <typename T>
std::string dump_raw_json(const T& msg, int indent = 2)
{
    if (msg.raw_json.empty())
        return "{}";
    return msg.raw_json.dump(indent);
}

} // namespace claude

#endif // CLAUDE_TYPES_HPP
