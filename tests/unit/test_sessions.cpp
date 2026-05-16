// Phase 3A unit tests for the C++ sessions subsystem.
// Covers: InMemorySessionStore, FileSessionStore, summary fold, transcript
// mirror batcher, conformance harness, store-backed free functions, and
// SessionStore mutations.

#include <gtest/gtest.h>

#include <claude/sessions/file_session_store.hpp>
#include <claude/sessions/free_functions.hpp>
#include <claude/sessions/in_memory_session_store.hpp>
#include <claude/sessions/session_import.hpp>
#include <claude/sessions/session_mutations.hpp>
#include <claude/sessions/session_resume.hpp>
#include <claude/sessions/session_store_validation.hpp>
#include <claude/sessions/session_summary.hpp>
#include <claude/sessions/transcript_mirror_batcher.hpp>
#include <claude/testing/session_store_conformance.hpp>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>

namespace fs = std::filesystem;
using claude::SessionKey;
using claude::SessionStoreEntry;
using claude::sessions::FileSessionStore;
using claude::sessions::InMemorySessionStore;
namespace SSCap = claude::SessionStoreCapability;

namespace
{
std::string unique_temp_dir(const char* prefix)
{
    auto base = fs::temp_directory_path();
    auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    fs::path p =
        base / (std::string(prefix) + "_" + std::to_string(stamp) + "_" +
                std::to_string(::getpid()));
    fs::create_directories(p);
    return p.string();
}

claude::json mk_user_entry(const std::string& uuid, const std::string& text,
                           const std::string& parent = "")
{
    claude::json j = {
        {"type", "user"},
        {"uuid", uuid},
        {"sessionId", "00000000-0000-4000-8000-000000000001"},
        {"timestamp", "2024-01-01T00:00:00.000Z"},
        {"message", {{"role", "user"}, {"content", text}}},
    };
    if (!parent.empty())
        j["parentUuid"] = parent;
    return j;
}

claude::json mk_assistant_entry(const std::string& uuid, const std::string& text,
                                const std::string& parent)
{
    claude::json j = {
        {"type", "assistant"},
        {"uuid", uuid},
        {"sessionId", "00000000-0000-4000-8000-000000000001"},
        {"timestamp", "2024-01-01T00:00:01.000Z"},
        {"parentUuid", parent},
        {"message", {{"role", "assistant"},
                     {"content", claude::json::array({claude::json{
                                     {"type", "text"}, {"text", text}}})}}},
    };
    return j;
}
} // namespace

// ----- InMemorySessionStore basics ----------------------------------------

TEST(Sessions_InMemoryStore, AppendLoadRoundtrip)
{
    InMemorySessionStore s;
    SessionKey k{"proj", "sess", std::nullopt};
    s.append(k, {claude::json{{"type", "x"}, {"uuid", "a"}}});
    auto loaded = s.load(k);
    ASSERT_TRUE(loaded.has_value());
    ASSERT_EQ(loaded->size(), 1u);
    EXPECT_EQ((*loaded)[0]["uuid"], "a");
}

TEST(Sessions_InMemoryStore, SizeAndClear)
{
    InMemorySessionStore s;
    s.append({"p", "a", std::nullopt}, {claude::json{{"type", "x"}}});
    s.append({"p", "b", std::nullopt}, {claude::json{{"type", "x"}}});
    s.append({"p", "a", std::string("sub")}, {claude::json{{"type", "x"}}});
    EXPECT_EQ(s.size(), 2u);
    s.clear();
    EXPECT_EQ(s.size(), 0u);
}

TEST(Sessions_InMemoryStore, CapabilityFlags)
{
    InMemorySessionStore s;
    EXPECT_EQ(s.capabilities(), SSCap::All);
}

// ----- FileSessionStore ----------------------------------------------------

TEST(Sessions_FileStore, AppendLoadOnDisk)
{
    std::string root = unique_temp_dir("file_session_store");
    {
        FileSessionStore s(root);
        SessionKey k{"proj", "00000000-0000-4000-8000-000000000001", std::nullopt};
        s.append(k, {claude::json{{"type", "user"}, {"uuid", "a"}}});
        auto l = s.load(k);
        ASSERT_TRUE(l.has_value());
        EXPECT_EQ(l->size(), 1u);
    }
    fs::remove_all(root);
}

