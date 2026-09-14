// Mirrors: reference/claude-agent-sdk-python session_resume / fork flow
//          (see _internal/session_resume.py and session_mutations.py).
//
// Demonstrates how to:
//   1. Append a transcript to an in-memory session store.
//   2. Materialize the resume into a temporary CLAUDE_CONFIG_DIR.
//   3. Fork the session into a new branch.
//
// Pure C++; does not require the claude CLI to be installed.

#include <claude/claude.hpp>
#include <claude/sessions/in_memory_session_store.hpp>
#include <claude/sessions/session_mutations.hpp>
#include <claude/sessions/session_resume.hpp>
#include <filesystem>
#include <iostream>
#include <memory>

namespace fs = std::filesystem;

namespace
{
claude::json user_entry(const std::string& uuid, const std::string& text,
                        const std::string& session_id)
{
    return claude::json{{"type", "user"},
                        {"uuid", uuid},
                        {"sessionId", session_id},
                        {"timestamp", "2024-01-01T00:00:00.000Z"},
                        {"message", {{"role", "user"}, {"content", text}}}};
}

claude::json assistant_entry(const std::string& uuid, const std::string& parent,
                             const std::string& text, const std::string& session_id)
{
    return claude::json{
        {"type", "assistant"},
        {"uuid", uuid},
        {"parentUuid", parent},
        {"sessionId", session_id},
        {"timestamp", "2024-01-01T00:00:01.000Z"},
        {"message",
         {{"role", "assistant"},
          {"content", claude::json::array({claude::json{{"type", "text"}, {"text", text}}})}}}};
}
} // namespace

int main()
{
    std::cout << "Claude SDK " << claude::version_string() << " - session resume / fork\n\n";

    auto store = std::make_shared<claude::sessions::InMemorySessionStore>();
    const std::string project_key = "demo-project";
    const std::string session_id = "11111111-2222-4333-8444-555555555555";
    claude::SessionKey key{project_key, session_id, std::nullopt};

    store->append(key,
                  {
                      user_entry("aaaaaaaa-0000-4000-8000-000000000001",
                                 "What's the capital of France?", session_id),
                      assistant_entry("bbbbbbbb-0000-4000-8000-000000000002",
                                      "aaaaaaaa-0000-4000-8000-000000000001", "Paris.", session_id),
                  });
    std::cout << "[append] stored " << store->load(key)->size() << " entries\n";

    // 1) Materialize the session into a temp CLAUDE_CONFIG_DIR so the CLI
    //    could resume from it.
    auto materialized =
        claude::sessions::materialize_resume_session(*store, project_key, session_id);
    std::cout << "[resume] materialized to " << materialized.config_dir.string() << "\n";
    fs::path jsonl = materialized.config_dir / "projects" / project_key / (session_id + ".jsonl");
    std::cout << "[resume] transcript exists: " << (fs::exists(jsonl) ? "yes" : "no") << "\n";
    materialized.cleanup();
    std::cout << "[resume] cleaned up: " << (fs::exists(materialized.config_dir) ? "no" : "yes")
              << "\n\n";

    // 2) Rename and tag the session — these mirror to the store as JSONL entries.
    claude::sessions::rename_session_via_store(*store, session_id, "Capitals of Europe",
                                               "demo-project");
    claude::sessions::tag_session_via_store(*store, session_id, std::string("demo"),
                                            "demo-project");
    std::cout << "[mutations] store entries after rename/tag: " << store->load(key)->size()
              << "\n\n";

    // 3) Fork the session.
    auto fork = claude::sessions::fork_session_via_store(*store, session_id, "demo-project");
    std::cout << "[fork] new session id: " << fork.session_id << "\n";
    auto forked = store->load({project_key, fork.session_id, std::nullopt});
    if (forked)
        std::cout << "[fork] forked transcript has " << forked->size() << " entries\n";

    return 0;
}
