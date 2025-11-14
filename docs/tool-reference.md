# Claude Code Built-in Tools Reference

This document lists all built-in tools available in Claude Code CLI and how to control them using the C++ SDK.

## Overview

Claude Code comes with a rich set of built-in tools that enable Claude to interact with your system. When using the C++ SDK, you can control which tools are available using the `allowed_tools` and `disallowed_tools` options.

## Tool Categories

### 📁 File Operations

#### `Read`
**Description:** Read file contents from the filesystem
**Capabilities:**
- Read text files (source code, JSON, markdown, etc.)
- Read images (PNG, JPG, etc.) - Claude can see images
- Read PDFs - Extracts text and visual content
- Read Jupyter notebooks (.ipynb) - Shows all cells with outputs

**Example Use Cases:**
- Analyzing source code
- Reviewing documentation
- Inspecting configuration files
- Understanding notebook implementations

**C++ Usage:**
```cpp
// Allow only read operations
opts.allowed_tools = {"Read"};

// Or disallow if building a code-generation-only bot
opts.disallowed_tools = {"Read"};
```

---

#### `Write`
**Description:** Create new files or overwrite existing files
**Capabilities:**
- Write text content to any path
- Create directories if needed (when supported by CLI)
- Overwrites existing files without warning

**Security Note:** Write operations can be dangerous. Use with caution or implement `tool_permission_callback` to validate paths.

**C++ Usage:**
```cpp
// Dangerous: Allow unrestricted writes
opts.allowed_tools = {"Write"};

// Better: Use tool permission callback for safety
opts.tool_permission_callback = [](const std::string& tool_name,
                                   const claude::json& input,
                                   const claude::ToolPermissionContext& ctx) {
    if (tool_name == "Write") {
        std::string path = input["file_path"];
        // Only allow writes to safe directory
        if (path.find("./output/") != 0) {
            return claude::PermissionResultDeny{"deny", "Only ./output/ allowed"};
        }
    }
    return claude::PermissionResultAllow{};
};
```

---

#### `Edit`
**Description:** Perform exact string replacements in existing files
**Capabilities:**
- Find and replace exact string matches
- Supports multi-line replacements
- Preserves file encoding and line endings

**C++ Usage:**
```cpp
opts.allowed_tools = {"Read", "Edit"}; // Common combination
```

---

#### `NotebookEdit`
**Description:** Edit Jupyter notebook (.ipynb) cells specifically
**Capabilities:**
- Replace cell contents
- Insert new cells
- Delete cells
- Supports both code and markdown cells

**C++ Usage:**
```cpp
// Enable for Jupyter notebook workflows
opts.allowed_tools = {"Read", "NotebookEdit"};
```

---

### 🔍 Search & Discovery

#### `Glob`
**Description:** Fast file pattern matching
**Capabilities:**
- Find files by pattern (e.g., `**/*.cpp`, `src/**/*.h`)
- Recursive directory traversal
- Returns paths sorted by modification time
- Very fast even on large codebases

**Example Patterns:**
- `**/*.cpp` - All C++ files recursively
- `src/**/*.{h,hpp}` - All headers in src/
- `test_*.cpp` - Test files in current directory

**C++ Usage:**
```cpp
// Essential for codebase navigation
opts.allowed_tools = {"Glob", "Read"};
```

---

#### `Grep`
**Description:** Powerful content search using ripgrep
**Capabilities:**
- Full regex support
- Search across entire codebase
- Filter by file type or glob pattern
- Context lines (before/after matches)
- Case-sensitive or case-insensitive
- Output modes: content, files, or counts

**C++ Usage:**
```cpp
// Allow code exploration
opts.allowed_tools = {"Grep", "Glob", "Read"};
```

---

### ⚙️ Execution & Shell

#### `Bash`
**Description:** Execute shell commands in a persistent session
**Capabilities:**
- Run any shell command
- Persistent environment across commands
- Background execution support
- Capture stdout and stderr
- Works on Linux, macOS, Windows (WSL, Git Bash, PowerShell)

**Security Warning:** `Bash` is extremely powerful and can modify your entire system. Use with extreme caution.

**C++ Usage:**
```cpp
// DANGEROUS: Full shell access
opts.allowed_tools = {"Bash"};

// SAFER: Block dangerous commands via callback
opts.tool_permission_callback = [](const std::string& tool_name,
                                   const claude::json& input,
                                   const claude::ToolPermissionContext& ctx) {
    if (tool_name == "Bash") {
        std::string cmd = input["command"];

        // Block destructive commands
        std::vector<std::string> dangerous = {"rm -rf", "dd if=", "mkfs", "sudo"};
        for (const auto& pattern : dangerous) {
            if (cmd.find(pattern) != std::string::npos) {
                return claude::PermissionResultDeny{"deny", "Dangerous command blocked"};
            }
        }
    }
    return claude::PermissionResultAllow{};
};
```

