# Claude SDK for C++

A modern C++17 library for interacting with the Claude Code CLI, providing both a simple one-shot `query()` API and a full bidirectional client with control‑protocol support.

[![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20Linux%20%7C%20macOS-blue)]()
[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue)]()
[![License](https://img.shields.io/badge/license-MIT-green)]()

Disclaimer: This is an independent, unofficial port and is not affiliated with or endorsed by Anthropic, PBC.

## Features

- Simple `query()` API for one‑shot requests.
- `ClaudeClient` for multi‑turn, bidirectional conversations.
- Control protocol support (interrupts, modes, dynamic model switching).
- Hook system (e.g., PreToolUse, PostToolUse, UserPromptSubmit).
- Tool permission callbacks with rich suggestion support.
- Custom agent configurations.
- MCP template API (Python‑style tool definition with templates).
- Cross‑platform: Windows, Linux, macOS.
- Easy integration via CMake and vcpkg.
- Type‑safe modern C++17 design.
- Well‑tested: 117/117 unit + integration tests passing.

## Prerequisites

- C++17 compiler (MSVC 19.14+, GCC 7+, Clang 5+).
- CMake 3.20+.
- vcpkg (recommended for dependencies).
- Claude Code CLI `>= 2.0.0`:
  - `npm install -g @anthropic-ai/claude-code`
- Optional: `fastmcpp` (C++ port of `fastmcp`) for MCP‑focused examples and integration.

### CLI Path Resolution

The SDK discovers the Claude Code CLI in this order:

- `ClaudeOptions.cli_path` (explicit path).
- `CLAUDE_CLI_PATH` environment variable.
- `PATH` search for `claude` (or `claude.cmd` on Windows).

You can pin a specific CLI binary by setting `opts.cli_path`.

## Quick Start

### Configure and Build

```bash
git clone https://github.com/yourorg/ccsdk.git
cd ccsdk/claude-agent-sdk-cpp

cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j

# Run tests
ctest --test-dir build -C Release -j 4 --output-on-failure
```

### Your First Query

```cpp
#include <claude/claude.hpp>
#include <iostream>

int main() {
    claude::ClaudeOptions opts;
    opts.permission_mode = "bypassPermissions";

    for (const auto& msg : claude::query("What is 2+2?", opts)) {
        if (claude::is_assistant_message(msg)) {
            const auto& assistant = std::get<claude::AssistantMessage>(msg);
            std::cout << claude::get_text_content(assistant.content) << "\n";
        }
    }

    return 0;
}
```

### Multi‑turn Conversation

```cpp
#include <claude/claude.hpp>
#include <iostream>
#include <string>

int main() {
    claude::ClaudeOptions opts;
    opts.permission_mode = "bypassPermissions";

    claude::ClaudeClient client(opts);
    client.connect();

    std::cout << "Connected! Type 'quit' to exit.\n\n";

    std::string input;
    while (std::getline(std::cin, input) && input != "quit") {
        client.send_query(input);

        for (const auto& msg : client.receive_messages()) {
            if (claude::is_assistant_message(msg)) {
                const auto& assistant = std::get<claude::AssistantMessage>(msg);
                std::cout << claude::get_text_content(assistant.content);
            }
            if (claude::is_result_message(msg)) {
                std::cout << "\n\n";
                break;
            }
        }
    }

    client.disconnect();
    return 0;
}
```

### Working Directory and Files

You can set the working directory for file operations (mirrors the Python SDK’s `cwd` option):

```cpp
#include <claude/claude.hpp>

int main() {
    claude::ClaudeOptions opts;
    opts.permission_mode = "bypassPermissions";
    opts.working_directory = "C:/path/to/project"; // or "/path/to/project" on POSIX

    for (const auto& msg : claude::query("List files in the project root", opts)) {
        // handle messages...
    }
}
```

Notes:

- Use an absolute path that exists and is accessible.
- See `docs/getting-started.md` for more working‑directory and file‑related guidance.

## Configuration Highlights

`ClaudeOptions` roughly mirrors the Python SDK’s options:

- Model selection: `model`, `fallback_model`, `max_thinking_tokens`, `max_budget_usd`, etc.
- System prompts: `system_prompt` or `system_prompt_append`.
- Tool control: `allowed_tools`, `disallowed_tools`.
- Permission modes: `"default"`, `"acceptEdits"`, `"plan"`, `"bypassPermissions"`.
- Session management: `continue_conversation`, `resume`.
- CLI options: `cli_path`, `working_directory`, `include_partial_messages`, `extra_args`, and
  hardening flags: `require_explicit_cli` (or `CLAUDE_AGENT_SDK_REQUIRE_EXPLICIT_CLI`) to disable
  PATH/home discovery, and `inherit_environment` (or `CLAUDE_AGENT_SDK_STRIP_ENV`) to prevent
  forwarding the parent environment to the CLI.
- Observability: `stderr_callback`, buffer limits, etc.

See `include/claude/types.hpp` and `docs/api-reference.md` for full details.

## Documentation

- `docs/getting-started.md` – installation, configuration, and first steps.
- `docs/api-reference.md` – complete API reference.
- `docs/tool-reference.md` – built‑in tools and security guidance.
- `docs/troubleshooting.md` – common issues and resolutions.
- `examples/` – working example programs.

## Status & Parity

- **Current version:** 0.1.15 (see `include/claude/version.hpp`).
- **Status:** Production ready.
- **Parity:** Designed to match the Python Claude Agent SDK 0.1.15 API, behavior, and ergonomics, including MCP template APIs and control protocol.
- **Tests:** See `kb/status.md` in the management repo for up-to-date test counts and parity details.

**Canonical rule:** The Python `claude-agent-sdk` is the canonical reference. This C++ port is expected to track its behavior and API (currently aligned with Python v0.1.15).

Upstream sync checkpoints: `docs/upstream-sync.md`.

## License

Licensed under the MIT License. See `LICENSE` and `NOTICE` for details.

This is a C++ port of [claude-agent-sdk-python](https://github.com/anthropics/claude-agent-sdk-python) by Anthropic, PBC.

## Support

- Issues: use the GitHub issue tracker for this repository.
- Documentation: see `docs/`.
- Examples: see `examples/`.
