/**
 * @file session_storage.hpp
 * @brief Session management with automatic message persistence
 *
 * C++ extension for the Claude Agent SDK that provides:
 * - Smart wrapper around ClaudeClient with operator-> access
 * - Automatic message storage to memory and JSON files
 * - Session resumption with history loading
 * - Cross-platform file-based persistence
 *
 * @warning This is a C++ extension - not part of Python SDK parity
 * @see kb/ccsdk/session-management.md
 * @see kb/ccsdk/cpp-extensions-design.md
 */

#pragma once

#include <claude/types.hpp>
#include <claude/client.hpp>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <memory>

namespace claude {
namespace ext {

/**
 * @brief Smart session wrapper with automatic message persistence
 *
 * SessionWrapper combines ClaudeClient with automatic message storage,
 * providing a convenient interface for managing conversation sessions
 * that persist across application restarts.
 *
 * Features:
 * - Smart pointer interface (operator->) for client access
 * - Automatic message storage (memory + JSON files)
 * - Session resumption with history loading
 * - RAII-style auto-save on disconnect
 *
 * Storage Strategy:
 * - Active session: Messages stored in RAM (std::vector)
 * - Persistence: Serialized to JSON on save/disconnect
 * - Resume: Load JSON from disk back to RAM
 *
 * @code
 * // Create new session
 * claude::ext::SessionWrapper session(opts);
 * session.connect();
 *
 * // Access client via operator->
 * session->send_query("Hello!");
 *
 * // Messages automatically stored
 * for (const auto& msg : session.receive_messages()) {
 *     std::cout << msg;
 * }
 *
 * session.disconnect();  // Auto-saves to disk
 *
 * // Resume later
 * auto history = session.load_history(session_id);
 * session.resume(session_id);
 * session->send_query("Continue...");
 * @endcode
 */
class SessionWrapper
{
public:
    /**
     * @brief Construct session wrapper with options
     * @param opts Claude client options
     * @param storage_dir Directory for session JSON files (default: .claude_sessions)
     */
    explicit SessionWrapper(
        const ClaudeOptions& opts,
        const std::string& storage_dir = ".claude_sessions"
    );

    /**
     * @brief Destructor - saves session if connected
     */
    ~SessionWrapper();

    // Non-copyable (owns client)
    SessionWrapper(const SessionWrapper&) = delete;
    SessionWrapper& operator=(const SessionWrapper&) = delete;

    // Movable
    SessionWrapper(SessionWrapper&&) noexcept = default;
    SessionWrapper& operator=(SessionWrapper&&) noexcept = default;

    // ========================================================================
    // Smart Pointer Interface (Client Access)
    // ========================================================================

    /**
     * @brief Access underlying client (smart pointer style)
     * @return Pointer to ClaudeClient
     *
     * @code
     * session->send_query("Hello");  // Like std::unique_ptr
     * session->interrupt();
     * @endcode
     */
    ClaudeClient* operator->() { return &client_; }

    /**
     * @brief Access underlying client (const)
     */
    const ClaudeClient* operator->() const { return &client_; }

    /**
     * @brief Dereference to client
     * @return Reference to ClaudeClient
     *
     * @code
     * ClaudeClient& client = *session;
     * client.send_query("Hello");
     * @endcode
     */
    ClaudeClient& operator*() { return client_; }

    /**
     * @brief Dereference to client (const)
     */
    const ClaudeClient& operator*() const { return client_; }

    // ========================================================================
    // Connection Management
    // ========================================================================

    /**
     * @brief Connect to Claude CLI
     * @throws CLINotFoundError if Claude CLI not installed
     * @throws CLIConnectionError if connection fails
     */
    void connect();

    /**
     * @brief Disconnect from Claude CLI and auto-save session
     *
     * Automatically saves current session to disk before disconnecting.
     */
    void disconnect();

    /**
     * @brief Check if client is connected
     */
    bool is_connected() const;

    // ========================================================================
    // Message Iteration with Auto-Storage
    // ========================================================================

    /**
     * @brief Iterator that automatically stores messages
     *
     * Wraps MessageStream::Iterator to automatically store
     * each message to the session's message vector and capture
     * session ID from ResultMessage.
     */
    class MessageIterator
    {
    public:
        using iterator_category = std::input_iterator_tag;
        using value_type = Message;
        using difference_type = std::ptrdiff_t;
        using pointer = const Message*;
        using reference = const Message&;

        /**
         * @brief Dereference iterator (auto-stores message on first access)
         * @return Current message
         */
        reference operator*() const;

        /**
         * @brief Advance to next message
         * @return Reference to this iterator
         */
        MessageIterator& operator++();

        /**
         * @brief Compare iterators
         */
        bool operator!=(const MessageIterator& other) const;

