#include <claude/sessions/file_session_store.hpp>
#include <claude/sessions/session_summary.hpp>

#include <chrono>
#include <fstream>
#include <sstream>
#include <system_error>

namespace claude::sessions
{

namespace fs = std::filesystem;

namespace
{
int64_t file_mtime_ms(const fs::path& p)
{
    std::error_code ec;
    auto ft = fs::last_write_time(p, ec);
    if (ec)
        return 0;
    // Use system_clock conversion (best-effort, not portable across stdlibs
    // but works on the major three).
    auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        ft - decltype(ft)::clock::now() + std::chrono::system_clock::now());
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               sctp.time_since_epoch())
        .count();
}
} // namespace

FileSessionStore::FileSessionStore(fs::path root) : root_(std::move(root))
{
    std::error_code ec;
    fs::create_directories(root_, ec);
}

fs::path FileSessionStore::file_path_for(const SessionKey& key) const
{
    fs::path p = root_ / key.project_key;
    if (key.subpath.has_value())
    {
        p /= key.session_id;
        // subpath may contain forward slashes — split and append portably.
        std::string sp = *key.subpath;
        size_t start = 0, pos;
        while ((pos = sp.find('/', start)) != std::string::npos)
        {
            p /= sp.substr(start, pos - start);
            start = pos + 1;
        }
        p /= sp.substr(start);
        p += ".jsonl";
    }
    else
    {
        p /= (key.session_id + ".jsonl");
    }
    return p;
}

fs::path FileSessionStore::summary_path_for(const std::string& project_key,
                                            const std::string& session_id) const
{
    return root_ / project_key / (session_id + ".summary.json");
}

void FileSessionStore::append(const SessionKey& key,
                              const std::vector<SessionStoreEntry>& entries)
{
    std::lock_guard<std::mutex> lk(mu_);
    if (entries.empty())
        return;
    fs::path path = file_path_for(key);
    std::error_code ec;
    fs::create_directories(path.parent_path(), ec);

    std::ofstream f(path, std::ios::app | std::ios::binary);
    if (!f.is_open())
        throw std::runtime_error("FileSessionStore: failed to open " + path.string());
    for (const auto& e : entries)
    {
        // Hoist "type" to front for byte-shape parity with disk path
        // (matches Python's _entries_to_jsonl helper).
        if (e.is_object() && e.contains("type"))
        {
            json reordered = json::object();
            reordered["type"] = e.at("type");
            for (auto it = e.begin(); it != e.end(); ++it)
            {
                if (it.key() != "type")
                    reordered[it.key()] = it.value();
            }
            f << reordered.dump() << '\n';
        }
        else
        {
            f << e.dump() << '\n';
        }
    }
    f.flush();
    f.close();

    if (!key.subpath.has_value())
    {
        // Update summary sidecar incrementally.
        fs::path spath = summary_path_for(key.project_key, key.session_id);
        std::optional<SessionSummaryEntry> prev;
        if (fs::exists(spath, ec))
        {
            std::ifstream sf(spath, std::ios::binary);
            try
            {
                json j;
                sf >> j;
                SessionSummaryEntry e;
                e.session_id = j.value("session_id", key.session_id);
                e.mtime = j.value("mtime", int64_t{0});
                e.data = j.value("data", json::object());
                prev = e;
            }
            catch (...)
            {
            }
        }
        SessionSummaryEntry folded = fold_session_summary(prev, key, entries);
        folded.mtime = file_mtime_ms(path);
        if (folded.mtime == 0)
            folded.mtime = std::chrono::duration_cast<std::chrono::milliseconds>(
                               std::chrono::system_clock::now().time_since_epoch())
                               .count();
        std::ofstream sf(spath, std::ios::trunc | std::ios::binary);
        json j;
        j["session_id"] = folded.session_id;
        j["mtime"] = folded.mtime;
        j["data"] = folded.data;
        sf << j.dump();
    }
}

