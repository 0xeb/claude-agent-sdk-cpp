#include "subprocess_transport.hpp"

#include <chrono>
#include <claude/errors.hpp>
#include <claude/version.hpp>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <regex>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#endif

namespace claude
{
namespace internal
{

// Default buffer size for JSON message parsing (1MB)
constexpr size_t DEFAULT_MAX_BUFFER_SIZE = 1024 * 1024;

// Minimum required Claude Code CLI version
constexpr const char* MINIMUM_CLAUDE_CODE_VERSION = "2.0.0";

// Platform-specific command line length limits
// Windows cmd.exe has a limit of 8191 characters, use 8000 for safety
// Other platforms have much higher limits
#ifdef _WIN32
constexpr size_t CMD_LENGTH_LIMIT = 8000;
#else
constexpr size_t CMD_LENGTH_LIMIT = 100000;
#endif

SubprocessTransport::SubprocessTransport(const ClaudeOptions& options, bool streaming_mode)
    : options_(options), streaming_mode_(streaming_mode),
      parser_(std::make_unique<protocol::MessageParser>(
          options.max_buffer_size.value_or(DEFAULT_MAX_BUFFER_SIZE)))
{
}

SubprocessTransport::SubprocessTransport(const std::string& prompt, const ClaudeOptions& options)
    : options_(options), streaming_mode_(false), one_shot_prompt_(prompt),
      parser_(std::make_unique<protocol::MessageParser>(
          options.max_buffer_size.value_or(DEFAULT_MAX_BUFFER_SIZE)))
{
}

SubprocessTransport::~SubprocessTransport()
{
    close();
}

void SubprocessTransport::connect()
{
    if (process_ && process_->is_running())
        return; // Already connected

    // Find CLI executable (use explicit override if provided)
    std::string cli_path = find_cli();

    // Check CLI version and warn if below minimum (non-blocking)
    check_claude_version(cli_path);

    // Build command arguments
    auto args = build_command();

    // Windows command line optimization
    // Check if command line is too long and optimize if needed
    if (!options_.agents.empty())
    {
        // Calculate estimated command line length
        size_t cmd_length = cli_path.length();
        for (const auto& arg : args)
            cmd_length += 1 + arg.length(); // +1 for space

        // If command line exceeds limit, use temp file for --agents JSON
        if (cmd_length > CMD_LENGTH_LIMIT)
        {
            // Find --agents argument
            auto it = std::find(args.begin(), args.end(), "--agents");
            if (it != args.end() && std::next(it) != args.end())
            {
                auto agents_idx = std::distance(args.begin(), it);
                std::string agents_json_value = args[agents_idx + 1];

                try
                {
                    // Create temporary file
                    namespace fs = std::filesystem;
                    fs::path temp_dir = fs::temp_directory_path();
                    fs::path temp_file =
                        temp_dir /
                        ("claude_agents_" +
                         std::to_string(
                             std::chrono::system_clock::now().time_since_epoch().count()) +
                         ".json");

                    // Write agents JSON to file
                    std::ofstream ofs(temp_file);
                    if (!ofs)
                        throw std::runtime_error("Failed to create temp file for --agents");
                    ofs << agents_json_value;
                    ofs.close();

                    // Track for cleanup
                    temp_files_.push_back(temp_file.string());

                    // Replace agents JSON with @filepath reference
                    args[agents_idx + 1] = "@" + temp_file.string();

                    std::cerr << "Command line length (" << cmd_length << ") exceeds limit ("
                              << CMD_LENGTH_LIMIT << "). "
                              << "Using temp file for --agents: " << temp_file << std::endl;
                }
                catch (const std::exception& e)
                {
                    std::cerr << "Warning: Failed to optimize command line length: " << e.what()
                              << std::endl;
                    // Continue with original args (may fail on Windows with very long command
                    // lines)
                }
            }
        }
    }

    // Configure process options
    subprocess::ProcessOptions proc_opts;
    proc_opts.redirect_stdin = true;
    proc_opts.redirect_stdout = true;
    // Redirect stderr if callback is present
    proc_opts.redirect_stderr = options_.stderr_callback.has_value();

    if (options_.working_directory)
        proc_opts.working_directory = *options_.working_directory;

    // Merge environment variables
    proc_opts.environment = options_.environment;
    proc_opts.environment["CLAUDE_CODE_ENTRYPOINT"] = "sdk-py";
    proc_opts.environment["CLAUDE_AGENT_SDK_VERSION"] = version_string();

    // Spawn process
    process_ = std::make_unique<subprocess::Process>();
    process_->spawn(cli_path, args, proc_opts);

    // For one-shot queries (--print mode), close stdin immediately
    // The prompt is on command line, no need to keep stdin open
    if (!streaming_mode_ && !one_shot_prompt_.empty())
        process_->stdin_pipe().close();

    // Start background reader threads
    start_reader();

    // Start stderr reader if callback is configured
    if (options_.stderr_callback.has_value())
        start_stderr_reader();

    ready_ = true;
}

void SubprocessTransport::write(const std::string& data)
{
    if (!is_ready())
        throw CLIConnectionError("Transport is not ready for writing");

    if (!process_ || !process_->is_running())
        throw CLIConnectionError("Cannot write to terminated process");

    process_->stdin_pipe().write(data);
    process_->stdin_pipe().flush();
}

std::vector<Message> SubprocessTransport::read_messages()
{
    std::unique_lock<std::mutex> lock(queue_mutex_);

    // Wait for messages with timeout
    queue_cv_.wait_for(lock, std::chrono::milliseconds(100),
                       [this] { return !message_queue_.empty() || queue_stopped_; });

    std::vector<Message> messages;

    // Drain available messages
    while (!message_queue_.empty())
    {
        messages.push_back(std::move(message_queue_.front()));
        message_queue_.pop();
    }

    return messages;
}

bool SubprocessTransport::has_messages() const
{
    std::lock_guard<std::mutex> lock(queue_mutex_);
    return !message_queue_.empty() || (!queue_stopped_ && is_running());
}

void SubprocessTransport::close()
{
    ready_ = false;

    // Clean up temporary files first
    for (const auto& temp_file : temp_files_)
    {
        try
        {
            std::filesystem::remove(temp_file);
        }
        catch (const std::exception& e)
        {
            // Ignore errors during cleanup
            std::cerr << "Warning: Failed to remove temp file " << temp_file << ": " << e.what()
                      << std::endl;
        }
    }
    temp_files_.clear();

    // Stop reader threads
    stop_reader();
    stop_stderr_reader();

    // Close process
    if (process_)
    {
        if (process_->stdin_pipe().is_open())
            process_->stdin_pipe().close();

        // Wait for process to exit
        auto exit_code = process_->try_wait();
        if (!exit_code)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            exit_code = process_->try_wait();
            if (!exit_code)
            {
                process_->terminate();
                process_->wait();
            }
        }

        process_.reset();
    }

