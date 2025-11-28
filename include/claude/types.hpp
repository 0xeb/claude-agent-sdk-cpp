#ifndef CLAUDE_TYPES_HPP
#define CLAUDE_TYPES_HPP

#include <functional>
#include <map>
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
// Agent and System Prompt Types (matches Python SDK)
// ============================================================================

/// Agent definition for custom agents
struct AgentDefinition
{
    std::string description;                                      // Required
    std::string prompt;                                           // Required
    std::optional<std::vector<std::string>> tools = std::nullopt; // Optional tools list
    std::optional<std::string> model = std::nullopt; // Optional: e.g., "claude-sonnet-4-5",
                                                     // "claude-opus-4", "claude-haiku-4", "inherit"
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
};

// ============================================================================
// Tool Permission Context and Result Types (matches Python SDK)
// ============================================================================

/// Context information for tool permission callbacks
struct ToolPermissionContext
{
    std::vector<PermissionUpdate> suggestions; // Permission suggestions from CLI
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

/// Supported hook event types
namespace HookEvent
{
constexpr const char* PreToolUse = "PreToolUse";
constexpr const char* PostToolUse = "PostToolUse";
constexpr const char* UserPromptSubmit = "UserPromptSubmit";
constexpr const char* Stop = "Stop";
constexpr const char* SubagentStop = "SubagentStop";
constexpr const char* PreCompact = "PreCompact";
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

// Content block variant
using ContentBlock = std::variant<TextBlock, ThinkingBlock, ToolUseBlock, ToolResultBlock>;

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
    json raw_json; // Original JSON from CLI (optional, for debugging)
};

struct AssistantMessage
{
    std::string type = "assistant";
    std::string role = "assistant";
    std::vector<ContentBlock> content;
    std::string model; // Model used for this assistant message (e.g., "claude-sonnet-4-5")
    std::optional<AssistantMessageError> error; // Error type if message contains an error
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

// Main message variant (includes protocol types)
using Message = std::variant<UserMessage, AssistantMessage, SystemMessage, ResultMessage,
                             StreamEvent, protocol::ControlRequest, protocol::ControlResponse>;

// Configuration options
struct ClaudeOptions
{
    std::string model;
    std::string fallback_model; // Secondary model to use if primary model fails or is unavailable
    std::string system_prompt;
    std::string system_prompt_append; // Append to default claude_code preset (mutually exclusive
                                      // with system_prompt)
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

    // Plugin configurations
    // List of plugins to load. Each plugin is passed to CLI via --plugin-dir flag.
    std::vector<SdkPluginConfig> plugins;

    bool include_partial_messages = false;
    std::string permission_prompt_tool_name;  // Tool to prompt for permissions
    std::string mcp_config;                   // MCP server configuration (JSON string or path)
    std::vector<std::string> add_dirs;        // Additional directories to add
    std::string settings;                     // Settings file path
    std::string resume;                       // Resume session ID
    std::vector<std::string> setting_sources; // Setting sources
    bool continue_conversation = false;       // Continue previous conversation
    bool fork_session = false;                // Fork the session
    std::optional<int> max_thinking_tokens;   // v0.1.6: limit thinking tokens
    std::optional<json> output_format;        // v0.1.8: Structured output format (JSON schema)

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
