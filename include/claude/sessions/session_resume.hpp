#ifndef CLAUDE_SESSIONS_SESSION_RESUME_HPP
#define CLAUDE_SESSIONS_SESSION_RESUME_HPP

#include <claude/sessions/session_store.hpp>
#include <claude/sessions/transcript_mirror_batcher.hpp>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>

namespace claude::sessions
{

/// Result of materialize_resume_session — describes a temp CLAUDE_CONFIG_DIR
/// laid out like ``~/.claude/`` and the cleanup callback that removes it.
///
/// Python: _internal/session_resume.py:MaterializedResume (line 50).
struct MaterializedResume
{
    std::filesystem::path config_dir;
    std::string resume_session_id;
    std::function<void()> cleanup;
};

/// Materialize a SessionStore-backed resume into a temp CLAUDE_CONFIG_DIR.
///
/// Loads the session entries from ``store`` and writes them as JSONL into
/// ``<config_dir>/projects/<project_key>/<session_id>.jsonl``. Subkeys
/// (subagent transcripts, agent_metadata) are materialized under the
/// matching subdirectory layout.
///
/// Returns the populated MaterializedResume. The caller is responsible for
/// invoking ``result.cleanup()`` after the subprocess exits.
///
/// Throws std::runtime_error when the session is not found.
///
/// Python: _internal/session_resume.py:materialize_resume_session (line 200).
MaterializedResume materialize_resume_session(SessionStore& store,
                                              const std::string& project_key,
                                              const std::string& session_id);

/// Construct a TranscriptMirrorBatcher whose path resolver maps temp-dir
/// paths back to SessionKeys. Used by the transport when wiring a session_store
/// to the running subprocess.
///
/// Python: _internal/session_resume.py:build_mirror_batcher (line 89).
std::shared_ptr<TranscriptMirrorBatcher>
build_mirror_batcher(std::shared_ptr<SessionStore> store,
                     const std::filesystem::path& projects_dir,
                     TranscriptMirrorBatcher::OnError on_error);

} // namespace claude::sessions

#endif // CLAUDE_SESSIONS_SESSION_RESUME_HPP
