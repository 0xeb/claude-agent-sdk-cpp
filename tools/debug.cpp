/**
 * debug.cpp - Ultimate Protocol Analysis Tool
 *
 * This example demonstrates comprehensive debugging and protocol analysis
 * of the Claude Code CLI IPC communication. It hooks into EVERY available
 * callback and uses carefully crafted prompts to trigger all major protocol
 * features.
 *
 * Features:
 * - All hook types (PreToolUse, PostToolUse, UserPromptSubmit, Stop, SubagentStop, PreCompact)
 * - Tool permission callback with detailed logging
 * - Stderr callback for CLI output monitoring
 * - Comprehensive message logging with timestamps
 * - Statistics tracking and analysis
 * - Control protocol observation
 *
 * This serves as the definitive reference for understanding how the
 * Claude Code CLI communicates with SDK clients.
 */

#include <atomic>
#include <chrono>
#include <claude/claude.hpp>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <mutex>
#include <sstream>
#include <thread>

using namespace claude;

// ============================================================================
// ANSI Color Codes for Terminal Output
// ============================================================================

namespace Color
{
const char* RESET = "\033[0m";
const char* RED = "\033[31m";
const char* GREEN = "\033[32m";
const char* YELLOW = "\033[33m";
const char* BLUE = "\033[34m";
const char* MAGENTA = "\033[35m";
const char* CYAN = "\033[36m";
const char* WHITE = "\033[37m";
const char* BOLD = "\033[1m";
} // namespace Color

// ============================================================================
// Debug Logger - Thread-safe logging with timestamps
// ============================================================================

class DebugLogger
{
  private:
    std::mutex mutex_;
    std::ofstream log_file_;
    std::ofstream verbose_log_file_;
    std::chrono::steady_clock::time_point start_time_;
    bool verbose_mode_;

    // Statistics
    std::atomic<int> pre_tool_use_count_{0};
    std::atomic<int> post_tool_use_count_{0};
    std::atomic<int> user_prompt_submit_count_{0};
    std::atomic<int> stop_count_{0};
    std::atomic<int> subagent_stop_count_{0};
    std::atomic<int> pre_compact_count_{0};
    std::atomic<int> permission_request_count_{0};
    std::atomic<int> stderr_line_count_{0};
    std::atomic<int> message_count_{0};
    std::atomic<int> assistant_message_count_{0};
    std::atomic<int> result_message_count_{0};
    std::atomic<int> stream_event_count_{0};
    std::atomic<int> control_request_count_{0};
    std::atomic<int> control_response_count_{0};

    std::map<std::string, int> tool_use_stats_;
    std::mutex stats_mutex_;

  public:
    DebugLogger(bool verbose = true)
        : start_time_(std::chrono::steady_clock::now()), verbose_mode_(verbose)
    {
        log_file_.open("debug_protocol.log", std::ios::out | std::ios::trunc);
        if (!log_file_.is_open())
            std::cerr << Color::RED << "[WARNING] Could not open log file" << Color::RESET << "\n";

        if (verbose_mode_)
        {
            verbose_log_file_.open("debug_protocol_verbose.json", std::ios::out | std::ios::trunc);
            if (!verbose_log_file_.is_open())
            {
                std::cerr << Color::RED << "[WARNING] Could not open verbose log file"
                          << Color::RESET << "\n";
            }
            else
            {
                // Start JSON array for structured LLM processing
                verbose_log_file_ << "[\n";
            }
        }
    }

    ~DebugLogger()
    {
        if (log_file_.is_open())
            log_file_.close();
        if (verbose_log_file_.is_open())
        {
            // Close JSON array
            verbose_log_file_ << "\n]\n";
            verbose_log_file_.close();
        }
    }

    // Log structured event to verbose JSON log (for LLM processing)
    void log_verbose_event(const std::string& event_type, const json& data)
    {
        if (!verbose_mode_ || !verbose_log_file_.is_open())
            return;

        std::lock_guard<std::mutex> lock(mutex_);

        static bool first_event = true;
        if (!first_event)
            verbose_log_file_ << ",\n";
        first_event = false;

        json event = {{"timestamp", get_timestamp()}, {"event_type", event_type}, {"data", data}};

        verbose_log_file_ << event.dump(2);
        verbose_log_file_.flush();
    }

