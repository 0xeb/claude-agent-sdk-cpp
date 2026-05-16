#include <claude/sessions/session_import.hpp>
#include <claude/sessions/session_mutations.hpp>

#include <fstream>
#include <stdexcept>

namespace claude::sessions
{

namespace fs = std::filesystem;

namespace
{

void append_jsonl_in_batches(const fs::path& path,
                             const SessionKey& key,
                             SessionStore& store,
                             std::size_t batch_size)
{
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open())
        throw std::runtime_error("Failed to open " + path.string());

    std::vector<SessionStoreEntry> batch;
    std::size_t nbytes = 0;
    std::string line;
    while (std::getline(f, line))
    {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (line.empty())
            continue;
        try
        {
            batch.push_back(json::parse(line));
        }
        catch (...)
        {
            continue;
        }
        nbytes += line.size();
        if (batch.size() >= batch_size || nbytes >= MAX_PENDING_BYTES)
        {
            store.append(key, batch);
            batch.clear();
            nbytes = 0;
        }
    }
    if (!batch.empty())
        store.append(key, batch);
}

} // namespace

void import_session_to_store(const std::string& session_id,
                             SessionStore& store,
                             const fs::path& jsonl_path,
                             const std::string& project_key,
                             bool include_subagents,
                             std::size_t batch_size)
{
    if (!validate_uuid(session_id))
        throw std::invalid_argument("Invalid session_id: " + session_id);
    if (batch_size == 0)
        batch_size = MAX_PENDING_ENTRIES;
    std::error_code ec;
    if (!fs::exists(jsonl_path, ec))
        throw std::runtime_error("Session " + session_id + " not found: " +
                                 jsonl_path.string());

    SessionKey main_key{project_key, session_id, std::nullopt};
    append_jsonl_in_batches(jsonl_path, main_key, store, batch_size);

    if (!include_subagents)
        return;

    fs::path session_dir = jsonl_path;
    session_dir.replace_extension();
    fs::path subagents_dir = session_dir / "subagents";
    if (!fs::exists(subagents_dir, ec) || !fs::is_directory(subagents_dir, ec))
        return;

    for (const auto& entry : fs::recursive_directory_iterator(subagents_dir, ec))
    {
        if (!entry.is_regular_file())
            continue;
        std::string name = entry.path().filename().string();
        const std::string suffix = ".jsonl";
        if (name.size() <= suffix.size())
            continue;
        if (name.compare(name.size() - suffix.size(), suffix.size(), suffix) != 0)
            continue;
        // Compute subpath relative to session_dir, /-joined, sans .jsonl.
        std::string ps = entry.path().string();
        std::string base = session_dir.string();
        if (ps.size() <= base.size() + 1)
            continue;
        std::string rel = ps.substr(base.size() + 1);
        for (auto& c : rel)
            if (c == '\\')
                c = '/';
        rel = rel.substr(0, rel.size() - suffix.size());
        SessionKey sub_key{project_key, session_id, rel};
        append_jsonl_in_batches(entry.path(), sub_key, store, batch_size);

        // Optional .meta.json sidecar.
        fs::path meta = entry.path();
        std::string mname = meta.filename().string();
        mname = mname.substr(0, mname.size() - suffix.size()) + ".meta.json";
        meta = meta.parent_path() / mname;
        if (fs::exists(meta, ec))
        {
            try
            {
                std::ifstream mf(meta, std::ios::binary);
                json j;
                mf >> j;
                json meta_entry = json::object();
                meta_entry["type"] = "agent_metadata";
                if (j.is_object())
                    for (auto it = j.begin(); it != j.end(); ++it)
                        meta_entry[it.key()] = it.value();
                store.append(sub_key, {meta_entry});
            }
            catch (...)
            {
            }
        }
    }
}

} // namespace claude::sessions
