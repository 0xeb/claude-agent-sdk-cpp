# Getting Started with Claude SDK for C++

This guide will help you get started with the Claude SDK for C++, from installation to your first working application.

## Prerequisites

Before you begin, ensure you have:

1. **C++17 or later compiler**
   - Windows: Visual Studio 2017 or later (MSVC 19.14+)
   - Linux: GCC 7+ or Clang 5+
   - macOS: Xcode 10+ (Clang 5+)

2. **CMake 3.20 or later**
   ```bash
   cmake --version  # Should show 3.20 or higher
   ```

3. **vcpkg** package manager
   ```bash
   git clone https://github.com/Microsoft/vcpkg.git
   cd vcpkg
   ./bootstrap-vcpkg.sh  # Linux/macOS
   ./bootstrap-vcpkg.bat # Windows
   ```

4. **Claude Code CLI**
   ```bash
   npm install -g @anthropic-ai/claude-code
   claude -v  # Verify installation
   ```

## Installation

### 1. Set Up vcpkg

Set the `VCPKG_ROOT` environment variable:

```bash
# Linux/macOS
export VCPKG_ROOT=/path/to/vcpkg

# Windows (PowerShell)
$env:VCPKG_ROOT="C:\path\to\vcpkg"

# Windows (CMD)
set VCPKG_ROOT=C:\path\to\vcpkg
```

### 2. Clone the Repository

```bash
git clone https://github.com/yourorg/ccsdk.git
cd ccsdk/cppclient
```

### 3. Configure the Project

Choose the appropriate CMake preset for your platform:

**Windows:**
```bash
cmake --preset windows
```

**Linux:**
```bash
cmake --preset linux
```

**macOS:**
```bash
cmake --preset macos
```

**Visual Studio 2022 (Windows):**
```bash
cmake --preset vs2022
start build/ClaudeSDK.sln  # Opens in Visual Studio
```

### 4. Build the Project

```bash
# Windows (command line)
cmake --build build-win --config Release

# Linux/macOS
cmake --build build-lnx --config Release  # or build-mac

# Windows (Visual Studio)
# Build from the IDE
```

### 5. Run Tests

```bash
# Windows
./build-win/tests/Release/test_claude.exe

# Linux/macOS
./build-lnx/tests/test_claude
```

All 88 tests should pass!

## Your First Program

### Step 1: Create a Simple Query

Create a file called `hello_claude.cpp`:

```cpp
#include <claude/claude.hpp>
#include <iostream>

int main() {
    // Configure options
    claude::ClaudeOptions opts;
    opts.permission_mode = "bypassPermissions";  // Skip permission prompts

    // Send a query
    auto messages = claude::query("Hello! What's 2+2?", opts);

    // Process the response
    for (const auto& msg : messages) {
        if (claude::is_assistant_message(msg)) {
            const auto& assistant = std::get<claude::AssistantMessage>(msg);
            std::cout << claude::get_text_content(assistant.content) << "\n";
        }
    }

    return 0;
}
```

### Step 2: Compile and Run

**Using CMake:**

```cmake
# CMakeLists.txt
cmake_minimum_required(VERSION 3.20)
project(HelloClaude)

set(CMAKE_CXX_STANDARD 17)

# Add Claude SDK directory
add_subdirectory(path/to/ccsdk/cppclient)

add_executable(hello_claude hello_claude.cpp)
target_link_libraries(hello_claude PRIVATE claude_sdk)
```

```bash
cmake -B build -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake
cmake --build build
./build/hello_claude
```

### Step 3: Understand the Output

You should see Claude's response:

```
Four.
```

Congratulations! You've made your first Claude SDK call.

## Working with Options

The `ClaudeOptions` struct gives you fine-grained control:

```cpp
claude::ClaudeOptions opts;

// Model selection
opts.model = "claude-sonnet-4-5";

// System prompt
opts.system_prompt = "You are a helpful coding assistant.";

// Tool control
opts.allowed_tools = {"Read", "Write", "Bash"};
opts.disallowed_tools = {"WebSearch"};

// Permission modes
// - "default": Ask for permissions
// - "acceptEdits": Auto-accept file edits
// - "plan": Plan mode
// - "bypassPermissions": Skip all prompts
opts.permission_mode = "plan";

// Conversation limits
opts.max_turns = 10;

// Working directory
opts.working_directory = "/path/to/project";

// Partial streaming (get tokens as they arrive)
opts.include_partial_messages = true;

// Custom CLI flags
opts.extra_args["custom-flag"] = "value";

// Pin a specific Claude CLI binary (overrides PATH detection)
#ifdef _WIN32
opts.cli_path = "C:/Users/you/AppData/Roaming/npm/claude.cmd";
#else
opts.cli_path = "/usr/local/bin/claude";
#endif

// Setting sources (advanced)
// Note: The C++ SDK only emits --setting-sources when non-empty.
opts.setting_sources = {"user", "project"};
```