    // Get elapsed time since start
    std::string get_timestamp()
    {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = now - start_time_;
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();

        auto hours = ms / 3600000;
        ms %= 3600000;
        auto minutes = ms / 60000;
        ms %= 60000;
        auto seconds = ms / 1000;
        ms %= 1000;

        std::ostringstream oss;
        oss << std::setfill('0') << std::setw(2) << hours << ":" << std::setw(2) << minutes << ":"
            << std::setw(2) << seconds << "." << std::setw(3) << ms;
        return oss.str();
    }

    // Thread-safe logging
    void log(const std::string& color, const std::string& category, const std::string& message)
    {
        std::lock_guard<std::mutex> lock(mutex_);

        std::string timestamp = get_timestamp();
        std::string output = std::string("[") + timestamp + "] " + category + ": " + message;

        // Console output (with color)
        std::cout << color << output << Color::RESET << "\n";

        // File output (no color)
        if (log_file_.is_open())
        {
            log_file_ << output << "\n";
            log_file_.flush();
        }
    }

    void log_section(const std::string& title)
    {
        std::string separator(80, '=');
        log(Color::BOLD, "SECTION", separator);
        log(Color::BOLD, "SECTION", title);
        log(Color::BOLD, "SECTION", separator);
    }

    void log_json(const std::string& color, const std::string& category, const std::string& label,
                  const json& data)
    {
        std::lock_guard<std::mutex> lock(mutex_);

        std::string timestamp = get_timestamp();
        std::ostringstream oss;
        oss << "[" << timestamp << "] " << category << ": " << label << "\n";
        oss << data.dump(2);

        std::cout << color << oss.str() << Color::RESET << "\n";

        if (log_file_.is_open())
        {
            log_file_ << oss.str() << "\n";
            log_file_.flush();
        }
    }

    // Statistics tracking
    void increment_hook_counter(const std::string& hook_type)
    {
        if (hook_type == HookEvent::PreToolUse)
            pre_tool_use_count_++;
        else if (hook_type == HookEvent::PostToolUse)
            post_tool_use_count_++;
        else if (hook_type == HookEvent::UserPromptSubmit)
            user_prompt_submit_count_++;
        else if (hook_type == HookEvent::Stop)
            stop_count_++;
        else if (hook_type == HookEvent::SubagentStop)
            subagent_stop_count_++;
        else if (hook_type == HookEvent::PreCompact)
            pre_compact_count_++;
    }

