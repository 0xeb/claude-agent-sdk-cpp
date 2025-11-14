#ifndef CLAUDE_ERRORS_HPP
#define CLAUDE_ERRORS_HPP

#include <memory>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>

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
    explicit MessageParseError(const std::string& message)
        : ClaudeError(message), data_(nullptr) {}

    MessageParseError(const std::string& message, const nlohmann::json& data)
        : ClaudeError(message), data_(std::make_shared<nlohmann::json>(data)) {}

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
