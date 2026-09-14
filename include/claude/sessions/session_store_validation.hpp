#ifndef CLAUDE_SESSIONS_SESSION_STORE_VALIDATION_HPP
#define CLAUDE_SESSIONS_SESSION_STORE_VALIDATION_HPP

#include <claude/sessions/session_store.hpp>
#include <claude/types.hpp>

namespace claude::sessions
{

/// Pre-flight validation for ClaudeOptions::session_store combinations.
///
/// Throws std::invalid_argument for invalid combinations:
///  * continue_conversation with session_store but no list_sessions support
///  * session_store combined with enable_file_checkpointing
///
/// Called before subprocess spawn so misconfiguration fails fast.
///
/// Python: _internal/session_store_validation.py:validate_session_store_options
/// (line 18).
void validate_session_store_options(const ClaudeOptions& options);

/// True if the store advertises a given optional capability bit. Convenience
/// wrapper around store.capabilities() & cap.
bool store_implements(const SessionStore& store, unsigned capability) noexcept;

} // namespace claude::sessions

#endif // CLAUDE_SESSIONS_SESSION_STORE_VALIDATION_HPP