    void track_tool_use(const std::string& tool_name)
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        tool_use_stats_[tool_name]++;
    }

    void increment_permission_request()
    {
        permission_request_count_++;
    }
    void increment_stderr_line()
    {
        stderr_line_count_++;
    }
    void increment_message()
    {
        message_count_++;
    }
    void increment_assistant_message()
    {
        assistant_message_count_++;
    }
    void increment_result_message()
    {
        result_message_count_++;
    }
    void increment_stream_event()
    {
        stream_event_count_++;
    }
    void increment_control_request()
    {
        control_request_count_++;
    }
    void increment_control_response()
    {
        control_response_count_++;
    }

    // Print comprehensive statistics
    void print_statistics()
    {
        log_section("PROTOCOL ANALYSIS STATISTICS");

        log(Color::CYAN, "STATS", "=== Message Statistics ===");
        log(Color::CYAN, "STATS", "  Total messages: " + std::to_string(message_count_.load()));
        log(Color::CYAN, "STATS",
            "  Assistant messages: " + std::to_string(assistant_message_count_.load()));
        log(Color::CYAN, "STATS",
            "  Result messages: " + std::to_string(result_message_count_.load()));
        log(Color::CYAN, "STATS", "  Stream events: " + std::to_string(stream_event_count_.load()));
        log(Color::CYAN, "STATS",
            "  Control requests: " + std::to_string(control_request_count_.load()));
        log(Color::CYAN, "STATS",
            "  Control responses: " + std::to_string(control_response_count_.load()));

        log(Color::CYAN, "STATS", "");
        log(Color::CYAN, "STATS", "=== Hook Statistics ===");
        log(Color::CYAN, "STATS",
            "  PreToolUse hooks: " + std::to_string(pre_tool_use_count_.load()));
        log(Color::CYAN, "STATS",
            "  PostToolUse hooks: " + std::to_string(post_tool_use_count_.load()));
        log(Color::CYAN, "STATS",
            "  UserPromptSubmit hooks: " + std::to_string(user_prompt_submit_count_.load()));
        log(Color::CYAN, "STATS", "  Stop hooks: " + std::to_string(stop_count_.load()));
        log(Color::CYAN, "STATS",
            "  SubagentStop hooks: " + std::to_string(subagent_stop_count_.load()));
        log(Color::CYAN, "STATS",
            "  PreCompact hooks: " + std::to_string(pre_compact_count_.load()));

        log(Color::CYAN, "STATS", "");
        log(Color::CYAN, "STATS", "=== Callback Statistics ===");
        log(Color::CYAN, "STATS",
            "  Permission requests: " + std::to_string(permission_request_count_.load()));
        log(Color::CYAN, "STATS", "  Stderr lines: " + std::to_string(stderr_line_count_.load()));

        if (!tool_use_stats_.empty())
        {
            log(Color::CYAN, "STATS", "");
            log(Color::CYAN, "STATS", "=== Tool Usage ===");
            std::lock_guard<std::mutex> lock(stats_mutex_);
            for (const auto& [tool, count] : tool_use_stats_)
                log(Color::CYAN, "STATS", "  " + tool + ": " + std::to_string(count));
        }
    }
};

// Global logger instance
static DebugLogger g_logger;

// ============================================================================
// Hook Callbacks - One for each hook event type
// ============================================================================

json pre_tool_use_hook(const json& input, const std::string& tool_use_id)
{
    g_logger.increment_hook_counter(HookEvent::PreToolUse);

    std::ostringstream oss;
    oss << "PreToolUse (id=" << tool_use_id << ")";

    if (input.contains("name"))
    {
        std::string tool_name = input["name"];
        oss << " tool=" << tool_name;
        g_logger.track_tool_use(tool_name);
    }

    g_logger.log_json(Color::GREEN, "HOOK", oss.str(), input);

    // Log to verbose structured log
    g_logger.log_verbose_event("hook_pre_tool_use",
                               {{"tool_use_id", tool_use_id}, {"hook_input", input}});

    return {{"continue", true}, {"suppressOutput", false}};
}

json post_tool_use_hook(const json& input, const std::string& tool_use_id)
{
    g_logger.increment_hook_counter(HookEvent::PostToolUse);

    std::ostringstream oss;
    oss << "PostToolUse (id=" << tool_use_id << ")";

    if (input.contains("name"))
        oss << " tool=" << input["name"];

    g_logger.log_json(Color::GREEN, "HOOK", oss.str(), input);

    // Log to verbose structured log
    g_logger.log_verbose_event("hook_post_tool_use",
                               {{"tool_use_id", tool_use_id}, {"hook_input", input}});

    return {{"continue", true}, {"suppressOutput", false}};
}

json user_prompt_submit_hook(const json& input, const std::string& /*tool_use_id*/)
{
    g_logger.increment_hook_counter(HookEvent::UserPromptSubmit);
    g_logger.log_json(Color::YELLOW, "HOOK", "UserPromptSubmit", input);

    // Log to verbose structured log
    g_logger.log_verbose_event("hook_user_prompt_submit", {{"hook_input", input}});

    return {{"continue", true}, {"suppressOutput", false}};
}

json stop_hook(const json& input, const std::string& /*tool_use_id*/)
{
    g_logger.increment_hook_counter(HookEvent::Stop);
    g_logger.log_json(Color::MAGENTA, "HOOK", "Stop (Claude became idle)", input);

    // Log to verbose structured log
    g_logger.log_verbose_event("hook_stop", {{"hook_input", input}});

    return {{"continue", true}, {"suppressOutput", false}};
}

