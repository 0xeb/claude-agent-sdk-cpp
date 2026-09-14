#ifndef CLAUDE_SESSIONS_SESSION_SUMMARY_HPP
#define CLAUDE_SESSIONS_SESSION_SUMMARY_HPP

#include <claude/sessions/session_store.hpp>
#include <claude/types.hpp>
#include <optional>
#include <string>
#include <vector>

namespace claude::sessions
{

/// Fold a batch of appended entries into the running summary for ``key``.
/// Adapters call this from inside append() to maintain the per-session
/// SessionSummaryEntry sidecar without re-reading the transcript.
///
/// Do not call this for keys with a subpath — subagent transcripts must not
/// contribute to the main session's summary. Guard with
/// ``if (!key.subpath.has_value())`` before calling.
///
/// ``prev`` is the previous summary for the same key (or std::nullopt for the
/// first append). All derived state lives in the opaque ``data`` JSON object.
/// ``mtime`` is NOT touched by the fold — adapters must stamp it after
/// persisting using the same clock as list_sessions() mtimes.
///
/// Python: session_summary.py:fold_session_summary (line 112).
SessionSummaryEntry fold_session_summary(const std::optional<SessionSummaryEntry>& prev,
                                         const SessionKey& key,
                                         const std::vector<SessionStoreEntry>& entries);

/// Convert a SessionSummaryEntry into an SDKSessionInfo. Returns std::nullopt
/// for sidechain sessions or sessions with no extractable summary.
///
/// Python: session_summary.py:summary_entry_to_sdk_info (line 192).
std::optional<SDKSessionInfo>
summary_entry_to_sdk_info(const SessionSummaryEntry& entry,
                          const std::optional<std::string>& project_path);

/// Extracts the first meaningful user prompt from a JSONL head chunk.
/// Skips tool_result, isMeta, isCompactSummary, command-name messages, and
/// auto-generated patterns. Truncates to 200 characters.
///
/// Python: sessions.py:_extract_first_prompt_from_head (line 258).
std::string extract_first_prompt_from_head(const std::string& head);

/// Parse an ISO-8601 timestamp string to Unix epoch milliseconds. Returns
/// std::nullopt if the string is not a valid ISO-8601 timestamp.
///
/// Python: session_summary.py:_iso_to_epoch_ms (line 39).
std::optional<int64_t> iso_to_epoch_ms(const std::string& ts);

} // namespace claude::sessions

#endif // CLAUDE_SESSIONS_SESSION_SUMMARY_HPP
