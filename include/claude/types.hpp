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
constexpr const char* PostToolUseFailure = "PostToolUseFailure";
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

/// Network configuration for sandbox
struct SandboxNetworkConfig
{
    std::optional<std::vector<std::string>>
        allowUnixSockets;                    // Unix socket paths accessible in sandbox
    std::optional<bool> allowAllUnixSockets; // Allow all Unix sockets (less secure)
    std::optional<bool> allowLocalBinding;   // Allow binding to localhost ports (macOS only)
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

    // Security: CLI path allowlist for production use (v0.1.11+)
    // If non-empty, only CLI paths in this list are allowed. Provides protection against
    // PATH poisoning and unauthorized CLI execution.
    std::vector<std::string> allowed_cli_paths;

    // Security: Optional SHA256 hash for CLI integrity verification (v0.1.11+)
    // If set, the CLI executable must match this hash. Format: 64-character hex string.
    std::optional<std::string> cli_hash_sha256;

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
    std::optional<int> max_thinking_tokens;   // v0.1.6: limit thinking tokens
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