---

#### `BashOutput`
**Description:** Retrieve output from background bash shells
**Capabilities:**
- Monitor long-running commands
- Get incremental output
- Filter output with regex

**C++ Usage:**
```cpp
// Enable background command monitoring
opts.allowed_tools = {"Bash", "BashOutput"};
```

---

#### `KillShell`
**Description:** Terminate running background shells
**Capabilities:**
- Kill shells by ID
- Cleanup hung processes

**C++ Usage:**
```cpp
opts.allowed_tools = {"Bash", "BashOutput", "KillShell"};
```

---

### 🌐 Web Access

#### `WebSearch`
**Description:** Search the web for current information
**Capabilities:**
- Perform web searches
- Get up-to-date information beyond Claude's knowledge cutoff
- Domain filtering (allow/block specific sites)

**Note:** Only available in certain regions (e.g., US)

**C++ Usage:**
```cpp
// Enable web research
opts.allowed_tools = {"WebSearch", "WebFetch"};
```

---

#### `WebFetch`
**Description:** Fetch and analyze content from URLs
**Capabilities:**
- Download web pages
- Convert HTML to markdown
- Process content with AI
- Handle redirects

**C++ Usage:**
```cpp
opts.allowed_tools = {"WebFetch"};
```

---

### 📋 Workflow & Task Management

#### `TodoWrite`
**Description:** Create and manage structured task lists
**Capabilities:**
- Track progress on complex tasks
- Mark tasks as pending/in_progress/completed
- Organize multi-step workflows
- Visible to user in UI

**C++ Usage:**
```cpp
// Enable task tracking (useful for complex projects)
opts.allowed_tools = {"TodoWrite"};
```

---

#### `Task`
**Description:** Launch specialized sub-agents for complex tasks
**Capabilities:**
- Spawn autonomous agents
- Specialized agent types:
  - `general-purpose` - Complex multi-step tasks
  - `Explore` - Fast codebase exploration
  - `statusline-setup` - Configure statusline
  - `output-style-setup` - Create output styles
- Agents run independently and return results

**C++ Usage:**
```cpp
// Enable sub-agent delegation
opts.allowed_tools = {"Task"};
```

**Important:** The `Task` tool is powerful for breaking down complex work. Each agent has access to its own subset of tools.

---

#### `SlashCommand`
**Description:** Execute custom slash commands defined in `.claude/commands/`
**Capabilities:**
- Run user-defined commands
- Expand to prompts
- Custom workflow automation

**C++ Usage:**
```cpp
opts.allowed_tools = {"SlashCommand"};
```

---

#### `ExitPlanMode`
**Description:** Exit planning mode and transition to implementation
**Capabilities:**
- Signal readiness to code
- Transition between planning and execution phases

**C++ Usage:**
```cpp
// Usually allowed by default
opts.allowed_tools = {"ExitPlanMode"};
```

---

## Common Tool Configurations

### Read-Only Mode (Safe Exploration)
```cpp
claude::ClaudeOptions opts;
opts.allowed_tools = {"Read", "Glob", "Grep"};
```

### Code Review Mode
```cpp
opts.allowed_tools = {"Read", "Glob", "Grep", "TodoWrite"};
```

### Full Development Mode
```cpp
opts.allowed_tools = {
    "Read", "Write", "Edit",
    "Glob", "Grep",
    "Bash",
    "TodoWrite", "Task"
};
```

### Sandboxed Mode (Deny Dangerous Operations)
```cpp
opts.disallowed_tools = {"Bash", "Write", "Edit", "WebFetch"};
// All other tools allowed by default
```

### Custom Tool Control (No Built-in Tools)
```cpp
// Disable ALL built-in tools
opts.disallowed_tools = {
    "Bash", "Read", "Write", "Edit", "NotebookEdit",
    "Glob", "Grep",
    "WebFetch", "WebSearch",
    "TodoWrite", "Task", "SlashCommand", "ExitPlanMode",
    "BashOutput", "KillShell"
};

// Now Claude only has custom tools you define via system prompt or MCP
```

---

## MCP Tools

In addition to built-in tools, Claude Code supports **MCP (Model Context Protocol)** tools:

- MCP servers provide additional tools (databases, APIs, etc.)
- MCP tool names are prefixed with `mcp__<server>__<tool>`
- Example: `mcp__calc__add`, `mcp__github__create_issue`

**C++ Usage:**
```cpp
// Allow specific MCP tools
opts.allowed_tools = {
    "mcp__calc__add",
    "mcp__calc__subtract",
    "mcp__github__create_issue"
};
```

See the MCP documentation for more details on creating custom MCP servers.

