#ifndef CLAUDE_SESSIONS_SESSION_MUTATIONS_HPP
#define CLAUDE_SESSIONS_SESSION_MUTATIONS_HPP

#include <claude/sessions/session_store.hpp>
#include <optional>
#include <string>

namespace claude::sessions
{

/// Result of a fork operation.
struct ForkSessionResult
{
    std::string session_id; ///< UUID of the new forked session.
};

/// Validate a UUID v4-shaped string. Python: sessions.py:_validate_uuid.
bool validate_uuid(const std::string& maybe_uuid);

/// Sanitize a string by removing dangerous Unicode (zero-width, directional
/// marks, private-use characters, etc.). Iteratively applies a limited form
/// of NFKC + category filter — best-effort for CLI filter compatibility.
///
/// Python: session_mutations.py:_sanitize_unicode (line 737).
std::string sanitize_unicode(const std::string& value);

/// Compute the project_key for a directory (canonical path + sanitization).
/// Defaults to the current working directory when ``directory`` is empty.
///
/// Python: sessions.py:project_key_for_directory (line 1418).
std::string project_key_for_directory(const std::string& directory = "");

// ---------------------------------------------------------------------------
// SessionStore-backed mutations
// ---------------------------------------------------------------------------

/// Rename a session by appending a ``custom-title`` entry to a SessionStore.
/// Async in Python; sync in C++ (callers wrap in a thread if needed).
///
/// Throws std::invalid_argument for invalid session_id or empty title.
///
/// Python: session_mutations.py:rename_session_via_store (line 769).
void rename_session_via_store(SessionStore& store,
                              const std::string& session_id,
                              const std::string& title,
                              const std::string& directory = "");

/// Tag a session by appending a ``tag`` entry. Pass empty/std::nullopt to
/// clear the tag.
///
/// Python: session_mutations.py:tag_session_via_store (line 810).
void tag_session_via_store(SessionStore& store,
                           const std::string& session_id,
                           const std::optional<std::string>& tag,
                           const std::string& directory = "");

/// Delete a session from the store. No-op when the store does not implement
/// delete (WORM-friendly).
///
/// Python: session_mutations.py:delete_session_via_store (line 851).
void delete_session_via_store(SessionStore& store,
                              const std::string& session_id,
                              const std::string& directory = "");

/// Fork a session into a new branch with fresh UUIDs.
/// ``up_to_message_id`` slices the source transcript at that message
/// (inclusive); empty string forks the whole transcript.
/// ``title`` overrides the derived "(fork)" title.
///
/// Throws std::invalid_argument for invalid IDs / empty source / missing
/// cutoff.
///
/// Python: session_mutations.py:fork_session_via_store (line 885).
ForkSessionResult fork_session_via_store(SessionStore& store,
                                         const std::string& session_id,
                                         const std::string& directory = "",
                                         const std::string& up_to_message_id = "",
                                         const std::optional<std::string>& title = std::nullopt);

} // namespace claude::sessions

#endif // CLAUDE_SESSIONS_SESSION_MUTATIONS_HPP
