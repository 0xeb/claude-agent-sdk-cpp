#ifndef CLAUDE_SESSIONS_FILE_SESSION_STORE_HPP
#define CLAUDE_SESSIONS_FILE_SESSION_STORE_HPP

#include <claude/sessions/session_store.hpp>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace claude::sessions
{

/// File-backed SessionStore that mirrors transcripts to disk as JSONL files.
///
/// Layout under ``root``:
///   <project_key>/<session_id>.jsonl              -- main transcript
///   <project_key>/<session_id>/<subpath>.jsonl    -- subagent / metadata
///   <project_key>/<session_id>.summary.json       -- summary sidecar
///
/// This adapter is provided as a C++ convenience — the Python SDK only ships
/// the in-memory reference store. The on-disk JSONL layout matches the
/// CLI's ``~/.claude/projects/`` directory shape so transcripts written here
/// are directly resumable by a CLI subprocess with CLAUDE_CONFIG_DIR pointed
/// at the parent of ``root``.
///
/// Thread-safe: all public methods acquire an internal mutex; concurrent
/// callers serialize on file I/O.
class FileSessionStore final : public SessionStore
{
  public:
    explicit FileSessionStore(std::filesystem::path root);
    ~FileSessionStore() override = default;

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

    /// Get the root directory of this store.
    const std::filesystem::path& root() const noexcept
    {
        return root_;
    }

  private:
    std::filesystem::path file_path_for(const SessionKey& key) const;
    std::filesystem::path summary_path_for(const std::string& project_key,
                                           const std::string& session_id) const;

    std::filesystem::path root_;
    mutable std::mutex mu_;
};

} // namespace claude::sessions

#endif // CLAUDE_SESSIONS_FILE_SESSION_STORE_HPP