TEST(Sessions_FileStore, SubpathLayout)
{
    std::string root = unique_temp_dir("file_session_store_sub");
    {
        FileSessionStore s(root);
        SessionKey main{"proj", "sess", std::nullopt};
        SessionKey sub{"proj", "sess", std::string("subagents/agent-a")};
        s.append(main, {claude::json{{"type", "user"}, {"uuid", "1"}}});
        s.append(sub, {claude::json{{"type", "user"}, {"uuid", "2"}}});
        auto sks = s.list_subkeys({"proj", "sess"});
        ASSERT_EQ(sks.size(), 1u);
        EXPECT_EQ(sks[0], "subagents/agent-a");
        auto ls = s.list_sessions("proj");
        ASSERT_EQ(ls.size(), 1u);
        EXPECT_EQ(ls[0].session_id, "sess");
    }
    fs::remove_all(root);
}

// ----- session_summary -----------------------------------------------------

TEST(Sessions_Summary, FoldsCustomTitleAndCreatedAt)
{
    using claude::sessions::fold_session_summary;
    SessionKey k{"proj", "s1", std::nullopt};
    std::vector<SessionStoreEntry> batch = {
        claude::json{{"type", "user"},
                     {"timestamp", "2024-01-01T00:00:00.000Z"},
                     {"customTitle", "first"},
                     {"message", {{"content", "hello world"}}}},
        claude::json{{"type", "user"},
                     {"timestamp", "2024-01-01T00:00:01.000Z"}},
    };
    auto folded = fold_session_summary(std::nullopt, k, batch);
    EXPECT_EQ(folded.session_id, "s1");
    EXPECT_EQ(folded.data["custom_title"], "first");
    EXPECT_TRUE(folded.data.contains("created_at"));
}

TEST(Sessions_Summary, CreatedAtScansFullHeadBuffer)
{
    // Regression test for Python commit 99479bc: created_at must scan the
    // whole head, not just the first line. A metadata-only entry can lead;
    // the next entry's timestamp is what we want.
    using claude::sessions::fold_session_summary;
    SessionKey k{"proj", "s1", std::nullopt};
    std::vector<SessionStoreEntry> batch = {
        claude::json{{"type", "permission-mode"}}, // no timestamp
        claude::json{{"type", "user"},
                     {"timestamp", "2024-06-15T12:34:56.789Z"},
                     {"message", {{"content", "hi"}}}},
    };
    auto folded = fold_session_summary(std::nullopt, k, batch);
    ASSERT_TRUE(folded.data.contains("created_at"));
    EXPECT_GT(folded.data["created_at"].get<int64_t>(),
              static_cast<int64_t>(1e12));
}

TEST(Sessions_Summary, IsoToEpochMs)
{
    using claude::sessions::iso_to_epoch_ms;
    auto v = iso_to_epoch_ms("1970-01-01T00:00:00.000Z");
    ASSERT_TRUE(v.has_value());
    EXPECT_EQ(*v, 0);
    auto v2 = iso_to_epoch_ms("2024-01-01T00:00:00.000Z");
    ASSERT_TRUE(v2.has_value());
    EXPECT_GT(*v2, static_cast<int64_t>(1700000000) * 1000);
    EXPECT_FALSE(iso_to_epoch_ms("not a timestamp").has_value());
}

// ----- TranscriptMirrorBatcher --------------------------------------------

TEST(Sessions_MirrorBatcher, EagerFlushOnSizeThreshold)
{
    using namespace claude::sessions;
    auto store = std::make_shared<InMemorySessionStore>();
    auto resolver = [](const std::string&) -> std::optional<SessionKey> {
        return SessionKey{"proj", "sess", std::nullopt};
    };
    std::atomic<int> err_count{0};
    auto on_err = [&](const std::optional<SessionKey>&, const std::string&) {
        ++err_count;
    };
    TranscriptMirrorBatcher batcher(store, resolver, on_err,
                                    /*max_entries=*/2, /*max_bytes=*/MAX_PENDING_BYTES);
    batcher.enqueue("/tmp/x.jsonl", {claude::json{{"type", "user"}, {"uuid", "1"}}});
    batcher.enqueue("/tmp/x.jsonl", {claude::json{{"type", "user"}, {"uuid", "2"}},
                                     claude::json{{"type", "user"}, {"uuid", "3"}}});
    batcher.flush();
    auto loaded = store->load({"proj", "sess", std::nullopt});
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->size(), 3u);
    EXPECT_EQ(err_count.load(), 0);
}

