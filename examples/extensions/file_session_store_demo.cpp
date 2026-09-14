// C++-specific extension example (no Python counterpart in src/).
//
// Demonstrates the in-tree ``FileSessionStore`` adapter writing transcripts to
// disk as JSONL — the same on-disk layout the CLI uses under
// ``~/.claude/projects/``. Pure C++; runs without the claude CLI.
//
// Use the templated API exclusively (no decorator macros) per
// examples/extensions/ policy.

#include <chrono>
#include <claude/claude.hpp>
#include <claude/sessions/file_session_store.hpp>
#include <claude/sessions/free_functions.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>

namespace fs = std::filesystem;

namespace
{
fs::path make_temp_store_root()
{
    auto base = fs::temp_directory_path();
    auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    fs::path p = base / ("claude_file_store_demo_" + std::to_string(stamp));
    fs::create_directories(p);
    return p;
}

claude::json user_entry(const std::string& uuid, const std::string& text,
                        const std::string& session_id)
{
    return claude::json{{"type", "user"},
                        {"uuid", uuid},
                        {"sessionId", session_id},
                        {"timestamp", "2024-01-01T00:00:00.000Z"},
                        {"message", {{"role", "user"}, {"content", text}}}};
}
} // namespace

int main()
{
    std::cout << "Claude SDK " << claude::version_string()
              << " - FileSessionStore adapter demo\n\n";

    fs::path root = make_temp_store_root();
    std::cout << "[setup] file store root: " << root.string() << "\n";

    auto store = std::make_shared<claude::sessions::FileSessionStore>(root);
    const std::string project_key = "demo-project";
    const std::string session_id = "11111111-2222-4333-8444-555555555555";
    claude::SessionKey key{project_key, session_id, std::nullopt};

    store->append(key, {
                           user_entry("aaaaaaaa-0000-4000-8000-000000000001",
                                      "Write a short haiku.", session_id),
                       });

    // Subagent transcripts go under <session>/<subpath>.jsonl
    claude::SessionKey sub{project_key, session_id, std::string("subagents/agent-a")};
    store->append(sub, {
                           user_entry("cccccccc-0000-4000-8000-000000000003",
                                      "Subagent reasoning step.", session_id),
                       });

    fs::path expected_main = root / project_key / (session_id + ".jsonl");
    fs::path expected_sub = root / project_key / session_id / "subagents" / "agent-a.jsonl";
    std::cout << "[disk] main JSONL exists: " << (fs::exists(expected_main) ? "yes" : "no") << " ("
              << expected_main.string() << ")\n";
    std::cout << "[disk] subagent JSONL exists: " << (fs::exists(expected_sub) ? "yes" : "no")
              << " (" << expected_sub.string() << ")\n";

    auto sessions_list = store->list_sessions(project_key);
    std::cout << "[list] sessions in project: " << sessions_list.size() << "\n";

    auto subkeys = store->list_subkeys({project_key, session_id});
    std::cout << "[list] subkeys for session: " << subkeys.size() << "\n";
    for (const auto& sk : subkeys)
        std::cout << "         - " << sk << "\n";

    auto messages = claude::sessions::get_session_messages_from_store(*store, session_id, "");
    std::cout << "[list] reconstructed " << messages.size() << " conversation message(s)\n";

    // Cleanup: in production this would be left to the operator. Keep the demo
    // self-contained.
    std::error_code ec;
    fs::remove_all(root, ec);
    std::cout << "\n[cleanup] removed temp root (error: " << ec.message() << ")\n";
    return 0;
}
