#ifndef CLAUDE_INTERNAL_MESSAGE_PARSER_HPP
#define CLAUDE_INTERNAL_MESSAGE_PARSER_HPP

#include <claude/types.hpp>
#include <optional>
#include <string>

namespace claude
{
namespace protocol
{

class MessageParser
{
  public:
    explicit MessageParser(size_t max_buffer_size = 1024 * 1024);

    // Parse a complete JSON message from string
    static Message parse_message(const std::string& json_str);

    // Add data to buffer and try to parse messages
    std::vector<Message> add_data(const std::string& data);

    // Check if buffer has buffered data
    bool has_buffered_data() const
    {
        return !buffer_.empty();
    }

    // Clear buffer
    void clear_buffer()
    {
        buffer_.clear();
    }

  private:
    std::string buffer_;
    size_t max_buffer_size_;

    // Try to extract one complete line from buffer
    std::optional<std::string> extract_line();

    // Parse content block from JSON
    static ContentBlock parse_content_block(const claude::json& j);

    // Parse specific message types
    static AssistantMessage parse_assistant_message(const claude::json& j);
    static UserMessage parse_user_message(const claude::json& j);
    static ResultMessage parse_result_message(const claude::json& j);
    static SystemMessage parse_system_message(const claude::json& j);
    static StreamEvent parse_stream_event(const claude::json& j);
    static claude::protocol::ControlRequest parse_control_request(const claude::json& j);
    static claude::protocol::ControlResponse parse_control_response(const claude::json& j);
};

} // namespace protocol
} // namespace claude

#endif // CLAUDE_INTERNAL_MESSAGE_PARSER_HPP