TEST(Sessions_MirrorBatcher, CancellationNoSpuriousErrors)
{
    // Regression for commit 9d2c650: destruction during pending work should
    // not invoke on_error with a cancellation-shaped message.
    using namespace claude::sessions;
    auto store = std::make_shared<InMemorySessionStore>();
    auto resolver = [](const std::string&) -> std::optional<SessionKey> {
        return SessionKey{"proj", "sess", std::nullopt};
    };
    std::atomic<int> err_count{0};
    auto on_err = [&](const std::optional<SessionKey>&, const std::string&) {
        ++err_count;
    };
    {
        TranscriptMirrorBatcher batcher(store, resolver, on_err);
        for (int i = 0; i < 5; ++i)
            batcher.enqueue("/tmp/x.jsonl",
                            {claude::json{{"type", "user"}, {"uuid", std::to_string(i)}}});
        // Implicit close() in dtor — should flush cleanly.
    }
    EXPECT_EQ(err_count.load(), 0);
    auto loaded = store->load({"proj", "sess", std::nullopt});
    ASSERT_TRUE(loaded.has_value());
    EXPECT_GE(loaded->size(), 5u);
}

TEST(Sessions_MirrorBatcher, RetriesOnAdapterFailureThenReports)
{
    using namespace claude::sessions;
    struct FlakyStore : claude::SessionStore
    {
        std::atomic<int> attempts{0};
        unsigned capabilities() const noexcept override { return 0; }
        void append(const SessionKey&, const std::vector<SessionStoreEntry>&) override
        {
            ++attempts;
            throw std::runtime_error("nope");
        }
        std::optional<std::vector<SessionStoreEntry>> load(const SessionKey&) override
        {
            return std::nullopt;
        }
    };
    auto store = std::make_shared<FlakyStore>();
    auto resolver = [](const std::string&) -> std::optional<SessionKey> {
        return SessionKey{"proj", "sess", std::nullopt};
    };
    std::atomic<int> err_count{0};
    auto on_err = [&](const std::optional<SessionKey>&, const std::string&) {
        ++err_count;
    };
    {
        TranscriptMirrorBatcher batcher(store, resolver, on_err);
        batcher.enqueue("/tmp/x.jsonl", {claude::json{{"type", "user"}, {"uuid", "1"}}});
        batcher.flush();
    }
    EXPECT_EQ(store->attempts.load(), MIRROR_APPEND_MAX_ATTEMPTS);
    EXPECT_EQ(err_count.load(), 1);
}

// ----- Conformance harness -------------------------------------------------

TEST(Sessions_Conformance, InMemoryPassesAllContracts)
{
    auto result = claude::testing::run_session_store_conformance(
        [] { return std::make_shared<InMemorySessionStore>(); });
    for (const auto& c : result.contracts)
        EXPECT_TRUE(c.passed || c.skipped)
            << c.name << ": " << c.message;
    EXPECT_TRUE(result.all_passed())
        << "passed=" << result.passed << " failed=" << result.failed
        << " skipped=" << result.skipped;
}

TEST(Sessions_Conformance, FileStorePassesAllContracts)
{
    std::string base = unique_temp_dir("file_session_conformance");
    std::atomic<int> counter{0};
    auto result = claude::testing::run_session_store_conformance(
        [&] {
            fs::path sub = fs::path(base) / std::to_string(counter.fetch_add(1));
            fs::create_directories(sub);
            return std::make_shared<FileSessionStore>(sub.string());
        });
    for (const auto& c : result.contracts)
        EXPECT_TRUE(c.passed || c.skipped)
            << c.name << ": " << c.message;
    EXPECT_TRUE(result.all_passed());
    fs::remove_all(base);
}

