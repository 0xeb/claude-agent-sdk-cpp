#include <claude/sessions/session_resume.hpp>
#include <claude/sessions/session_store_validation.hpp>

#include <chrono>
#include <fstream>
#include <random>
#include <sstream>
#include <stdexcept>

namespace claude::sessions
{

namespace fs = std::filesystem;

namespace
{

fs::path make_unique_temp_dir()
{
    auto base = fs::temp_directory_path();
    auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    static thread_local std::mt19937_64 rng(static_cast<uint64_t>(now));
    for (int attempt = 0; attempt < 16; ++attempt)
    {
        std::ostringstream oss;
        oss << "claude-sdk-resume-" << std::hex << rng();
        fs::path candidate = base / oss.str();
        std::error_code ec;
        if (fs::create_directories(candidate, ec))
            return candidate;
    }
    throw std::runtime_error("Failed to allocate temp resume directory");
}

void write_jsonl(const fs::path& path, const std::vector<json>& entries)
{
    std::error_code ec;
    fs::create_directories(path.parent_path(), ec);
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f.is_open())
        throw std::runtime_error("Failed to write " + path.string());
    for (const auto& e : entries)
    {
        // Hoist "type" to front (Python parity helper).
        if (e.is_object() && e.contains("type"))
        {
            json reordered = json::object();
            reordered["type"] = e.at("type");
            for (auto it = e.begin(); it != e.end(); ++it)
                if (it.key() != "type")
                    reordered[it.key()] = it.value();
            f << reordered.dump() << '\n';
        }
        else
        {
            f << e.dump() << '\n';
        }
    }
}

// Best-effort split of "subagents/foo/agent-xxx" into a path under session_dir.
fs::path subpath_to_path(const fs::path& session_dir, const std::string& subpath)
{
    fs::path p = session_dir;
    size_t start = 0, pos;
    while ((pos = subpath.find('/', start)) != std::string::npos)
    {
        p /= subpath.substr(start, pos - start);
        start = pos + 1;
    }
    p /= subpath.substr(start);
    p += ".jsonl";
    return p;
}

} // namespace

MaterializedResume materialize_resume_session(SessionStore& store,
                                              const std::string& project_key,
                                              const std::string& session_id)
{
    SessionKey main_key{project_key, session_id, std::nullopt};
    auto loaded = store.load(main_key);
    if (!loaded.has_value() || loaded->empty())
        throw std::runtime_error("Session " + session_id + " not found in store");

    fs::path config_dir = make_unique_temp_dir();
    fs::path projects_dir = config_dir / "projects";
    fs::path project_dir = projects_dir / project_key;
    fs::path main_path = project_dir / (session_id + ".jsonl");
    write_jsonl(main_path, *loaded);

    if (store_implements(store, SessionStoreCapability::ListSubkeys))
    {
        auto subkeys = store.list_subkeys({project_key, session_id});
        fs::path session_dir = project_dir / session_id;
        for (const auto& sp : subkeys)
        {
            SessionKey sub_key{project_key, session_id, sp};
            auto sub_loaded = store.load(sub_key);
            if (!sub_loaded.has_value())
                continue;
            // Split agent_metadata entries into .meta.json sidecars; transcript
            // entries written as .jsonl.
            std::vector<json> transcript;
            std::vector<json> meta;
            for (const auto& e : *sub_loaded)
            {
                if (e.is_object() && e.value("type", "") == "agent_metadata")
                    meta.push_back(e);
                else
                    transcript.push_back(e);
            }
            fs::path target = subpath_to_path(session_dir, sp);
            if (!transcript.empty())
                write_jsonl(target, transcript);
            if (!meta.empty())
            {
                fs::path meta_path = target;
                std::string fn = meta_path.filename().string();
                fn = fn.substr(0, fn.size() - std::string(".jsonl").size()) +
                     ".meta.json";
                meta_path = meta_path.parent_path() / fn;
                std::error_code ec;
                fs::create_directories(meta_path.parent_path(), ec);
                std::ofstream f(meta_path, std::ios::binary | std::ios::trunc);
                json combined = json::object();
                // Reduce the metadata list to a single object (Python parity:
                // import path round-trips a single .meta.json sidecar).
                for (const auto& m : meta)
                    if (m.is_object())
                        for (auto it = m.begin(); it != m.end(); ++it)
                            if (it.key() != "type")
                                combined[it.key()] = it.value();
                f << combined.dump();
            }
        }
    }

    MaterializedResume out;
    out.config_dir = config_dir;
    out.resume_session_id = session_id;
    out.cleanup = [config_dir]() {
        std::error_code ec;
        fs::remove_all(config_dir, ec);
    };
    return out;
}

std::shared_ptr<TranscriptMirrorBatcher>
build_mirror_batcher(std::shared_ptr<SessionStore> store,
                     const std::filesystem::path& projects_dir,
                     TranscriptMirrorBatcher::OnError on_error)
{
    fs::path pd = projects_dir;
    FilePathToKey resolver = [pd](const std::string& file_path) -> std::optional<SessionKey> {
        std::error_code ec;
        fs::path p(file_path);
        fs::path rel = fs::relative(p, pd, ec);
        if (ec)
            return std::nullopt;
        auto parts_str = rel.string();
        // Walk path components.
        std::vector<std::string> parts;
        for (const auto& seg : rel)
        {
            std::string s = seg.string();
            if (!s.empty() && s != ".")
                parts.push_back(std::move(s));
        }
        if (parts.empty() || parts[0] == "..")
            return std::nullopt;
        if (parts.size() < 2)
            return std::nullopt;
        const std::string suffix = ".jsonl";
        auto ends_with = [&](const std::string& s) {
            return s.size() >= suffix.size() &&
                   s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
        };
        if (parts.size() == 2 && ends_with(parts[1]))
        {
            SessionKey k;
            k.project_key = parts[0];
            k.session_id = parts[1].substr(0, parts[1].size() - suffix.size());
            return k;
        }
        if (parts.size() >= 4)
        {
            std::string last = parts.back();
            if (ends_with(last))
                parts.back() = last.substr(0, last.size() - suffix.size());
            std::string subpath;
            for (size_t i = 2; i < parts.size(); ++i)
            {
                if (i > 2)
                    subpath += '/';
                subpath += parts[i];
            }
            SessionKey k;
            k.project_key = parts[0];
            k.session_id = parts[1];
            k.subpath = subpath;
            return k;
        }
        return std::nullopt;
    };
    return std::make_shared<TranscriptMirrorBatcher>(std::move(store), std::move(resolver),
                                                     std::move(on_error));
}

} // namespace claude::sessions
