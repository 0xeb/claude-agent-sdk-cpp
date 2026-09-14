#ifndef CLAUDE_INTERNAL_TRANSPORT_SUBPROCESS_ENV_HPP
#define CLAUDE_INTERNAL_TRANSPORT_SUBPROCESS_ENV_HPP

#include "../subprocess/process.hpp"

#include <claude/types.hpp>
#include <claude/version.hpp>

namespace claude::internal
{

inline void apply_sdk_environment(subprocess::ProcessOptions& proc_opts,
                                  const ClaudeOptions& options, const std::string& entrypoint)
{
    /// Python parity (commit 6d77aef): CLAUDE_CODE_ENTRYPOINT defaults to
    /// the SDK entrypoint name but options.environment can override it (user
    /// override wins). Previously this unconditionally overwrote the value.
    if (proc_opts.environment.find("CLAUDE_CODE_ENTRYPOINT") == proc_opts.environment.end())
        proc_opts.environment["CLAUDE_CODE_ENTRYPOINT"] = entrypoint;
    proc_opts.environment["CLAUDE_AGENT_SDK_VERSION"] = version_string();
    if (options.enable_file_checkpointing)
        proc_opts.environment["CLAUDE_CODE_ENABLE_SDK_FILE_CHECKPOINTING"] = "true";
}

} // namespace claude::internal

#endif // CLAUDE_INTERNAL_TRANSPORT_SUBPROCESS_ENV_HPP