// ----- Free functions: store-backed listings -------------------------------

TEST(Sessions_FreeFunctions, ListAndGetMessagesViaStore)
{
    using namespace claude::sessions;
    auto store = std::make_shared<InMemorySessionStore>();
    std::string project_key = project_key_for_directory();

    SessionKey k{project_key, "00000000-0000-4000-8000-000000000001", std::nullopt};
    store->append(k, {
        mk_user_entry("aaaaaaaa-0000-4000-8000-000000000001", "hi there"),
        mk_assistant_entry("bbbbbbbb-0000-4000-8000-000000000002", "hello",
                           "aaaaaaaa-0000-4000-8000-000000000001"),
    });

    auto msgs = get_session_messages_from_store(
        *store, "00000000-0000-4000-8000-000000000001", "");
    ASSERT_EQ(msgs.size(), 2u);
    EXPECT_EQ(msgs[0].type, "user");
    EXPECT_EQ(msgs[1].type, "assistant");

    auto infos = list_sessions_from_store(*store, "");
    ASSERT_GE(infos.size(), 1u);
    EXPECT_EQ(infos[0].session_id, "00000000-0000-4000-8000-000000000001");
}

// ----- Mutations ----------------------------------------------------------

TEST(Sessions_Mutations, RenameTagDeleteRoundtrip)
{
    using namespace claude::sessions;
    auto store = std::make_shared<InMemorySessionStore>();
    std::string sid = "11111111-2222-4333-8444-555555555555";
    SessionKey k{project_key_for_directory(), sid, std::nullopt};
    store->append(k, {mk_user_entry("aaaaaaaa-0000-4000-8000-000000000001", "hi")});

    rename_session_via_store(*store, sid, "My title");
    tag_session_via_store(*store, sid, std::string("experiment"));

    auto loaded = store->load(k);
    ASSERT_TRUE(loaded.has_value());
    bool seen_title = false, seen_tag = false;
    for (const auto& e : *loaded)
    {
        if (e["type"] == "custom-title" && e["customTitle"] == "My title")
            seen_title = true;
        if (e["type"] == "tag" && e["tag"] == "experiment")
            seen_tag = true;
    }
    EXPECT_TRUE(seen_title);
    EXPECT_TRUE(seen_tag);

    delete_session_via_store(*store, sid);
    EXPECT_FALSE(store->load(k).has_value());
}

TEST(Sessions_Mutations, ForkCreatesIndependentSession)
{
    using namespace claude::sessions;
    auto store = std::make_shared<InMemorySessionStore>();
    std::string sid = "11111111-2222-4333-8444-555555555555";
    SessionKey k{project_key_for_directory(), sid, std::nullopt};
    store->append(k, {
        mk_user_entry("aaaaaaaa-0000-4000-8000-000000000001", "first"),
        mk_assistant_entry("bbbbbbbb-0000-4000-8000-000000000002", "reply",
                           "aaaaaaaa-0000-4000-8000-000000000001"),
    });
    auto result = fork_session_via_store(*store, sid);
    EXPECT_NE(result.session_id, sid);
    EXPECT_TRUE(validate_uuid(result.session_id));
    auto forked =
        store->load({project_key_for_directory(), result.session_id, std::nullopt});
    ASSERT_TRUE(forked.has_value());
    EXPECT_GE(forked->size(), 2u);
    // The fork rewrites UUIDs.
    bool reused = false;
    for (const auto& e : *forked)
        if (e.contains("uuid") &&
            (e["uuid"] == "aaaaaaaa-0000-4000-8000-000000000001" ||
             e["uuid"] == "bbbbbbbb-0000-4000-8000-000000000002"))
            reused = true;
    EXPECT_FALSE(reused);
}

TEST(Sessions_Mutations, RenameRejectsInvalidInputs)
{
    using namespace claude::sessions;
    InMemorySessionStore s;
    EXPECT_THROW(rename_session_via_store(s, "not-a-uuid", "x"),
                 std::invalid_argument);
    EXPECT_THROW(rename_session_via_store(s, "11111111-2222-4333-8444-555555555555",
                                          "   "),
                 std::invalid_argument);
}

