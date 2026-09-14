#ifndef CLAUDE_SESSIONS_SESSION_IMPORT_HPP
#define CLAUDE_SESSIONS_SESSION_IMPORT_HPP

#include <claude/sessions/session_store.hpp>
#include <claude/sessions/transcript_mirror_batcher.hpp>
#include <filesystem>
#include <string>

namespace claude::sessions
{

/// Replay a local on-disk JSONL session transcript into a SessionStore.
///
/// Streams the file line-by-line and calls store.append() every ``batch_size``
/// entries (or 1 MiB of line bytes, whichever comes first). Sub-agent
/// transcripts under ``<session_id>/subagents/**`` are imported when
/// ``include_subagents`` is true.
///
/// Adapters should treat ``entry["uuid"]`` as an idempotency key so re-import
/// is duplicate-safe.
///
/// Throws std::invalid_argument for invalid session_id, std::runtime_error if
/// the file does not exist.
///
/// Python: _internal/session_import.py:import_session_to_store (line 28).
void import_session_to_store(const std::string& session_id,
                             SessionStore& store,
                             const std::filesystem::path& jsonl_path,
                             const std::string& project_key,
                             bool include_subagents = true,
                             std::size_t batch_size = MAX_PENDING_ENTRIES);

} // namespace claude::sessions

#endif // CLAUDE_SESSIONS_SESSION_IMPORT_HPP
