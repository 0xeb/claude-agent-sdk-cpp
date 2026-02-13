#include "internal/message_parser.hpp"

#include <atomic>
#include <chrono>
#include <claude/client.hpp>
#include <claude/errors.hpp>
#include <claude/protocol/control.hpp>
#include <claude/transport.hpp>
#include <claude/version.hpp>
#include <condition_variable>
#include <cstdlib>
#include <mutex>
#include <nlohmann/json.hpp>
#include <queue>
#include <thread>

namespace claude
{

namespace
{
int get_initialize_timeout_ms()
{
    int initialize_timeout_ms = 60000;
    if (const char* env = std::getenv("CLAUDE_CODE_STREAM_CLOSE_TIMEOUT"))
    {
        try
        {
            int parsed = std::stoi(env);
            if (parsed > initialize_timeout_ms)
                initialize_timeout_ms = parsed;
        }
        catch (...)
        {
            // Ignore parse errors; keep default
        }
    }
    return initialize_timeout_ms;
}

// Stream close timeout used for first-result-gated stdin close behavior.
// Uses the same CLAUDE_CODE_STREAM_CLOSE_TIMEOUT environment variable as
// the Python SDK, expressed directly in milliseconds (default: 60000).
int get_stream_close_timeout_ms()
{
    int stream_close_timeout_ms = 60000;
    if (const char* env = std::getenv("CLAUDE_CODE_STREAM_CLOSE_TIMEOUT"))
    {
        try
        {
            int parsed = std::stoi(env);
            if (parsed > 0)
                stream_close_timeout_ms = parsed;
        }
        catch (...)
        {
            // Ignore parse errors; keep default
        }
    }
    return stream_close_timeout_ms;
}

// Convert Python-friendly hook output keys to CLI-expected keys.
// - async_     -> async
// - continue_  -> continue
// Prefer explicit standard keys if both are present; underscore keys act as aliases.
static json convert_hook_output_for_cli(const json& hook_output)
{
    // Defensive: only process objects; return as-is for arrays/primitives
    if (!hook_output.is_object())
        return hook_output;

    json converted = json::object();

    // First copy all keys except the underscore variants
    for (auto it = hook_output.begin(); it != hook_output.end(); ++it)
    {
        const std::string& key = it.key();
        if (key == "async_" || key == "continue_")
            continue;
        converted[key] = it.value();
    }

    // Map underscore variants if the standard key wasn't provided explicitly
    if (hook_output.contains("async_") && !converted.contains("async"))
        converted["async"] = hook_output.at("async_");
    if (hook_output.contains("continue_") && !converted.contains("continue"))
        converted["continue"] = hook_output.at("continue_");

    return converted;
}
} // namespace

// Exposed for unit testing of environment-driven initialize timeout
int claude_test_get_initialize_timeout_ms()
{
    return get_initialize_timeout_ms();
}

// MessageStream::Impl - Thread-safe message queue
class MessageStream::Impl
{
  public:
    std::queue<Message> queue_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    bool stopped_ = false;
    bool end_of_response_ = false;

    void push_message(Message&& msg)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(std::move(msg));
        cv_.notify_one();
    }

    std::optional<Message> pop_message()
    {
        std::unique_lock<std::mutex> lock(mutex_);

        // Wait for message or stop/end
        cv_.wait(lock, [this] { return !queue_.empty() || stopped_ || end_of_response_; });

        if (queue_.empty())
            return std::nullopt;

        Message msg = std::move(queue_.front());
        queue_.pop();
        return msg;
    }

    std::optional<Message> pop_message_for(std::chrono::milliseconds timeout)
    {
        std::unique_lock<std::mutex> lock(mutex_);

        if (!cv_.wait_for(lock, timeout,
                          [this] { return !queue_.empty() || stopped_ || end_of_response_; }))
            return std::nullopt;

        if (queue_.empty())
            return std::nullopt;

        Message msg = std::move(queue_.front());
        queue_.pop();
        return msg;
    }

    void stop()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stopped_ = true;
        cv_.notify_all();
    }

    void mark_end_of_response()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        end_of_response_ = true;
        cv_.notify_all();
    }

    void reset_for_new_query()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        end_of_response_ = false;
    }

    bool has_more() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return !queue_.empty() || (!stopped_ && !end_of_response_);
    }
};

// ClaudeClient::Impl - Main implementation using Transport
class ClaudeClient::Impl
{
  public:
    ClaudeOptions options_;
    std::unique_ptr<Transport> transport_;
    std::unique_ptr<protocol::ControlProtocol> control_protocol_;

