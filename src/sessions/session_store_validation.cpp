#include <claude/sessions/session_store_validation.hpp>

#include <stdexcept>

namespace claude::sessions
{

bool store_implements(const SessionStore& store, unsigned capability) noexcept
{
    return (store.capabilities() & capability) != 0;
}

void validate_session_store_options(const ClaudeOptions& options)
{
    if (!options.session_store)
        return;

    if (options.continue_conversation && options.resume.empty() &&
        !store_implements(*options.session_store, SessionStoreCapability::ListSessions))
    {
        throw std::invalid_argument(
            "continue_conversation with session_store requires the store to "
            "implement list_sessions()");
    }

    if (options.enable_file_checkpointing)
    {
        throw std::invalid_argument(
            "session_store cannot be combined with enable_file_checkpointing "
            "(checkpoints are local-disk only and would diverge from the "
            "mirrored transcript)");
    }
}

} // namespace claude::sessions
