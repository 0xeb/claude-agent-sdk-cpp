#ifndef CLAUDE_INTERNAL_SUBPROCESS_TRANSPORT_HPP
#define CLAUDE_INTERNAL_SUBPROCESS_TRANSPORT_HPP

#include "../message_parser.hpp"
#include "../subprocess/process.hpp"

#include <atomic>
#include <claude/claude.hpp>
#include <claude/transport.hpp>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>

namespace claude
{
namespace internal
{

/**
 * Subprocess transport implementation using Claude Code CLI.
 *
 * This transport manages a subprocess running the Claude Code CLI,
 * communicating via stdin/stdout using line-delimited JSON.
 */
class SubprocessTransport : public Transport
{
  public:
    explicit SubprocessTransport(const ClaudeOptions& options, bool streaming_mode = true);
    SubprocessTransport(const std::string& prompt,
                        const ClaudeOptions& options); // One-shot constructor
    ~SubprocessTransport() override;

    // Transport interface
    void connect() override;
    void write(const std::string& data) override;
    std::vector<Message> read_messages() override;
    bool has_messages() const override;
    void close() override;
    bool is_ready() const override;
    void end_input() override;
    long get_pid() const override;
    bool is_running() const override;

  private:
    // Build CLI command with options
    std::vector<std::string> build_command() const;

    // Find CLI executable (honors options_.cli_path when provided)
    std::string find_cli() const;

    // Check CLI version and warn if below minimum
    void check_claude_version(const std::string& cli_path);

    // Background reader thread (stdout)
    void reader_loop();
    void start_reader();
    void stop_reader();

    // Background stderr reader thread
    void stderr_reader_loop();
    void start_stderr_reader();
    void stop_stderr_reader();

    // Options
    ClaudeOptions options_;
    bool streaming_mode_;
    std::string one_shot_prompt_; // For print mode

    // Process management
    std::unique_ptr<subprocess::Process> process_;

    // Message parsing
    std::unique_ptr<protocol::MessageParser> parser_;

    // Thread-safe message queue
    mutable std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    std::queue<Message> message_queue_;
    bool queue_stopped_ = false;

    // Serialize stdin writes and coordinate with close/end_input
    mutable std::mutex write_mutex_;

    // Reader thread (stdout)
    std::thread reader_thread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> ready_{false};

    // Stderr reader thread
    std::thread stderr_reader_thread_;
    std::atomic<bool> stderr_running_{false};

    // Temporary files for command line optimization
    // Tracks temp files created for --agents JSON when command line is too long
    std::vector<std::string> temp_files_;
};

} // namespace internal
} // namespace claude

#endif // CLAUDE_INTERNAL_SUBPROCESS_TRANSPORT_HPP
