#include <claude/sessions/session_summary.hpp>
#include <claude/testing/session_store_conformance.hpp>

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace claude::testing
{

namespace
{

// Build an entry satisfying SessionStoreEntry (Python: _e() helper).
claude::json e(claude::json d)
{
    claude::json out = {{"type", "x"}};
    if (d.is_object())
        for (auto it = d.begin(); it != d.end(); ++it)
            out[it.key()] = it.value();
    return out;
}

bool capability_for(const std::string& method, unsigned& mask)
{
    if (method == ConformanceOptional::ListSessions)
    {
        mask = claude::SessionStoreCapability::ListSessions;
        return true;
    }
    if (method == ConformanceOptional::ListSessionSummaries)
    {
        mask = claude::SessionStoreCapability::ListSessionSummaries;
        return true;
    }
    if (method == ConformanceOptional::DeleteSession)
    {
        mask = claude::SessionStoreCapability::DeleteSession;
        return true;
    }
    if (method == ConformanceOptional::ListSubkeys)
    {
        mask = claude::SessionStoreCapability::ListSubkeys;
        return true;
    }
    return false;
}

bool has_optional(const claude::SessionStore& store, const std::string& method,
                  const std::set<std::string>& skip)
{
    if (skip.count(method))
        return false;
    unsigned mask = 0;
    if (!capability_for(method, mask))
        return false;
    return (store.capabilities() & mask) != 0;
}

const claude::SessionKey KEY = {"proj", "sess", std::nullopt};

class ContractRunner
{
  public:
    void run(const std::string& name, bool skipped, std::function<void()> body,
             ConformanceResult& result)
    {
        ConformanceContractResult r;
        r.name = name;
        r.skipped = skipped;
        if (skipped)
        {
            r.passed = true;
            ++result.skipped;
            result.contracts.push_back(std::move(r));
            return;
        }
        try
        {
            body();
            r.passed = true;
            ++result.passed;
        }
        catch (const std::exception& ex)
        {
            r.passed = false;
            r.message = ex.what();
            ++result.failed;
        }
        catch (...)
        {
            r.passed = false;
            r.message = "unknown exception";
            ++result.failed;
        }
        result.contracts.push_back(std::move(r));
    }
};

void check(bool cond, const std::string& message)
{
    if (!cond)
        throw std::runtime_error("assertion failed: " + message);
}

} // namespace

ConformanceResult run_session_store_conformance(StoreFactory make_store,
                                                const ConformanceOptions& options)
{
    ConformanceResult result;
    static const std::set<std::string> valid = {
        ConformanceOptional::ListSessions,
        ConformanceOptional::ListSessionSummaries,
        ConformanceOptional::DeleteSession,
        ConformanceOptional::ListSubkeys,
    };
    for (const auto& m : options.skip_optional)
        if (!valid.count(m))
            throw std::invalid_argument("unknown optional method in skip_optional: " + m);

    auto probe = make_store();
    bool has_list_sessions =
        has_optional(*probe, ConformanceOptional::ListSessions, options.skip_optional);
    bool has_list_summaries = has_optional(
        *probe, ConformanceOptional::ListSessionSummaries, options.skip_optional);
    bool has_delete =
        has_optional(*probe, ConformanceOptional::DeleteSession, options.skip_optional);
    bool has_list_subkeys =
        has_optional(*probe, ConformanceOptional::ListSubkeys, options.skip_optional);

    ContractRunner runner;

    // 1. append then load returns same entries in same order
    runner.run("1.append_load_roundtrip", false, [&] {
        auto s = make_store();
        s->append(KEY, {e({{"uuid", "b"}, {"n", 1}}), e({{"uuid", "a"}, {"n", 2}})});
        auto loaded = s->load(KEY);
        check(loaded.has_value() && loaded->size() == 2,
              "expected 2 loaded entries");
        check((*loaded)[0]["uuid"] == "b" && (*loaded)[1]["uuid"] == "a",
              "order mismatch");
    }, result);

    // 2. load unknown key returns nullopt
    runner.run("2.load_unknown_returns_nullopt", false, [&] {
        auto s = make_store();
        check(!s->load({"proj", "nope", std::nullopt}).has_value(),
              "unknown session_id should return nullopt");
        s->append(KEY, {e({{"uuid", "x"}, {"n", 1}})});
        check(!s->load({KEY.project_key, KEY.session_id, std::string("nope")}).has_value(),
              "unknown subpath should return nullopt");
    }, result);

    // 3. multiple appends preserve call order
    runner.run("3.multi_append_order", false, [&] {
        auto s = make_store();
        s->append(KEY, {e({{"uuid", "z"}, {"n", 1}})});
        s->append(KEY, {e({{"uuid", "a"}, {"n", 2}}), e({{"uuid", "m"}, {"n", 3}})});
        s->append(KEY, {e({{"uuid", "b"}, {"n", 4}})});
        auto l = s->load(KEY);
        check(l.has_value() && l->size() == 4, "expected 4");
        check((*l)[0]["uuid"] == "z" && (*l)[1]["uuid"] == "a" &&
                  (*l)[2]["uuid"] == "m" && (*l)[3]["uuid"] == "b",
              "order");
    }, result);

    // 4. append([]) is no-op
    runner.run("4.append_empty_noop", false, [&] {
        auto s = make_store();
        s->append(KEY, {e({{"uuid", "a"}, {"n", 1}})});
        s->append(KEY, {});
        auto l = s->load(KEY);
        check(l.has_value() && l->size() == 1, "still 1 entry");
    }, result);

    // 5. subpath keys are stored independently of main
    runner.run("5.subpath_independence", false, [&] {
        auto s = make_store();
        claude::SessionKey sub{KEY.project_key, KEY.session_id, std::string("subagents/agent-1")};
        s->append(KEY, {e({{"uuid", "m"}, {"n", 1}})});
        s->append(sub, {e({{"uuid", "s"}, {"n", 1}})});
        auto m = s->load(KEY);
        auto sb = s->load(sub);
        check(m && m->size() == 1 && (*m)[0]["uuid"] == "m", "main");
        check(sb && sb->size() == 1 && (*sb)[0]["uuid"] == "s", "sub");
    }, result);

    // 6. project_key isolation
    runner.run("6.project_isolation", false, [&] {
        auto s = make_store();
        s->append({"A", "s1", std::nullopt}, {e({{"from", "A"}})});
        s->append({"B", "s1", std::nullopt}, {e({{"from", "B"}})});
        auto a = s->load({"A", "s1", std::nullopt});
        auto b = s->load({"B", "s1", std::nullopt});
        check(a && a->size() == 1 && (*a)[0]["from"] == "A", "A");
        check(b && b->size() == 1 && (*b)[0]["from"] == "B", "B");
        if (has_list_sessions)
        {
            check(s->list_sessions("A").size() == 1, "list A");
            check(s->list_sessions("B").size() == 1, "list B");
        }
    }, result);

    // 7. list_sessions returns session_ids for project
    runner.run("7.list_sessions_basics", !has_list_sessions, [&] {
        auto s = make_store();
        s->append({"proj", "a", std::nullopt}, {e({{"n", 1}})});
        s->append({"proj", "b", std::nullopt}, {e({{"n", 1}})});
        s->append({"other", "c", std::nullopt}, {e({{"n", 1}})});
        auto sessions = s->list_sessions("proj");
        std::vector<std::string> ids;
        for (const auto& x : sessions)
            ids.push_back(x.session_id);
        std::sort(ids.begin(), ids.end());
        check(ids.size() == 2 && ids[0] == "a" && ids[1] == "b", "ids");
        for (const auto& x : sessions)
            check(std::isfinite(static_cast<double>(x.mtime)) &&
                      x.mtime > static_cast<int64_t>(1e12),
                  "mtime epoch-ms");
        check(s->list_sessions("never-appended-project").empty(), "empty proj");
    }, result);

    // 8. list_sessions excludes subagent subpaths
    runner.run("8.list_sessions_excludes_subpaths", !has_list_sessions, [&] {
        auto s = make_store();
        s->append({"proj", "main", std::nullopt}, {e({{"n", 1}})});
        s->append({"proj", "main", std::string("subagents/agent-1")},
                  {e({{"n", 1}})});
        auto sessions = s->list_sessions("proj");
        check(sessions.size() == 1 && sessions[0].session_id == "main",
              "main only");
    }, result);

    // 14. list_session_summaries
    runner.run("14.list_session_summaries", !has_list_summaries, [&] {
        auto s = make_store();
        claude::SessionKey key{"proj", "summ-sess", std::nullopt};
        s->append(key, {
            e({{"timestamp", "2024-01-01T00:00:00.000Z"}, {"customTitle", "first"}}),
            e({{"timestamp", "2024-01-01T00:00:01.000Z"}}),
        });
        s->append(key, {
            e({{"timestamp", "2024-01-01T00:00:02.000Z"}, {"customTitle", "second"}}),
        });
        s->append({"other", "elsewhere", std::nullopt},
                  {e({{"timestamp", "2024-01-01T00:00:00.000Z"}})});
        auto summaries = s->list_session_summaries("proj");
        check(summaries.size() == 1 && summaries[0].session_id == "summ-sess",
              "single summary");
        const auto& summ = summaries[0];
        check(std::isfinite(static_cast<double>(summ.mtime)) &&
                  summ.mtime > static_cast<int64_t>(1e12),
              "mtime epoch-ms");
        if (has_list_sessions)
        {
            auto ls = s->list_sessions("proj");
            int64_t known = 0;
            for (const auto& x : ls)
                if (x.session_id == "summ-sess")
                    known = x.mtime;
            check(summ.mtime >= known, "summary mtime >= list mtime");
        }
        check(summ.data.is_object(), "data is object");
        auto refolded = claude::sessions::fold_session_summary(
            summ, key, {e({{"timestamp", "2024-01-01T00:00:03.000Z"}})});
        check(refolded.session_id == "summ-sess", "refold session_id");
        check(refolded.mtime == summ.mtime,
              "fold preserves prev.mtime verbatim");
        // Subagent append must NOT affect main summary.
        s->append({key.project_key, key.session_id, std::string("subagents/agent-1")},
                  {e({{"timestamp", "2024-01-01T00:00:09.000Z"},
                      {"customTitle", "subagent"}})});
        auto after = s->list_session_summaries("proj");
        check(after.size() == 1 && after[0].data == summ.data,
              "subagent shouldn't pollute main summary");
        check(s->list_session_summaries("never-appended-project").empty(),
              "unknown proj empty");
        if (has_delete)
        {
            s->delete_session(key);
            check(s->list_session_summaries("proj").empty(),
                  "delete clears summary");
        }
    }, result);

    // 9. delete main then load returns nullopt
    runner.run("9.delete_returns_nullopt", !has_delete, [&] {
        auto s = make_store();
        s->delete_session({"proj", "never-written", std::nullopt});
        s->append(KEY, {e({{"n", 1}})});
        s->delete_session(KEY);
        check(!s->load(KEY).has_value(), "deleted");
    }, result);

    // 10. delete main cascades to subkeys
    runner.run("10.delete_cascades", !has_delete, [&] {
        auto s = make_store();
        claude::SessionKey sub1{KEY.project_key, KEY.session_id, std::string("subagents/agent-1")};
        claude::SessionKey sub2{KEY.project_key, KEY.session_id, std::string("subagents/agent-2")};
        claude::SessionKey other{"proj", "sess2", std::nullopt};
        claude::SessionKey other_proj{"other-proj", KEY.session_id, std::nullopt};
        s->append(KEY, {e({{"n", 1}})});
        s->append(sub1, {e({{"n", 1}})});
        s->append(sub2, {e({{"n", 1}})});
        s->append(other, {e({{"n", 1}})});
        s->append(other_proj, {e({{"n", 1}})});

        s->delete_session(KEY);

        check(!s->load(KEY).has_value(), "main gone");
        check(!s->load(sub1).has_value(), "sub1 gone");
        check(!s->load(sub2).has_value(), "sub2 gone");
        auto lo = s->load(other);
        check(lo && lo->size() == 1, "other preserved");
        auto lop = s->load(other_proj);
        check(lop && lop->size() == 1, "other_proj preserved");
        if (has_list_subkeys)
            check(s->list_subkeys({KEY.project_key, KEY.session_id}).empty(),
                  "subkeys gone");
    }, result);

    // 11. delete with subpath removes only that subkey
    runner.run("11.delete_subpath_only", !has_delete, [&] {
        auto s = make_store();
        claude::SessionKey sub1{KEY.project_key, KEY.session_id, std::string("subagents/agent-1")};
        claude::SessionKey sub2{KEY.project_key, KEY.session_id, std::string("subagents/agent-2")};
        s->append(KEY, {e({{"n", 1}})});
        s->append(sub1, {e({{"n", 1}})});
        s->append(sub2, {e({{"n", 1}})});

        s->delete_session(sub1);

        check(!s->load(sub1).has_value(), "sub1 gone");
        auto l2 = s->load(sub2);
        check(l2 && l2->size() == 1, "sub2 preserved");
        auto lm = s->load(KEY);
        check(lm && lm->size() == 1, "main preserved");
        if (has_list_subkeys)
        {
            auto sks = s->list_subkeys({KEY.project_key, KEY.session_id});
            check(sks.size() == 1 && sks[0] == "subagents/agent-2",
                  "only agent-2 remains");
        }
    }, result);

    // 12. list_subkeys returns subpaths (excluding other sessions)
    runner.run("12.list_subkeys_scoped", !has_list_subkeys, [&] {
        auto s = make_store();
        s->append(KEY, {e({{"n", 1}})});
        s->append({KEY.project_key, KEY.session_id, std::string("subagents/agent-1")},
                  {e({{"n", 1}})});
        s->append({KEY.project_key, KEY.session_id, std::string("subagents/agent-2")},
                  {e({{"n", 1}})});
        s->append({KEY.project_key, "other-sess", std::string("subagents/agent-x")},
                  {e({{"n", 1}})});
        auto sks = s->list_subkeys({KEY.project_key, KEY.session_id});
        std::sort(sks.begin(), sks.end());
        check(sks.size() == 2 && sks[0] == "subagents/agent-1" &&
                  sks[1] == "subagents/agent-2",
              "expected two subkeys for this session");
    }, result);

    // 13. list_subkeys excludes main transcript
    runner.run("13.list_subkeys_excludes_main", !has_list_subkeys, [&] {
        auto s = make_store();
        s->append(KEY, {e({{"n", 1}})});
        check(s->list_subkeys({KEY.project_key, KEY.session_id}).empty(),
              "no subkeys yet");
        check(s->list_subkeys({"proj", "never-appended"}).empty(),
              "unknown session returns empty");
    }, result);

    return result;
}

} // namespace claude::testing