// ----- session_import -----------------------------------------------------

TEST(Sessions_Import, ReplayJsonlIntoStore)
{
    using namespace claude::sessions;
    auto root = unique_temp_dir("session_import");
    fs::path jsonl_path = fs::path(root) / "imported.jsonl";
    std::string sid = "11111111-2222-4333-8444-555555555555";
    {
        std::ofstream f(jsonl_path, std::ios::binary);
        for (const auto& e :
             {mk_user_entry("aaaaaaaa-0000-4000-8000-000000000001", "one"),
              mk_assistant_entry("bbbbbbbb-0000-4000-8000-000000000002", "two",
                                 "aaaaaaaa-0000-4000-8000-000000000001")})
            f << e.dump() << '\n';
    }
    auto store = std::make_shared<InMemorySessionStore>();
    import_session_to_store(sid, *store, jsonl_path, "proj-key");
    auto loaded = store->load({"proj-key", sid, std::nullopt});
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->size(), 2u);
    fs::remove_all(root);
}

// ----- session_resume -----------------------------------------------------

TEST(Sessions_Resume, MaterializeWritesJsonlAndCleansUp)
{
    using namespace claude::sessions;
    auto store = std::make_shared<InMemorySessionStore>();
    std::string sid = "11111111-2222-4333-8444-555555555555";
    SessionKey k{"proj-key", sid, std::nullopt};
    store->append(k, {mk_user_entry("aaaaaaaa-0000-4000-8000-000000000001", "hi")});
    auto m = materialize_resume_session(*store, "proj-key", sid);
    EXPECT_EQ(m.resume_session_id, sid);
    fs::path expected = m.config_dir / "projects" / "proj-key" / (sid + ".jsonl");
    EXPECT_TRUE(fs::exists(expected));
    ASSERT_TRUE(static_cast<bool>(m.cleanup));
    m.cleanup();
    EXPECT_FALSE(fs::exists(m.config_dir));
}

TEST(Sessions_Resume, BuildMirrorBatcherResolvesPaths)
{
    using namespace claude::sessions;
    auto store = std::make_shared<InMemorySessionStore>();
    fs::path projects_dir = fs::temp_directory_path() / "claude-resume-projects-test";
    fs::create_directories(projects_dir);
    auto batcher = build_mirror_batcher(store, projects_dir, nullptr);
    ASSERT_TRUE(batcher);
    fs::path file = projects_dir / "myproj" /
                    "11111111-2222-4333-8444-555555555555.jsonl";
    batcher->enqueue(file.string(),
                     {claude::json{{"type", "user"}, {"uuid", "x"}}});
    batcher->flush();
    auto loaded = store->load(
        {"myproj", "11111111-2222-4333-8444-555555555555", std::nullopt});
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->size(), 1u);
    fs::remove_all(projects_dir);
}

// ----- Validation ---------------------------------------------------------

TEST(Sessions_Validation, RejectsConflictingOptions)
{
    using namespace claude::sessions;
    claude::ClaudeOptions opts;
    opts.session_store = std::make_shared<InMemorySessionStore>();
    opts.enable_file_checkpointing = true;
    EXPECT_THROW(validate_session_store_options(opts), std::invalid_argument);
}

TEST(Sessions_Validation, ContinueRequiresListSessions)
{
    using namespace claude::sessions;
    struct MinimalStore : claude::SessionStore
    {
        unsigned capabilities() const noexcept override { return 0; }
        void append(const SessionKey&, const std::vector<SessionStoreEntry>&) override {}
        std::optional<std::vector<SessionStoreEntry>> load(const SessionKey&) override
        {
            return std::nullopt;
        }
    };
    claude::ClaudeOptions opts;
    opts.session_store = std::make_shared<MinimalStore>();
    opts.continue_conversation = true;
    EXPECT_THROW(validate_session_store_options(opts), std::invalid_argument);
    // OK when resume is set explicitly.
    opts.resume = "11111111-2222-4333-8444-555555555555";
    EXPECT_NO_THROW(validate_session_store_options(opts));
}