        /**
         * @brief Compare iterators
         */
        bool operator==(const MessageIterator& other) const;

    private:
        friend class SessionWrapper;
        friend struct MessageRange;

        MessageIterator();
        MessageIterator(SessionWrapper* wrapper, MessageStream::Iterator it);

        SessionWrapper* wrapper_;
        MessageStream::Iterator inner_;
        mutable bool stored_;  // Track if current message was stored
    };

    /**
     * @brief Range object that owns the MessageStream and provides auto-storing iterators
     *
     * This object owns the MessageStream to ensure proper lifetime management.
     * Iterators returned by begin()/end() reference this stream safely.
     */
    struct MessageRange {
        SessionWrapper* wrapper;
        MessageStream stream;  // Owns the stream!

        MessageRange(SessionWrapper* w, MessageStream&& s)
            : wrapper(w), stream(std::move(s)) {}

        MessageIterator begin() {
            return MessageIterator(wrapper, stream.begin());
        }

        MessageIterator end() {
            return MessageIterator(wrapper, stream.end());
        }
    };

    /**
     * @brief Receive messages with automatic storage
     * @return MessageRange that owns the stream and auto-stores messages
     *
     * Each message is automatically stored to memory as you iterate.
     * Session ID is captured from ResultMessage.
     *
     * @code
     * for (const auto& msg : session.receive_messages()) {
     *     // msg is automatically stored
     *     display_message(msg);
     * }
     * @endcode
     */
    MessageRange receive_messages() {
        return MessageRange(this, client_.receive_messages());
    }

    // ========================================================================
    // History Management
    // ========================================================================

    /**
     * @brief Load messages from a previous session
     * @param session_id Session ID to load
     * @return Vector of loaded messages
     * @throws std::runtime_error if session file not found
     *
     * Loads messages from disk into memory. The loaded messages
     * become the current session's history.
     *
     * @code
     * auto history = session.load_history("session_abc123");
     * for (const auto& msg : history) {
     *     display_message(msg);
     * }
     * @endcode
     */
    std::vector<Message> load_history(const std::string& session_id);

    /**
     * @brief Save current session to disk
     *
     * Serializes messages_ vector to JSON and writes to
     * {storage_dir}/{session_id}.json
     *
     * Called automatically on disconnect(), but can be called
     * manually for checkpointing.
     */
    void save_history();

    /**
     * @brief List all saved session IDs
     * @return Vector of session IDs
     *
     * @code
     * auto sessions = session.list_sessions();
     * for (const auto& id : sessions) {
     *     std::cout << "Session: " << id << "\n";
     * }
     * @endcode
     */
    std::vector<std::string> list_sessions() const;

    // ========================================================================
    // Session Operations
    // ========================================================================

    /**
     * @brief Resume a previous session
     * @param session_id Session ID to resume
     *
     * Updates options with resume flag, loads history, and reconnects.
     * The Claude CLI will load the session context, and this wrapper
     * loads the message history from disk.
     *
     * @code
     * session.resume("session_abc123");
     * session->send_query("Continue from before...");
     * @endcode
     */
    void resume(const std::string& session_id);

    /**
     * @brief Get current session ID
     * @return Session ID (empty if no session active)
     */
    std::string session_id() const { return current_session_id_; }

    /**
     * @brief Get all stored messages
     * @return Reference to message vector
     */
    const std::vector<Message>& messages() const { return messages_; }

    /**
     * @brief Get storage directory
     */
    std::string storage_directory() const { return storage_dir_; }

private:
    /**
     * @brief Store a message to the messages_ vector
     * @param msg Message to store
     *
     * Also captures session ID from ResultMessage.
     */
    void store_message(const Message& msg);

    /**
     * @brief Serialize message to JSON
     */
    nlohmann::json message_to_json(const Message& msg) const;

    /**
     * @brief Deserialize message from JSON
     */
    Message json_to_message(const nlohmann::json& j) const;

    /**
     * @brief Serialize content blocks to JSON
     */
    nlohmann::json content_to_json(const std::vector<ContentBlock>& content) const;

    /**
     * @brief Deserialize content blocks from JSON
     */
    std::vector<ContentBlock> json_to_content(const nlohmann::json& j) const;

    /**
     * @brief Get current timestamp string
     */
    static std::string get_timestamp();

    // Member variables
    ClaudeClient client_;           ///< Underlying Claude client
    ClaudeOptions opts_;            ///< Client options (for resume)
    std::string storage_dir_;       ///< Directory for JSON files
    std::string current_session_id_; ///< Current session ID
    std::vector<Message> messages_; ///< Message history (in-memory)
    bool connected_;                ///< Connection state
};

} // namespace ext
} // namespace claude