json subagent_stop_hook(const json& input, const std::string& /*tool_use_id*/)
{
    g_logger.increment_hook_counter(HookEvent::SubagentStop);
    g_logger.log_json(Color::MAGENTA, "HOOK", "SubagentStop", input);

    // Log to verbose structured log
    g_logger.log_verbose_event("hook_subagent_stop", {{"hook_input", input}});

    return {{"continue", true}, {"suppressOutput", false}};
}

json pre_compact_hook(const json& input, const std::string& /*tool_use_id*/)
{
    g_logger.increment_hook_counter(HookEvent::PreCompact);
    g_logger.log_json(Color::YELLOW, "HOOK", "PreCompact (context window management)", input);

    // Log to verbose structured log
    g_logger.log_verbose_event("hook_pre_compact", {{"hook_input", input}});

    return {{"continue", true}, {"suppressOutput", false}};
}

// ============================================================================
// Tool Permission Callback - Intercepts all permission requests
// ============================================================================

PermissionResult tool_permission_callback(const std::string& tool_name, const json& input,
                                          const ToolPermissionContext& context)
{
    g_logger.increment_permission_request();

    std::ostringstream oss;
    oss << "Tool Permission Request: " << tool_name;
    g_logger.log_json(Color::BLUE, "PERMISSION", oss.str(), input);

    // Collect suggestions as JSON array
    json suggestions_json = json::array();
    if (!context.suggestions.empty())
    {
        g_logger.log(Color::BLUE, "PERMISSION",
                     "  Suggestions count: " + std::to_string(context.suggestions.size()));
        for (size_t i = 0; i < context.suggestions.size(); i++)
        {
            const auto& suggestion = context.suggestions[i];
            json suggestion_json = suggestion.to_json();
            g_logger.log(Color::BLUE, "PERMISSION",
                         "  Suggestion " + std::to_string(i) + ": " + suggestion_json.dump());
            suggestions_json.push_back(suggestion_json);
        }
    }

    // Log to verbose structured log
    g_logger.log_verbose_event(
        "tool_permission_request",
        {{"tool_name", tool_name}, {"input", input}, {"suggestions", suggestions_json}});

    // Always allow for debugging purposes
    PermissionResultAllow result;
    result.behavior = "allow";

    g_logger.log(Color::GREEN, "PERMISSION", "  -> ALLOWED");
    return result;
}

// ============================================================================
// Stderr Callback - Captures CLI stderr output
// ============================================================================

void stderr_callback(const std::string& line)
{
    g_logger.increment_stderr_line();
    g_logger.log(Color::RED, "STDERR", line);

    // Log to verbose structured log
    g_logger.log_verbose_event("cli_stderr", {{"line", line}});
}

// ============================================================================
// Configure Options with All Hooks and Callbacks
// ============================================================================

ClaudeOptions configure_debug_options()
{
    ClaudeOptions opts;

    // Use tool permission callback instead of bypass
    // This allows us to see all permission requests
    opts.tool_permission_callback = tool_permission_callback;

    // Capture stderr from CLI
    opts.stderr_callback = stderr_callback;

    // Register ALL hook types
    opts.hooks[HookEvent::PreToolUse] = {HookMatcher{std::nullopt, {pre_tool_use_hook}}};

    opts.hooks[HookEvent::PostToolUse] = {HookMatcher{std::nullopt, {post_tool_use_hook}}};

    opts.hooks[HookEvent::UserPromptSubmit] = {
        HookMatcher{std::nullopt, {user_prompt_submit_hook}}};

    opts.hooks[HookEvent::Stop] = {HookMatcher{std::nullopt, {stop_hook}}};

    opts.hooks[HookEvent::SubagentStop] = {HookMatcher{std::nullopt, {subagent_stop_hook}}};

    opts.hooks[HookEvent::PreCompact] = {HookMatcher{std::nullopt, {pre_compact_hook}}};

    return opts;
}

