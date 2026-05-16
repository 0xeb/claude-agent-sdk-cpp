#ifndef CLAUDE_SESSIONS_TRANSCRIPT_MIRROR_BATCHER_HPP
#define CLAUDE_SESSIONS_TRANSCRIPT_MIRROR_BATCHER_HPP

#include <claude/sessions/session_store.hpp>
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace claude::sessions
{

constexpr std::size_t MAX_PENDING_ENTRIES = 500;
constexpr std::size_t MAX_PENDING_BYTES = 1u << 20; // 1 MiB
constexpr double SEND_TIMEOUT_SECONDS = 60.0;
constexpr int MIRROR_APPEND_MAX_ATTEMPTS = 3;

/// Optional path -> SessionKey resolver. Mirror frames identify their target
/// via a file path (matching the Python ``file_path_to_session_key`` helper);
/// callers wire a resolver appropriate to their environment. Returns
/// std::nullopt to drop the frame with a logged warning.
using FilePathToKey =
    std::function<std::optional<SessionKey>(const std::string& file_path)>;

/// Batching layer between transcript_mirror frames and a SessionStore.
///
/// The CLI subprocess emits ``{"type": "transcript_mirror", ...}`` frames
/// interleaved with normal SDK messages. The receive loop peels these off
/// and calls enqueue(); the batcher flushes them to store.append() either
/// when a result message arrives (explicit flush) or when the pending
/// buffer exceeds size thresholds (eager background flush).
///
/// The eager flush runs on a dedicated worker thread; the destructor cancels
/// the worker cleanly without spurious error logs (commit 9d2c650).
///
/// Adapter failures are retried up to MIRROR_APPEND_MAX_ATTEMPTS times with
/// short backoff; only the final failure is reported via on_error.
///
/// Thread-safe: enqueue/flush/close may be called from any thread.
///
/// Python: _internal/transcript_mirror_batcher.py:TranscriptMirrorBatcher.
class TranscriptMirrorBatcher
{
  public:
    using OnError = std::function<void(const std::optional<SessionKey>&, const std::string&)>;

    TranscriptMirrorBatcher(std::shared_ptr<SessionStore> store,
                            FilePathToKey resolver,
                            OnError on_error,
                            std::size_t max_pending_entries = MAX_PENDING_ENTRIES,
                            std::size_t max_pending_bytes = MAX_PENDING_BYTES);
    ~TranscriptMirrorBatcher();

    TranscriptMirrorBatcher(const TranscriptMirrorBatcher&) = delete;
    TranscriptMirrorBatcher& operator=(const TranscriptMirrorBatcher&) = delete;

    /// Buffer a mirror frame. Schedules an eager flush if size thresholds
    /// are exceeded. Non-blocking; never raises.
    void enqueue(const std::string& file_path,
                 std::vector<SessionStoreEntry> entries);

    /// Flush all pending entries synchronously. Blocks until the worker
    /// has drained the queue.
    void flush();

    /// Final flush before teardown. Never raises.
    void close();

  private:
    struct PendingFrame
    {
        std::string file_path;
        std::vector<SessionStoreEntry> entries;
        std::size_t bytes;
    };

    void worker_loop();
    void do_flush_locked(std::vector<PendingFrame> items);

    std::shared_ptr<SessionStore> store_;
    FilePathToKey resolver_;
    OnError on_error_;
    std::size_t max_entries_;
    std::size_t max_bytes_;

    std::mutex mu_;
    std::condition_variable cv_;
    std::deque<PendingFrame> pending_;
    std::size_t pending_entries_ = 0;
    std::size_t pending_bytes_ = 0;
    bool drain_requested_ = false;
    bool stop_ = false;
    // Generation counter incremented after every completed drain — flush()
    // waits for a generation > the one observed when it requested a drain.
    std::uint64_t generation_ = 0;
    std::thread worker_;
};

} // namespace claude::sessions

#endif // CLAUDE_SESSIONS_TRANSCRIPT_MIRROR_BATCHER_HPP
