# Claude C++ SDK Examples

## Basic Examples

### basic_query.cpp
Simple one-shot query using the `query()` API.

```bash
./basic_query
```

**Features:**
- One-shot query with context cleanup
- Permission bypass mode
- Simple response streaming

### streaming_client.cpp
Bidirectional client with multi-turn conversation support.

```bash
./streaming_client
```

**Features:**
- Multi-turn conversations with context preservation
- Message streaming
- Clean connection lifecycle

## Advanced Examples

### control_protocol.cpp
Demonstrates all control protocol operations.

**Features:**
- `interrupt()` - Cancel ongoing Claude responses
- `set_permission_mode()` - Change permission handling dynamically
- `set_model()` - Switch between Claude models
- Three demo modes: permissions, model, interrupt

```bash
./control_protocol              # Run all demos
./control_protocol permissions  # Permission mode switching demo
./control_protocol model        # Model switching demo
./control_protocol interrupt    # Interrupt functionality demo
```

### streaming_with_hooks.cpp
Demonstrates hook registration and automatic approval.

**Features:**
- Hook callbacks (bash, edit)
- Tool permission callbacks
- Multi-turn conversation
- Automatic approval of all operations

```bash
./streaming_with_hooks
```

### multi_turn.cpp
Multi-turn conversation demonstrating context preservation.

```bash
./multi_turn
```

**Features:**
- Predefined conversation flow
- Context preservation across turns
- Demonstrates memory across multiple queries

### tool_permissions.cpp
Fine-grained tool permission control.

**Shows:**
- Allowing specific tools only
- Denying dangerous operations
- Tool filtering patterns
- Permission callback implementation

```bash
./tool_permissions
```

## Running Examples

### Build all examples

**Linux/macOS:**
```bash
cmake --preset linux  # or macos
cmake --build build-lnx --config Release
```

**Windows:**
```bash
cmake --preset windows
cmake --build build-win --config Release
```

### Run specific example

**Linux/macOS:**
```bash
./build-lnx/examples/control_protocol
./build-lnx/examples/streaming_with_hooks
./build-lnx/examples/multi_turn
./build-lnx/examples/tool_permissions
```

**Windows:**
```bash
./build-win/examples/Release/control_protocol.exe
./build-win/examples/Release/streaming_with_hooks.exe
./build-win/examples/Release/multi_turn.exe
./build-win/examples/Release/tool_permissions.exe
```

## Example Comparison with Python SDK

| Python Example | C++ Equivalent | Status |
| --- | --- | --- |
| basic_query.py | basic_query.cpp | ✅ Complete |
| streaming_mode.py | streaming_with_hooks.cpp | ✅ Complete |
| (none) | multi_turn.cpp | ➕ Extra |
| (none) | tool_permissions.cpp | ➕ Extra |

## API Reference

### One-shot Query API
```cpp
#include <claude/claude.hpp>

claude::ClaudeOptions opts;
opts.permission_mode = "bypassPermissions";

for (const auto& msg : claude::query("Hello Claude!", opts)) {
    if (claude::is_assistant_message(msg)) {
        std::cout << claude::get_text_content(msg);
    }
}
```

### Bidirectional Client API
```cpp
#include <claude/claude.hpp>

claude::ClaudeClient client(opts);
client.connect();

// Send query
client.send_query("Hello!");

// Stream response
for (const auto& msg : client.receive_messages()) {
    if (claude::is_assistant_message(msg)) {
        std::cout << claude::get_text_content(msg);
    }
    if (claude::is_done_message(msg)) {
        break;
    }
}

client.disconnect();
```

### Hook Callbacks
```cpp
opts.hooks = {
    claude::hooks::BASH_BEFORE_EXECUTE,
    claude::hooks::EDIT_BEFORE
};

opts.hook_callback = [](const std::string& hook, const claude::json& payload) {
    // Return true to approve, false to deny
    return true;
};
```

### Tool Permissions
```cpp
opts.permission_mode = "default";

opts.tool_permission_callback = [](const std::string& tool, const claude::json& args) {
    // Return true to allow, false to deny
    return tool == "Read" || tool == "Glob";
};
```

## Notes

- **Permission Modes:** `bypassPermissions` (no prompts), `default` (callbacks), `acceptEdits`, or `plan`
- **Context Preservation:** Multi-turn conversations maintain context automatically
- **Error Handling:** All examples include basic exception handling
- **Streaming:** Message streaming provides real-time response updates

## See Also

- **kb/reviewer_user.md** - Latest parity findings, Sync Timeline, and test status
- **kb/ccsdk/claude-agent-sdk-python-notes.md** - Python SDK reference
- **cppclient/include/claude/claude.hpp** - Full API documentation