// ============================================================================
// Test Prompts - Designed to trigger various protocol features
// ============================================================================

struct TestPrompt
{
    std::string name;
    std::string prompt;
    std::string description;
};

std::vector<TestPrompt> get_test_prompts()
{
    return {{"File Write Operation",
             "Create a file called debug_test.txt and write 'Hello from debug test' to it. "
             "Then read it back to confirm.",
             "Triggers Write and Read tools, PreToolUse/PostToolUse hooks"},
            {"Bash Command", "Run the command 'echo Testing Bash Integration' using bash.",
             "Triggers Bash tool, shows command execution flow"},
            {"Multi-step Task",
             "Create a file test1.txt with line 'first', then append 'second', "
             "then read the file. Tell me what's in it.",
             "Multiple tool uses, demonstrates conversation flow"},
            {"Direct Answer", "What is 2+2? Just answer directly, don't use any tools.",
             "Simple response, minimal tool usage"}};
}

// ============================================================================
// Message Processing and Logging
// ============================================================================

void process_messages(ClaudeClient& client)
{
    for (const auto& msg : client.receive_messages())
    {
        g_logger.increment_message();

        if (is_assistant_message(msg))
        {
            g_logger.increment_assistant_message();
            const auto& assistant = std::get<AssistantMessage>(msg);

            std::string text = get_text_content(assistant.content);
            if (!text.empty())
            {
                // Truncate long messages for console
                std::string display_text = text.length() > 200 ? text.substr(0, 200) + "..." : text;
                g_logger.log(Color::WHITE, "ASSISTANT", display_text);
            }

            // Build content blocks for verbose log
            json content_blocks = json::array();
            for (const auto& block : assistant.content)
            {
                if (std::holds_alternative<TextBlock>(block))
                {
                    const auto& text_block = std::get<TextBlock>(block);
                    content_blocks.push_back({{"type", "text"}, {"text", text_block.text}});
                }
                else if (std::holds_alternative<ThinkingBlock>(block))
                {
                    const auto& thinking_block = std::get<ThinkingBlock>(block);
                    content_blocks.push_back({{"type", "thinking"},
                                              {"thinking", thinking_block.thinking},
                                              {"signature", thinking_block.signature}});
                }
                else if (std::holds_alternative<ToolUseBlock>(block))
                {
                    const auto& tool_use = std::get<ToolUseBlock>(block);
                    std::ostringstream oss;
                    oss << "Tool: " << tool_use.name << " (id=" << tool_use.id << ")";
                    g_logger.log_json(Color::CYAN, "TOOL_USE", oss.str(), tool_use.input);

                    content_blocks.push_back({{"type", "tool_use"},
                                              {"id", tool_use.id},
                                              {"name", tool_use.name},
                                              {"input", tool_use.input}});
                }
                else if (std::holds_alternative<ToolResultBlock>(block))
                {
                    const auto& tool_result = std::get<ToolResultBlock>(block);
                    content_blocks.push_back({{"type", "tool_result"},
                                              {"tool_use_id", tool_result.tool_use_id},
                                              {"content", tool_result.content},
                                              {"is_error", tool_result.is_error}});
                }
            }

            // Log to verbose structured log with FULL RAW JSON
            g_logger.log_verbose_event(
                "message_assistant",
                {
                    {"role", assistant.role},
                    {"model", assistant.model},
                    {"content", content_blocks},
                    {"raw_json", assistant.raw_json} // COMPLETE raw JSON from CLI
                });
        }
        else if (is_result_message(msg))
        {
            g_logger.increment_result_message();
            const auto& result = std::get<ResultMessage>(msg);

            std::ostringstream oss;
            oss << "Result: " << result.subtype << " | Duration: " << result.duration_ms << "ms"
                << " | Turns: " << result.num_turns << " | Cost: $" << std::fixed
                << std::setprecision(4) << result.total_cost_usd();
            g_logger.log(Color::GREEN, "RESULT", oss.str());

            // Log to verbose structured log with FULL details
            g_logger.log_verbose_event(
                "message_result",
                {
                    {"subtype", result.subtype},
                    {"session_id", result.result.session_id},
                    {"conversation_id", result.result.conversation_id},
                    {"duration_ms", result.duration_ms},
                    {"duration_api_ms", result.duration_api_ms},
                    {"num_turns", result.num_turns},
                    {"usage",
                     {{"input_tokens", result.result.usage.input_tokens},
                      {"output_tokens", result.result.usage.output_tokens},
                      {"cache_creation_input_tokens",
                       result.result.usage.cache_creation_input_tokens},
                      {"cache_read_input_tokens", result.result.usage.cache_read_input_tokens}}},
                    {"cost",
                     {{"total", result.result.cost.total},
                      {"input", result.result.cost.input},
                      {"output", result.result.cost.output}}},
                    {"raw_json", result.raw_json} // COMPLETE raw JSON from CLI
                });

            break;
        }
        else if (is_stream_event(msg))
        {
            g_logger.increment_stream_event();
            const auto& stream = std::get<StreamEvent>(msg);

            std::ostringstream oss;
            oss << "Stream: " << stream.event << " (index=" << stream.index << ")";
            g_logger.log(Color::BLUE, "STREAM", oss.str());

            // Log to verbose structured log with FULL stream data
            g_logger.log_verbose_event(
                "message_stream",
                {
                    {"event", stream.event},
                    {"index", stream.index},
                    {"uuid", stream.uuid},
                    {"session_id", stream.session_id},
                    {"parent_tool_use_id", stream.parent_tool_use_id.value_or("")},
                    {"data", stream.data},
                    {"raw_json", stream.raw_json} // COMPLETE raw JSON from CLI
                });
        }
        else if (is_control_request(msg))
        {
            g_logger.increment_control_request();
            const auto& control = std::get<protocol::ControlRequest>(msg);

            std::string method_str = control.request.contains("method")
                                         ? control.request["method"].get<std::string>()
                                         : "unknown";

            g_logger.log_json(Color::YELLOW, "CONTROL_REQ", "Control Request: " + method_str,
                              control.request);

            // Log to verbose structured log
            g_logger.log_verbose_event("message_control_request",
                                       {{"request_id", control.request_id},
                                        {"type", control.type},
                                        {"request", control.request}});
        }
        else if (is_control_response(msg))
        {
            g_logger.increment_control_response();
            const auto& control = std::get<protocol::ControlResponse>(msg);
            std::ostringstream oss;
            oss << "Control Response (id=" << control.response.request_id << ")";

            json response_data = {{"subtype", control.response.subtype},
                                  {"request_id", control.response.request_id},
                                  {"response", control.response.response},
                                  {"error", control.response.error}};

            g_logger.log_json(Color::YELLOW, "CONTROL_RSP", oss.str(), response_data);

            // Log to verbose structured log
            g_logger.log_verbose_event("message_control_response",
                                       {{"request_id", control.response.request_id},
                                        {"subtype", control.response.subtype},
                                        {"response", control.response.response},
                                        {"error", control.response.error}});
        }
        else if (is_system_message(msg))
        {
            const auto& system = std::get<SystemMessage>(msg);
            g_logger.log(Color::YELLOW, "SYSTEM",
                         "Type: " + system.subtype + " | " + system.content);

            // Log to verbose structured log
            g_logger.log_verbose_event("message_system",
                                       {
                                           {"subtype", system.subtype},
                                           {"content", system.content},
                                           {"raw_json", system.raw_json} // COMPLETE raw JSON
                                       });
        }
    }
}

