# Changelog

All notable changes to the Claude SDK for C++ will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- Comprehensive documentation suite
  - README.md with quick start guide
  - Getting Started guide (docs/getting-started.md)
  - Troubleshooting guide (docs/troubleshooting.md)
- New examples:
  - conversation.cpp - Interactive multi-turn conversation
  - error_handling.cpp - Comprehensive error handling patterns
- CHANGELOG.md for tracking version history

## [0.1.0-dev] - 2025-10-12

### Added
- **Core Features**
  - `query()` function for one-shot requests
  - `ClaudeClient` class for bidirectional communication
  - Multi-turn conversation support with context preservation
  - Streaming message iteration

- **Control Protocol**
  - `interrupt()` - Stop current operation
  - `set_permission_mode()` - Change permission mode dynamically
  - `set_model()` - Switch models mid-conversation
  - Initialize handshake with CLI
  - Control request/response correlation

- **Hook System**
  - PreToolUse hook support
  - PostToolUse hook support
  - UserPromptSubmit hook support
  - Stop, SubagentStop, PreCompact hooks
  - Pattern-based matcher filtering
  - Multiple hooks per event type

- **Tool Permissions**
  - Rich callback API with ToolPermissionContext
  - Permission suggestions from CLI
  - Allow/Deny result types with optional input modification
  - Auto-wiring of permission_prompt_tool_name="stdio"

- **Advanced Options**
  - Custom agent definitions support
  - Extra CLI arguments with auto-prepending "--"
  - System prompt configuration
  - Tool allow/deny lists
  - Permission modes (default, acceptEdits, plan, bypassPermissions)
  - Working directory specification
  - Max turns limiting
  - Partial message streaming

- **Type System**
  - Full message type definitions (User, Assistant, System, Result, Stream)
  - Content blocks (Text, Thinking, ToolUse, ToolResult)
  - Usage and cost tracking
  - Raw JSON preservation for debugging

- **Platform Support**
  - Windows implementation (fully tested)
  - Linux/POSIX implementation (needs testing)
  - macOS support (needs testing)
  - CMake presets for all platforms

- **Testing**
  - 88 comprehensive tests
  - Unit tests for all components
  - Integration tests with real CLI
  - 100% test pass rate

- **Examples**
  - basic_query.cpp - Simple one-shot query
  - streaming_client.cpp - Multi-turn conversation
  - streaming_with_hooks.cpp - Hook system demonstration
  - tool_permissions.cpp - Permission callback example
  - multi_turn.cpp - Context-aware conversations
  - partial_streaming.cpp - Real-time streaming
  - query_with_tools.cpp - Tool control
  - conversation.cpp - Interactive CLI
  - error_handling.cpp - Error handling patterns

- **Build System**
  - CMake 3.20+ support
  - vcpkg integration
  - Multiple build presets (windows, linux, macos, vs2022)
  - Separate build directories per platform

### Fixed
- Tool permission callbacks now fire correctly with auto-wiring (2025-10-12)
- Safe null handling in PermissionUpdate::to_json (2025-10-12)
- Test suite structure (TEST/TEST_F mixing resolved) (2025-10-12)
- Agent definitions JSON serialization (2025-10-12)
- Extra args auto-prepending of "--" prefix (2025-10-12)
- Message buffering race condition (messages no longer dropped)
- Control protocol request/response correlation
- Hook callback ID registration and dispatch

### Security
- Input validation for all CLI arguments
- Safe JSON parsing with error handling
- Subprocess security (no shell injection)

### Known Limitations
- MCP (Model Context Protocol) support deferred to v0.2.0
- Linux/macOS testing pending (implementations complete but untested)
- Documentation API reference pending Doxygen generation

## [0.0.1] - 2025-10-01

### Added
- Initial project structure
- Basic subprocess management
- JSON message parsing
- CLI transport layer
- Minimal viable query() function

---

## Version History Summary

- **v0.1.0-dev** (2025-10-12): Full Python SDK parity (except MCP), 88 tests passing
- **v0.0.1** (2025-10-01): Initial MVP with basic query support

## Upgrade Guide

### From 0.0.1 to 0.1.0-dev

**Breaking Changes:**
- None - fully backward compatible

**New Features:**
- Control protocol methods on ClaudeClient
- Hook system (opt-in)
- Tool permission callbacks (opt-in)
- Advanced options (agents, extra_args)

**Recommended Updates:**
1. Add error handling for all calls (see examples/error_handling.cpp)
2. Consider using control protocol for dynamic behavior
3. Explore hook system for advanced workflows
4. Use tool permission callbacks for security-sensitive applications

## Future Roadmap

### v0.2.0 (Planned)
- MCP (Model Context Protocol) support
- In-process MCP servers
- SDK server registration
- JSONRPC routing
- Cross-platform testing and validation
- Performance optimizations

### v0.3.0 (Ideas)
- Async/await API
- Python bindings (pybind11)
- Package distribution (vcpkg, conan)
- CI/CD pipeline
- Benchmark suite

---

**Note:** This SDK follows the [Python Claude Agent SDK](https://github.com/anthropics/anthropic-sdk-python) structure and maintains parity with its features.
