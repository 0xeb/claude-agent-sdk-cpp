#ifndef CLAUDE_CLIENT_HPP
#define CLAUDE_CLIENT_HPP

#include <chrono>
#include <claude/transport.hpp>
#include <claude/types.hpp>
#include <memory>
#include <optional>
#include <string>

namespace claude
{

// Forward declarations
namespace subprocess
{
class Process;
}

namespace transport
{
class SubprocessCLITransport;
}

namespace protocol
{
class MessageParser;
}

// Iterator for streaming messages from Claude
class MessageStream
{
  public:
    class Iterator
    {
      public:
        using iterator_category = std::input_iterator_tag;
        using value_type = Message;
        using difference_type = std::ptrdiff_t;
        using pointer = const Message*;
        using reference = const Message&;

        Iterator();
        explicit Iterator(MessageStream* stream);

        reference operator*() const;
        pointer operator->() const;
        Iterator& operator++();

        bool operator==(const Iterator& other) const;
        bool operator!=(const Iterator& other) const;

      private:
        MessageStream* stream_;
        std::optional<Message> current_;
        bool is_end_;

        void fetch_next();
    };

    MessageStream();
    ~MessageStream();

    // No copy, move only
    MessageStream(const MessageStream&) = delete;
    MessageStream& operator=(const MessageStream&) = delete;
    MessageStream(MessageStream&&) noexcept;
    MessageStream& operator=(MessageStream&&) noexcept;

    Iterator begin();
    Iterator end();

    // Get next message (blocking)
    std::optional<Message> get_next();

    // Get next message with timeout (returns nullopt on timeout or end)
    std::optional<Message> get_next_for(std::chrono::milliseconds timeout);

    // Check if more messages available
    bool has_more() const;

    // Stop streaming
    void stop();

  private:
    friend class ClaudeClient;

    class Impl;
    std::shared_ptr<Impl> impl_;
};

// Main client class for bidirectional communication with Claude Code CLI
class ClaudeClient
{
  public:
    explicit ClaudeClient(const ClaudeOptions& options = ClaudeOptions{});
    // Test-only/advanced: inject a custom transport implementation.
    ClaudeClient(const ClaudeOptions& options, std::unique_ptr<Transport> transport);
    ~ClaudeClient();

    // No copy, move only
    ClaudeClient(const ClaudeClient&) = delete;
    ClaudeClient& operator=(const ClaudeClient&) = delete;
    ClaudeClient(ClaudeClient&&) noexcept;
    ClaudeClient& operator=(ClaudeClient&&) noexcept;

    // Connection lifecycle
    void connect();
    void disconnect();
    void force_disconnect();
    bool is_connected() const;

    // Get process ID of the Claude CLI process
    // Returns 0 if not connected
    long get_pid() const;

    // Send query
    // session_id defaults to "default" for multi-turn conversation continuity (Python parity)
    void send_query(const std::string& prompt, const std::string& session_id = "default");

    // Receive messages
    MessageStream receive_messages();

    // Convenience: receive all messages until result message
    std::vector<Message> receive_response();

    // Control operations
    void interrupt();
    void set_permission_mode(const std::string& mode);
    void set_model(const std::string& model);
    void rewind_files(const std::string& user_message_id);
    /// MCP status (streaming mode only). Returns CLI-reported MCP server status map.
    /// Throws CLIConnectionError if not connected.
    json get_mcp_status();

    // Initialization info (parity with Python get_server_info())
    // Returns initialization data (commands, output styles, capabilities) if available.
    std::optional<json> get_server_info() const;

  private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace claude

#endif // CLAUDE_CLIENT_HPP
