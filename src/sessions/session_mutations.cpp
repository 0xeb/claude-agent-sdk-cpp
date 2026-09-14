#include <claude/sessions/session_mutations.hpp>
#include <claude/sessions/session_store_validation.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <random>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace claude::sessions
{

namespace
{

// Python: _UUID_RE
const std::regex& uuid_re()
{
    static const std::regex re(
        R"(^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$)",
        std::regex::icase);
    return re;
}

std::string strip(const std::string& s)
{
    size_t a = 0, b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a])))
        ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1])))
        --b;
    return s.substr(a, b - a);
}

std::string iso_now_utc()
{
    using namespace std::chrono;
    auto now = system_clock::now();
    auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;
    std::time_t t = system_clock::to_time_t(now);
    std::tm gm{};
#ifdef _WIN32
    gmtime_s(&gm, &t);
#else
    gmtime_r(&t, &gm);
#endif
    char buf[40];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02d.%03dZ",
                  gm.tm_year + 1900, gm.tm_mon + 1, gm.tm_mday,
                  gm.tm_hour, gm.tm_min, gm.tm_sec, static_cast<int>(ms.count()));
    return std::string(buf);
}

std::string make_uuid_v4()
{
    static thread_local std::mt19937_64 rng(
        std::chrono::steady_clock::now().time_since_epoch().count() ^
        std::hash<std::thread::id>{}(std::this_thread::get_id()));
    uint64_t a = rng();
    uint64_t b = rng();
    // Set version (v4) and variant bits.
    a = (a & 0xFFFFFFFFFFFF0FFFULL) | 0x0000000000004000ULL;
    b = (b & 0x3FFFFFFFFFFFFFFFULL) | 0x8000000000000000ULL;
    char buf[40];
    std::snprintf(buf, sizeof(buf),
                  "%08x-%04x-%04x-%04x-%012llx",
                  static_cast<unsigned>(a >> 32),
                  static_cast<unsigned>((a >> 16) & 0xFFFF),
                  static_cast<unsigned>(a & 0xFFFF),
                  static_cast<unsigned>((b >> 48) & 0xFFFF),
                  static_cast<unsigned long long>(b & 0xFFFFFFFFFFFFULL));
    return std::string(buf);
}

// 32-bit JS-style djb2-ish hash matching Python _simple_hash.
std::string simple_hash_base36(const std::string& s)
{
    uint32_t h = 0;
    for (char c : s)
    {
        h = (h << 5) - h + static_cast<unsigned char>(c);
    }
    int32_t signed_h = static_cast<int32_t>(h);
    uint64_t abs_h = signed_h < 0 ? static_cast<uint64_t>(-static_cast<int64_t>(signed_h))
                                  : static_cast<uint64_t>(signed_h);
    if (abs_h == 0)
        return "0";
    static const char* digits = "0123456789abcdefghijklmnopqrstuvwxyz";
    std::string out;
    while (abs_h > 0)
    {
        out.push_back(digits[abs_h % 36]);
        abs_h /= 36;
    }
    std::reverse(out.begin(), out.end());
    return out;
}

constexpr size_t MAX_SANITIZED_LENGTH = 200;

std::string sanitize_path(const std::string& name)
{
    std::string sanitized = name;
    for (auto& c : sanitized)
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9')))
            c = '-';
    if (sanitized.size() <= MAX_SANITIZED_LENGTH)
        return sanitized;
    std::string h = simple_hash_base36(name);
    return sanitized.substr(0, MAX_SANITIZED_LENGTH) + "-" + h;
}

} // namespace

bool validate_uuid(const std::string& maybe_uuid)
{
    return std::regex_match(maybe_uuid, uuid_re());
}

std::string sanitize_unicode(const std::string& value)
{
    // C++ has no portable Unicode categorization in the standard library; we
    // implement the explicit-range strip from Python plus passthrough.
    // Iterates until stable (max 10) — matches Python.
    auto strip_ranges = [](const std::string& in) -> std::string {
        std::string out;
        out.reserve(in.size());
        // Walk byte-by-byte and decode UTF-8 to test code points.
        size_t i = 0;
        while (i < in.size())
        {
            unsigned char c = static_cast<unsigned char>(in[i]);
            uint32_t cp = 0;
            size_t len = 1;
            if (c < 0x80)
            {
                cp = c;
                len = 1;
            }
            else if ((c & 0xE0) == 0xC0 && i + 1 < in.size())
            {
                cp = ((c & 0x1F) << 6) |
                     (static_cast<unsigned char>(in[i + 1]) & 0x3F);
                len = 2;
            }
            else if ((c & 0xF0) == 0xE0 && i + 2 < in.size())
            {
                cp = ((c & 0x0F) << 12) |
                     ((static_cast<unsigned char>(in[i + 1]) & 0x3F) << 6) |
                     (static_cast<unsigned char>(in[i + 2]) & 0x3F);
                len = 3;
            }
            else if ((c & 0xF8) == 0xF0 && i + 3 < in.size())
            {
                cp = ((c & 0x07) << 18) |
                     ((static_cast<unsigned char>(in[i + 1]) & 0x3F) << 12) |
                     ((static_cast<unsigned char>(in[i + 2]) & 0x3F) << 6) |
                     (static_cast<unsigned char>(in[i + 3]) & 0x3F);
                len = 4;
            }
            else
            {
                // Malformed — skip the byte.
                ++i;
                continue;
            }
            bool strip =
                (cp >= 0x200B && cp <= 0x200F) || (cp >= 0x202A && cp <= 0x202E) ||
                (cp >= 0x2066 && cp <= 0x2069) || cp == 0xFEFF ||
                (cp >= 0xE000 && cp <= 0xF8FF);
            if (!strip)
                out.append(in, i, len);
            i += len;
        }
        return out;
    };

    std::string cur = value;
    for (int iter = 0; iter < 10; ++iter)
    {
        std::string prev = cur;
        cur = strip_ranges(cur);
        if (cur == prev)
            break;
    }
    return cur;
}

