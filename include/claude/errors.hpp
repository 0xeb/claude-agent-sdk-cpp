#ifndef CLAUDE_ERRORS_HPP
#define CLAUDE_ERRORS_HPP

#include <memory>
#include <nlohmann/json.hpp>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace claude
{

// Base exception
class ClaudeError : public std::runtime_error
{
  public:
    explicit ClaudeError(const std::string& message) : std::runtime_error(message) {}
};

// CLI not found
class CLINotFoundError : public ClaudeError
{
  public:
    explicit CLINotFoundError(const std::string& message) : ClaudeError(message) {}
};

// Connection error
class CLIConnectionError : public ClaudeError
{
  public:
    explicit CLIConnectionError(const std::string& message) : ClaudeError(message) {}
};

// Process error
class ProcessError : public ClaudeError
{
  public:
    ProcessError(const std::string& message, int exit_code)
        : ClaudeError(message), exit_code_(exit_code)
    {
    }

    int exit_code() const
    {
        return exit_code_;
    }

  private:
    int exit_code_;
};

/// Thrown when the CLI exits after reporting a terminal error result.
///
/// The CLI ends a failed run by emitting a `result` message with
/// `is_error: true` and then exiting non-zero. This replaces the bare
/// "exit code 1" ProcessError for that case and carries the result's payload,
/// so callers can branch on *why* the run failed without matching on strings.
///
/// Subclasses ProcessError, so existing `catch (const ProcessError&)` handlers
/// keep working.
class ResultError : public ProcessError
{
  public:
    ResultError(const std::string& message,
                int exit_code,
                std::string subtype,
                std::optional<std::string> terminal_reason,
                std::vector<std::string> errors)
        : ProcessError(message, exit_code),
          subtype_(std::move(subtype)),
          terminal_reason_(std::move(terminal_reason)),
          errors_(std::move(errors))
    {
    }

    /// Result subtype: "error_max_turns", "error_during_execution", ... or
    /// "success" when the agent loop completed but the last turn was an API
    /// error.
    const std::string& subtype() const noexcept { return subtype_; }

    /// Why the run terminated, when the CLI reported it (e.g. "api_error").
    const std::optional<std::string>& terminal_reason() const noexcept
    {
        return terminal_reason_;
    }

    /// Error strings from the result frame, normalized: blanks and non-strings
    /// dropped so these always agree with the exception message.
    const std::vector<std::string>& errors() const noexcept { return errors_; }

  private:
    std::string subtype_;
    std::optional<std::string> terminal_reason_;
    std::vector<std::string> errors_;
};

/// Normalize a result frame's `errors` field to clean strings.
///
/// The CLI emits an array of strings; a bare string is tolerated (older or
/// buggy emitters) and non-string or blank entries are dropped, so the
/// structured ResultError::errors and the exception text always agree.
inline std::vector<std::string> normalize_result_errors(const nlohmann::json& raw)
{
    const auto clean = [](const std::string& s)
    {
        const auto first = s.find_first_not_of(" \t\n\r\f\v");
        if (first == std::string::npos)
            return std::string{};
        const auto last = s.find_last_not_of(" \t\n\r\f\v");
        return s.substr(first, last - first + 1);
    };

    std::vector<std::string> out;
    if (raw.is_string())
    {
        if (auto s = clean(raw.get<std::string>()); !s.empty())
            out.push_back(std::move(s));
        return out;
    }
    if (!raw.is_array())
        return out;
    for (const auto& entry : raw)
    {
        if (!entry.is_string())
            continue;
        if (auto s = clean(entry.get<std::string>()); !s.empty())
            out.push_back(std::move(s));
    }
    return out;
}

// JSON decode error
class JSONDecodeError : public ClaudeError
{
  public:
    explicit JSONDecodeError(const std::string& message) : ClaudeError(message) {}
};

// Message parse error
class MessageParseError : public ClaudeError
{
  public:
    explicit MessageParseError(const std::string& message) : ClaudeError(message), data_(nullptr) {}

    MessageParseError(const std::string& message, const nlohmann::json& data)
        : ClaudeError(message), data_(std::make_shared<nlohmann::json>(data))
    {
    }

    // Get the optional data associated with the parse error
    const nlohmann::json* data() const
    {
        return data_.get();
    }

  private:
    std::shared_ptr<nlohmann::json> data_;
};

} // namespace claude

#endif // CLAUDE_ERRORS_HPP
