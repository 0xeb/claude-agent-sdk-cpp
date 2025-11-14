#ifndef CLAUDE_TRANSPORT_HPP
#define CLAUDE_TRANSPORT_HPP

#include <claude/types.hpp>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace claude
{

// Forward declarations
class ClaudeOptions;

/**
 * Abstract transport interface for Claude communication.
 *
 * This is a low-level transport interface that handles raw I/O with the Claude
 * process or service. The ClaudeClient and query() functions build on top of this
 * to implement the control protocol and message routing.
 *
 * Implementations include:
 * - SubprocessTransport: Local subprocess using stdin/stdout
 * - Future: NetworkTransport, RemoteTransport, etc.
 *
 * This interface mirrors the Python SDK's Transport class for consistency.
 */
class Transport
{
  public:
    virtual ~Transport() = default;

    /**
     * Connect the transport and prepare for communication.
     * For subprocess transports, this starts the process.
     * For network transports, this establishes the connection.
     */
    virtual void connect() = 0;

    /**
     * Write raw data to the transport.
     * @param data Raw string data to write (typically JSON + newline)
     */
    virtual void write(const std::string& data) = 0;

    /**
     * Read and parse messages from the transport.
     * This is a blocking call that returns available messages.
     * Returns empty vector when no more messages are available.
     *
     * For streaming transports, this may return partial results.
     * Callers should handle message accumulation as needed.
     */
    virtual std::vector<Message> read_messages() = 0;

    /**
     * Check if transport has more messages available.
     * Returns true if there might be more messages to read.
     * Returns false if the transport is definitely closed/ended.
     */
    virtual bool has_messages() const = 0;

    /**
     * Close the transport connection and clean up resources.
     */
    virtual void close() = 0;

    /**
     * Check if transport is ready for communication.
     * @return True if transport is ready to send/receive messages
     */
    virtual bool is_ready() const = 0;

    /**
     * End the input stream (close stdin for process transports).
     * This signals to the remote end that no more input will be sent.
     */
    virtual void end_input() = 0;

    /**
     * Get the process ID for subprocess transports.
     * Returns 0 for non-subprocess transports.
     */
    virtual long get_pid() const { return 0; }

    /**
     * Check if the transport is still running/connected.
     */
    virtual bool is_running() const = 0;
};

// Factory functions for creating transports
std::unique_ptr<Transport> create_subprocess_transport(const ClaudeOptions& options);
std::unique_ptr<Transport> create_oneshot_transport(const std::string& prompt, const ClaudeOptions& options);

} // namespace claude

#endif // CLAUDE_TRANSPORT_HPP