std::string project_key_for_directory(const std::string& directory)
{
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::path p = directory.empty() ? fs::current_path(ec) : fs::path(directory);
    auto canonical = fs::weakly_canonical(p, ec);
    std::string s = ec ? p.string() : canonical.string();
    return sanitize_path(s);
}

void rename_session_via_store(SessionStore& store,
                              const std::string& session_id,
                              const std::string& title,
                              const std::string& directory)
{
    if (!validate_uuid(session_id))
        throw std::invalid_argument("Invalid session_id: " + session_id);
    std::string stripped = strip(title);
    if (stripped.empty())
        throw std::invalid_argument("title must be non-empty");

    SessionKey key{project_key_for_directory(directory), session_id, std::nullopt};
    json entry = {
        {"type", "custom-title"},
        {"customTitle", stripped},
        {"sessionId", session_id},
        {"uuid", make_uuid_v4()},
        {"timestamp", iso_now_utc()},
    };
    store.append(key, {entry});
}

void tag_session_via_store(SessionStore& store,
                           const std::string& session_id,
                           const std::optional<std::string>& tag,
                           const std::string& directory)
{
    if (!validate_uuid(session_id))
        throw std::invalid_argument("Invalid session_id: " + session_id);
    std::string final_tag;
    if (tag.has_value())
    {
        std::string sanitized = strip(sanitize_unicode(*tag));
        if (sanitized.empty())
            throw std::invalid_argument("tag must be non-empty (use std::nullopt to clear)");
        final_tag = std::move(sanitized);
    }
    SessionKey key{project_key_for_directory(directory), session_id, std::nullopt};
    json entry = {
        {"type", "tag"},
        {"tag", final_tag},
        {"sessionId", session_id},
        {"uuid", make_uuid_v4()},
        {"timestamp", iso_now_utc()},
    };
    store.append(key, {entry});
}

void delete_session_via_store(SessionStore& store,
                              const std::string& session_id,
                              const std::string& directory)
{
    if (!validate_uuid(session_id))
        throw std::invalid_argument("Invalid session_id: " + session_id);
    if (!store_implements(store, SessionStoreCapability::DeleteSession))
        return;
    SessionKey key{project_key_for_directory(directory), session_id, std::nullopt};
    store.delete_session(key);
}

namespace
{

const std::unordered_set<std::string>& transcript_types()
{
    static const std::unordered_set<std::string> s = {
        "user", "assistant", "attachment", "system", "progress"};
    return s;
}

} // namespace

