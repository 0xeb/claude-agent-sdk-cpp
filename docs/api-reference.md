# Claude C++ SDK API Reference

## Table of Contents

- [Core Functions](#core-functions)
  - [query()](#query)
- [Classes](#classes)
  - [ClaudeClient](#claudeclient)
  - [MessageStream](#messagestream)
  - [ClaudeOptions](#claudeoptions)
- [Message Types](#message-types)
  - [Message Variant](#message-variant)
  - [Content Blocks](#content-blocks)
- [Control Protocol](#control-protocol)
- [Hooks System](#hooks-system)
- [Tool Permissions](#tool-permissions)
- [Error Types](#error-types)
- [Helper Functions](#helper-functions)

---

## Core Functions

### query()

One-shot query function for simple Claude interactions.

```cpp
std::vector<Message> query(
    const std::string& prompt,
    const ClaudeOptions& options = ClaudeOptions{}
);
```

#### Parameters

- `prompt` - The query string to send to Claude
- `options` - Configuration options (optional, uses defaults if not provided)

#### Returns

- `std::vector<Message>` - Vector of messages from Claude's response

#### Example

```cpp
#include <claude/claude.hpp>

claude::ClaudeOptions opts;
opts.permission_mode = "bypassPermissions";

for (const auto& msg : claude::query("What is 2+2?", opts)) {
    if (claude::is_assistant_message(msg)) {
        const auto& assistant = std::get<claude::AssistantMessage>(msg);
        std::cout << claude::get_text_content(assistant.content) << "\n";
    }
}
```

#### Exceptions

- `CLINotFoundError` - Claude CLI not installed or not in PATH
- `ProcessError` - Process execution failed
- `JSONDecodeError` - Failed to parse response
- `ClaudeError` - General SDK error

---

## Classes

### ClaudeClient

Main client class for bidirectional communication with Claude.

#### Constructor

```cpp
explicit ClaudeClient(const ClaudeOptions& options = ClaudeOptions{});
```

#### Methods

##### connect()

```cpp
void connect();
```

Establishes connection to Claude CLI. Starts background reader thread and sends initialization handshake.

##### disconnect()

```cpp
void disconnect();
```

Closes connection and cleans up resources.

##### is_connected()

```cpp
bool is_connected() const;
```

Returns true if connected to Claude CLI.

##### get_pid()

```cpp
long get_pid() const;
```

Returns process ID of Claude CLI subprocess, or 0 if not connected.

##### send_query()

```cpp
void send_query(
    const std::string& prompt,
    const std::string& session_id = ""
);
```

Sends a query to Claude. Optionally specify session ID for conversation tracking.

##### receive_messages()

```cpp
MessageStream receive_messages();
```

Returns a `MessageStream` iterator for receiving responses.

##### receive_response()

```cpp
std::vector<Message> receive_response();
```

Convenience method that collects all messages until result message.

##### interrupt()

```cpp
void interrupt();
```

Interrupts the current Claude operation.

##### set_permission_mode()

```cpp
void set_permission_mode(const std::string& mode);
```

Changes permission mode dynamically. Valid modes:
- `"default"` - Standard permission handling
- `"acceptEdits"` - Auto-accept file edits
- `"plan"` - Planning mode
- `"bypassPermissions"` - No permission prompts

##### set_model()

```cpp
void set_model(const std::string& model);
```

Switches to a different Claude model mid-conversation.

##### get_server_info()

```cpp
std::optional<json> get_server_info() const;
```

Returns initialization information (when available) after `connect()`, including:
- Available commands and system capabilities
- Output style information

Returns `std::nullopt` if the CLI does not support initialization or `connect()` has not completed.

#### Example

```cpp
claude::ClaudeClient client(opts);
client.connect();

client.send_query("Hello Claude!");

for (const auto& msg : client.receive_messages()) {
    if (claude::is_assistant_message(msg)) {
        const auto& assistant = std::get<claude::AssistantMessage>(msg);
        std::cout << claude::get_text_content(assistant.content);
    }
    if (claude::is_result_message(msg)) {
        break;
    }
}

// Control operations
client.interrupt();
client.set_permission_mode("plan");
client.set_model("claude-opus-4");

client.disconnect();
```

---

### MessageStream

Iterator for streaming messages from Claude.

#### Methods

##### begin()

```cpp
Iterator begin();
```

Returns iterator to first message.

##### end()

```cpp
Iterator end();
```

Returns end iterator.

##### get_next()

```cpp
std::optional<Message> get_next();
```

Gets next message (blocking).

##### has_more()

```cpp
bool has_more() const;
```

Returns true if more messages are available.

##### stop()

```cpp
void stop();
```

Stops message streaming.

#### Usage

```cpp
MessageStream stream = client.receive_messages();
for (const auto& msg : stream) {
    // Process messages
}
```

---

### ClaudeOptions

Configuration options for Claude SDK.

```cpp
struct ClaudeOptions {
    // Model selection
    std::string model;
    std::string fallback_model;  // Secondary model to use if primary model fails (v0.1.6+)

    // System prompt
    std::string system_prompt;

    // Tool control
    std::vector<std::string> allowed_tools;
    std::vector<std::string> disallowed_tools;

    // Permission mode
    std::string permission_mode;

    // Session management
    std::optional<int> max_turns;
    std::optional<std::string> working_directory;
    std::map<std::string, std::string> environment;
    // Optional explicit path to Claude CLI (overrides PATH)
    std::string cli_path;
    bool include_partial_messages = false;

    // Advanced settings
    std::string permission_prompt_tool_name;
    std::string mcp_config;
    std::vector<std::string> add_dirs;
    std::string settings;
    std::string resume;
    std::vector<std::string> setting_sources;
    bool continue_conversation = false;
    bool fork_session = false;

    // Hooks and callbacks
    std::map<std::string, std::vector<HookMatcher>> hooks;
    std::optional<ToolPermissionCallback> tool_permission_callback;

    // Custom agents
    std::map<std::string, AgentDefinition> agents;

    // Extra CLI arguments
    std::map<std::string, std::string> extra_args;
};
```

---

## Message Types

### Message Variant

The main message type is a variant that can hold different message types:

```cpp
using Message = std::variant<
    UserMessage,
    AssistantMessage,
    SystemMessage,
    ResultMessage,
    StreamEvent,
    protocol::ControlRequest,
    protocol::ControlResponse
>;
```

### Message Structures

#### AssistantMessage

```cpp
struct AssistantMessage {
    std::string type = "assistant";
    std::string role = "assistant";
    std::vector<ContentBlock> content;
    json raw_json;  // Original JSON from CLI
};
```

#### UserMessage

```cpp
struct UserMessage {
    std::string type = "user";
    std::string role = "user";
    std::vector<ContentBlock> content;
    json raw_json;
};
```

#### SystemMessage

```cpp
struct SystemMessage {
    std::string type = "system";
    std::string content;
    std::string subtype;   // e.g., "init"
    json raw_json;
};
```

#### ResultMessage

```cpp
struct ResultMessage {
    std::string type = "result";
    std::string subtype;   // e.g., "success" | "error" (if provided by CLI)
    struct Result {
        std::string session_id;
        std::string conversation_id;
        UsageInfo usage;
        CostInfo cost;
    } result;
    // Python-parity flat fields
    int duration_ms = 0;
    int duration_api_ms = 0;
    int num_turns = 0;
    json raw_json;
};
```

Convenience accessors (Python-parity):

```cpp
// Nested accessors exposed as flat helpers
const std::string& ResultMessage::session_id() const;
const std::string& ResultMessage::conversation_id() const;
double ResultMessage::total_cost_usd() const;   // returns result.cost.total
bool ResultMessage::is_error() const;           // true when subtype == "error"
```

### Content Blocks

Content can be one of several block types:

```cpp
using ContentBlock = std::variant<
    TextBlock,
    ThinkingBlock,
    ToolUseBlock,
    ToolResultBlock
>;
```

#### TextBlock

```cpp
struct TextBlock {
    std::string type = "text";
    std::string text;
};
```

#### ThinkingBlock

```cpp
struct ThinkingBlock {
    std::string type = "thinking";
    std::string thinking;
    std::string signature;  // Cryptographic signature
};
```

#### ToolUseBlock

```cpp
struct ToolUseBlock {
    std::string type = "tool_use";
    std::string id;
    std::string name;
    json input;
};
```

#### ToolResultBlock

```cpp
struct ToolResultBlock {
    std::string type = "tool_result";
    std::string tool_use_id;
    json content;  // Can be string, array, or null
    bool is_error = false;
};
```

---

## Control Protocol

The control protocol enables dynamic control over Claude's behavior.

### Interrupt

```cpp
client.interrupt();
```

Cancels the current operation. Useful for stopping long-running responses.

### Permission Mode

```cpp
client.set_permission_mode("bypassPermissions");
```

Available modes:
- `"default"` - Standard permission handling with callbacks
- `"acceptEdits"` - Automatically accept file edits
- `"plan"` - Planning mode for complex tasks
- `"bypassPermissions"` - No permission prompts

### Model Switching

```cpp
client.set_model("claude-opus-4");
```

Switch between available Claude models during a conversation.

---

## Hooks System

Register callbacks for specific events during Claude's operation.

### Hook Events

```cpp
namespace HookEvent {
    constexpr const char* PreToolUse = "PreToolUse";
    constexpr const char* PostToolUse = "PostToolUse";
    constexpr const char* UserPromptSubmit = "UserPromptSubmit";
    constexpr const char* Stop = "Stop";
    constexpr const char* SubagentStop = "SubagentStop";
    constexpr const char* PreCompact = "PreCompact";
}
```

### Hook Registration

```cpp
claude::ClaudeOptions opts;

opts.hooks[claude::HookEvent::PreToolUse] = {
    claude::HookMatcher{
        "Bash",  // Match pattern (glob-style)
        {
            [](const json& input, const std::string& tool_use_id) -> json {
                std::cout << "About to run Bash command\n";
                return {
                    {"continue", true},
                    {"updatedInput", input}  // Optional: modify input
                };
            }
        }
    }
};
```

### Hook Callback Signature

```cpp
using HookCallback = std::function<json(
    const json& input,
    const std::string& tool_use_id
)>;
```

### Hook Response Format

```cpp
{
    "continue": true,           // Whether to continue
    "updatedInput": {...},      // Optional: modified input
    "message": "Hook message"   // Optional: message to display
}
```

### Implementation Notes

**Python SDK vs C++ SDK:**

The C++ SDK uses JSON-based hook inputs (`const json& input`), providing flexibility and dynamic typing. In contrast, the Python SDK uses strongly-typed input structures for each hook event type.

**Why JSON in C++?**
- Simplifies the API (single callback signature for all hook types)
- Provides runtime flexibility for inspecting hook-specific fields
- Avoids complex template or variant-based type systems
- Matches the CLI's wire format directly

Both approaches are functionally equivalent and provide full access to hook event data. The C++ approach prioritizes simplicity and flexibility, while the Python approach prioritizes compile-time type safety.

**Example:** A `PreToolUse` hook receives `input` as JSON containing fields like `toolName`, `toolInput`, etc. You can access these fields dynamically:

```cpp
[](const json& input, const std::string& tool_use_id) -> json {
    std::string tool_name = input["toolName"];
    json tool_input = input["toolInput"];
    // ... process hook data
    return {{"continue", true}};
}
```

---

## Tool Permissions

Fine-grained control over tool execution.

### Permission Callback

```cpp
using ToolPermissionCallback = std::function<PermissionResult(
    const std::string& tool_name,
    const json& input,
    const ToolPermissionContext& context
)>;
```

### Permission Results

#### Allow

```cpp
struct PermissionResultAllow {
    std::string behavior = "allow";
    std::optional<json> updated_input;
    std::optional<std::vector<PermissionUpdate>> updated_permissions;
};
```

#### Deny

```cpp
struct PermissionResultDeny {
    std::string behavior = "deny";
    std::string message = "";
    bool interrupt = false;
};
```

### Example

```cpp
opts.tool_permission_callback = [](
    const std::string& tool_name,
    const json& input,
    const ToolPermissionContext& context
) -> PermissionResult {

    // Allow only safe tools
    if (tool_name == "Read" || tool_name == "Grep") {
        return PermissionResultAllow{};
    }

    // Deny dangerous operations
    if (tool_name == "Bash") {
        return PermissionResultDeny{
            .behavior = "deny",
            .message = "Bash commands not allowed",
            .interrupt = false
        };
    }

    // Allow with modified input
    return PermissionResultAllow{
        .behavior = "allow",
        .updated_input = modified_input
    };
};
```

---

## Error Types

### Exception Hierarchy

```
ClaudeError (base)
├── CLINotFoundError      - Claude CLI not installed
├── CLIConnectionError    - Connection issues
├── ProcessError          - Process execution errors
├── JSONDecodeError       - JSON parsing errors
└── MessageParseError     - Message structure errors
```

### Error Handling

```cpp
try {
    auto result = claude::query("Hello!", opts);

} catch (const claude::CLINotFoundError& e) {
    std::cerr << "Claude CLI not found. Install with:\n";
    std::cerr << "npm install -g @anthropic-ai/claude-code\n";

} catch (const claude::ProcessError& e) {
    std::cerr << "Process error: " << e.what() << "\n";
    std::cerr << "Exit code: " << e.exit_code() << "\n";
    std::cerr << "Stderr: " << e.stderr() << "\n";

} catch (const claude::JSONDecodeError& e) {
    std::cerr << "JSON parsing error: " << e.what() << "\n";

} catch (const claude::MessageParseError& e) {
    std::cerr << "Message parsing error: " << e.what() << "\n";
    std::cerr << "JSON: " << e.json().dump(2) << "\n";

} catch (const claude::ClaudeError& e) {
    std::cerr << "Claude SDK error: " << e.what() << "\n";
}
```

---

## Helper Functions

### Type Checking

```cpp
bool is_assistant_message(const Message& msg);
bool is_user_message(const Message& msg);
bool is_system_message(const Message& msg);
bool is_result_message(const Message& msg);
bool is_stream_event(const Message& msg);
bool is_control_request(const Message& msg);
bool is_control_response(const Message& msg);
```

### Content Extraction

```cpp
// Get text from content blocks
std::string get_text_content(const std::vector<ContentBlock>& content);

// Extract text from assistant message
if (is_assistant_message(msg)) {
    const auto& assistant = std::get<AssistantMessage>(msg);
    std::string text = get_text_content(assistant.content);
}
```

### JSON Utilities

```cpp
// Dump raw JSON for debugging
template <typename T>
std::string dump_raw_json(const T& msg, int indent = 2);

// Example
if (is_assistant_message(msg)) {
    const auto& assistant = std::get<AssistantMessage>(msg);
    std::cout << dump_raw_json(assistant) << "\n";
}
```

### Stream Event Helpers

```cpp
// Reconstruct a Python-style event dictionary from the split C++ representation
json StreamEvent::event_payload() const;

// Alias for readability
json StreamEvent::event_dict() const;  // same as event_payload()
```

Note: The C++ `StreamEvent` uses `type = "stream"` on the outer message for backward compatibility, while the Python SDK may emit an outward type of `"stream_event"`. Use `event_dict()` for a canonical, Python-style event payload; the parser accepts and normalizes both shapes.

---

## Notes on CLI Flag Emission

- The C++ SDK only emits `--setting-sources` when non-empty. The Python SDK may emit it with an empty value. Both behaviors are accepted by the CLI and functionally equivalent.

---

## Custom Agents

Define custom agents with specific prompts and tools.

```cpp
struct AgentDefinition {
    std::string description;                     // Required
    std::string prompt;                          // Required
    std::optional<std::vector<std::string>> tools;  // Optional
    std::optional<std::string> model;           // Optional
};
```

### Example

```cpp
opts.agents["code-reviewer"] = claude::AgentDefinition{
    .description = "Reviews code for bugs and improvements",
    .prompt = "You are an expert code reviewer. Analyze code for:\n"
              "- Bugs and potential issues\n"
              "- Performance improvements\n"
              "- Best practices\n"
              "Be constructive and specific.",
    .tools = {{"Read", "Grep", "Glob"}},
    .model = "claude-sonnet-4-5"
};

opts.agents["test-writer"] = claude::AgentDefinition{
    .description = "Writes comprehensive unit tests",
    .prompt = "You are a test engineer. Write thorough unit tests.",
    .tools = {{"Read", "Write", "Edit"}},
    .model = "claude-opus-4"
};
```

---

## Thread Safety

- `ClaudeClient` is **not** thread-safe for concurrent operations
- `MessageStream` can be used from one thread at a time
- Hook callbacks execute on the message reader thread
- Tool permission callbacks execute on the message reader thread
- Control protocol operations are internally synchronized

---

## Best Practices

1. **Always disconnect**: Use RAII or ensure `disconnect()` is called
2. **Check message types**: Use `is_*_message()` helpers before `std::get`
3. **Handle exceptions**: Wrap SDK calls in try-catch blocks
4. **Use bypassPermissions carefully**: Only for trusted environments
5. **Stream for responsiveness**: Use `receive_messages()` for real-time updates
6. **Validate tool permissions**: Implement callbacks for production use

---

## Version Information

```cpp
#include <claude/version.hpp>

std::cout << "Claude SDK Version: "
          << CLAUDE_VERSION_MAJOR << "."
          << CLAUDE_VERSION_MINOR << "."
          << CLAUDE_VERSION_PATCH << "\n";
```

Current version: 0.1.0-dev

---

## See Also

- [Getting Started Guide](getting-started.md)
- [Troubleshooting](troubleshooting.md)
- [Examples](../examples/)
- [Python SDK Reference](https://github.com/anthropics/anthropic-sdk-python)
