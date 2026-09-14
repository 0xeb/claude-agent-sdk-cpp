# Changelog

## 0.2.82 — Parity with Python `claude-agent-sdk` v0.2.82

Tracking branch: `feature/parity-v0.2.82`.

### Phase 2A — Types foundation (`f5bf9bf`)
- `EffortLevel` enum + `xhigh` value, `ThinkingConfig` variant
  (Adaptive / Enabled / Disabled).
- `SDKSessionInfo`, `SessionKey`, `MirrorErrorMessage`.
- `HookEventMessage`, `RateLimitEvent` / `RateLimitInfo`, `DeferredToolUse`,
  `TaskStarted` / `TaskProgress` / `TaskNotification`, `McpServerStatus`.
- New options on `ClaudeOptions`: `strict_mcp_config`, `include_hook_events`,
  `skills`, `session_id`, `session_store`, `session_store_flush`, `thinking`.
- `AgentDefinition` expanded with `background`, `effort`, `permissionMode`,
  `disallowedTools`, `maxTurns`, `initialPrompt`, `skills`, `memory`,
  `mcpServers`.
- `ResultMessage` gains `api_error_status`, `stop_reason`,
  `deferred_tool_use`; `AssistantMessage` preserves per-turn usage.
- `ToolPermissionContext` enrichment + new hook decision `"defer"`.

### Phase 3A — Sessions subsystem (`26e69bf`)
- `claude::SessionStore` abstract base (TS-parity protocol) +
  `InMemorySessionStore` / `FileSessionStore` first-party adapters.
- `session_resume`, `session_mutations` (rename / tag / delete / fork),
  `session_summary`, `session_import`, `transcript_mirror_batcher`,
  `session_store_validation`.
- Store-backed free functions: `list_sessions_from_store`,
  `get_session_info_from_store`, `get_session_messages_from_store`,
  `list_subagents_from_store`, `get_subagent_messages_from_store`.
- Public conformance harness in `claude/testing/session_store_conformance.hpp`
  for downstream adapter authors.

### Phase 4A — Client / transport (`67d454b`)
- `ClaudeClient::get_context_usage()`, MCP control methods
  (`reconnect_mcp_server`, `toggle_mcp_server`, `stop_task`).
- Cancel-request handling, string-prompt connect parity, stderr-callback
  isolation, subprocess cleanup, graceful shutdown.
- `server_tool_use` / `advisor_tool_result` parser blocks, unknown-type
  tolerance, fine-grained streaming when `include_partial_messages = true`.

### Phase 5 — Examples + integration tests + version bump
- New mirrored examples (Python-parity names):
  `examples/session_store_usage.cpp`, `examples/session_resume.cpp`,
  `examples/hook_events.cpp`, `examples/context_usage.cpp`.
- New C++-specific extension example:
  `examples/extensions/file_session_store_demo.cpp` (templated MCP API only;
  no `..` parent paths in includes).
- New integration test binary entries in `tests/integration/test_sessions_e2e.cpp`
  covering end-to-end append/resume/fork, mirror batcher → store, conformance
  harness against both first-party adapters, JSONL import → resume, and
  cross-feature hook-event round-trip through the store.
- Version bumped to `0.2.82` in `include/claude/version.hpp`,
  `CMakeLists.txt`, and `vcpkg.json`.