    // Background reading
    std::thread reader_thread_;
    std::atomic<bool> running_{false};

    // Persistent message queue - exists independently of stream lifecycle
    std::shared_ptr<MessageStream::Impl> persistent_message_queue_;

    bool connected_ = false;
    bool initialized_ = false;
    json initialization_result_;

    // Stream-close gating: track when a ResultMessage has been seen for the
    // currently active query so we can optionally delay stdin close in SDK MCP
    // / hook scenarios (mirrors Python _first_result_event behavior).
    mutable std::mutex first_result_mutex_;
    std::condition_variable first_result_cv_;
    bool first_result_seen_ = false;
    bool has_active_query_ = false;

    // Hook callback registry
    std::map<std::string, HookCallback> hook_callbacks_;
    int next_callback_id_ = 0;

    explicit Impl(const ClaudeOptions& opts)
        : options_(opts), control_protocol_(std::make_unique<protocol::ControlProtocol>()),
          persistent_message_queue_(std::make_shared<MessageStream::Impl>())
    {
        // Create transport (default to subprocess for now)
        transport_ = create_subprocess_transport(options_);
    }

    Impl(const ClaudeOptions& opts, std::unique_ptr<Transport> transport)
        : options_(opts), control_protocol_(std::make_unique<protocol::ControlProtocol>()),
          persistent_message_queue_(std::make_shared<MessageStream::Impl>())
    {
        transport_ = std::move(transport);
    }

    ~Impl()
    {
        stop_reader();
    }

    void start_reader()
    {
        running_ = true;
        reader_thread_ = std::thread(&Impl::reader_loop, this);
    }

    void stop_reader()
    {
        if (running_)
        {
            running_ = false;
            if (reader_thread_.joinable())
                reader_thread_.join();
        }
    }

    void on_new_query_started()
    {
        std::lock_guard<std::mutex> lock(first_result_mutex_);
        first_result_seen_ = false;
        has_active_query_ = true;
    }

    void on_result_message()
    {
        {
            std::lock_guard<std::mutex> lock(first_result_mutex_);
            first_result_seen_ = true;
            has_active_query_ = false;
        }
        first_result_cv_.notify_all();
    }

    void notify_reader_stopped()
    {
        std::lock_guard<std::mutex> lock(first_result_mutex_);
        has_active_query_ = false;
        first_result_cv_.notify_all();
    }

    void wait_for_first_result_if_needed()
    {
        // Only apply gating when hooks or SDK MCP handlers are configured.
        bool has_hooks = !options_.hooks.empty();
        bool has_sdk_mcp = !options_.sdk_mcp_handlers.empty();

        if (!has_hooks && !has_sdk_mcp)
            return;

        int timeout_ms = get_stream_close_timeout_ms();
        if (timeout_ms <= 0)
            return;

        std::unique_lock<std::mutex> lock(first_result_mutex_);

        // If there is no active query or we've already seen a result, do not wait.
        if (!has_active_query_ || first_result_seen_)
            return;

        auto predicate = [this]
        {
            return !has_active_query_ || first_result_seen_ || !running_ || !transport_ ||
                   !transport_->is_running();
        };

        first_result_cv_.wait_for(lock, std::chrono::milliseconds(timeout_ms), predicate);
    }

