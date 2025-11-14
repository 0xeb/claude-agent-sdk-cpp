#ifndef CLAUDE_HOOKS_HPP
#define CLAUDE_HOOKS_HPP

#include <string>
#include <vector>

namespace claude
{
namespace hooks
{

// Standard Claude Code hooks
// These hooks are triggered by the CLI at various points during tool execution

/// Triggered before executing a bash command
/// Payload: {"command": string, "cwd": string}
constexpr const char* BASH_BEFORE_EXECUTE = "bash_before_execute";

/// Triggered after a bash command completes
/// Payload: {"command": string, "exit_code": int, "output": string}
constexpr const char* BASH_AFTER_EXECUTE = "bash_after_execute";

/// Triggered before editing a file
/// Payload: {"file": string, "old_string": string, "new_string": string}
constexpr const char* EDIT_BEFORE = "edit_before";

/// Triggered after editing a file
/// Payload: {"file": string, "success": bool}
constexpr const char* EDIT_AFTER = "edit_after";

/// Triggered before writing a file
/// Payload: {"file": string, "content": string}
constexpr const char* WRITE_BEFORE = "write_before";

/// Triggered after writing a file
/// Payload: {"file": string, "success": bool}
constexpr const char* WRITE_AFTER = "write_after";

/// Get all standard hooks as a vector
/// Useful for registering all hooks at once
inline std::vector<std::string> all_standard_hooks()
{
    return {
        BASH_BEFORE_EXECUTE,
        BASH_AFTER_EXECUTE,
        EDIT_BEFORE,
        EDIT_AFTER,
        WRITE_BEFORE,
        WRITE_AFTER
    };
}

} // namespace hooks
} // namespace claude

#endif // CLAUDE_HOOKS_HPP