std::optional<std::vector<SessionStoreEntry>>
FileSessionStore::load(const SessionKey& key)
{
    std::lock_guard<std::mutex> lk(mu_);
    fs::path path = file_path_for(key);
    std::error_code ec;
    if (!fs::exists(path, ec))
        return std::nullopt;
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open())
        return std::nullopt;
    std::vector<SessionStoreEntry> out;
    std::string line;
    while (std::getline(f, line))
    {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (line.empty())
            continue;
        try
        {
            out.push_back(json::parse(line));
        }
        catch (...)
        {
        }
    }
    return out;
}

std::vector<SessionStoreListEntry>
FileSessionStore::list_sessions(const std::string& project_key)
{
    std::lock_guard<std::mutex> lk(mu_);
    std::vector<SessionStoreListEntry> out;
    fs::path pdir = root_ / project_key;
    std::error_code ec;
    if (!fs::exists(pdir, ec) || !fs::is_directory(pdir, ec))
        return out;
    for (const auto& entry : fs::directory_iterator(pdir, ec))
    {
        if (!entry.is_regular_file())
            continue;
        const auto& p = entry.path();
        std::string name = p.filename().string();
        // Skip summary sidecars.
        const std::string suffix = ".jsonl";
        if (name.size() <= suffix.size())
            continue;
        if (name.compare(name.size() - suffix.size(), suffix.size(), suffix) != 0)
            continue;
        SessionStoreListEntry e;
        e.session_id = name.substr(0, name.size() - suffix.size());
        e.mtime = file_mtime_ms(p);
        out.push_back(std::move(e));
    }
    return out;
}

std::vector<SessionSummaryEntry>
FileSessionStore::list_session_summaries(const std::string& project_key)
{
    std::lock_guard<std::mutex> lk(mu_);
    std::vector<SessionSummaryEntry> out;
    fs::path pdir = root_ / project_key;
    std::error_code ec;
    if (!fs::exists(pdir, ec))
        return out;
    const std::string suffix = ".summary.json";
    for (const auto& entry : fs::directory_iterator(pdir, ec))
    {
        if (!entry.is_regular_file())
            continue;
        std::string name = entry.path().filename().string();
        if (name.size() <= suffix.size())
            continue;
        if (name.compare(name.size() - suffix.size(), suffix.size(), suffix) != 0)
            continue;
        std::ifstream sf(entry.path(), std::ios::binary);
        try
        {
            json j;
            sf >> j;
            SessionSummaryEntry e;
            e.session_id = j.value("session_id",
                                   name.substr(0, name.size() - suffix.size()));
            e.mtime = j.value("mtime", int64_t{0});
            e.data = j.value("data", json::object());
            out.push_back(std::move(e));
        }
        catch (...)
        {
        }
    }
    return out;
}

void FileSessionStore::delete_session(const SessionKey& key)
{
    std::lock_guard<std::mutex> lk(mu_);
    std::error_code ec;
    fs::path path = file_path_for(key);
    fs::remove(path, ec);
    if (!key.subpath.has_value())
    {
        fs::remove(summary_path_for(key.project_key, key.session_id), ec);
        fs::path subdir = root_ / key.project_key / key.session_id;
        if (fs::exists(subdir, ec))
            fs::remove_all(subdir, ec);
    }
}

std::vector<std::string>
FileSessionStore::list_subkeys(const SessionListSubkeysKey& key)
{
    std::lock_guard<std::mutex> lk(mu_);
    std::vector<std::string> out;
    fs::path subdir = root_ / key.project_key / key.session_id;
    std::error_code ec;
    if (!fs::exists(subdir, ec) || !fs::is_directory(subdir, ec))
        return out;
    const std::string suffix = ".jsonl";
    for (const auto& entry : fs::recursive_directory_iterator(subdir, ec))
    {
        if (!entry.is_regular_file())
            continue;
        std::string ps = entry.path().string();
        std::string root_s = subdir.string();
        if (ps.size() <= root_s.size())
            continue;
        std::string rel = ps.substr(root_s.size() + 1);
        // Normalize separators to forward slash.
        for (auto& c : rel)
            if (c == '\\')
                c = '/';
        if (rel.size() <= suffix.size())
            continue;
        if (rel.compare(rel.size() - suffix.size(), suffix.size(), suffix) != 0)
            continue;
        out.push_back(rel.substr(0, rel.size() - suffix.size()));
    }
    return out;
}

} // namespace claude::sessions
