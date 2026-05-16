#include <claude/sessions/transcript_mirror_batcher.hpp>

#include <chrono>
#include <map>
#include <stdexcept>
#include <thread>

namespace claude::sessions
{

namespace
{
const std::chrono::milliseconds backoff_for(int attempt)
{
    // attempt is the 0-based index of the *next* attempt to make.
    // Match Python's MIRROR_APPEND_BACKOFF_S = (0.2, 0.8).
    switch (attempt)
    {
    case 1: return std::chrono::milliseconds(200);
    case 2: return std::chrono::milliseconds(800);
    default: return std::chrono::milliseconds(0);
    }
}
} // namespace

TranscriptMirrorBatcher::TranscriptMirrorBatcher(
    std::shared_ptr<SessionStore> store,
    FilePathToKey resolver,
    OnError on_error,
    std::size_t max_pending_entries,
    std::size_t max_pending_bytes)
    : store_(std::move(store)),
      resolver_(std::move(resolver)),
      on_error_(std::move(on_error)),
      max_entries_(max_pending_entries),
      max_bytes_(max_pending_bytes)
{
    if (!store_)
        throw std::invalid_argument("TranscriptMirrorBatcher: store must be non-null");
    if (!resolver_)
        throw std::invalid_argument("TranscriptMirrorBatcher: resolver must be non-null");
    worker_ = std::thread(&TranscriptMirrorBatcher::worker_loop, this);
}

TranscriptMirrorBatcher::~TranscriptMirrorBatcher()
{
    // Final flush + clean shutdown — no spurious error logs on cancellation
    // (matches Python commit 9d2c650). Exceptions from on_error / store are
    // swallowed since the local-disk transcript is already durable.
    try
    {
        close();
    }
    catch (...)
    {
    }
    {
        std::lock_guard<std::mutex> lk(mu_);
        stop_ = true;
        cv_.notify_all();
    }
    if (worker_.joinable())
        worker_.join();
}

void TranscriptMirrorBatcher::enqueue(const std::string& file_path,
                                      std::vector<SessionStoreEntry> entries)
{
    // Approximate wire size — one dump per frame, not per entry (Python parity).
    std::size_t entries_count = entries.size();
    std::string raw;
    try
    {
        json arr = entries;
        raw = arr.dump();
    }
    catch (...)
    {
        raw.clear();
    }
    std::size_t sz = raw.size();
    {
        std::lock_guard<std::mutex> lk(mu_);
        pending_.push_back({file_path, std::move(entries), sz});
        pending_entries_ += entries_count;
        pending_bytes_ += sz;
        if (pending_entries_ > max_entries_ || pending_bytes_ > max_bytes_)
        {
            drain_requested_ = true;
            cv_.notify_all();
        }
    }
}

void TranscriptMirrorBatcher::flush()
{
    std::unique_lock<std::mutex> lk(mu_);
    std::uint64_t target_gen = generation_ + 1;
    drain_requested_ = true;
    cv_.notify_all();
    cv_.wait(lk, [&] { return generation_ >= target_gen || stop_; });
}

void TranscriptMirrorBatcher::close()
{
    flush();
}

void TranscriptMirrorBatcher::worker_loop()
{
    for (;;)
    {
        std::vector<PendingFrame> items;
        {
            std::unique_lock<std::mutex> lk(mu_);
            cv_.wait(lk, [&] { return stop_ || drain_requested_; });
            if (stop_ && pending_.empty() && !drain_requested_)
                return;
            // Detach the pending buffer.
            items.assign(std::make_move_iterator(pending_.begin()),
                         std::make_move_iterator(pending_.end()));
            pending_.clear();
            pending_entries_ = 0;
            pending_bytes_ = 0;
            drain_requested_ = false;
        }

        // Perform the flush outside the lock so enqueues can keep
        // accumulating into a fresh buffer.
        try
        {
            if (!items.empty())
                do_flush_locked(std::move(items));
        }
        catch (...)
        {
            // Never raises — store.append errors are caught inside do_flush_locked;
            // any escape here is purely defensive.
        }

        {
            std::lock_guard<std::mutex> lk(mu_);
            ++generation_;
            cv_.notify_all();
        }
    }
}

void TranscriptMirrorBatcher::do_flush_locked(std::vector<PendingFrame> items)
{
    // Coalesce by file_path (Python parity).
    std::vector<std::string> path_order;
    std::map<std::string, std::vector<SessionStoreEntry>> by_path;
    for (auto& it : items)
    {
        auto found = by_path.find(it.file_path);
        if (found == by_path.end())
        {
            path_order.push_back(it.file_path);
            by_path.emplace(it.file_path, std::move(it.entries));
        }
        else
        {
            for (auto& e : it.entries)
                found->second.push_back(std::move(e));
        }
    }

    std::vector<std::pair<std::optional<SessionKey>, std::string>> errors;
    for (const auto& fp : path_order)
    {
        auto& entries = by_path[fp];
        if (entries.empty())
            continue;
        auto key = resolver_(fp);
        if (!key.has_value())
            continue; // unresolvable path, drop with warning (caller logs)

        std::exception_ptr last_err;
        bool ok = false;
        for (int attempt = 0; attempt < MIRROR_APPEND_MAX_ATTEMPTS; ++attempt)
        {
            if (attempt > 0)
                std::this_thread::sleep_for(backoff_for(attempt));
            try
            {
                store_->append(*key, entries);
                ok = true;
                break;
            }
            catch (...)
            {
                last_err = std::current_exception();
            }
        }
        if (!ok)
        {
            std::string msg = "store.append failed";
            try
            {
                if (last_err)
                    std::rethrow_exception(last_err);
            }
            catch (const std::exception& e)
            {
                msg = e.what();
            }
            catch (...)
            {
            }
            errors.emplace_back(key, msg);
        }
    }

    for (const auto& kv : errors)
    {
        try
        {
            if (on_error_)
                on_error_(kv.first, kv.second);
        }
        catch (...)
        {
            // Suppress — never raises from drain (Python contract).
        }
    }
}

} // namespace claude::sessions
