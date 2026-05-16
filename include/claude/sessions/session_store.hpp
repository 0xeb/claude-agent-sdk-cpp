#ifndef CLAUDE_SESSIONS_SESSION_STORE_HPP
#define CLAUDE_SESSIONS_SESSION_STORE_HPP

#include <claude/types.hpp>
#include <nlohmann/json.hpp>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace claude
{

/// One JSONL transcript line as observed by a SessionStore adapter.
/// Python: SessionStoreEntry (TypedDict). Modeled here as opaque JSON since
/// the union of transcript shapes is large and adapters should treat entries
/// as pass-through blobs.
using SessionStoreEntry = json;

/// Entry returned by SessionStore::list_sessions.
struct SessionStoreListEntry
{
    std::string session_id;
    /// Last-modified time in Unix epoch milliseconds.
    int64_t mtime = 0;
};

/// Incrementally-maintained session summary.
struct SessionSummaryEntry
{
    std::string session_id;
    /// Storage write time of the sidecar, in Unix epoch milliseconds.
    int64_t mtime = 0;
    /// Opaque SDK-owned summary state. Persist verbatim; do not interpret.
    json data;
};

/// Key argument to SessionStore::list_subkeys (no subpath).
struct SessionListSubkeysKey
{
    std::string project_key;
    std::string session_id;
};

/// Session metadata returned by list_sessions().
/// Python: SDKSessionInfo.
struct SDKSessionInfo
{
    std::string session_id;
    std::string summary;
    int64_t last_modified = 0;
    std::optional<int64_t> file_size = std::nullopt;
    std::optional<std::string> custom_title = std::nullopt;
    std::optional<std::string> first_prompt = std::nullopt;
    std::optional<std::string> git_branch = std::nullopt;
    std::optional<std::string> cwd = std::nullopt;
    std::optional<std::string> tag = std::nullopt;
    std::optional<int64_t> created_at = std::nullopt;
};

/// A user or assistant message from a session transcript.
/// Python: SessionMessage. type is "user" or "assistant".
struct SessionMessage
{
    std::string type;
    std::string uuid;
    std::string session_id;
    json message;
    std::optional<std::string> parent_tool_use_id = std::nullopt;
};

/// Abstract base class for session-store adapters. Mirrors the Python
/// SessionStore Protocol (commit 6e3d54f). Only append() and load() are
/// required; the remaining methods throw std::logic_error by default to
/// signal "not implemented" — call sites may probe these via dynamic_cast or
/// equivalent before invoking.
///
/// NOTE: Phase 2A defines this interface only. InMemorySessionStore and the
/// transcript mirror plumbing are landed in Phase 3A.
class SessionStore
{
  public:
    virtual ~SessionStore() = default;

    /// Mirror a batch of transcript entries (called AFTER local write succeeds).
    virtual void append(const SessionKey& key,
                        const std::vector<SessionStoreEntry>& entries) = 0;

    /// Load a full session for resume. Returns std::nullopt for keys that were
    /// never written.
    virtual std::optional<std::vector<SessionStoreEntry>> load(const SessionKey& key) = 0;

    /// List sessions for a project_key. Default: not implemented.
    virtual std::vector<SessionStoreListEntry> list_sessions(const std::string& /*project_key*/)
    {
        throw std::logic_error("SessionStore::list_sessions not implemented");
    }

    /// Return incrementally-maintained summaries for all sessions in one call.
    virtual std::vector<SessionSummaryEntry>
    list_session_summaries(const std::string& /*project_key*/)
    {
        throw std::logic_error("SessionStore::list_session_summaries not implemented");
    }

    /// Delete a session (cascades to subkeys when key has no subpath).
    virtual void delete_session(const SessionKey& /*key*/)
    {
        throw std::logic_error("SessionStore::delete_session not implemented");
    }

    /// List all subpath keys under a session (e.g. subagent transcripts).
    virtual std::vector<std::string> list_subkeys(const SessionListSubkeysKey& /*key*/)
    {
        throw std::logic_error("SessionStore::list_subkeys not implemented");
    }
};

} // namespace claude

#endif // CLAUDE_SESSIONS_SESSION_STORE_HPP
