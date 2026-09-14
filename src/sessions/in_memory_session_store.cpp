#include <claude/sessions/in_memory_session_store.hpp>
#include <claude/sessions/session_summary.hpp>

#include <algorithm>
#include <chrono>

namespace claude::sessions
{

std::string session_key_to_string(const SessionKey& key)
{
    std::string out = key.project_key + "/" + key.session_id;
    if (key.subpath.has_value() && !key.subpath->empty())
    {
        out += "/";
        out += *key.subpath;
    }
    return out;
}

namespace
{
int64_t now_ms()
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}
} // namespace

InMemorySessionStore::InMemorySessionStore() = default;

int64_t InMemorySessionStore::next_mtime_locked()
{
    int64_t t = now_ms();
    if (t <= last_mtime_)
        t = last_mtime_ + 1;
    last_mtime_ = t;
    return t;
}

void InMemorySessionStore::append(const SessionKey& key,
                                  const std::vector<SessionStoreEntry>& entries)
{
    std::lock_guard<std::mutex> lk(mu_);
    std::string k = session_key_to_string(key);
    auto& bucket = store_[k];
    bucket.insert(bucket.end(), entries.begin(), entries.end());
    int64_t t = next_mtime_locked();

    // Maintain per-session summary sidecar (main transcripts only).
    if (!key.subpath.has_value())
    {
        auto sk = std::make_pair(key.project_key, key.session_id);
        auto it = summaries_.find(sk);
        std::optional<SessionSummaryEntry> prev;
        if (it != summaries_.end())
            prev = it->second;
        SessionSummaryEntry folded = fold_session_summary(prev, key, entries);
        folded.mtime = t;
        summaries_[sk] = std::move(folded);
    }
    mtimes_[k] = t;
}

std::optional<std::vector<SessionStoreEntry>>
InMemorySessionStore::load(const SessionKey& key)
{
    std::lock_guard<std::mutex> lk(mu_);
    auto it = store_.find(session_key_to_string(key));
    if (it == store_.end())
        return std::nullopt;
    return it->second;
}

std::vector<SessionStoreListEntry>
InMemorySessionStore::list_sessions(const std::string& project_key)
{
    std::lock_guard<std::mutex> lk(mu_);
    std::vector<SessionStoreListEntry> out;
    std::string prefix = project_key + "/";
    for (const auto& kv : store_)
    {
        if (kv.first.size() <= prefix.size() ||
            kv.first.compare(0, prefix.size(), prefix) != 0)
            continue;
        std::string rest = kv.first.substr(prefix.size());
        // main transcripts have no second '/'
        if (rest.find('/') != std::string::npos)
            continue;
        SessionStoreListEntry e;
        e.session_id = rest;
        auto mit = mtimes_.find(kv.first);
        e.mtime = mit != mtimes_.end() ? mit->second : 0;
        out.push_back(std::move(e));
    }
    return out;
}

std::vector<SessionSummaryEntry>
InMemorySessionStore::list_session_summaries(const std::string& project_key)
{
    std::lock_guard<std::mutex> lk(mu_);
    std::vector<SessionSummaryEntry> out;
    for (const auto& kv : summaries_)
    {
        if (kv.first.first == project_key)
            out.push_back(kv.second);
    }
    return out;
}

void InMemorySessionStore::delete_session(const SessionKey& key)
{
    std::lock_guard<std::mutex> lk(mu_);
    std::string k = session_key_to_string(key);
    store_.erase(k);
    mtimes_.erase(k);
    if (!key.subpath.has_value())
    {
        summaries_.erase(std::make_pair(key.project_key, key.session_id));
        std::string prefix = key.project_key + "/" + key.session_id + "/";
        for (auto it = store_.begin(); it != store_.end();)
        {
            if (it->first.compare(0, prefix.size(), prefix) == 0)
            {
                mtimes_.erase(it->first);
                it = store_.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }
}

std::vector<std::string>
InMemorySessionStore::list_subkeys(const SessionListSubkeysKey& key)
{
    std::lock_guard<std::mutex> lk(mu_);
    std::string prefix = key.project_key + "/" + key.session_id + "/";
    std::vector<std::string> out;
    for (const auto& kv : store_)
    {
        if (kv.first.size() > prefix.size() &&
            kv.first.compare(0, prefix.size(), prefix) == 0)
            out.push_back(kv.first.substr(prefix.size()));
    }
    return out;
}

size_t InMemorySessionStore::size() const
{
    std::lock_guard<std::mutex> lk(mu_);
    size_t count = 0;
    for (const auto& kv : store_)
    {
        auto pos = kv.first.find('/');
        if (pos != std::string::npos &&
            kv.first.find('/', pos + 1) == std::string::npos)
            ++count;
    }
    return count;
}

void InMemorySessionStore::clear()
{
    std::lock_guard<std::mutex> lk(mu_);
    store_.clear();
    mtimes_.clear();
    summaries_.clear();
    last_mtime_ = 0;
}

} // namespace claude::sessions
