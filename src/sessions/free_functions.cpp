#include <claude/sessions/free_functions.hpp>
#include <claude/sessions/session_mutations.hpp>
#include <claude/sessions/session_store_validation.hpp>
#include <claude/sessions/session_summary.hpp>

#include <algorithm>
#include <set>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace claude::sessions
{

namespace
{

const std::unordered_set<std::string>& chain_entry_types()
{
    static const std::unordered_set<std::string> s = {
        "user", "assistant", "progress", "system", "attachment"};
    return s;
}

std::vector<json> filter_transcript_entries(const std::vector<SessionStoreEntry>& entries)
{
    std::vector<json> out;
    for (const auto& e : entries)
    {
        if (!e.is_object())
            continue;
        auto t = e.value("type", std::string());
        if (chain_entry_types().count(t) && e.contains("uuid") &&
            e.at("uuid").is_string())
            out.push_back(e);
    }
    return out;
}

bool is_visible_message(const json& e)
{
    auto t = e.value("type", std::string());
    if (t != "user" && t != "assistant")
        return false;
    if (e.value("isMeta", false))
        return false;
    if (e.value("isSidechain", false))
        return false;
    if (e.contains("teamName") && e["teamName"].is_string() &&
        !e["teamName"].get<std::string>().empty())
        return false;
    return true;
}

SessionMessage to_session_message(const json& e)
{
    SessionMessage m;
    auto t = e.value("type", std::string());
    m.type = (t == "user") ? "user" : "assistant";
    m.uuid = e.value("uuid", std::string());
    m.session_id = e.value("sessionId", std::string());
    if (e.contains("message"))
        m.message = e["message"];
    return m;
}

std::vector<json> build_conversation_chain(const std::vector<json>& entries)
{
    if (entries.empty())
        return {};
    std::unordered_map<std::string, const json*> by_uuid;
    std::unordered_map<std::string, size_t> index;
    for (size_t i = 0; i < entries.size(); ++i)
    {
        const std::string& u = entries[i].at("uuid").get_ref<const std::string&>();
        by_uuid[u] = &entries[i];
        index[u] = i;
    }
    std::unordered_set<std::string> parent_uuids;
    for (const auto& e : entries)
    {
        auto p = e.value("parentUuid", std::string());
        if (!p.empty())
            parent_uuids.insert(p);
    }
    std::vector<const json*> terminals;
    for (const auto& e : entries)
        if (parent_uuids.find(e.at("uuid").get<std::string>()) == parent_uuids.end())
            terminals.push_back(&e);

    std::vector<const json*> leaves;
    for (const json* term : terminals)
    {
        const json* cur = term;
        std::unordered_set<std::string> seen;
        while (cur)
        {
            std::string uid = cur->at("uuid").get<std::string>();
            if (!seen.insert(uid).second)
                break;
            auto t = cur->value("type", std::string());
            if (t == "user" || t == "assistant")
            {
                leaves.push_back(cur);
                break;
            }
            auto p = cur->value("parentUuid", std::string());
            cur = p.empty() ? nullptr : by_uuid[p];
        }
    }
    if (leaves.empty())
        return {};

    auto is_main = [&](const json* e) {
        if (e->value("isSidechain", false))
            return false;
        if (e->value("isMeta", false))
            return false;
        if (e->contains("teamName") && (*e)["teamName"].is_string() &&
            !(*e)["teamName"].get<std::string>().empty())
            return false;
        return true;
    };

    std::vector<const json*> main_leaves;
    for (const json* l : leaves)
        if (is_main(l))
            main_leaves.push_back(l);

    auto pick_best = [&](const std::vector<const json*>& cands) -> const json* {
        const json* best = cands[0];
        size_t best_idx = index[best->at("uuid").get<std::string>()];
        for (size_t i = 1; i < cands.size(); ++i)
        {
            size_t ci = index[cands[i]->at("uuid").get<std::string>()];
            if (ci > best_idx)
            {
                best = cands[i];
                best_idx = ci;
            }
        }
        return best;
    };
    const json* leaf = !main_leaves.empty() ? pick_best(main_leaves) : pick_best(leaves);

    std::vector<json> chain;
    std::unordered_set<std::string> seen;
    while (leaf)
    {
        std::string uid = leaf->at("uuid").get<std::string>();
        if (!seen.insert(uid).second)
            break;
        chain.push_back(*leaf);
        auto p = leaf->value("parentUuid", std::string());
        leaf = p.empty() ? nullptr : by_uuid[p];
    }
    std::reverse(chain.begin(), chain.end());
    return chain;
}

std::vector<json> build_subagent_chain(const std::vector<json>& entries)
{
    if (entries.empty())
        return {};
    std::unordered_map<std::string, const json*> by_uuid;
    for (const auto& e : entries)
        by_uuid[e.at("uuid").get<std::string>()] = &e;

    const json* leaf = nullptr;
    for (auto it = entries.rbegin(); it != entries.rend(); ++it)
    {
        auto t = it->value("type", std::string());
        if (t == "user" || t == "assistant")
        {
            leaf = &*it;
            break;
        }
    }
    if (!leaf)
        return {};
    std::vector<json> chain;
    std::unordered_set<std::string> seen;
    while (leaf)
    {
        std::string uid = leaf->at("uuid").get<std::string>();
        if (!seen.insert(uid).second)
            break;
        chain.push_back(*leaf);
        auto p = leaf->value("parentUuid", std::string());
        leaf = p.empty() ? nullptr : by_uuid[p];
    }
    std::reverse(chain.begin(), chain.end());
    return chain;
}

std::vector<SessionMessage> apply_paging(const std::vector<SessionMessage>& msgs,
                                         std::optional<int> limit, int offset)
{
    if (offset <= 0 && !limit.has_value())
        return msgs;
    int start = std::max(0, offset);
    if (start >= static_cast<int>(msgs.size()))
        return {};
    int end = static_cast<int>(msgs.size());
    if (limit.has_value() && *limit > 0)
        end = std::min(end, start + *limit);
    return std::vector<SessionMessage>(msgs.begin() + start, msgs.begin() + end);
}

} // namespace

std::vector<SDKSessionInfo>
list_sessions_from_store(SessionStore& store,
                         const std::string& directory,
                         std::optional<int> limit,
                         int offset)
{
    std::string project_key = project_key_for_directory(directory);
    std::string project_path = directory;

    bool has_summaries = store_implements(store, SessionStoreCapability::ListSessionSummaries);
    bool has_list = store_implements(store, SessionStoreCapability::ListSessions);

    if (!has_summaries && !has_list)
        throw std::invalid_argument(
            "session_store implements neither list_session_summaries() nor "
            "list_sessions() -- cannot list sessions.");

    auto load_jsonl_and_lite_parse =
        [&](const std::string& sid, int64_t mtime) -> std::optional<SDKSessionInfo> {
        SessionKey k{project_key, sid, std::nullopt};
        auto loaded = store.load(k);
        if (!loaded.has_value() || loaded->empty())
            return std::nullopt;
        // Fold all entries into a synthetic summary, then convert to info.
        SessionSummaryEntry folded =
            fold_session_summary(std::nullopt, k, *loaded);
        folded.mtime = mtime;
        auto info = summary_entry_to_sdk_info(
            folded, project_path.empty() ? std::nullopt
                                         : std::optional<std::string>(project_path));
        if (info)
            info->last_modified = mtime;
        return info;
    };

    std::vector<SDKSessionInfo> results;

    if (has_summaries)
    {
        auto summaries = store.list_session_summaries(project_key);
        std::vector<SessionStoreListEntry> listing;
        std::unordered_map<std::string, int64_t> known;
        if (has_list)
        {
            listing = store.list_sessions(project_key);
            for (const auto& e : listing)
                known[e.session_id] = e.mtime;
        }

        struct Slot
        {
            int64_t mtime;
            std::optional<SDKSessionInfo> info;
            std::string session_id;
            bool needs_load = false;
        };
        std::vector<Slot> slots;
        std::unordered_set<std::string> fresh_ids;
        for (const auto& s : summaries)
        {
            if (has_list)
            {
                auto it = known.find(s.session_id);
                if (it == known.end())
                    continue;
                if (s.mtime < it->second)
                    continue;
            }
            auto info = summary_entry_to_sdk_info(
                s, project_path.empty() ? std::nullopt
                                        : std::optional<std::string>(project_path));
            if (!info)
            {
                fresh_ids.insert(s.session_id);
                continue;
            }
            Slot sl;
            sl.mtime = s.mtime;
            sl.info = std::move(info);
            sl.session_id = s.session_id;
            slots.push_back(std::move(sl));
            fresh_ids.insert(s.session_id);
        }
        if (has_list)
        {
            for (const auto& e : listing)
            {
                if (fresh_ids.count(e.session_id))
                    continue;
                Slot sl;
                sl.mtime = e.mtime;
                sl.session_id = e.session_id;
                sl.needs_load = true;
                slots.push_back(std::move(sl));
            }
        }
        std::sort(slots.begin(), slots.end(),
                  [](const Slot& a, const Slot& b) { return a.mtime > b.mtime; });
        // Paginate before per-session load (Python parity).
        int start = std::max(0, offset);
        if (start > static_cast<int>(slots.size()))
            start = static_cast<int>(slots.size());
        std::vector<Slot> page(slots.begin() + start, slots.end());
        if (limit.has_value() && *limit > 0 &&
            static_cast<int>(page.size()) > *limit)
            page.resize(*limit);
        for (auto& sl : page)
            if (sl.needs_load)
                sl.info = load_jsonl_and_lite_parse(sl.session_id, sl.mtime);
        for (auto& sl : page)
            if (sl.info.has_value())
                results.push_back(std::move(*sl.info));
        return results;
    }

    // Slow path: list_sessions only.
    auto listing = store.list_sessions(project_key);
    for (const auto& e : listing)
    {
        auto info = load_jsonl_and_lite_parse(e.session_id, e.mtime);
        if (info)
            results.push_back(std::move(*info));
    }
    std::sort(results.begin(), results.end(),
              [](const SDKSessionInfo& a, const SDKSessionInfo& b) {
                  return a.last_modified > b.last_modified;
              });
    int start = std::max(0, offset);
    if (start > static_cast<int>(results.size()))
        start = static_cast<int>(results.size());
    std::vector<SDKSessionInfo> out(results.begin() + start, results.end());
    if (limit.has_value() && *limit > 0 &&
        static_cast<int>(out.size()) > *limit)
        out.resize(*limit);
    return out;
}

std::optional<SDKSessionInfo>
get_session_info_from_store(SessionStore& store,
                            const std::string& session_id,
                            const std::string& directory)
{
    if (!validate_uuid(session_id))
        return std::nullopt;
    std::string project_key = project_key_for_directory(directory);
    SessionKey k{project_key, session_id, std::nullopt};
    auto loaded = store.load(k);
    if (!loaded.has_value() || loaded->empty())
        return std::nullopt;
    // Best-effort mtime from last entry timestamp.
    int64_t mtime = 0;
    for (auto it = loaded->rbegin(); it != loaded->rend(); ++it)
    {
        if (it->is_object() && it->contains("timestamp") &&
            (*it)["timestamp"].is_string())
        {
            auto v = iso_to_epoch_ms((*it)["timestamp"].get<std::string>());
            if (v)
            {
                mtime = *v;
                break;
            }
        }
    }
    SessionSummaryEntry folded = fold_session_summary(std::nullopt, k, *loaded);
    folded.mtime = mtime;
    auto info = summary_entry_to_sdk_info(
        folded,
        directory.empty() ? std::nullopt : std::optional<std::string>(directory));
    if (info)
        info->last_modified = mtime;
    return info;
}

std::vector<SessionMessage>
get_session_messages_from_store(SessionStore& store,
                                const std::string& session_id,
                                const std::string& directory,
                                std::optional<int> limit, int offset)
{
    if (!validate_uuid(session_id))
        return {};
    std::string project_key = project_key_for_directory(directory);
    SessionKey k{project_key, session_id, std::nullopt};
    auto loaded = store.load(k);
    if (!loaded.has_value() || loaded->empty())
        return {};
    auto entries = filter_transcript_entries(*loaded);
    auto chain = build_conversation_chain(entries);
    std::vector<SessionMessage> msgs;
    for (const auto& e : chain)
        if (is_visible_message(e))
            msgs.push_back(to_session_message(e));
    return apply_paging(msgs, limit, offset);
}

std::vector<std::string>
list_subagents_from_store(SessionStore& store,
                          const std::string& session_id,
                          const std::string& directory)
{
    if (!validate_uuid(session_id))
        return {};
    if (!store_implements(store, SessionStoreCapability::ListSubkeys))
        throw std::invalid_argument(
            "session_store does not implement list_subkeys() -- cannot list subagents.");
    std::string project_key = project_key_for_directory(directory);
    auto subkeys = store.list_subkeys({project_key, session_id});
    std::vector<std::string> ids;
    std::set<std::string> seen;
    const std::string prefix = "subagents/";
    const std::string agent_prefix = "agent-";
    for (const auto& sk : subkeys)
    {
        if (sk.compare(0, prefix.size(), prefix) != 0)
            continue;
        auto pos = sk.rfind('/');
        std::string last = pos == std::string::npos ? sk : sk.substr(pos + 1);
        if (last.compare(0, agent_prefix.size(), agent_prefix) == 0)
        {
            std::string id = last.substr(agent_prefix.size());
            if (seen.insert(id).second)
                ids.push_back(id);
        }
    }
    return ids;
}

std::vector<SessionMessage>
get_subagent_messages_from_store(SessionStore& store,
                                 const std::string& session_id,
                                 const std::string& agent_id,
                                 const std::string& directory,
                                 std::optional<int> limit, int offset)
{
    if (!validate_uuid(session_id) || agent_id.empty())
        return {};
    std::string project_key = project_key_for_directory(directory);
    std::string subpath = "subagents/agent-" + agent_id;
    if (store_implements(store, SessionStoreCapability::ListSubkeys))
    {
        auto subkeys = store.list_subkeys({project_key, session_id});
        std::string target = "agent-" + agent_id;
        bool found = false;
        for (const auto& sk : subkeys)
        {
            if (sk.compare(0, std::string("subagents/").size(), "subagents/") != 0)
                continue;
            auto pos = sk.rfind('/');
            std::string last = pos == std::string::npos ? sk : sk.substr(pos + 1);
            if (last == target)
            {
                subpath = sk;
                found = true;
                break;
            }
        }
        if (!found)
            return {};
    }
    SessionKey k{project_key, session_id, subpath};
    auto loaded = store.load(k);
    if (!loaded.has_value() || loaded->empty())
        return {};
    // Drop synthetic agent_metadata entries.
    std::vector<json> transcript;
    for (const auto& e : *loaded)
        if (!(e.is_object() && e.value("type", "") == "agent_metadata"))
            transcript.push_back(e);
    if (transcript.empty())
        return {};
    auto entries = filter_transcript_entries(transcript);
    auto chain = build_subagent_chain(entries);
    std::vector<SessionMessage> msgs;
    for (const auto& e : chain)
    {
        auto t = e.value("type", std::string());
        if (t == "user" || t == "assistant")
            msgs.push_back(to_session_message(e));
    }
    return apply_paging(msgs, limit, offset);
}

} // namespace claude::sessions
