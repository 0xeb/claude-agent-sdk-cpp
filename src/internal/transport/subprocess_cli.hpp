#ifndef CLAUDE_TRANSPORT_SUBPROCESS_CLI_HPP
#define CLAUDE_TRANSPORT_SUBPROCESS_CLI_HPP

#include "../message_parser.hpp"
#include "../subprocess/process.hpp"

#include <atomic>
#include <claude/types.hpp>
#include <memory>
#include <optional>
#include <string>
#include <thread>

namespace claude
{
namespace transport
{

class SubprocessCLITransport
{
  public:
    SubprocessCLITransport(const std::string& prompt, const ClaudeOptions& options = {},
                           const std::optional<std::string>& cli_path = std::nullopt);

    ~SubprocessCLITransport();

    // No copy, move only
    SubprocessCLITransport(const SubprocessCLITransport&) = delete;
    SubprocessCLITransport& operator=(const SubprocessCLITransport&) = delete;
    SubprocessCLITransport(SubprocessCLITransport&&) noexcept;
    SubprocessCLITransport& operator=(SubprocessCLITransport&&) noexcept;

    // Connect to CLI (spawn process)
    void connect();

    // Check if connected and ready
    bool is_ready() const;

    // Read messages (returns all messages from CLI)
    std::vector<Message> read_messages();

    // Write message (for streaming mode)
    void write_message(const std::string& json);

    // Close connection
    void close();

  private:
    std::string prompt_;
    ClaudeOptions options_;
    std::string cli_path_;
    bool is_streaming_;
    std::unique_ptr<subprocess::Process> process_;
    std::unique_ptr<protocol::MessageParser> parser_;
    bool ready_;

    // Stderr handling
    std::unique_ptr<std::thread> stderr_thread_;
    std::atomic<bool> stderr_stop_flag_{false};

    // Temporary files for command line optimization
    std::vector<std::string> temp_files_;

    // Build command line arguments
    std::vector<std::string> build_command();

    // Build settings value, merging sandbox settings if provided
    std::optional<std::string> build_settings_value() const;

    // Find CLI executable
    static std::string find_cli(const std::optional<std::string>& hint);

    // Check CLI version
    void check_cli_version();

    // Stderr reader thread function
    void stderr_reader();
};

} // namespace transport
} // namespace claude

#endif // CLAUDE_TRANSPORT_SUBPROCESS_CLI_HPP
