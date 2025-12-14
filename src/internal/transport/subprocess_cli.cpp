#include "subprocess_cli.hpp"

#include "cli_verification.hpp"
#include "subprocess_env.hpp"

#include <chrono>
#include <claude/errors.hpp>
#include <claude/version.hpp>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <regex>
#include <sstream>

namespace claude
{
namespace transport
{

// Platform-specific command line length limits
// Windows cmd.exe has a limit of 8191 characters, use 8000 for safety
// Other platforms have much higher limits
#ifdef _WIN32
constexpr size_t CMD_LENGTH_LIMIT = 8000;
#else
constexpr size_t CMD_LENGTH_LIMIT = 100000;
#endif

namespace
{
std::string write_agents_temp_file(const std::string& contents,
                                   std::vector<std::string>& temp_files)
{
    namespace fs = std::filesystem;
    auto make_name = []
    {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<int> dist(0, 15);
        const char* digits = "0123456789abcdef";
        std::string hex(8, '0');
        for (auto& c : hex)
            c = digits[dist(gen)];
        return std::string("claude_agents-") + hex + ".json";
    };
    // Security: Prevent symlink attacks with TOCTOU-safe file creation
    fs::path temp_file;
    int max_attempts = 10;
    for (int attempt = 0; attempt < max_attempts; ++attempt)
    {
        temp_file = fs::temp_directory_path() / make_name();

        std::error_code ec;
        // Check if path already exists (could be a symlink planted by attacker)
        if (fs::exists(temp_file, ec))
            continue; // Try a different name

        // Create file exclusively (fails if file exists)
        std::ofstream ofs(temp_file, std::ios::binary | std::ios::out | std::ios::trunc);
        if (!ofs)
            continue; // Try again with different name

        // Verify it's not a symlink after creation
        if (fs::is_symlink(temp_file, ec))
        {
            ofs.close();
            fs::remove(temp_file, ec);
            throw std::runtime_error("Symlink detected after temp file creation: " +
                                     temp_file.string());
        }

        // Write contents
        ofs << contents;
        ofs.close();

        // Final verification before returning
        if (fs::is_symlink(temp_file, ec))
        {
            fs::remove(temp_file, ec);
            throw std::runtime_error("Symlink race detected for temp file: " + temp_file.string());
        }

        // Success
        temp_files.push_back(temp_file.string());
        return temp_file.string();
    }

    throw std::runtime_error("Failed to create secure temp file after " +
                             std::to_string(max_attempts) + " attempts");
}
} // namespace

SubprocessCLITransport::SubprocessCLITransport(const std::string& prompt,
                                               const ClaudeOptions& options,
                                               const std::optional<std::string>& cli_path)
    : prompt_(prompt), options_(options), cli_path_(find_cli(cli_path)),
      is_streaming_(false) // Will be set based on prompt type
      ,
      parser_(std::make_unique<protocol::MessageParser>(options.max_message_buffer_size)),
      ready_(false)
{
}

SubprocessCLITransport::~SubprocessCLITransport()
{
    close();
}

SubprocessCLITransport::SubprocessCLITransport(SubprocessCLITransport&&) noexcept = default;
SubprocessCLITransport&
SubprocessCLITransport::operator=(SubprocessCLITransport&&) noexcept = default;

void SubprocessCLITransport::connect()
{
    if (process_)
        return; // Already connected

    // Check version unless explicitly skipped via environment variable
    if (!std::getenv("CLAUDE_AGENT_SDK_SKIP_VERSION_CHECK"))
        check_cli_version();

    auto args = build_command();

    // Windows command line optimization
    // Check if command line is too long and optimize if needed
    if (!options_.agents.empty())
    {
        // Calculate estimated command line length
        size_t cmd_length = cli_path_.length();
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
                    auto temp_file = write_agents_temp_file(agents_json_value, temp_files_);

                    // Replace agents JSON with @filepath reference
                    args[agents_idx + 1] = "@" + temp_file;

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

    subprocess::ProcessOptions proc_opts;
    if (options_.working_directory)
        proc_opts.working_directory = *options_.working_directory;

    // Enable stderr redirection if callback is set
    if (options_.stderr_callback.has_value())
        proc_opts.redirect_stderr = true;

    // Security: Environment variable sanitization
    bool strip_env = std::getenv("CLAUDE_AGENT_SDK_STRIP_ENV") != nullptr;
    bool should_sanitize = options_.sanitize_environment || strip_env;

    if (should_sanitize)
    {
        // Security: Only forward essential system variables and allowlisted vars
        proc_opts.inherit_environment = false;

        // Build allowlist of essential system variables
        std::vector<std::string> essential_vars = {
            "PATH",         // Required for CLI execution
            "HOME",         // Required for home directory access
            "USERPROFILE",  // Windows equivalent of HOME
            "TEMP",         // Temporary directory
            "TMP",          // Alternative temp directory
            "SystemRoot",   // Windows system root
            "HOMEDRIVE",    // Windows home drive
            "HOMEPATH",     // Windows home path
            "APPDATA",      // Windows app data
            "LOCALAPPDATA", // Windows local app data
            "LANG",         // Locale settings
            "LC_ALL",       // Locale settings
            "TERM",         // Terminal type
            "SHELL",        // Unix shell
        };

        // Add essential variables that exist in parent environment
        for (const auto& var_name : essential_vars)
            if (const char* value = std::getenv(var_name.c_str()))
                proc_opts.environment[var_name] = value;

        // Add allowlisted variables from options
        for (const auto& var_name : options_.allowed_env_vars)
            if (const char* value = std::getenv(var_name.c_str()))
                proc_opts.environment[var_name] = value;
    }
    else
    {
        // Legacy behavior: inherit full environment
        proc_opts.inherit_environment = options_.inherit_environment;
    }

    // Merge custom environment variables (these always override)
    for (const auto& [key, value] : options_.environment)
        proc_opts.environment[key] = value;

    // SDK-specific variables (always set)
    apply_sdk_environment(proc_opts, options_, "sdk-cpp");

    // Windows fix: .cmd files don't work with CreateProcess stdout redirection
    // If we found claude.cmd, call node.exe with cli.js directly
    std::string executable = cli_path_;
    std::vector<std::string> final_args = args;

#ifdef _WIN32
    if (cli_path_.length() >= 4 && cli_path_.substr(cli_path_.length() - 4) == ".cmd")
    {
        // Derive cli.js path from claude.cmd
        // C:\...\npm\claude.cmd -> C:\...\npm\node_modules\@anthropic-ai\claude-code\cli.js
        std::filesystem::path cmd_path(cli_path_);
        std::filesystem::path cli_js =
            cmd_path.parent_path() / "node_modules" / "@anthropic-ai" / "claude-code" / "cli.js";

        if (!std::filesystem::exists(cli_js))
            throw CLINotFoundError("Claude Code cli.js not found at: " + cli_js.string());

        // Find node.exe
        auto node_path = subprocess::find_executable("node");
        if (!node_path)
            throw CLINotFoundError("node.exe not found in PATH");

        // Use node.exe with cli.js as first argument
        executable = *node_path;
        final_args.insert(final_args.begin(), cli_js.string());
    }
#endif

    process_ = std::make_unique<subprocess::Process>();

// Debug: Print what we're spawning
#ifdef _DEBUG
    std::cerr << "[DEBUG] Spawning: " << executable << "\n";
    for (size_t i = 0; i < final_args.size(); ++i)
        std::cerr << "[DEBUG]   arg[" << i << "]: " << final_args[i] << "\n";
#endif

    try
    {
        process_->spawn(executable, final_args, proc_opts);

        // For one-shot queries (--print mode), close stdin immediately
        // The prompt is on command line, no need to keep stdin open
        if (!is_streaming_)
            process_->stdin_pipe().close();

        // Start stderr reader thread if callback is set
        if (options_.stderr_callback.has_value())
        {
            stderr_stop_flag_ = false;
            stderr_thread_ =
                std::make_unique<std::thread>(&SubprocessCLITransport::stderr_reader, this);
        }

        ready_ = true;
    }
    catch (const std::exception& e)
    {
        throw CLIConnectionError(std::string("Failed to start Claude Code: ") + e.what());
    }
}

// Helper function to serialize sandbox settings to JSON
static json sandbox_to_json(const SandboxSettings& sandbox)
{
    json j;

    if (sandbox.enabled.has_value())
        j["enabled"] = *sandbox.enabled;
    if (sandbox.autoAllowBashIfSandboxed.has_value())
        j["autoAllowBashIfSandboxed"] = *sandbox.autoAllowBashIfSandboxed;
    if (sandbox.excludedCommands.has_value())
        j["excludedCommands"] = *sandbox.excludedCommands;
    if (sandbox.allowUnsandboxedCommands.has_value())
        j["allowUnsandboxedCommands"] = *sandbox.allowUnsandboxedCommands;

    if (sandbox.network.has_value())
    {
        json net;
        if (sandbox.network->allowUnixSockets.has_value())
            net["allowUnixSockets"] = *sandbox.network->allowUnixSockets;
        if (sandbox.network->allowAllUnixSockets.has_value())
            net["allowAllUnixSockets"] = *sandbox.network->allowAllUnixSockets;
        if (sandbox.network->allowLocalBinding.has_value())
            net["allowLocalBinding"] = *sandbox.network->allowLocalBinding;
        if (sandbox.network->httpProxyPort.has_value())
            net["httpProxyPort"] = *sandbox.network->httpProxyPort;
        if (sandbox.network->socksProxyPort.has_value())
            net["socksProxyPort"] = *sandbox.network->socksProxyPort;
        if (!net.empty())
            j["network"] = net;
    }

    if (sandbox.ignoreViolations.has_value())
    {
        json ignore;
        if (sandbox.ignoreViolations->file.has_value())
            ignore["file"] = *sandbox.ignoreViolations->file;
        if (sandbox.ignoreViolations->network.has_value())
            ignore["network"] = *sandbox.ignoreViolations->network;
        if (!ignore.empty())
            j["ignoreViolations"] = ignore;
    }

    if (sandbox.enableWeakerNestedSandbox.has_value())
        j["enableWeakerNestedSandbox"] = *sandbox.enableWeakerNestedSandbox;

    return j;
}

std::optional<std::string> SubprocessCLITransport::build_settings_value() const
{
    bool has_settings = !options_.settings.empty();
    bool has_sandbox = options_.sandbox.has_value();

    // If neither settings nor sandbox, return nullopt
    if (!has_settings && !has_sandbox)
        return std::nullopt;

    // If only settings and no sandbox, pass through as-is
    if (has_settings && !has_sandbox)
        return options_.settings;

    // If we have sandbox settings, we need to merge into a JSON object
    json settings_obj;

    if (has_settings)
    {
        std::string settings_str = options_.settings;

        // Trim whitespace
        settings_str.erase(0, settings_str.find_first_not_of(" \t\n\r"));
        settings_str.erase(settings_str.find_last_not_of(" \t\n\r") + 1);

        // Check if settings is a JSON string or a file path
        if (!settings_str.empty() && settings_str[0] == '{' && settings_str.back() == '}')
        {
            // Parse JSON string
            try
            {
                settings_obj = json::parse(settings_str);
            }
            catch (const json::parse_error&)
            {
                // If parsing fails, treat as file path
                std::filesystem::path settings_path(settings_str);
                if (std::filesystem::exists(settings_path))
                {
                    std::ifstream file(settings_path);
                    if (file.is_open())
                        file >> settings_obj;
                }
            }
        }
        else
        {
            // It's a file path - read and parse
            std::filesystem::path settings_path(settings_str);
            if (std::filesystem::exists(settings_path))
            {
                std::ifstream file(settings_path);
                if (file.is_open())
                    file >> settings_obj;
            }
        }
    }

    // Merge sandbox settings
    if (has_sandbox)
        settings_obj["sandbox"] = sandbox_to_json(*options_.sandbox);

    return settings_obj.dump();
}

std::vector<std::string> SubprocessCLITransport::build_command()
{
    std::vector<std::string> cmd;

    // Base options
    cmd.push_back("--output-format");
    cmd.push_back("stream-json");
    cmd.push_back("--verbose");

    // System prompt (mutually exclusive: system_prompt OR system_prompt_append)
    // Always pass --system-prompt (CLI requires it for proper parsing)
    if (!options_.system_prompt.empty())
    {
        cmd.push_back("--system-prompt");
        cmd.push_back(options_.system_prompt);
    }
    else if (!options_.system_prompt_append.empty())
    {
        cmd.push_back("--append-system-prompt");
        cmd.push_back(options_.system_prompt_append);
    }
    else
    {
        cmd.push_back("--system-prompt");
        cmd.push_back(""); // Empty string required by CLI
    }

    // Base tools configuration (ClaudeAgentOptions.tools)
    if (options_.tools.has_value())
    {
        // Explicit tools list – empty list disables all built-in tools
        std::ostringstream oss;
        const auto& tools = *options_.tools;
        for (size_t i = 0; i < tools.size(); ++i)
        {
            if (i > 0)
                oss << ",";
            oss << tools[i];
        }
        cmd.push_back("--tools");
        cmd.push_back(oss.str());
    }
    else if (options_.tools_preset.has_value())
    {
        // Preset mapping – "claude_code" preset maps to CLI "default"
        std::string preset = *options_.tools_preset;
        std::string cli_value = (preset == "claude_code") ? "default" : preset;
        cmd.push_back("--tools");
        cmd.push_back(cli_value);
    }

    // Allowed tools
    if (!options_.allowed_tools.empty())
    {
        cmd.push_back("--allowedTools");
        std::ostringstream oss;
        for (size_t i = 0; i < options_.allowed_tools.size(); ++i)
        {
            if (i > 0)
                oss << ",";
            oss << options_.allowed_tools[i];
        }
        cmd.push_back(oss.str());
    }

    // Disallowed tools
    if (!options_.disallowed_tools.empty())
    {
        cmd.push_back("--disallowedTools");
        std::ostringstream oss;
        for (size_t i = 0; i < options_.disallowed_tools.size(); ++i)
        {
            if (i > 0)
                oss << ",";
            oss << options_.disallowed_tools[i];
        }
        cmd.push_back(oss.str());
    }

    // Model
    if (!options_.model.empty())
    {
        cmd.push_back("--model");
        cmd.push_back(options_.model);
    }

    // Fallback model
    if (!options_.fallback_model.empty())
    {
        cmd.push_back("--fallback-model");
        cmd.push_back(options_.fallback_model);
    }

    // Beta headers (ClaudeAgentOptions.betas)
    if (!options_.betas.empty())
    {
        std::ostringstream oss;
        for (size_t i = 0; i < options_.betas.size(); ++i)
        {
            if (i > 0)
                oss << ",";
            oss << options_.betas[i];
        }
        cmd.push_back("--betas");
        cmd.push_back(oss.str());
    }

    // Permission mode
    if (!options_.permission_mode.empty())
    {
        cmd.push_back("--permission-mode");
        cmd.push_back(options_.permission_mode);
    }

    // Max turns
    if (options_.max_turns)
    {
        cmd.push_back("--max-turns");
        cmd.push_back(std::to_string(*options_.max_turns));
    }

    // v0.1.6: Max budget USD
    if (options_.max_budget_usd)
    {
        cmd.push_back("--max-budget-usd");
        cmd.push_back(std::to_string(*options_.max_budget_usd));
    }

    // Partial messages
    if (options_.include_partial_messages)
        cmd.push_back("--include-partial-messages");

    // Permission prompt tool
    if (!options_.permission_prompt_tool_name.empty())
    {
        cmd.push_back("--permission-prompt-tool");
        cmd.push_back(options_.permission_prompt_tool_name);
    }

    // Continue conversation
    if (options_.continue_conversation)
        cmd.push_back("--continue");

    // Resume session
    if (!options_.resume.empty())
    {
        cmd.push_back("--resume");
        cmd.push_back(options_.resume);
    }

    // Settings file or JSON (with optional sandbox merge)
    auto settings_value = build_settings_value();
    if (settings_value.has_value())
    {
        cmd.push_back("--settings");
        cmd.push_back(*settings_value);
    }

    // Additional directories (repeated flag)
    for (const auto& dir : options_.add_dirs)
    {
        cmd.push_back("--add-dir");
        cmd.push_back(dir);
    }

    // Setting sources (CSV format) - always pass (CLI requires it for proper parsing)
    std::ostringstream oss_sources;
    if (!options_.setting_sources.empty())
    {
        for (size_t i = 0; i < options_.setting_sources.size(); ++i)
        {
            if (i > 0)
                oss_sources << ",";
            oss_sources << options_.setting_sources[i];
        }
    }
    // else: oss_sources remains empty string
    cmd.push_back("--setting-sources");
    cmd.push_back(oss_sources.str()); // May be empty string, which CLI expects

    // Fork session
    if (options_.fork_session)
        cmd.push_back("--fork-session");

    // Agent definitions (JSON format)
    if (!options_.agents.empty())
    {
        json agents_json = json::object();
        for (const auto& [name, def] : options_.agents)
        {
            json agent_obj = json::object();
            agent_obj["description"] = def.description;
            agent_obj["prompt"] = def.prompt;
            if (def.tools.has_value())
                agent_obj["tools"] = *def.tools;
            if (def.model.has_value())
                agent_obj["model"] = *def.model;
            agents_json[name] = agent_obj;
        }
        cmd.push_back("--agents");
        cmd.push_back(agents_json.dump());
    }

    // Plugins
    // Add --plugin-dir for each plugin (only "local" type supported currently)
    for (const auto& plugin : options_.plugins)
    {
        if (plugin.type == "local")
        {
            cmd.push_back("--plugin-dir");
            cmd.push_back(plugin.path);
        }
    }

    // Input mode
    if (is_streaming_)
    {
        cmd.push_back("--input-format");
        cmd.push_back("stream-json");
    }
    else
    {
        // One-shot mode
        cmd.push_back("--print");
        cmd.push_back("--");
        cmd.push_back(prompt_);
    }

    // v0.1.6: Max thinking tokens
    if (options_.max_thinking_tokens)
    {
        cmd.push_back("--max-thinking-tokens");
        cmd.push_back(std::to_string(*options_.max_thinking_tokens));
    }

    return cmd;
}

std::string SubprocessCLITransport::find_cli(const std::optional<std::string>& hint)
{
    const bool require_explicit = options_.require_explicit_cli ||
                                  (std::getenv("CLAUDE_AGENT_SDK_REQUIRE_EXPLICIT_CLI") != nullptr);

    // Helper lambda to validate discovered CLI path
    auto validate_cli_path = [this](const std::string& path) -> std::string
    {
        namespace fs = std::filesystem;

        // Check file exists
        std::error_code ec;
        if (!fs::exists(path, ec) || ec)
            throw CLINotFoundError("CLI path does not exist: " + path);

        // Security: Check allowlist if configured
        if (!claude_agent_sdk::internal::verify_cli_path_allowed(path, options_.allowed_cli_paths))
        {
            throw CLINotFoundError("CLI path not in allowlist: " + path +
                                   ". Configure allowed_cli_paths or use explicit path.");
        }

        // Security: Verify hash if configured
        std::string error_msg;
        if (!claude_agent_sdk::internal::verify_cli_hash(path, options_.cli_hash_sha256, error_msg))
            throw CLINotFoundError("CLI integrity check failed: " + error_msg);

        return path;
    };

    if (hint)
        return validate_cli_path(*hint);

    if (!options_.cli_path.empty())
        return validate_cli_path(options_.cli_path);

    if (const char* env_cli = std::getenv("CLAUDE_CLI_PATH"))
        return validate_cli_path(std::string(env_cli));

    if (require_explicit)
        throw CLINotFoundError("CLAUDE_AGENT_SDK_REQUIRE_EXPLICIT_CLI is set; provide cli_path, "
                               "CLAUDE_CLI_PATH, or explicit find_cli hint");

    // Try PATH first
    if (auto path = subprocess::find_executable("claude"))
        return validate_cli_path(*path);

    // Common locations
    std::vector<std::filesystem::path> locations;

#ifdef _WIN32
    if (const char* appdata = std::getenv("APPDATA"))
        locations.push_back(std::filesystem::path(appdata) / "npm" / "claude.cmd");
    if (const char* localappdata = std::getenv("LOCALAPPDATA"))
        locations.push_back(std::filesystem::path(localappdata) / "npm" / "claude.cmd");
#else
    if (const char* home = std::getenv("HOME"))
    {
        locations.push_back(std::filesystem::path(home) / ".npm-global" / "bin" / "claude");
        locations.push_back(std::filesystem::path(home) / ".local" / "bin" / "claude");
        locations.push_back(std::filesystem::path(home) / ".claude" / "local" / "claude"); // v0.1.8
    }
    locations.push_back("/usr/local/bin/claude");
#endif

    for (const auto& loc : locations)
    {
        std::error_code ec;
        if (std::filesystem::exists(loc, ec) && !ec)
            return validate_cli_path(loc.string());
    }

    throw CLINotFoundError("Claude Code not found. Install with:\n"
                           "  npm install -g @anthropic-ai/claude-code\n");
}

void SubprocessCLITransport::check_cli_version()
{
    // Check Claude Code version - enforces minimum or warns depending on configuration
    // When enforce_version_check is true (default), throws on version mismatch
    const bool should_skip = std::getenv("CLAUDE_AGENT_SDK_SKIP_VERSION_CHECK") != nullptr &&
                             !options_.enforce_version_check;
    if (should_skip)
        return; // Skip check only if explicitly allowed

    try
    {
        subprocess::Process version_proc;
        version_proc.spawn(cli_path_, {"-v"});

        std::string output = version_proc.stdout_pipe().read_line();
        version_proc.wait();

        // Extract first semantic version occurrence from output.
        // Expected formats: "X.Y.Z" or "X.Y.Z (Claude Code)".
        std::smatch match;
        std::regex ver_re(R"((\d+)\.(\d+)\.(\d+))");
        if (std::regex_search(output, match, ver_re) && match.size() >= 4)
        {
            auto to_int = [](const std::ssub_match& s) { return std::stoi(s.str()); };
            int major = to_int(match[1]);
            int minor = to_int(match[2]);
            int patch = to_int(match[3]);

            const int REQ_MAJOR = 2;
            const int REQ_MINOR = 0;
            const int REQ_PATCH = 0;

            auto is_less = [&](int aM, int aN, int aP, int bM, int bN, int bP)
            {
                if (aM != bM)
                    return aM < bM;
                if (aN != bN)
                    return aN < bN;
                return aP < bP;
            };

            if (is_less(major, minor, patch, REQ_MAJOR, REQ_MINOR, REQ_PATCH))
            {
                std::ostringstream oss;
                oss << "Claude Code CLI version " << major << "." << minor << "." << patch
                    << " is too old; require >= " << REQ_MAJOR << "." << REQ_MINOR << "."
                    << REQ_PATCH;

                // Security: Fail hard if enforce_version_check is enabled
                if (options_.enforce_version_check)
                {
                    oss << ". Set enforce_version_check=false to bypass (not recommended).\n"
                        << "Install or upgrade with:\n  npm install -g @anthropic-ai/claude-code\n";
                    throw CLIConnectionError(oss.str());
                }

                // Otherwise, just warn
                std::string warning =
                    "Warning: " + oss.str() + ". Some features may not work correctly.";
                std::cerr << warning << std::endl;
            }
        }
        else
        {
            // If parsing fails, proceed without blocking; the next steps may still succeed.
        }
    }
    catch (const CLIConnectionError&)
    {
        throw; // propagate enforced minimum version error
    }
    catch (const std::exception&)
    {
        // If version check fails due to unexpected errors, proceed; connection will validate later.
    }
}

std::vector<Message> SubprocessCLITransport::read_messages()
{
    if (!process_ || !ready_)
        throw CLIConnectionError("Not connected");

    std::vector<Message> all_messages;

    // Security: Track total bytes read and message count to prevent memory exhaustion
    size_t total_bytes_read = 0;

    // Read until EOF
    // Just call read() directly - it will block until data arrives or EOF
    // Don't use has_data() on Windows - PeekNamedPipe is unreliable
    char buffer[4096];

    while (true)
    {
        // Security: Check if we've exceeded total read limit
        if (total_bytes_read >= options_.max_total_read_bytes)
        {
            throw JSONDecodeError("Exceeded maximum total read bytes of " +
                                  std::to_string(options_.max_total_read_bytes) + " (read " +
                                  std::to_string(total_bytes_read) + " bytes)");
        }

        size_t n = process_->stdout_pipe().read(buffer, sizeof(buffer));

        if (n == 0)
            break; // EOF

        total_bytes_read += n;

        std::string data(buffer, n);
        auto messages = parser_->add_data(data);

        all_messages.insert(all_messages.end(), messages.begin(), messages.end());

        // Security: Check if we've exceeded message count limit
        if (all_messages.size() >= options_.max_messages_per_read)
        {
            throw JSONDecodeError("Exceeded maximum messages per read of " +
                                  std::to_string(options_.max_messages_per_read) +
                                  " (accumulated " + std::to_string(all_messages.size()) +
                                  " messages)");
        }
    }

    // Process any remaining buffered data in parser
    if (parser_->has_buffered_data())
    {
        // Add a newline to flush any partial line
        auto messages = parser_->add_data("\n");

        // Security: Check message limit before adding final messages
        if (all_messages.size() + messages.size() > options_.max_messages_per_read)
        {
            throw JSONDecodeError(
                "Exceeded maximum messages per read of " +
                std::to_string(options_.max_messages_per_read) + " (would accumulate " +
                std::to_string(all_messages.size() + messages.size()) + " messages)");
        }

        all_messages.insert(all_messages.end(), messages.begin(), messages.end());
    }

    // Wait for process to complete
    int exit_code = process_->wait();
    if (exit_code != 0)
    {
        throw ProcessError("Claude Code exited with error code " + std::to_string(exit_code),
                           exit_code);
    }

    return all_messages;
}

void SubprocessCLITransport::write_message(const std::string& json)
{
    if (!process_ || !ready_)
        throw CLIConnectionError("Not connected");

    process_->stdin_pipe().write(json + "\n");
    process_->stdin_pipe().flush();
}

void SubprocessCLITransport::close()
{
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

    // Stop stderr reader thread first
    if (stderr_thread_)
    {
        stderr_stop_flag_ = true;
        if (stderr_thread_->joinable())
            stderr_thread_->join();
        stderr_thread_.reset();
    }

    if (process_)
    {
        if (process_->is_running())
        {
            process_->terminate();
            process_->wait();
        }
        process_.reset();
        ready_ = false;
    }
}

bool SubprocessCLITransport::is_ready() const
{
    return ready_ && process_ && process_->is_running();
}

void SubprocessCLITransport::stderr_reader()
{
    if (!process_ || !options_.stderr_callback.has_value())
        return;

    try
    {
        auto& stderr_pipe = process_->stderr_pipe();
        char buffer[4096];

        while (!stderr_stop_flag_)
        {
            // Check if data is available (with timeout to allow checking stop flag)
            if (!stderr_pipe.has_data(100)) // 100ms timeout
                continue;

            // Read available data
            size_t n = stderr_pipe.read(buffer, sizeof(buffer) - 1);
            if (n == 0)
                break; // EOF

            // Null-terminate and process as string
            buffer[n] = '\0';
            std::string data(buffer, n);

            // Split into lines and invoke callback for each
            size_t start = 0;
            size_t pos = 0;
            while ((pos = data.find('\n', start)) != std::string::npos)
            {
                std::string line = data.substr(start, pos - start);
                if (!line.empty() || pos > start) // Include empty lines if explicit
                {
                    try
                    {
                        (*options_.stderr_callback)(line);
                    }
                    catch (...)
                    {
                        // Ignore callback exceptions to avoid crashing the reader thread
                    }
                }
                start = pos + 1;
            }

            // Handle remaining partial line (if any) - will be processed next iteration
            // For simplicity, we invoke callback for partial lines too
            if (start < data.size())
            {
                std::string line = data.substr(start);
                if (!line.empty())
                {
                    try
                    {
                        (*options_.stderr_callback)(line);
                    }
                    catch (...)
                    {
                        // Ignore callback exceptions
                    }
                }
            }
        }
    }
    catch (...)
    {
        // Ignore errors in stderr reader to avoid crashing
        // Stderr is informational only, not critical for operation
    }
}

} // namespace transport
} // namespace claude