---

## Tool Permission Callbacks

For fine-grained control, use `tool_permission_callback` instead of allow/deny lists:

```cpp
opts.tool_permission_callback = [](
    const std::string& tool_name,
    const claude::json& input,
    const claude::ToolPermissionContext& context
) -> claude::PermissionResult {

    // 1. Log all tool usage
    std::cout << "Tool requested: " << tool_name << "\n";

    // 2. Allow read-only operations
    if (tool_name == "Read" || tool_name == "Glob" || tool_name == "Grep") {
        return claude::PermissionResultAllow{};
    }

    // 3. Block dangerous Bash commands
    if (tool_name == "Bash") {
        std::string cmd = input["command"];
        if (cmd.find("rm -rf") != std::string::npos) {
            return claude::PermissionResultDeny{
                "deny",
                "Dangerous command blocked: rm -rf",
                true  // interrupt conversation
            };
        }
    }

    // 4. Sandbox file writes
    if (tool_name == "Write" || tool_name == "Edit") {
        std::string path = input.value("file_path", "");
        if (path.find("./safe/") != 0) {
            // Redirect to safe directory
            claude::json modified = input;
            modified["file_path"] = "./safe/" + path;
            return claude::PermissionResultAllow{
                "allow",
                modified  // updated_input
            };
        }
    }

    // 5. Interactive approval for unknown tools
    std::cout << "Allow tool '" << tool_name << "'? (y/N): ";
    std::string response;
    std::getline(std::cin, response);

    if (response == "y" || response == "yes") {
        return claude::PermissionResultAllow{};
    } else {
        return claude::PermissionResultDeny{"deny", "User denied permission"};
    }
};
```

See `examples/tool_permissions.cpp` for a complete example.

---

## Best Practices

### 1. **Principle of Least Privilege**
Only enable tools necessary for your use case.

```cpp
// Good: Minimal tools for code review
opts.allowed_tools = {"Read", "Glob", "Grep"};

// Bad: Everything enabled by default
// (default behavior if no allowed_tools specified)
```

### 2. **Use Callbacks for Dynamic Control**
Rather than static allow/deny lists, use callbacks for context-aware decisions:

```cpp
opts.tool_permission_callback = my_security_callback;
```

### 3. **Combine Allow Lists with Callbacks**
Allow certain tools freely, use callbacks for others:

```cpp
opts.allowed_tools = {"Read", "Glob", "Grep"};  // Auto-approved
opts.tool_permission_callback = [](/*...*/) {
    // Only called for tools not in allowed_tools
    // Handle Write, Edit, Bash, etc.
};
```

### 4. **Sandbox File Operations**
Redirect writes to safe directories:

```cpp
if (tool_name == "Write") {
    claude::json safe_input = input;
    safe_input["file_path"] = "./sandbox/" + /* basename */;
    return claude::PermissionResultAllow{"allow", safe_input};
}
```

### 5. **Audit Logging**
Always log tool usage for security audits:

```cpp
std::ofstream audit_log("tool_audit.log", std::ios::app);
opts.tool_permission_callback = [&audit_log](/*...*/) {
    audit_log << timestamp() << " | " << tool_name
              << " | " << input.dump() << "\n";
    // ... permission logic
};
```

---

## Tool String Constants

For convenience, you can define constants in your code:

```cpp
namespace claude::tools {
    // File operations
    constexpr const char* Read = "Read";
    constexpr const char* Write = "Write";
    constexpr const char* Edit = "Edit";
    constexpr const char* NotebookEdit = "NotebookEdit";

    // Search
    constexpr const char* Glob = "Glob";
    constexpr const char* Grep = "Grep";

    // Execution
    constexpr const char* Bash = "Bash";
    constexpr const char* BashOutput = "BashOutput";
    constexpr const char* KillShell = "KillShell";

    // Web
    constexpr const char* WebSearch = "WebSearch";
    constexpr const char* WebFetch = "WebFetch";

    // Workflow
    constexpr const char* TodoWrite = "TodoWrite";
    constexpr const char* Task = "Task";
    constexpr const char* SlashCommand = "SlashCommand";
    constexpr const char* ExitPlanMode = "ExitPlanMode";
}

// Usage:
opts.allowed_tools = {
    claude::tools::Read,
    claude::tools::Glob,
    claude::tools::Grep
};
```

---

## See Also

- **API Reference:** `api-reference.md` - Full C++ SDK API documentation
- **Getting Started:** `getting-started.md` - Quick start guide
- **Examples:** `../examples/tool_permissions.cpp` - Tool control examples
- **Knowledge Base:** `../../kb/ccsdk/tools-and-interception.md` - Deep dive into tool system

---

**Last Updated:** 2025-10-15
**SDK Version:** 0.1.0
