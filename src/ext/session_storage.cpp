/**
 * @file session_storage.cpp
 * @brief Implementation of SessionWrapper with message persistence
 */

#include <chrono>
#include <claude/ext/session_storage.hpp>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace claude
{
namespace ext
{

// ============================================================================
// SessionWrapper Implementation
// ============================================================================

SessionWrapper::SessionWrapper(const ClaudeOptions& opts, const std::string& storage_dir)
    : client_(opts), opts_(opts), storage_dir_(storage_dir), connected_(false)
{
    // Create storage directory if it doesn't exist
    fs::create_directories(storage_dir_);
}

SessionWrapper::~SessionWrapper()
{
    if (connected_ && !current_session_id_.empty())
    {
        try
        {
            save_history();
        }
        catch (...)
        {
            // Suppress exceptions in destructor
        }
    }
}

void SessionWrapper::connect()
{
    client_.connect();
    connected_ = true;
}

void SessionWrapper::disconnect()
{
    if (connected_ && !current_session_id_.empty())
        save_history();
    client_.disconnect();
    connected_ = false;
}

bool SessionWrapper::is_connected() const
{
    return connected_;
}

// ============================================================================
// Message Iterator Implementation
// ============================================================================

SessionWrapper::MessageIterator::MessageIterator() : wrapper_(nullptr), inner_(), stored_(false) {}

SessionWrapper::MessageIterator::MessageIterator(SessionWrapper* wrapper,
                                                 MessageStream::Iterator it)
    : wrapper_(wrapper), inner_(it), stored_(false)
{
}

const Message& SessionWrapper::MessageIterator::operator*() const
{
    // Store message on first dereference
    if (!stored_ && wrapper_)
    {
        wrapper_->store_message(*inner_);
        stored_ = true;
    }
    return *inner_;
}

SessionWrapper::MessageIterator& SessionWrapper::MessageIterator::operator++()
{
    ++inner_;
    stored_ = false; // Reset for next message
    return *this;
}

bool SessionWrapper::MessageIterator::operator!=(const MessageIterator& other) const
{
    return inner_ != other.inner_;
}

bool SessionWrapper::MessageIterator::operator==(const MessageIterator& other) const
{
    return inner_ == other.inner_;
}

// ============================================================================
// History Management
// ============================================================================

std::vector<Message> SessionWrapper::load_history(const std::string& session_id)
{
    std::string filename = storage_dir_ + "/" + session_id + ".json";

    if (!fs::exists(filename))
        throw std::runtime_error("Session file not found: " + filename);

    std::ifstream file(filename);
    if (!file)
        throw std::runtime_error("Failed to open session file: " + filename);

    json session_data;
    file >> session_data;

    current_session_id_ = session_data["session_id"];

    // Deserialize messages
    messages_.clear();
    for (const auto& msg_json : session_data["messages"])
        messages_.push_back(json_to_message(msg_json));

    return messages_;
}

void SessionWrapper::save_history()
{
    if (current_session_id_.empty())
    {
        // No session ID yet, nothing to save
        return;
    }

    json session_data;
    session_data["session_id"] = current_session_id_;
    session_data["timestamp"] = get_timestamp();
    session_data["message_count"] = messages_.size();

    // Serialize messages
    json messages_json = json::array();
    for (const auto& msg : messages_)
        messages_json.push_back(message_to_json(msg));
    session_data["messages"] = messages_json;

    // Write to file
    std::string filename = storage_dir_ + "/" + current_session_id_ + ".json";
    std::ofstream file(filename);
    if (!file)
        throw std::runtime_error("Failed to open session file: " + filename);

    file << std::setw(2) << session_data << std::endl;
}

std::vector<std::string> SessionWrapper::list_sessions() const
{
    std::vector<std::string> sessions;

    if (!fs::exists(storage_dir_))
        return sessions;

    for (const auto& entry : fs::directory_iterator(storage_dir_))
        if (entry.path().extension() == ".json")
            sessions.push_back(entry.path().stem().string());

    return sessions;
}

// ============================================================================
// Session Operations
// ============================================================================

void SessionWrapper::resume(const std::string& session_id)
{
    // Load history first
    load_history(session_id);

    // Update options with resume flag
    opts_.resume = session_id;

    // Recreate client with resume options
    if (connected_)
        client_.disconnect();

    client_ = ClaudeClient(opts_);
    client_.connect();
    connected_ = true;
}

// ============================================================================
// Private Helper Methods
// ============================================================================

void SessionWrapper::store_message(const Message& msg)
{
    messages_.push_back(msg);

    // Capture session ID from ResultMessage
    if (std::holds_alternative<ResultMessage>(msg))
    {
        const auto& result = std::get<ResultMessage>(msg);
        current_session_id_ = result.session_id();
    }
}

json SessionWrapper::message_to_json(const Message& msg) const
{
    json j;

    if (std::holds_alternative<UserMessage>(msg))
    {
        const auto& user = std::get<UserMessage>(msg);
        j["type"] = "user";
        j["content"] = content_to_json(user.content);
        if (user.uuid.has_value())
            j["uuid"] = *user.uuid;
        if (user.parent_tool_use_id.has_value())
            j["parent_tool_use_id"] = *user.parent_tool_use_id;
    }
    else if (std::holds_alternative<AssistantMessage>(msg))
    {
        const auto& assistant = std::get<AssistantMessage>(msg);
        j["type"] = "assistant";
        j["content"] = content_to_json(assistant.content);
        j["model"] = assistant.model;
    }
    else if (std::holds_alternative<SystemMessage>(msg))
    {
        const auto& system = std::get<SystemMessage>(msg);
        j["type"] = "system";
        j["subtype"] = system.subtype;
        j["content"] = system.content;
    }
    else if (std::holds_alternative<ResultMessage>(msg))
    {
        const auto& result = std::get<ResultMessage>(msg);
        j["type"] = "result";
        j["session_id"] = result.session_id();
        j["num_turns"] = result.num_turns_value();
        j["duration_ms"] = result.duration_ms_value();
        j["total_cost_usd"] = result.total_cost_usd();
    }

    return j;
}

Message SessionWrapper::json_to_message(const json& j) const
{
    std::string type = j["type"];

    if (type == "user")
    {
        UserMessage user;
        user.content = json_to_content(j["content"]);
        if (j.contains("uuid") && j["uuid"].is_string())
            user.uuid = j["uuid"].get<std::string>();
        if (j.contains("parent_tool_use_id") && j["parent_tool_use_id"].is_string())
            user.parent_tool_use_id = j["parent_tool_use_id"].get<std::string>();
        return user;
    }
    else if (type == "assistant")
    {
        AssistantMessage assistant;
        assistant.content = json_to_content(j["content"]);
        assistant.model = j.value("model", "");
        return assistant;
    }
    else if (type == "system")
    {
        SystemMessage system;
        system.subtype = j["subtype"];
        system.content = j.value("content", "");
        return system;
    }
    else if (type == "result")
    {
        // Reconstruct ResultMessage (simplified)
        ResultMessage result;
        // Note: ResultMessage internals may vary - this is basic reconstruction
        return result;
    }

    throw std::runtime_error("Unknown message type: " + type);
}

json SessionWrapper::content_to_json(const std::vector<ContentBlock>& content) const
{
    json blocks = json::array();

    for (const auto& block : content)
    {
        if (std::holds_alternative<TextBlock>(block))
        {
            const auto& text = std::get<TextBlock>(block);
            blocks.push_back({{"type", "text"}, {"text", text.text}});
        }
        else if (std::holds_alternative<ThinkingBlock>(block))
        {
            const auto& thinking = std::get<ThinkingBlock>(block);
            blocks.push_back({{"type", "thinking"},
                              {"thinking", thinking.thinking},
                              {"signature", thinking.signature}});
        }
        else if (std::holds_alternative<ToolUseBlock>(block))
        {
            const auto& tool_use = std::get<ToolUseBlock>(block);
            blocks.push_back({{"type", "tool_use"},
                              {"id", tool_use.id},
                              {"name", tool_use.name},
                              {"input", tool_use.input}});
        }
        else if (std::holds_alternative<ToolResultBlock>(block))
        {
            const auto& tool_result = std::get<ToolResultBlock>(block);
            json block_json = {{"type", "tool_result"},
                               {"tool_use_id", tool_result.tool_use_id},
                               {"content", tool_result.content},
                               {"is_error", tool_result.is_error}};

            blocks.push_back(block_json);
        }
    }

    return blocks;
}

std::vector<ContentBlock> SessionWrapper::json_to_content(const json& j) const
{
    std::vector<ContentBlock> content;

    for (const auto& block_json : j)
    {
        std::string type = block_json["type"];

        if (type == "text")
        {
            TextBlock text;
            text.text = block_json["text"];
            content.push_back(text);
        }
        else if (type == "thinking")
        {
            ThinkingBlock thinking;
            thinking.thinking = block_json["thinking"];
            thinking.signature = block_json.value("signature", "");
            content.push_back(thinking);
        }
        else if (type == "tool_use")
        {
            ToolUseBlock tool_use;
            tool_use.id = block_json["id"];
            tool_use.name = block_json["name"];
            tool_use.input = block_json["input"];
            content.push_back(tool_use);
        }
        else if (type == "tool_result")
        {
            ToolResultBlock tool_result;
            tool_result.tool_use_id = block_json["tool_use_id"];
            tool_result.content = block_json.value("content", json());
            tool_result.is_error = block_json.value("is_error", false);

            content.push_back(tool_result);
        }
    }

    return content;
}

std::string SessionWrapper::get_timestamp()
{
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

} // namespace ext
} // namespace claude
