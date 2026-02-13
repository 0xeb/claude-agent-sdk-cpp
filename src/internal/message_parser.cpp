#include "message_parser.hpp"

#include <claude/errors.hpp>
#include <claude/protocol/control.hpp>
#include <sstream>

namespace claude
{
namespace protocol
{

MessageParser::MessageParser(size_t max_buffer_size) : max_buffer_size_(max_buffer_size) {}

Message MessageParser::parse_message(const std::string& json_str)
{
    try
    {
        json j = json::parse(json_str);

        std::string type = j.at("type").get<std::string>();

        if (type == "assistant")
        {
            return parse_assistant_message(j);
        }
        else if (type == "result")
        {
            return parse_result_message(j);
        }
        else if (type == "system")
        {
            return parse_system_message(j);
        }
        else if (type == "stream_event" || type == "stream")
        {
            return parse_stream_event(j);
        }
        else if (type == "user")
        {
            return parse_user_message(j);
        }
        else if (type == "control_request")
        {
            return parse_control_request(j);
        }
        else if (type == "control_response")
        {
            return parse_control_response(j);
        }
        else
        {
            // Use MessageParseError for unknown message types - this is a parsing issue, not JSON
            // decoding
            throw MessageParseError("Unknown message type: " + type, j);
        }
    }
    catch (const json::exception& e)
    {
        throw JSONDecodeError(std::string("JSON parse error: ") + e.what());
    }
}

std::vector<Message> MessageParser::add_data(const std::string& data)
{
    buffer_ += data;

    if (buffer_.size() > max_buffer_size_)
    {
        size_t size = buffer_.size();
        buffer_.clear();
        throw JSONDecodeError("Buffer exceeded maximum size of " +
                              std::to_string(max_buffer_size_) + " bytes (was " +
                              std::to_string(size) + ")");
    }

    std::vector<Message> messages;

    while (auto line = extract_line())
    {
        if (line->empty())
            continue;

        try
        {
            messages.push_back(parse_message(*line));
        }
        catch (const JSONDecodeError&)
        {
            // If parse fails, we might have partial JSON
            // Re-add the line to buffer and wait for more data
            buffer_ = *line + "\n" + buffer_;
            break;
        }
    }

    return messages;
}

std::optional<std::string> MessageParser::extract_line()
{
    size_t pos = buffer_.find('\n');
    if (pos == std::string::npos)
        return std::nullopt;

    std::string line = buffer_.substr(0, pos);
    buffer_.erase(0, pos + 1);

    return line;
}

ContentBlock MessageParser::parse_content_block(const claude::json& j)
{
    std::string type = j.at("type").get<std::string>();

    if (type == "text")
    {
        TextBlock block;
        block.text = j.at("text").get<std::string>();
        return block;
    }
    else if (type == "thinking")
    {
        ThinkingBlock block;
        block.thinking = j.at("thinking").get<std::string>();
        // Extract signature if present
        if (j.contains("signature"))
            block.signature = j.at("signature").get<std::string>();
        return block;
    }
    else if (type == "tool_use")
    {
        ToolUseBlock block;
        block.id = j.at("id").get<std::string>();
        block.name = j.at("name").get<std::string>();
        block.input = j.at("input");
        return block;
    }
    else if (type == "tool_result")
    {
        ToolResultBlock block;
        block.tool_use_id = j.at("tool_use_id").get<std::string>();
        if (j.contains("is_error"))
            block.is_error = j.at("is_error").get<bool>();
        // Content can be: string, array of content blocks, or null
        if (j.contains("content"))
            block.content = j["content"]; // Store raw JSON
        else
            block.content = nullptr; // Explicit null if no content
        return block;
    }
    else
    {
        // Use MessageParseError for unknown content block types
        throw MessageParseError("Unknown content block type: " + type, j);
    }
}

AssistantMessage MessageParser::parse_assistant_message(const claude::json& j)
{
    AssistantMessage msg;
    msg.raw_json = j; // Store original JSON

    // The CLI wraps the actual message in a "message" field
    const claude::json* message_ptr = &j;
    if (j.contains("message"))
        message_ptr = &j["message"];

    const auto& message = *message_ptr;
    if (message.contains("content") && message["content"].is_array())
        for (const auto& content_json : message["content"])
            msg.content.push_back(parse_content_block(content_json));

    // Extract model field (present in assistant messages)
    if (message.contains("model") && message["model"].is_string())
        msg.model = message["model"].get<std::string>();

    // Extract error field if present (from outer object, not inner message)
    if (j.contains("error") && j["error"].is_string())
    {
        std::string error_str = j["error"].get<std::string>();
        if (error_str == "authentication_failed")
            msg.error = AssistantMessageError::AuthenticationFailed;
        else if (error_str == "billing_error")
            msg.error = AssistantMessageError::BillingError;
        else if (error_str == "rate_limit")
            msg.error = AssistantMessageError::RateLimit;
        else if (error_str == "invalid_request")
            msg.error = AssistantMessageError::InvalidRequest;
        else if (error_str == "server_error")
            msg.error = AssistantMessageError::ServerError;
        else
            msg.error = AssistantMessageError::Unknown;
    }

    return msg;
}

UserMessage MessageParser::parse_user_message(const claude::json& j)
{
    UserMessage msg;
    msg.raw_json = j; // Store original JSON

    if (j.contains("uuid") && j["uuid"].is_string())
        msg.uuid = j["uuid"].get<std::string>();
    if (j.contains("parent_tool_use_id") && j["parent_tool_use_id"].is_string())
        msg.parent_tool_use_id = j["parent_tool_use_id"].get<std::string>();
    if (j.contains("tool_use_result") && j["tool_use_result"].is_object())
        msg.tool_use_result = j["tool_use_result"];

    // The CLI may wrap the actual message in a "message" field
    const claude::json* message_ptr = &j;
    if (j.contains("message"))
        message_ptr = &j["message"];

    const auto& message = *message_ptr;
    if (message.contains("content") && message["content"].is_array())
        for (const auto& content_json : message["content"])
            msg.content.push_back(parse_content_block(content_json));

    return msg;
}

ResultMessage MessageParser::parse_result_message(const claude::json& j)
{
    ResultMessage msg;
    msg.raw_json = j; // Store original JSON

    // The CLI puts session_id, usage, etc. at the top level, not nested under "result"
    msg.result.session_id = j.value("session_id", "");
    msg.result.conversation_id = j.value("conversation_id", "");

    if (j.contains("usage"))
    {
        const auto& usage = j["usage"];
        msg.result.usage.input_tokens = usage.value("input_tokens", 0);
        msg.result.usage.output_tokens = usage.value("output_tokens", 0);
        msg.result.usage.cache_creation_input_tokens =
            usage.value("cache_creation_input_tokens", 0);
        msg.result.usage.cache_read_input_tokens = usage.value("cache_read_input_tokens", 0);
    }

    // Cost can be at top level as total_cost_usd, or in a cost object
    if (j.contains("total_cost_usd"))
    {
        msg.result.cost.total = j["total_cost_usd"].get<double>();
    }
    else if (j.contains("cost"))
    {
        const auto& cost = j["cost"];
        msg.result.cost.total = cost.value("total", 0.0);
        msg.result.cost.input = cost.value("input", 0.0);
        msg.result.cost.output = cost.value("output", 0.0);
    }

    // Additional telemetry fields
    if (j.contains("duration_ms"))
        msg.duration_ms = j.value("duration_ms", 0);
    if (j.contains("duration_api_ms"))
        msg.duration_api_ms = j.value("duration_api_ms", 0);
    if (j.contains("num_turns"))
        msg.num_turns = j.value("num_turns", 0);

    // Structured output if present
    if (j.contains("structured_output") && !j["structured_output"].is_null())
        msg.structured_output = j["structured_output"];

    // Result subtype if present
    if (j.contains("subtype") && j["subtype"].is_string())
        msg.subtype = j["subtype"].get<std::string>();

    return msg;
}

SystemMessage MessageParser::parse_system_message(const claude::json& j)
{
    SystemMessage msg;
    msg.raw_json = j; // Store original JSON

    // Content is optional - system messages with subtype="init" don't have content
    if (j.contains("content"))
    {
        if (j["content"].is_string())
        {
            msg.content = j["content"].get<std::string>();
        }
        else
        {
            // If content is not a string, serialize it as JSON
            msg.content = j["content"].dump();
        }
    }
    else
    {
        // No content field - use empty string or serialize the whole message
        msg.content = "";
    }
    // Set subtype if present
    if (j.contains("subtype") && j["subtype"].is_string())
        msg.subtype = j["subtype"].get<std::string>();
    return msg;
}

StreamEvent MessageParser::parse_stream_event(const claude::json& j)
{
    StreamEvent event;
    event.raw_json = j; // Store original JSON

    // Support two formats:
    // 1. Nested: {"type":"stream_event","event":{"type":"content_block_delta",...}}
    // 2. Flat: {"type":"stream","event":"content_block_delta","index":0}

    if (j.contains("event"))
    {
        if (j["event"].is_object())
        {
            // Nested format
            const auto& event_obj = j["event"];
            event.event = event_obj.at("type").get<std::string>();
            event.index = event_obj.value("index", 0);
            event.data = event_obj; // Store the event object for access to delta, etc.

            // Optional identifiers within nested object (fallback if top-level missing)
            if (event.uuid.empty() && event_obj.contains("uuid") && event_obj["uuid"].is_string())
                event.uuid = event_obj["uuid"].get<std::string>();
            if (event.session_id.empty() && event_obj.contains("session_id") &&
                event_obj["session_id"].is_string())
            {
                event.session_id = event_obj["session_id"].get<std::string>();
            }
            if (!event.parent_tool_use_id.has_value() && event_obj.contains("parent_tool_use_id"))
            {
                if (event_obj["parent_tool_use_id"].is_string())
                    event.parent_tool_use_id = event_obj["parent_tool_use_id"].get<std::string>();
                else if (event_obj["parent_tool_use_id"].is_null())
                    event.parent_tool_use_id = std::nullopt;
            }
        }
        else if (j["event"].is_string())
        {
            // Flat format
            event.event = j["event"].get<std::string>();
            event.index = j.value("index", 0);

            // Check for identifiers in data field if present
            if (j.contains("data") && j["data"].is_object())
            {
                const auto& data_obj = j["data"];
                event.data = data_obj; // Store the data object

                // Extract identifiers from data if present
                if (event.uuid.empty() && data_obj.contains("uuid") && data_obj["uuid"].is_string())
                    event.uuid = data_obj["uuid"].get<std::string>();
                if (event.session_id.empty() && data_obj.contains("session_id") &&
                    data_obj["session_id"].is_string())
                {
                    event.session_id = data_obj["session_id"].get<std::string>();
                }
                if (!event.parent_tool_use_id.has_value() &&
                    data_obj.contains("parent_tool_use_id"))
                {
                    if (data_obj["parent_tool_use_id"].is_string())
                    {
                        event.parent_tool_use_id =
                            data_obj["parent_tool_use_id"].get<std::string>();
                    }
                    else if (data_obj["parent_tool_use_id"].is_null())
                    {
                        event.parent_tool_use_id = std::nullopt;
                    }
                }
            }
            else
            {
                event.data = j; // Store the whole object if no data field
            }
        }
        else
        {
            throw MessageParseError("stream event 'event' field must be object or string", j);
        }
    }
    else
    {
        throw MessageParseError("stream event message missing 'event' field", j);
    }

    // Top-level identifiers when provided by CLI
    if (j.contains("uuid") && j["uuid"].is_string())
        event.uuid = j["uuid"].get<std::string>();
    if (j.contains("session_id") && j["session_id"].is_string())
        event.session_id = j["session_id"].get<std::string>();
    if (j.contains("parent_tool_use_id"))
    {
        if (j["parent_tool_use_id"].is_string())
            event.parent_tool_use_id = j["parent_tool_use_id"].get<std::string>();
        else if (j["parent_tool_use_id"].is_null())
            event.parent_tool_use_id = std::nullopt;
    }

    return event;
}

claude::protocol::ControlRequest MessageParser::parse_control_request(const claude::json& j)
{
    claude::protocol::ControlRequest msg;
    msg.request_id = j.at("request_id").get<std::string>();
    msg.request = j.at("request");
    return msg;
}

claude::protocol::ControlResponse MessageParser::parse_control_response(const claude::json& j)
{
    claude::protocol::ControlResponse msg;
    const auto& response = j.at("response");
    msg.response.subtype = response.at("subtype").get<std::string>();
    msg.response.request_id = response.at("request_id").get<std::string>();

    // Response data is optional (might be null for error case)
    if (response.contains("response") && !response["response"].is_null())
        msg.response.response = response["response"];

    // Error message is optional (only present on error)
    if (response.contains("error") && !response["error"].is_null())
        msg.response.error = response["error"].get<std::string>();

    return msg;
}

} // namespace protocol
} // namespace claude
