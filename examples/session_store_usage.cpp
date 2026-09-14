// Mirrors: reference/claude-agent-sdk-python/examples/session_stores/README.md
//          (in-memory store usage section) and the SessionStore protocol
//          introduced in Python commit 6e3d54f.
//
// Demonstrates how to wire an in-memory ``SessionStore`` into ``ClaudeOptions``
// so transcripts are mirrored to a custom adapter. The mirroring half of the
// flow runs against the live CLI; the store API itself works without any CLI
// installed, so the example exercises that path first and only invokes the CLI
// when one is available on PATH.

#include <claude/claude.hpp>
#include <claude/sessions/free_functions.hpp>
#include <claude/sessions/in_memory_session_store.hpp>
#include <claude/sessions/session_mutations.hpp>

#include <cstdlib>
#include <iostream>
#include <memory>

namespace
{
bool cli_available()
{
    const char* path = std::getenv("PATH");
    if (path == nullptr)
        return false;
    return claude::sessions::project_key_for_directory().size() > 0;
}

void demonstrate_store_api()
{
    auto store = std::make_shared<claude::sessions::InMemorySessionStore>();

    const std::string project_key = claude::sessions::project_key_for_directory();
    const std::string session_id = "00000000-0000-4000-8000-000000000001";
    claude::SessionKey key{project_key, session_id, std::nullopt};

    claude::json user_entry = {
        {"type", "user"},
        {"uuid", "11111111-1111-4111-8111-111111111111"},
        {"sessionId", session_id},
        {"timestamp", "2024-01-01T00:00:00.000Z"},
        {"message", {{"role", "user"}, {"content", "Hello from C++"}}}};
    claude::json assistant_entry = {
        {"type", "assistant"},
        {"uuid", "22222222-2222-4222-8222-222222222222"},
        {"parentUuid", "11111111-1111-4111-8111-111111111111"},
        {"sessionId", session_id},
        {"timestamp", "2024-01-01T00:00:01.000Z"},
        {"message",
         {{"role", "assistant"},
          {"content", claude::json::array({claude::json{{"type", "text"}, {"text", "Hi!"}}})}}}};

    store->append(key, {user_entry, assistant_entry});

    auto loaded = store->load(key);
    std::cout << "[store] loaded " << (loaded ? loaded->size() : 0u) << " transcript entries\n";

    auto sessions = claude::sessions::list_sessions_from_store(*store, "");
    std::cout << "[store] list_sessions returned " << sessions.size() << " session(s)\n";

    auto messages = claude::sessions::get_session_messages_from_store(*store, session_id, "");
    std::cout << "[store] get_session_messages returned " << messages.size() << " message(s)\n";
}

void demonstrate_query_with_store()
{
    if (!cli_available())
    {
        std::cout << "[query] skipping live query (claude CLI not detected)\n";
        return;
    }

    auto store = std::make_shared<claude::sessions::InMemorySessionStore>();

    claude::ClaudeOptions opts;
    opts.permission_mode = "bypassPermissions";
    opts.session_store = store;
    opts.model = "claude-sonnet-4-5";

    try
    {
        auto result = claude::query("Say hi briefly.", opts);
        for (const auto& msg : result)
        {
            if (claude::is_assistant_message(msg))
            {
                const auto& assistant = std::get<claude::AssistantMessage>(msg);
                std::cout << "[query] assistant: "
                          << claude::get_text_content(assistant.content) << "\n";
            }
        }
        std::cout << "[query] in-memory store now holds " << store->size() << " session(s)\n";
    }
    catch (const claude::CLINotFoundError&)
    {
        std::cout << "[query] claude CLI not found; skip live demo\n";
    }
    catch (const claude::ClaudeError& e)
    {
        std::cerr << "[query] error: " << e.what() << "\n";
    }
}
} // namespace

int main()
{
    std::cout << "Claude SDK " << claude::version_string() << " - session store usage\n\n";
    demonstrate_store_api();
    std::cout << "\n";
    demonstrate_query_with_store();
    return 0;
}