    // Clear message queue
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        std::queue<Message> empty;
        message_queue_.swap(empty);
        queue_stopped_ = true;
    }
}

bool SubprocessTransport::is_ready() const
{
    return ready_ && process_ && process_->is_running();
}

void SubprocessTransport::end_input()
{
    if (process_ && process_->stdin_pipe().is_open())
        process_->stdin_pipe().close();
}

long SubprocessTransport::get_pid() const
{
    if (process_)
        return static_cast<long>(process_->pid());
    return 0;
}

bool SubprocessTransport::is_running() const
{
    return process_ && process_->is_running();
}

std::vector<std::string> SubprocessTransport::build_command() const
{
    std::vector<std::string> args;

    // Output format - always stream-json for SDK
    args.push_back("--output-format");
    args.push_back("stream-json");

    // Input format depends on mode
    if (streaming_mode_)
    {
        args.push_back("--input-format");
        args.push_back("stream-json");
    }

    // Required when using stream-json output format
    args.push_back("--verbose");

    // System prompt handling
    // - If system_prompt_append is set, use --append-system-prompt (appends to claude_code preset)
    // - Otherwise, if system_prompt is set, use --system-prompt (custom prompt)
    // - If neither is set, pass empty string (CLI requirement for proper parsing)
    if (!options_.system_prompt_append.empty())
    {
        args.push_back("--append-system-prompt");
        args.push_back(options_.system_prompt_append);
    }
    else if (!options_.system_prompt.empty())
    {
        args.push_back("--system-prompt");
        args.push_back(options_.system_prompt);
    }
    else
    {
        args.push_back("--system-prompt");
        args.push_back(""); // Empty string required by CLI
    }

    // Allowed tools
    if (!options_.allowed_tools.empty())
    {
        std::string tools_str;
        for (size_t i = 0; i < options_.allowed_tools.size(); ++i)
        {
            if (i > 0)
                tools_str += ",";
            tools_str += options_.allowed_tools[i];
        }
        args.push_back("--allowedTools");
        args.push_back(tools_str);
    }

    // Max turns
    if (options_.max_turns)
    {
        args.push_back("--max-turns");
        args.push_back(std::to_string(*options_.max_turns));
    }

    // v0.1.6: Max budget USD
    if (options_.max_budget_usd)
    {
        args.push_back("--max-budget-usd");
        args.push_back(std::to_string(*options_.max_budget_usd));
    }

    // Disallowed tools
    if (!options_.disallowed_tools.empty())
    {
        std::string tools_str;
        for (size_t i = 0; i < options_.disallowed_tools.size(); ++i)
        {
            if (i > 0)
                tools_str += ",";
            tools_str += options_.disallowed_tools[i];
        }
        args.push_back("--disallowedTools");
        args.push_back(tools_str);
    }

    // Model
    if (!options_.model.empty())
    {
        args.push_back("--model");
        args.push_back(options_.model);
    }

    // Fallback model
    if (!options_.fallback_model.empty())
    {
        args.push_back("--fallback-model");
        args.push_back(options_.fallback_model);
    }

    // Permission prompt tool
    std::string permission_tool_name = options_.permission_prompt_tool_name;
    if (options_.tool_permission_callback.has_value() && permission_tool_name.empty())
        permission_tool_name = "stdio";

    if (!permission_tool_name.empty())
    {
        args.push_back("--permission-prompt-tool");
        args.push_back(permission_tool_name);
    }

    // Permission mode
    if (!options_.permission_mode.empty())
    {
        args.push_back("--permission-mode");
        args.push_back(options_.permission_mode);
    }

    // Continue conversation
    if (options_.continue_conversation)
        args.push_back("--continue");

    // Resume session
    if (!options_.resume.empty())
    {
        args.push_back("--resume");
        args.push_back(options_.resume);
    }

    // Settings file
    if (!options_.settings.empty())
    {
        args.push_back("--settings");
        args.push_back(options_.settings);
    }

    // Additional directories
    for (const auto& dir : options_.add_dirs)
    {
        args.push_back("--add-dir");
        args.push_back(dir);
    }

    // MCP configuration
    if (!options_.mcp_config.empty())
    {
        args.push_back("--mcp-config");
        args.push_back(options_.mcp_config);
    }

    // Partial messages
    if (options_.include_partial_messages)
        args.push_back("--include-partial-messages");

    // Fork session
    if (options_.fork_session)
        args.push_back("--fork-session");

    // Setting sources (always pass, CLI requires it for proper parsing)
    std::string sources_str;
    if (!options_.setting_sources.empty())
    {
        for (size_t i = 0; i < options_.setting_sources.size(); ++i)
        {
            if (i > 0)
                sources_str += ",";
            sources_str += options_.setting_sources[i];
        }
    }
    // else: sources_str remains empty string
    args.push_back("--setting-sources");
    args.push_back(sources_str); // May be empty string, which CLI expects

    // Note: Working directory is set via ProcessOptions.working_directory,
    // not via a CLI flag (the --working-directory flag is not supported)

    // Agents support
    if (!options_.agents.empty())
    {
        json agents_json = json::object();

        for (const auto& [name, agent_def] : options_.agents)
        {
            json agent_obj = json::object();
            agent_obj["description"] = agent_def.description;
            agent_obj["prompt"] = agent_def.prompt;

            if (agent_def.tools.has_value())
                agent_obj["tools"] = *agent_def.tools;
            if (agent_def.model.has_value())
                agent_obj["model"] = *agent_def.model;

            agents_json[name] = agent_obj;
        }

        args.push_back("--agents");
        args.push_back(agents_json.dump());
    }

    // Plugins
    // Add --plugin-dir for each plugin (only "local" type supported currently)
    for (const auto& plugin : options_.plugins)
    {
        if (plugin.type == "local")
        {
            args.push_back("--plugin-dir");
            args.push_back(plugin.path);
        }
    }

    // Extra args
    for (const auto& [flag, value] : options_.extra_args)
    {
        std::string full_flag = flag;
        if (full_flag.substr(0, 2) != "--" && !full_flag.empty())
            full_flag = "--" + full_flag;

        args.push_back(full_flag);

        if (!value.empty())
            args.push_back(value);
    }

    // v0.1.6: Max thinking tokens
    if (options_.max_thinking_tokens)
    {
        args.push_back("--max-thinking-tokens");
        args.push_back(std::to_string(*options_.max_thinking_tokens));
    }

    // v0.1.8: Output format / JSON schema
    if (options_.output_format.has_value())
    {
        const json& format = *options_.output_format;
        if (format.is_object() && format.contains("type") && format["type"] == "json_schema")
        {
            if (format.contains("schema"))
            {
                args.push_back("--json-schema");
                args.push_back(format["schema"].dump());
            }
        }
    }

    // For one-shot mode, add --print and the prompt
    if (!streaming_mode_ && !one_shot_prompt_.empty())
    {
        args.push_back("--print");
        args.push_back("--");
        args.push_back(one_shot_prompt_);
    }

    return args;
}

