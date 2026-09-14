#ifndef CLAUDE_SESSIONS_FREE_FUNCTIONS_HPP
#define CLAUDE_SESSIONS_FREE_FUNCTIONS_HPP

#include <claude/sessions/session_store.hpp>
#include <optional>
#include <string>
#include <vector>

namespace claude::sessions
{

/// List sessions backed by a SessionStore.
///
/// Returns sessions sorted by ``last_modified`` descending. Uses the store's
/// list_session_summaries() fast path when available (single batch call),
/// gap-filling with per-session load() for sessions whose sidecar is missing
/// or stale. Otherwise falls back to one load() per session.
///
/// Throws std::invalid_argument if the store implements neither
/// list_session_summaries nor list_sessions.
///
/// Python: sessions.py:list_sessions_from_store (line 1571).
std::vector<SDKSessionInfo>
list_sessions_from_store(SessionStore& store,
                         const std::string& directory = "",
                         std::optional<int> limit = std::nullopt,
                         int offset = 0);

/// Read metadata for a single session from a SessionStore.
///
/// Python: sessions.py:get_session_info_from_store (line 1728).
std::optional<SDKSessionInfo>
get_session_info_from_store(SessionStore& store,
                            const std::string& session_id,
                            const std::string& directory = "");

/// Read a session's conversation messages from a SessionStore.
///
/// Builds the conversation chain via parentUuid links and returns
/// user/assistant messages in chronological order. Returns empty for invalid
/// session_id or empty session.
///
/// Python: sessions.py:get_session_messages_from_store (line 1758).
std::vector<SessionMessage>
get_session_messages_from_store(SessionStore& store,
                                const std::string& session_id,
                                const std::string& directory = "",
                                std::optional<int> limit = std::nullopt,
                                int offset = 0);

/// List subagent IDs for a session.
///
/// Throws std::invalid_argument if the store does not implement list_subkeys.
///
/// Python: sessions.py:list_subagents_from_store (line 1795).
std::vector<std::string>
list_subagents_from_store(SessionStore& store,
                          const std::string& session_id,
                          const std::string& directory = "");

/// Read a subagent's conversation messages from a SessionStore.
///
/// Python: sessions.py:get_subagent_messages_from_store (line 1844).
std::vector<SessionMessage>
get_subagent_messages_from_store(SessionStore& store,
                                 const std::string& session_id,
                                 const std::string& agent_id,
                                 const std::string& directory = "",
                                 std::optional<int> limit = std::nullopt,
                                 int offset = 0);

} // namespace claude::sessions

#endif // CLAUDE_SESSIONS_FREE_FUNCTIONS_HPP