    void reader_loop()
    {
        try
        {
            while (running_ && transport_->is_running())
            {
                // Read messages from transport
                auto messages = transport_->read_messages();

                if (messages.empty() && !transport_->has_messages())
                {
                    // No more messages and transport is closed
                    break;
                }

                // Route messages
                for (auto& msg : messages)
                {
                    // Check if this is a control response
                    if (is_control_response(msg))
                    {
                        auto& ctrl_resp = std::get<protocol::ControlResponse>(msg);
                        control_protocol_->handle_response(ctrl_resp);
                        continue; // Don't push to message stream
                    }

                    // Check if this is a control request (from CLI to SDK)
                    if (is_control_request(msg))
                    {
                        auto& ctrl_req = std::get<protocol::ControlRequest>(msg);
                        handle_control_request(ctrl_req);
                        continue; // Don't push to message stream
                    }

                    // Regular message - always push to persistent queue
                    bool is_result = is_result_message(msg);

                    persistent_message_queue_->push_message(std::move(msg));

                    if (is_result)
                    {
                        persistent_message_queue_->mark_end_of_response();
                        on_result_message();
                    }
                }

                // Small sleep to avoid busy waiting
                if (messages.empty())
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
        catch (const std::exception& e)
        {
            // Error in reader loop – stop queue and fail all pending control requests
            persistent_message_queue_->stop();
            control_protocol_->fail_all_pending(e.what());
        }
        catch (...)
        {
            persistent_message_queue_->stop();
            control_protocol_->fail_all_pending("Unknown error in reader loop");
        }

        // Mark persistent queue as ended
        persistent_message_queue_->stop();
        notify_reader_stopped();
    }

    void initialize()
    {
        // Determine initialize timeout from environment (CLAUDE_CODE_STREAM_CLOSE_TIMEOUT in ms,
        // min 60000)
        int initialize_timeout_ms = get_initialize_timeout_ms();

        // Send initialize control request
        auto write_func = [this](const std::string& data) { transport_->write(data); };

        // Build hooks configuration for initialization
        json hooks_config = json::object();

        if (!options_.hooks.empty())
        {
            for (const auto& [event, matchers] : options_.hooks)
            {
                if (matchers.empty())
                    continue;

                json matchers_array = json::array();

                for (const auto& matcher : matchers)
                {
                    // Generate callback IDs for all hooks in this matcher
                    json callback_ids = json::array();

                    for (const auto& callback : matcher.hooks)
                    {
                        std::string callback_id = "hook_" + std::to_string(next_callback_id_++);
                        hook_callbacks_[callback_id] = callback;
                        callback_ids.push_back(callback_id);
                    }

                    // Build matcher entry
                    json matcher_entry = {{"hookCallbackIds", callback_ids}};

                    // Add matcher pattern if specified
                    if (matcher.matcher.has_value())
                        matcher_entry["matcher"] = *matcher.matcher;
                    else
                        matcher_entry["matcher"] = nullptr;

                    // Add timeout if specified
                    if (matcher.timeout.has_value())
                        matcher_entry["timeout"] = *matcher.timeout;

                    matchers_array.push_back(matcher_entry);
                }

                hooks_config[event] = matchers_array;
            }
        }

        // Build agents configuration for initialization
        json agents_config = json(nullptr);
        if (!options_.agents.empty())
        {
            agents_config = json::object();
            for (const auto& [name, def] : options_.agents)
            {
                json agent_obj = json::object();
                agent_obj["description"] = def.description;
                agent_obj["prompt"] = def.prompt;
                if (def.tools.has_value())
                    agent_obj["tools"] = *def.tools;
                if (def.model.has_value())
                    agent_obj["model"] = *def.model;
                agents_config[name] = agent_obj;
            }
        }

        // Build initialize request data
        json request_data = {{"hooks", hooks_config.empty() ? nullptr : hooks_config},
                             {"agents", agents_config}};

        // Send initialize request and wait for response
        try
        {
            initialization_result_ = control_protocol_->send_request(
                write_func, "initialize", request_data, initialize_timeout_ms);
            initialized_ = true;
        }
        catch (const std::exception&)
        {
            // Initialization failed - allow SDK to work with older CLI versions
            initialized_ = false;
        }
    }

    void handle_control_request(const protocol::ControlRequest& request)
    {
        // Extract request data
        if (!request.request.contains("subtype") || request.request_id.empty())
            return; // Malformed request

        std::string subtype = request.request.value("subtype", "");

        if (subtype == "hook_callback")
        {
            handle_hook_callback(request.request_id, request.request);
        }
        else if (subtype == "can_use_tool")
        {
            handle_can_use_tool(request.request_id, request.request);
        }
        else if (subtype == "mcp_message")
        {
            // SDK MCP message bridging: route to in-process handler if provided
            // Expected request.request payload: { server_name: string, message: object }
            std::string server_name = request.request.value("server_name", "");
            json mcp_message = request.request.value("message", json::object());
            json response_envelope = {{"type", "control_response"},
                                      {"response",
                                       {{"subtype", "error"},
                                        {"request_id", request.request_id},
                                        {"error", "MCP handler not configured"}}}};

            try
            {
                if (server_name.empty() || !mcp_message.is_object())
                    throw std::runtime_error("Missing server_name or message for MCP request");

                auto it = options_.sdk_mcp_handlers.find(server_name);
                if (it == options_.sdk_mcp_handlers.end())
                    throw std::runtime_error("No SDK MCP handler for server: " + server_name);

                // Note: allowed_tools enforcement is done by the CLI, not here.
                // The CLI already prefixes MCP tool names (mcp__<server>__<tool>) and
                // checks permissions before sending the tools/call request to the SDK.

                // Invoke handler
                json mcp_response = it->second(mcp_message);

                response_envelope = {{"type", "control_response"},
                                     {"response",
                                      {{"subtype", "success"},
                                       {"request_id", request.request_id},
                                       {"response", {{"mcp_response", mcp_response}}}}}};
            }
            catch (const std::exception& e)
            {
                response_envelope = {{"type", "control_response"},
                                     {"response",
                                      {{"subtype", "error"},
                                       {"request_id", request.request_id},
                                       {"error", e.what()}}}};
            }

            send_control_response(response_envelope);
        }
        // Unknown subtypes are ignored (forward compatibility)
    }

    void handle_hook_callback(const std::string& request_id, const json& request)
    {
        // Extract callback_id from request
        std::string callback_id = request.value("callback_id", "");
        json input = request.value("input", json::object());
        std::string tool_use_id = request.value("tool_use_id", "");

        // Hook field conversion is handled by convert_hook_output_for_cli()

        // Look up callback function
        auto it = hook_callbacks_.find(callback_id);
        if (it == hook_callbacks_.end())
        {
            // Callback not found - send error response
            json error_response = {{"type", "control_response"},
                                   {"response",
                                    {{"subtype", "error"},
                                     {"request_id", request_id},
                                     {"error", "No hook callback found for ID: " + callback_id}}}};
            send_control_response(error_response);
            return;
        }

        // Call user callback
        json hook_output = it->second(input, tool_use_id);

        // Convert Python-style keys (async_, continue_) to CLI keys (async, continue)
        json converted_output = convert_hook_output_for_cli(hook_output);

        // Send success response with hook output
        json success_response = {
            {"type", "control_response"},
            {"response",
             {{"subtype", "success"}, {"request_id", request_id}, {"response", converted_output}}}};
        send_control_response(success_response);
    }

    void handle_can_use_tool(const std::string& request_id, const json& request)
    {
        // Extract tool permission request data
        std::string tool_name = request.value("tool_name", "");
        json input = request.value("input", json::object());

        // Parse permission suggestions from CLI
        ToolPermissionContext context;
        if (request.contains("permission_suggestions") &&
            request["permission_suggestions"].is_array())
        {
            for (const auto& suggestion_json : request["permission_suggestions"])
            {
                PermissionUpdate suggestion;
                suggestion.type = suggestion_json.value("type", "");

                // Parse optional fields based on type
                if (suggestion_json.contains("rules"))
                {
                    std::vector<PermissionRuleValue> rules;
                    for (const auto& rule_json : suggestion_json["rules"])
                    {
                        PermissionRuleValue rule;
                        rule.tool_name = rule_json.value("toolName", "");
                        if (rule_json.contains("ruleContent") &&
                            !rule_json["ruleContent"].is_null())
                        {
                            rule.rule_content = rule_json["ruleContent"].get<std::string>();
                        }
                        rules.push_back(rule);
                    }
                    suggestion.rules = rules;
                }

                if (suggestion_json.contains("behavior"))
                    suggestion.behavior = suggestion_json["behavior"].get<std::string>();

                if (suggestion_json.contains("mode"))
                    suggestion.mode = suggestion_json["mode"].get<std::string>();

                if (suggestion_json.contains("directories"))
                {
                    suggestion.directories =
                        suggestion_json["directories"].get<std::vector<std::string>>();
                }

                if (suggestion_json.contains("destination"))
                    suggestion.destination = suggestion_json["destination"].get<std::string>();

                context.suggestions.push_back(suggestion);
            }
        }

        // Call user callback or default to allow
        PermissionResult result;
        if (options_.tool_permission_callback)
        {
            result = options_.tool_permission_callback.value()(tool_name, input, context);
        }
        else
        {
            // Default to allow
            result = PermissionResultAllow{};
        }

        // Build response data based on result type
        json response_data;

        if (std::holds_alternative<PermissionResultAllow>(result))
        {
            const auto& allow = std::get<PermissionResultAllow>(result);
            response_data["behavior"] = allow.behavior;

            // Add updatedInput if provided, otherwise use original input
            if (allow.updated_input.has_value())
                response_data["updatedInput"] = *allow.updated_input;
            else
                response_data["updatedInput"] = input;

            // Add updated permissions if provided
            if (allow.updated_permissions.has_value())
            {
                json permissions_array = json::array();
                for (const auto& perm : *allow.updated_permissions)
                    permissions_array.push_back(perm.to_json());
                response_data["updatedPermissions"] = permissions_array;
            }
        }
        else if (std::holds_alternative<PermissionResultDeny>(result))
        {
            const auto& deny = std::get<PermissionResultDeny>(result);
            response_data["behavior"] = deny.behavior;
            response_data["message"] = deny.message;

            // Add interrupt flag if set
            if (deny.interrupt)
                response_data["interrupt"] = deny.interrupt;
        }

        // Send success response with proper envelope structure
        json success_response = {
            {"type", "control_response"},
            {"response",
             {{"subtype", "success"}, {"request_id", request_id}, {"response", response_data}}}};
        send_control_response(success_response);
    }

    void send_control_response(const json& response)
    {
        if (!transport_ || !transport_->is_running())
            return; // Can't send if transport is not running

        std::string line = response.dump() + "\n";
        transport_->write(line);
    }
};

// ============================================================================
// ClaudeClient implementation
// ============================================================================

ClaudeClient::ClaudeClient(const ClaudeOptions& options) : impl_(std::make_unique<Impl>(options)) {}

ClaudeClient::ClaudeClient(const ClaudeOptions& options, std::unique_ptr<Transport> transport)
    : impl_(std::make_unique<Impl>(options, std::move(transport)))
{
}

ClaudeClient::~ClaudeClient()
{
    if (impl_)
        disconnect();
}

ClaudeClient::ClaudeClient(ClaudeClient&&) noexcept = default;
ClaudeClient& ClaudeClient::operator=(ClaudeClient&&) noexcept = default;

void ClaudeClient::connect()
{
    if (impl_->connected_)
        return;

    // Connect transport
    impl_->transport_->connect();

    // Start background reader thread
    impl_->start_reader();

    // Send initialize control request
    impl_->initialize();

    impl_->connected_ = true;
}

void ClaudeClient::disconnect()
{
    if (!impl_ || !impl_->connected_)
        return;

    // For SDK MCP / hook scenarios, wait briefly for the first result
    // before closing stdin to mirror Python's first-result-gated stream
    // close behavior.
    impl_->wait_for_first_result_if_needed();

    // End input to allow the CLI process to finish cleanly.
    if (impl_->transport_)
        impl_->transport_->end_input();

    // Stop reader thread first
    impl_->stop_reader();

    // Close transport
    if (impl_->transport_)
    {
        impl_->transport_->close();
        impl_->transport_.reset();
    }

    impl_->connected_ = false;
}

void ClaudeClient::force_disconnect()
{
    if (!impl_ || !impl_->connected_)
        return;

    // Skip first-result gating for fast shutdown (used for timeouts).
    if (impl_->transport_)
        impl_->transport_->end_input();

    impl_->stop_reader();

    if (impl_->transport_)
    {
        impl_->transport_->close();
        impl_->transport_.reset();
    }

    impl_->connected_ = false;
}

bool ClaudeClient::is_connected() const
{
    return impl_ && impl_->connected_ && impl_->transport_ && impl_->transport_->is_running();
}

long ClaudeClient::get_pid() const
{
    if (impl_ && impl_->transport_)
        return impl_->transport_->get_pid();
    return 0;
}

void ClaudeClient::send_query(const std::string& prompt, const std::string& session_id)
{
    if (!is_connected())
        throw CLIConnectionError("Not connected to Claude CLI");

    // Reset the end_of_response flag for this new query
    impl_->persistent_message_queue_->reset_for_new_query();

    // Track a new active query for first-result-gated stream close behavior.
    impl_->on_new_query_started();

    // Build user message JSON in the format the CLI expects
    // Always include session_id for multi-turn conversation continuity (Python parity)
    json msg = {{"type", "user"},
                {"message", {{"role", "user"}, {"content", prompt}}},
                {"parent_tool_use_id", nullptr},
                {"session_id", session_id}};

    // Write to transport
    std::string json_str = msg.dump() + "\n";
    impl_->transport_->write(json_str);
}

MessageStream ClaudeClient::receive_messages()
{
    if (!is_connected())
        throw CLIConnectionError("Not connected to Claude CLI");

    // Return a stream that references the persistent message queue
    MessageStream stream;
    stream.impl_ = impl_->persistent_message_queue_;
    return stream;
}

std::vector<Message> ClaudeClient::receive_response()
{
    std::vector<Message> messages;

    auto stream = receive_messages();
    for (const auto& msg : stream)
    {
        messages.push_back(msg);
        if (is_result_message(msg))
            break;
    }

    return messages;
}

void ClaudeClient::interrupt()
{
    if (!is_connected())
        throw CLIConnectionError("Not connected to Claude CLI");

    // Create write function that captures the transport
    auto write_func = [this](const std::string& data) { impl_->transport_->write(data); };

    json request_data = {};
    impl_->control_protocol_->send_request(write_func, "interrupt", request_data);
}

void ClaudeClient::set_permission_mode(const std::string& mode)
{
    if (!is_connected())
        throw CLIConnectionError("Not connected to Claude CLI");

    // Create write function that captures the transport
    auto write_func = [this](const std::string& data) { impl_->transport_->write(data); };

    json request_data = {{"mode", mode}};
    impl_->control_protocol_->send_request(write_func, "set_permission_mode", request_data);
}

void ClaudeClient::set_model(const std::string& model)
{
    if (!is_connected())
        throw CLIConnectionError("Not connected to Claude CLI");

    // Create write function that captures the transport
    auto write_func = [this](const std::string& data) { impl_->transport_->write(data); };

    json request_data = {{"model", model}};
    impl_->control_protocol_->send_request(write_func, "set_model", request_data);
}

void ClaudeClient::rewind_files(const std::string& user_message_id)
{
    if (!is_connected())
        throw CLIConnectionError("Not connected to Claude CLI");

    auto write_func = [this](const std::string& data) { impl_->transport_->write(data); };

    json request_data = {{"user_message_id", user_message_id}};
    impl_->control_protocol_->send_request(write_func, "rewind_files", request_data);
}

json ClaudeClient::get_mcp_status()
{
    if (!is_connected())
        throw CLIConnectionError("Not connected to Claude CLI");

    auto write_func = [this](const std::string& data) { impl_->transport_->write(data); };
    json request_data = json::object();
    return impl_->control_protocol_->send_request(write_func, "mcp_status", request_data);
}

std::optional<json> ClaudeClient::get_server_info() const
{
    if (!impl_ || !impl_->connected_ || !impl_->initialized_)
        return std::nullopt;
    return impl_->initialization_result_;
}

// ============================================================================
// MessageStream implementation
// ============================================================================

MessageStream::MessageStream() : impl_(std::make_shared<Impl>()) {}

MessageStream::~MessageStream() = default;

MessageStream::MessageStream(MessageStream&&) noexcept = default;
MessageStream& MessageStream::operator=(MessageStream&&) noexcept = default;

MessageStream::Iterator MessageStream::begin()
{
    return Iterator(this);
}

MessageStream::Iterator MessageStream::end()
{
    return Iterator();
}

std::optional<Message> MessageStream::get_next()
{
    return impl_->pop_message();
}

std::optional<Message> MessageStream::get_next_for(std::chrono::milliseconds timeout)
{
    return impl_->pop_message_for(timeout);
}

bool MessageStream::has_more() const
{
    return impl_->has_more();
}

void MessageStream::stop()
{
    impl_->stop();
}

// ============================================================================
// MessageStream::Iterator implementation
// ============================================================================

MessageStream::Iterator::Iterator() : stream_(nullptr), is_end_(true) {}

MessageStream::Iterator::Iterator(MessageStream* stream) : stream_(stream), is_end_(false)
{
    fetch_next();
}

void MessageStream::Iterator::fetch_next()
{
    if (!stream_)
    {
        is_end_ = true;
        return;
    }

    current_ = stream_->get_next();
    if (!current_)
        is_end_ = true;
}

MessageStream::Iterator::reference MessageStream::Iterator::operator*() const
{
    if (!current_)
        throw std::runtime_error("Dereferencing end iterator");
    return *current_;
}

MessageStream::Iterator::pointer MessageStream::Iterator::operator->() const
{
    return &(operator*());
}

MessageStream::Iterator& MessageStream::Iterator::operator++()
{
    fetch_next();
    return *this;
}

bool MessageStream::Iterator::operator==(const Iterator& other) const
{
    if (is_end_ && other.is_end_)
        return true;
    if (is_end_ || other.is_end_)
        return false;
    return stream_ == other.stream_;
}

bool MessageStream::Iterator::operator!=(const Iterator& other) const
{
    return !(*this == other);
}

} // namespace claude