std::string SubprocessTransport::find_cli() const
{
    // If user provided explicit CLI path, prefer it
    if (!options_.cli_path.empty())
    {
        // Validate the provided path exists
        namespace fs = std::filesystem;
        fs::path p(options_.cli_path);
        if (fs::exists(p))
            return options_.cli_path;
        // Path does not exist
        throw CLINotFoundError(std::string("Claude Code not found at: ") + options_.cli_path);
    }

    // Environment override: CLAUDE_CLI_PATH
    if (const char* env_cli = std::getenv("CLAUDE_CLI_PATH"))
    {
        namespace fs = std::filesystem;
        fs::path p(env_cli);
        if (fs::exists(p))
            return std::string(env_cli);
    }

    // Fallback: search PATH for 'claude'
    if (auto result = subprocess::find_executable("claude"))
        return *result;

    // Check local installation
    if (const char* home = std::getenv("HOME"))
    {
        namespace fs = std::filesystem;
        fs::path local_cli = fs::path(home) / ".claude" / "local" / "claude";
        if (fs::exists(local_cli))
            return local_cli.string();
    }

    throw CLINotFoundError("Could not find 'claude' executable in PATH. "
                           "Please install: npm install -g @anthropic-ai/claude-code");
}

void SubprocessTransport::check_claude_version(const std::string& cli_path)
{
    // Check Claude Code version and warn if below minimum
    // Non-blocking: all exceptions are caught and ignored
    try
    {
        subprocess::Process version_process;
        subprocess::ProcessOptions opts;
        opts.redirect_stdout = true;
        opts.redirect_stderr = true;

        std::vector<std::string> args = {"-v"};
        version_process.spawn(cli_path, args, opts);

        // Wait for process with timeout (2 seconds)
        auto start_time = std::chrono::steady_clock::now();
        constexpr auto timeout = std::chrono::seconds(2);

        std::string output;
        while (version_process.is_running())
        {
            auto elapsed = std::chrono::steady_clock::now() - start_time;
            if (elapsed > timeout)
            {
                version_process.terminate();
                return; // Timeout - silently ignore
            }

            // Try reading stdout with short timeout
            if (version_process.stdout_pipe().has_data(50))
            {
                char buffer[256];
                size_t n = version_process.stdout_pipe().read(buffer, sizeof(buffer));
                if (n > 0)
                    output.append(buffer, n);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        // Wait for process to complete
        version_process.wait();

        // Parse version string using regex: ([0-9]+\.[0-9]+\.[0-9]+)
        std::regex version_regex(R"(([0-9]+\.[0-9]+\.[0-9]+))");
        std::smatch match;

        if (std::regex_search(output, match, version_regex) && match.size() > 1)
        {
            std::string version_str = match[1].str();

            // Parse version parts
            std::vector<int> version_parts;
            std::stringstream ss(version_str);
            std::string part;
            while (std::getline(ss, part, '.'))
                version_parts.push_back(std::stoi(part));

            // Parse minimum version parts
            std::vector<int> min_parts;
            std::stringstream min_ss(MINIMUM_CLAUDE_CODE_VERSION);
            while (std::getline(min_ss, part, '.'))
                min_parts.push_back(std::stoi(part));

            // Compare versions
            if (version_parts < min_parts)
            {
                std::string warning = "Warning: Claude Code version " + version_str +
                                      " is unsupported in the Agent SDK. "
                                      "Minimum required version is " +
                                      std::string(MINIMUM_CLAUDE_CODE_VERSION) +
                                      ". Some features may not work correctly.";

                // Send warning via stderr callback if present, otherwise to stderr
                if (options_.stderr_callback.has_value())
                {
                    try
                    {
                        (*options_.stderr_callback)(warning);
                    }
                    catch (...)
                    {
                        // Ignore callback exceptions
                    }
                }
                else
                {
                    std::cerr << warning << std::endl;
                }
            }
        }
    }
    catch (...)
    {
        // Silently ignore all errors - version check is non-critical
    }
}

void SubprocessTransport::reader_loop()
{
    try
    {
        while (running_ && process_ && process_->is_running())
        {
            // Check if stdout has data with timeout
            if (!process_->stdout_pipe().has_data(100))
                continue;

            // Read data from stdout
            char buffer[4096];
            size_t n = process_->stdout_pipe().read(buffer, sizeof(buffer));

            if (n == 0)
                break; // EOF reached

            // Parse messages
            std::string data(buffer, n);
            auto messages = parser_->add_data(data);

            // Add messages to queue
            if (!messages.empty())
            {
                std::lock_guard<std::mutex> lock(queue_mutex_);
                for (auto& msg : messages)
                    message_queue_.push(std::move(msg));
                queue_cv_.notify_all();
            }
        }
    }
    catch (const std::exception&)
    {
        // Error in reader loop
    }

    // Mark queue as stopped
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        queue_stopped_ = true;
        queue_cv_.notify_all();
    }
}

void SubprocessTransport::start_reader()
{
    running_ = true;
    reader_thread_ = std::thread(&SubprocessTransport::reader_loop, this);
}

void SubprocessTransport::stop_reader()
{
    if (running_)
    {
        running_ = false;
        if (reader_thread_.joinable())
            reader_thread_.join();
    }
}

void SubprocessTransport::stderr_reader_loop()
{
    // Read stderr line-by-line and invoke callback
    try
    {
        while (stderr_running_ && process_ && process_->is_running())
        {
            // Check if stderr has data with timeout
            if (!process_->stderr_pipe().has_data(100))
                continue;

            // Read a line from stderr (returns empty string on EOF)
            std::string line = process_->stderr_pipe().read_line();

            // Empty line indicates EOF or error
            if (line.empty() && !process_->is_running())
                break;

            // Remove trailing whitespace
            while (!line.empty() && (line.back() == '\r' || line.back() == '\n'))
                line.pop_back();

            // Skip empty lines
            if (line.empty())
                continue;

            // Invoke callback if still configured
            if (options_.stderr_callback.has_value())
            {
                try
                {
                    (*options_.stderr_callback)(line);
                }
                catch (...)
                {
                    // Ignore exceptions from user callback
                }
            }
        }
    }
    catch (const std::exception&)
    {
        // Error in stderr reader loop - silently ignore
    }
}

void SubprocessTransport::start_stderr_reader()
{
    stderr_running_ = true;
    stderr_reader_thread_ = std::thread(&SubprocessTransport::stderr_reader_loop, this);
}

void SubprocessTransport::stop_stderr_reader()
{
    if (stderr_running_)
    {
        stderr_running_ = false;
        if (stderr_reader_thread_.joinable())
            stderr_reader_thread_.join();
    }
}

} // namespace internal

// Factory functions
std::unique_ptr<Transport> create_subprocess_transport(const ClaudeOptions& options)
{
    return std::make_unique<internal::SubprocessTransport>(options);
}

std::unique_ptr<Transport> create_oneshot_transport(const std::string& prompt,
                                                    const ClaudeOptions& options)
{
    return std::make_unique<internal::SubprocessTransport>(prompt, options);
}

} // namespace claude