ForkSessionResult fork_session_via_store(SessionStore& store,
                                         const std::string& session_id,
                                         const std::string& directory,
                                         const std::string& up_to_message_id,
                                         const std::optional<std::string>& title)
{
    if (!validate_uuid(session_id))
        throw std::invalid_argument("Invalid session_id: " + session_id);
    if (!up_to_message_id.empty() && !validate_uuid(up_to_message_id))
        throw std::invalid_argument("Invalid up_to_message_id: " + up_to_message_id);

    SessionKey src_key{project_key_for_directory(directory), session_id, std::nullopt};
    auto loaded_opt = store.load(src_key);
    if (!loaded_opt.has_value() || loaded_opt->empty())
        throw std::runtime_error("Session " + session_id + " not found");

    std::vector<json> transcript;
    std::vector<json> content_replacements;
    for (const auto& e : *loaded_opt)
    {
        if (!e.is_object())
            continue;
        std::string t = e.value("type", "");
        if (transcript_types().count(t) && e.contains("uuid") && e.at("uuid").is_string())
            transcript.push_back(e);
        else if (t == "content-replacement" && e.value("sessionId", "") == session_id &&
                 e.contains("replacements") && e.at("replacements").is_array())
        {
            for (const auto& r : e.at("replacements"))
                content_replacements.push_back(r);
        }
    }

    // Filter out sidechains.
    std::vector<json> filtered;
    for (auto& e : transcript)
        if (!e.value("isSidechain", false))
            filtered.push_back(std::move(e));
    transcript = std::move(filtered);
    if (transcript.empty())
        throw std::runtime_error("Session " + session_id + " has no messages to fork");

    if (!up_to_message_id.empty())
    {
        int cutoff = -1;
        for (size_t i = 0; i < transcript.size(); ++i)
        {
            if (transcript[i].value("uuid", "") == up_to_message_id)
            {
                cutoff = static_cast<int>(i);
                break;
            }
        }
        if (cutoff < 0)
            throw std::runtime_error("Message " + up_to_message_id +
                                     " not found in session " + session_id);
        transcript.resize(cutoff + 1);
    }

    std::unordered_map<std::string, std::string> uuid_map;
    for (const auto& e : transcript)
        uuid_map[e.at("uuid").get<std::string>()] = make_uuid_v4();

    std::vector<json> writable;
    for (const auto& e : transcript)
        if (e.value("type", "") != "progress")
            writable.push_back(e);
    if (writable.empty())
        throw std::runtime_error("Session " + session_id + " has no messages to fork");

    std::unordered_map<std::string, const json*> by_uuid;
    for (const auto& e : transcript)
        by_uuid[e.at("uuid").get<std::string>()] = &e;

    std::string forked_session_id = make_uuid_v4();
    std::string now = iso_now_utc();
    std::vector<json> out_entries;

    for (size_t i = 0; i < writable.size(); ++i)
    {
        const json& original = writable[i];
        std::string new_uuid = uuid_map[original.at("uuid").get<std::string>()];

        json new_parent = nullptr;
        std::string parent_id =
            original.value("parentUuid", std::string());
        while (!parent_id.empty())
        {
            auto it = by_uuid.find(parent_id);
            if (it == by_uuid.end())
                break;
            const json& parent = *it->second;
            if (parent.value("type", "") != "progress")
            {
                auto mit = uuid_map.find(parent_id);
                if (mit != uuid_map.end())
                    new_parent = mit->second;
                break;
            }
            parent_id = parent.value("parentUuid", std::string());
        }

        std::string ts =
            (i == writable.size() - 1) ? now : original.value("timestamp", now);
        json logical = nullptr;
        if (original.contains("logicalParentUuid") &&
            original["logicalParentUuid"].is_string())
        {
            auto mit = uuid_map.find(original["logicalParentUuid"].get<std::string>());
            if (mit != uuid_map.end())
                logical = mit->second;
            else
                logical = original["logicalParentUuid"];
        }

        json forked = original;
        forked["uuid"] = new_uuid;
        forked["parentUuid"] = new_parent;
        forked["logicalParentUuid"] = logical;
        forked["sessionId"] = forked_session_id;
        forked["timestamp"] = ts;
        forked["isSidechain"] = false;
        forked["forkedFrom"] = {
            {"sessionId", session_id},
            {"messageUuid", original.at("uuid").get<std::string>()},
        };
        for (const char* k :
             {"teamName", "agentName", "slug", "sourceToolAssistantUUID"})
            forked.erase(k);
        out_entries.push_back(std::move(forked));
    }

    if (!content_replacements.empty())
    {
        json cr = {
            {"type", "content-replacement"},
            {"sessionId", forked_session_id},
            {"replacements", content_replacements},
            {"uuid", make_uuid_v4()},
            {"timestamp", now},
        };
        out_entries.push_back(std::move(cr));
    }

    std::string fork_title;
    if (title.has_value())
        fork_title = strip(*title);
    if (fork_title.empty())
    {
        // Derive from raw entries — last customTitle/aiTitle wins; no first-prompt
        // fallback in the simplified port (callers can pass title explicitly).
        std::string ct, at;
        for (const auto& e : *loaded_opt)
        {
            if (e.is_object())
            {
                if (e.contains("customTitle") && e["customTitle"].is_string())
                    ct = e["customTitle"].get<std::string>();
                if (e.contains("aiTitle") && e["aiTitle"].is_string())
                    at = e["aiTitle"].get<std::string>();
            }
        }
        std::string base = !ct.empty() ? ct : (!at.empty() ? at : std::string("Forked session"));
        fork_title = base + " (fork)";
    }

    json title_entry = {
        {"type", "custom-title"},
        {"sessionId", forked_session_id},
        {"customTitle", fork_title},
        {"uuid", make_uuid_v4()},
        {"timestamp", now},
    };
    out_entries.push_back(std::move(title_entry));

    SessionKey dst_key{src_key.project_key, forked_session_id, std::nullopt};
    store.append(dst_key, out_entries);

    return ForkSessionResult{forked_session_id};
}

} // namespace claude::sessions