## Multi-turn Conversations

For interactive conversations, use `ClaudeClient`:

```cpp
#include <claude/claude.hpp>
#include <iostream>
#include <string>

int main() {
    claude::ClaudeOptions opts;
    opts.permission_mode = "bypassPermissions";

    // Create and connect client
    claude::ClaudeClient client(opts);
    client.connect();

    std::cout << "Connected to Claude. Type 'quit' to exit.\n\n";

    // Conversation loop
    std::string input;
    while (true) {
        std::cout << "You: ";
        std::getline(std::cin, input);

        if (input == "quit" || input.empty()) {
            break;
        }

        // Send message
        client.send_query(input);

        // Receive response
        std::cout << "Claude: ";
        for (const auto& msg : client.receive_messages()) {
            if (claude::is_assistant_message(msg)) {
                const auto& assistant = std::get<claude::AssistantMessage>(msg);
                std::cout << claude::get_text_content(assistant.content);
            }
            if (claude::is_result_message(msg)) {
                std::cout << "\n\n";
                break;  // End of response
            }
        }
    }

    // Clean up
    client.disconnect();
    std::cout << "Goodbye!\n";

    return 0;
}
```

## Error Handling

Always wrap SDK calls in try-catch blocks:

```cpp
#include <claude/claude.hpp>
#include <iostream>

int main() {
    try {
        claude::ClaudeOptions opts;
        opts.permission_mode = "bypassPermissions";

        auto messages = claude::query("Hello!", opts);

        for (const auto& msg : messages) {
            if (claude::is_assistant_message(msg)) {
                const auto& assistant = std::get<claude::AssistantMessage>(msg);
                std::cout << claude::get_text_content(assistant.content) << "\n";
            }
        }

    } catch (const claude::CLINotFoundError& e) {
        std::cerr << "Error: Claude CLI not found\n";
        std::cerr << "Install with: npm install -g @anthropic-ai/claude-code\n";
        return 1;

    } catch (const claude::ProcessError& e) {
        std::cerr << "Process error: " << e.what() << "\n";
        std::cerr << "Exit code: " << e.exit_code() << "\n";
        return 1;

    } catch (const claude::JSONDecodeError& e) {
        std::cerr << "JSON parsing error: " << e.what() << "\n";
        return 1;

    } catch (const claude::ClaudeError& e) {
        std::cerr << "Claude SDK error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
```

## Next Steps

Now that you have the basics, explore more features:

1. **[Examples](../examples/)** - See working code examples
   - `streaming_client.cpp` - Full conversation client
   - `streaming_with_hooks.cpp` - Using hooks
   - `tool_permissions.cpp` - Tool permission callbacks

2. **[API Reference](api-reference.md)** - Complete API documentation

3. **[Troubleshooting](troubleshooting.md)** - Common issues and solutions

## Common Patterns

### Pattern 1: Simple Question & Answer

```cpp
auto messages = claude::query("Explain async/await in C++", opts);
for (const auto& msg : messages) {
    if (claude::is_assistant_message(msg)) {
        std::cout << claude::get_text_content(
            std::get<claude::AssistantMessage>(msg).content
        );
    }
}
```

### Pattern 2: Tool Use Monitoring

```cpp
for (const auto& msg : messages) {
    if (claude::is_assistant_message(msg)) {
        const auto& assistant = std::get<claude::AssistantMessage>(msg);
        for (const auto& block : assistant.content) {
            if (std::holds_alternative<claude::ToolUseBlock>(block)) {
                const auto& tool = std::get<claude::ToolUseBlock>(block);
                std::cout << "Using tool: " << tool.name << "\n";
            }
        }
    }
}
```

### Pattern 3: Extract Usage Info

```cpp
for (const auto& msg : messages) {
    if (claude::is_result_message(msg)) {
        const auto& result = std::get<claude::ResultMessage>(msg);
        std::cout << "Input tokens: " << result.result.usage.input_tokens << "\n";
        std::cout << "Output tokens: " << result.result.usage.output_tokens << "\n";
        std::cout << "Cost: $" << result.result.cost.total << "\n";
    }
}
```

## Tips for Success

1. **Always set permission_mode** - Use `"bypassPermissions"` for non-interactive scripts

2. **Handle all exception types** - Use specific catch blocks for better error messages

3. **Clean up resources** - Call `client.disconnect()` or use RAII patterns

4. **Check is_result_message** - This signals the end of a response

5. **Use working_directory** - Set the correct working directory for file operations

6. **Test incrementally** - Start with simple queries before adding complexity

## Getting Help

- **GitHub Issues:** Report bugs or request features
- **Examples Directory:** See working code for common scenarios
- **Troubleshooting Guide:** Common issues and solutions

---

**Next:** [API Reference](api-reference.md) | [Troubleshooting](troubleshooting.md)
