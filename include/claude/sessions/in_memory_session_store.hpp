#ifndef CLAUDE_SESSIONS_IN_MEMORY_SESSION_STORE_HPP
#define CLAUDE_SESSIONS_IN_MEMORY_SESSION_STORE_HPP

#include <claude/sessions/session_store.hpp>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace claude::sessions
{

/// In-memory reference SessionStore for testing and development.
///
/// Stores entries in a map keyed by composite ``project_key/session_id``
/// (with optional ``/subpath`` suffix). Not suitable for production — data is
/// lost when the process exits. Maintains per-session
/// SessionSummaryEntry sidecars incrementally inside append() so
/// list_session_summaries() never re-reads.
///
/// Thread-safe: all public methods acquire an internal mutex.
///
/// Python: _internal/session_store.py:InMemorySessionStore (line 35).
class InMemorySessionStore final : public SessionStore
{
  public:
    InMemorySessionStore();
    ~InMemorySessionStore() override = default;

    unsigned capabilities() const noexcept override
    {
        return SessionStoreCapability::All;
    }

    void append(const SessionKey& key,
                const std::vector<SessionStoreEntry>& entries) override;
    std::optional<std::vector<SessionStoreEntry>> load(const SessionKey& key) override;
    std::vector<SessionStoreListEntry> list_sessions(const std::string& project_key) override;
    std::vector<SessionSummaryEntry>
    list_session_summaries(const std::string& project_key) override;
    void delete_session(const SessionKey& key) override;
    std::vector<std::string> list_subkeys(const SessionListSubkeysKey& key) override;

    // ----- Test helpers (parity with Python InMemorySessionStore) ----------

    /// Number of stored main-transcript sessions (no subpath).
    size_t size() const;

    /// Clear all stored state.
    void clear();

  private:
    int64_t next_mtime_locked();

    mutable std::mutex mu_;
    // composite key "project_key/session_id" or "project_key/session_id/subpath"
    std::map<std::string, std::vector<SessionStoreEntry>> store_;
    std::map<std::string, int64_t> mtimes_;
    // (project_key, session_id) -> summary entry
    std::map<std::pair<std::string, std::string>, SessionSummaryEntry> summaries_;
    int64_t last_mtime_ = 0;
};

/// Build the composite map key for a SessionKey. Mirrors Python's
/// ``_key_to_string`` helper.
std::string session_key_to_string(const SessionKey& key);

} // namespace claude::sessions

#endif // CLAUDE_SESSIONS_IN_MEMORY_SESSION_STORE_HPP