// ============================================================================
// Main Program
// ============================================================================

int main()
{
    g_logger.log_section("CLAUDE CODE CLI - ULTIMATE PROTOCOL ANALYSIS");
    g_logger.log(Color::CYAN, "INFO",
                 "This tool hooks ALL available callbacks and logs the complete IPC protocol");
    g_logger.log(Color::CYAN, "INFO", "Output: Console + debug_protocol.log");
    g_logger.log(Color::CYAN, "INFO", "");

    try
    {
        // Configure with all debugging hooks
        ClaudeOptions opts = configure_debug_options();

        g_logger.log(Color::GREEN, "SETUP", "Configured all hooks:");
        g_logger.log(Color::GREEN, "SETUP", "  - PreToolUse, PostToolUse, UserPromptSubmit");
        g_logger.log(Color::GREEN, "SETUP", "  - Stop, SubagentStop, PreCompact");
        g_logger.log(Color::GREEN, "SETUP", "  - Tool Permission Callback");
        g_logger.log(Color::GREEN, "SETUP", "  - Stderr Callback");
        g_logger.log(Color::GREEN, "SETUP", "");

        // Connect to Claude
        ClaudeClient client(opts);
        g_logger.log(Color::GREEN, "CONNECT", "Connecting to Claude Code CLI...");
        client.connect();
        g_logger.log(Color::GREEN, "CONNECT",
                     "Connected! PID: " + std::to_string(client.get_pid()));
        g_logger.log(Color::GREEN, "CONNECT", "");

        // Get server info
        auto server_info = client.get_server_info();
        if (server_info.has_value())
            g_logger.log_json(Color::CYAN, "SERVER_INFO", "Initialization Data", *server_info);

        // Run all test prompts
        auto test_prompts = get_test_prompts();

        for (size_t i = 0; i < test_prompts.size(); i++)
        {
            const auto& test = test_prompts[i];

            g_logger.log_section("TEST " + std::to_string(i + 1) + ": " + test.name);
            g_logger.log(Color::YELLOW, "TEST", "Description: " + test.description);
            g_logger.log(Color::YELLOW, "TEST", "Prompt: " + test.prompt);
            g_logger.log(Color::YELLOW, "TEST", "");

            // Send query
            g_logger.log(Color::CYAN, "SEND", "Sending query...");
            client.send_query(test.prompt);

            // Process response
            g_logger.log(Color::CYAN, "RECEIVE", "Processing messages...");
            process_messages(client);

            // Brief pause between tests
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            g_logger.log(Color::WHITE, "TEST", "");
        }

        // Give hooks time to complete
        g_logger.log(Color::CYAN, "INFO", "Waiting for final hooks...");
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));

        // Disconnect
        g_logger.log(Color::GREEN, "DISCONNECT", "Disconnecting...");
        client.disconnect();
        g_logger.log(Color::GREEN, "DISCONNECT", "Disconnected successfully");
        g_logger.log(Color::GREEN, "DISCONNECT", "");

        // Print final statistics
        g_logger.print_statistics();

        g_logger.log_section("PROTOCOL ANALYSIS COMPLETE");
        g_logger.log(Color::GREEN, "SUCCESS", "Logs saved:");
        g_logger.log(Color::GREEN, "SUCCESS", "  - debug_protocol.log (human-readable)");
        g_logger.log(Color::GREEN, "SUCCESS",
                     "  - debug_protocol_verbose.json (structured, LLM-friendly)");
        g_logger.log(Color::CYAN, "INFO", "");
        g_logger.log(Color::CYAN, "INFO", "The verbose JSON log contains:");
        g_logger.log(Color::CYAN, "INFO", "  - Complete raw JSON from CLI for every message");
        g_logger.log(Color::CYAN, "INFO", "  - All hook inputs and metadata");
        g_logger.log(Color::CYAN, "INFO", "  - Tool permission requests with suggestions");
        g_logger.log(Color::CYAN, "INFO", "  - CLI stderr output");
        g_logger.log(Color::CYAN, "INFO", "  - Timestamps for every event");
        g_logger.log(Color::CYAN, "INFO", "  - Structured format ready for LLM analysis");
    }
    catch (const CLINotFoundError& /*e*/)
    {
        g_logger.log(Color::RED, "ERROR", "Claude CLI not found!");
        g_logger.log(Color::RED, "ERROR", "Install with: npm install -g @anthropic-ai/claude-code");
        return 1;
    }
    catch (const std::exception& e)
    {
        g_logger.log(Color::RED, "ERROR", std::string("Exception: ") + e.what());
        return 1;
    }

    return 0;
}
