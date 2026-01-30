#ifdef _WIN32

#include "process.hpp"

#include <algorithm>
#include <filesystem>
#include <sstream>
#include <stdexcept>
#include <windows.h>

namespace claude
{
namespace subprocess
{

// Platform-specific handle structures
struct PipeHandle
{
    HANDLE handle = INVALID_HANDLE_VALUE;

    ~PipeHandle()
    {
        if (handle != INVALID_HANDLE_VALUE)
            CloseHandle(handle);
    }
};

struct ProcessHandle
{
    HANDLE process_handle = INVALID_HANDLE_VALUE;
    HANDLE thread_handle = INVALID_HANDLE_VALUE;
    DWORD process_id = 0;
    bool running = false;
    int exit_code = -1;

    ~ProcessHandle()
    {
        if (thread_handle != INVALID_HANDLE_VALUE)
            CloseHandle(thread_handle);
        if (process_handle != INVALID_HANDLE_VALUE)
            CloseHandle(process_handle);
    }
};
// =============================================================================
// Job Object for child process cleanup
// =============================================================================

// Singleton job object that kills all child processes when parent exits.
// Using JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE ensures spawned servers die
// when the client process terminates (even abnormally).
static HANDLE get_child_process_job()
{
    static HANDLE job = []() -> HANDLE {
        HANDLE h = CreateJobObjectA(nullptr, nullptr);
        if (h)
        {
            JOBOBJECT_EXTENDED_LIMIT_INFORMATION info = {};
            info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
            SetInformationJobObject(h, JobObjectExtendedLimitInformation, &info, sizeof(info));
        }
        return h;
    }();
    return job;
}

// Helper function to create error messages
static std::string get_last_error_message()
{
    DWORD error = GetLastError();
    if (error == 0)
        return "No error";

    LPSTR buffer = nullptr;
    size_t size = FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&buffer, 0, nullptr);

    std::string message(buffer, size);
    LocalFree(buffer);
    return message;
}

// Helper function to quote argument if needed
static std::string quote_if_needed(const std::string& arg)
{
    // Check if argument needs quoting (contains spaces or special chars)
    // On Windows, many characters are special in cmd.exe and need quoting
    bool needs_quotes = arg.empty();
    if (!needs_quotes)
    {
        for (char c : arg)
        {
            // Space, tab, and cmd.exe special characters
            if (c == ' ' || c == '\t' || c == '"' || c == '{' || c == '}' || c == '(' || c == ')' ||
                c == '[' || c == ']' || c == '<' || c == '>' || c == '|' || c == '&' || c == '^' ||
                c == '%' || c == '!' || c == ',' || c == ';' || c == '=' || c == ':')
            {
                needs_quotes = true;
                break;
            }
        }
    }

    if (!needs_quotes)
        return arg;

    // Quote and escape
    std::string result = "\"";
    for (char c : arg)
        if (c == '"')
            result += "\\\"";
        else if (c == '\\')
            result += "\\\\";
        else
            result += c;
    result += "\"";
    return result;
}

// ReadPipe implementation
ReadPipe::ReadPipe() : handle_(std::make_unique<PipeHandle>()) {}

ReadPipe::~ReadPipe()
{
    close();
}

ReadPipe::ReadPipe(ReadPipe&& other) noexcept = default;
ReadPipe& ReadPipe::operator=(ReadPipe&& other) noexcept = default;

size_t ReadPipe::read(char* buffer, size_t size)
{
    if (!is_open())
        throw std::runtime_error("Pipe is not open");

    DWORD bytes_read = 0;
    BOOL success =
        ReadFile(handle_->handle, buffer, static_cast<DWORD>(size), &bytes_read, nullptr);

    if (!success)
    {
        DWORD error = GetLastError();
        if (error == ERROR_BROKEN_PIPE)
            return 0; // EOF
        throw std::runtime_error("Read failed: " + get_last_error_message());
    }

    return bytes_read;
}

std::string ReadPipe::read_line(size_t max_size)
{
    std::string line;
    line.reserve(256);

    char ch;
    while (line.size() < max_size)
    {
        size_t bytes_read = read(&ch, 1);
        if (bytes_read == 0)
            break; // EOF

        line.push_back(ch);
        if (ch == '\n')
            break;
    }

    return line;
}

bool ReadPipe::has_data(int timeout_ms)
{
    if (!is_open())
        return false;

    DWORD bytes_available = 0;
    BOOL success = PeekNamedPipe(handle_->handle, nullptr, 0, nullptr, &bytes_available, nullptr);

    if (!success)
        return false;
    return bytes_available > 0;
}

void ReadPipe::close()
{
    if (handle_ && handle_->handle != INVALID_HANDLE_VALUE)
    {
        CloseHandle(handle_->handle);
        handle_->handle = INVALID_HANDLE_VALUE;
    }
}

bool ReadPipe::is_open() const
{
    return handle_ && handle_->handle != INVALID_HANDLE_VALUE;
}

// WritePipe implementation
WritePipe::WritePipe() : handle_(std::make_unique<PipeHandle>()) {}

WritePipe::~WritePipe()
{
    close();
}

WritePipe::WritePipe(WritePipe&& other) noexcept = default;
WritePipe& WritePipe::operator=(WritePipe&& other) noexcept = default;

size_t WritePipe::write(const char* data, size_t size)
{
    if (!is_open())
        throw std::runtime_error("Pipe is not open");

    DWORD bytes_written = 0;
    BOOL success =
        WriteFile(handle_->handle, data, static_cast<DWORD>(size), &bytes_written, nullptr);

    if (!success)
        throw std::runtime_error("Write failed: " + get_last_error_message());

    return bytes_written;
}

size_t WritePipe::write(const std::string& data)
{
    return write(data.data(), data.size());
}

void WritePipe::flush()
{
    if (is_open())
        FlushFileBuffers(handle_->handle);
}

void WritePipe::close()
{
    if (handle_ && handle_->handle != INVALID_HANDLE_VALUE)
    {
        CloseHandle(handle_->handle);
        handle_->handle = INVALID_HANDLE_VALUE;
    }
}

bool WritePipe::is_open() const
{
    return handle_ && handle_->handle != INVALID_HANDLE_VALUE;
}

// Process implementation
Process::Process() : handle_(std::make_unique<ProcessHandle>()) {}

Process::~Process()
{
    if (is_running())
    {
        terminate();
        wait();
    }
}

Process::Process(Process&& other) noexcept = default;
Process& Process::operator=(Process&& other) noexcept = default;

void Process::spawn(const std::string& executable, const std::vector<std::string>& args,
                    const ProcessOptions& options)
{
    // Create security attributes for inheritable handles
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = nullptr;

    // Create pipes for stdin
    HANDLE stdin_read = INVALID_HANDLE_VALUE;
    HANDLE stdin_write = INVALID_HANDLE_VALUE;
    if (options.redirect_stdin)
    {
        if (!CreatePipe(&stdin_read, &stdin_write, &sa, 0))
            throw std::runtime_error("Failed to create stdin pipe: " + get_last_error_message());
        // Ensure write handle is not inherited
        SetHandleInformation(stdin_write, HANDLE_FLAG_INHERIT, 0);

        stdin_ = std::make_unique<WritePipe>();
        stdin_->handle_->handle = stdin_write;
    }

    // Create pipes for stdout
    HANDLE stdout_read = INVALID_HANDLE_VALUE;
    HANDLE stdout_write = INVALID_HANDLE_VALUE;
    if (options.redirect_stdout)
    {
        if (!CreatePipe(&stdout_read, &stdout_write, &sa, 0))
            throw std::runtime_error("Failed to create stdout pipe: " + get_last_error_message());
        // Ensure read handle is not inherited
        SetHandleInformation(stdout_read, HANDLE_FLAG_INHERIT, 0);

        stdout_ = std::make_unique<ReadPipe>();
        stdout_->handle_->handle = stdout_read;
    }

    // Create pipes for stderr
    HANDLE stderr_read = INVALID_HANDLE_VALUE;
    HANDLE stderr_write = INVALID_HANDLE_VALUE;
    if (options.redirect_stderr)
    {
        if (!CreatePipe(&stderr_read, &stderr_write, &sa, 0))
            throw std::runtime_error("Failed to create stderr pipe: " + get_last_error_message());
        // Ensure read handle is not inherited
        SetHandleInformation(stderr_read, HANDLE_FLAG_INHERIT, 0);

        stderr_ = std::make_unique<ReadPipe>();
        stderr_->handle_->handle = stderr_read;
    }

    // Build command line
    // Note: Batch files (.cmd/.bat) should be handled by caller
    // They should convert to direct executable calls (e.g., node.exe cli.js)
    std::ostringstream cmdline;
    cmdline << quote_if_needed(executable);

    for (const auto& arg : args)
        cmdline << " " << quote_if_needed(arg);
    std::string cmdline_str = cmdline.str();

    // Build environment block
    // If inherit_environment is false, do not pull parent environment; use only provided vars.
    std::string env_block;
    bool provide_env_block = !options.inherit_environment || !options.environment.empty();
    if (provide_env_block)
    {
        // Start with either inherited environment (if allowed) or an empty map.
        std::map<std::string, std::string> env_map;

        if (options.inherit_environment)
        {
            char* env_strings = GetEnvironmentStringsA();
            if (env_strings)
            {
                char* current = env_strings;
                while (*current != '\0')
                {
                    std::string entry(current);
                    size_t eq_pos = entry.find('=');
                    if (eq_pos != std::string::npos && eq_pos > 0)
                    {
                        std::string key = entry.substr(0, eq_pos);
                        std::string value = entry.substr(eq_pos + 1);
                        env_map[key] = value;
                    }
                    current += entry.length() + 1;
                }
                FreeEnvironmentStringsA(env_strings);
            }
        }

        // Merge with custom environment (custom variables override inherited when present)
        for (const auto& [key, value] : options.environment)
            env_map[key] = value;

        if (env_map.empty())
        {
            // Empty block still needs double-null terminator to disable inheritance
            env_block.push_back('\0');
        }
        else
        {
            for (const auto& [key, value] : env_map)
                env_block += key + "=" + value + '\0';
            env_block += '\0';
        }
    }

    // Create null handle for stderr if not redirecting
    // This completely isolates the subprocess from the parent's console
    HANDLE null_handle = INVALID_HANDLE_VALUE;
    if (!options.redirect_stderr)
    {
        SECURITY_ATTRIBUTES null_sa;
        null_sa.nLength = sizeof(SECURITY_ATTRIBUTES);
        null_sa.bInheritHandle = TRUE;
        null_sa.lpSecurityDescriptor = nullptr;
        null_handle = CreateFileA("NUL", GENERIC_WRITE, FILE_SHARE_WRITE, &null_sa, OPEN_EXISTING,
                                  FILE_ATTRIBUTE_NORMAL, nullptr);
    }

    // Build list of handles to inherit
    std::vector<HANDLE> handles_to_inherit;
    if (stdin_read != INVALID_HANDLE_VALUE)
        handles_to_inherit.push_back(stdin_read);
    if (stdout_write != INVALID_HANDLE_VALUE)
        handles_to_inherit.push_back(stdout_write);
    if (stderr_write != INVALID_HANDLE_VALUE)
        handles_to_inherit.push_back(stderr_write);
    else if (null_handle != INVALID_HANDLE_VALUE)
        handles_to_inherit.push_back(null_handle);

    // Setup STARTUPINFOEX with explicit handle list
    // This prevents inheriting ALL handles which can cause "bad file descriptor" errors
    STARTUPINFOEXA si;
    ZeroMemory(&si, sizeof(si));
    si.StartupInfo.cb = sizeof(si);
    si.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    si.StartupInfo.hStdInput = options.redirect_stdin ? stdin_read : GetStdHandle(STD_INPUT_HANDLE);
    si.StartupInfo.hStdOutput =
        options.redirect_stdout ? stdout_write : GetStdHandle(STD_OUTPUT_HANDLE);
    si.StartupInfo.hStdError =
        options.redirect_stderr
            ? stderr_write
            : (null_handle != INVALID_HANDLE_VALUE ? null_handle : GetStdHandle(STD_ERROR_HANDLE));

    // Initialize the attribute list for handle inheritance
    SIZE_T attr_size = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &attr_size);
    si.lpAttributeList = (LPPROC_THREAD_ATTRIBUTE_LIST)HeapAlloc(GetProcessHeap(), 0, attr_size);
    if (!si.lpAttributeList)
        throw std::runtime_error("Failed to allocate attribute list");

    if (!InitializeProcThreadAttributeList(si.lpAttributeList, 1, 0, &attr_size))
    {
        HeapFree(GetProcessHeap(), 0, si.lpAttributeList);
        throw std::runtime_error("Failed to init attribute list: " + get_last_error_message());
    }

    // Only specify handle list if we have handles to inherit
    if (!handles_to_inherit.empty())
    {
        if (!UpdateProcThreadAttribute(
                si.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, handles_to_inherit.data(),
                handles_to_inherit.size() * sizeof(HANDLE), nullptr, nullptr))
        {
            DeleteProcThreadAttributeList(si.lpAttributeList);
            HeapFree(GetProcessHeap(), 0, si.lpAttributeList);
            throw std::runtime_error("Failed to update attribute list: " +
                                     get_last_error_message());
        }
    }

    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));

    // Create the process with:
    // - EXTENDED_STARTUPINFO_PRESENT: for explicit handle list (prevents bad FD errors)
    // - CREATE_NO_WINDOW: prevents console window popup when launched from GUI apps
    DWORD creation_flags = EXTENDED_STARTUPINFO_PRESENT | CREATE_NO_WINDOW;

    BOOL success = CreateProcessA(
        nullptr,                                // Application name
        const_cast<char*>(cmdline_str.c_str()), // Command line
        nullptr,                                // Process security attributes
        nullptr,                                // Thread security attributes
        TRUE,                                   // Inherit handles (only those in the list)
        creation_flags,                         // Creation flags
        provide_env_block ? const_cast<char*>(env_block.data()) : nullptr, // Environment
        options.working_directory.empty() ? nullptr : options.working_directory.c_str(),
        (LPSTARTUPINFOA)&si, // Startup info (cast to STARTUPINFOA)
        &pi                  // Process information
    );

    // Clean up attribute list
    DeleteProcThreadAttributeList(si.lpAttributeList);
    HeapFree(GetProcessHeap(), 0, si.lpAttributeList);

    // Close child pipe handles in parent
    if (stdin_read != INVALID_HANDLE_VALUE)
        CloseHandle(stdin_read);
    if (stdout_write != INVALID_HANDLE_VALUE)
        CloseHandle(stdout_write);
    if (stderr_write != INVALID_HANDLE_VALUE)
        CloseHandle(stderr_write);
    if (null_handle != INVALID_HANDLE_VALUE)
        CloseHandle(null_handle);

    if (!success)
        throw std::runtime_error("Failed to create process: " + get_last_error_message());

    // Store process information
    handle_->process_handle = pi.hProcess;
    handle_->thread_handle = pi.hThread;
    handle_->process_id = pi.dwProcessId;
    handle_->running = true;

    // Assign to job object so child dies when parent dies
    HANDLE job = get_child_process_job();
    if (job)
        AssignProcessToJobObject(job, pi.hProcess);
}

WritePipe& Process::stdin_pipe()
{
    if (!stdin_)
        throw std::runtime_error("stdin not redirected");
    return *stdin_;
}

ReadPipe& Process::stdout_pipe()
{
    if (!stdout_)
        throw std::runtime_error("stdout not redirected");
    return *stdout_;
}

ReadPipe& Process::stderr_pipe()
{
    if (!stderr_)
        throw std::runtime_error("stderr not redirected");
    return *stderr_;
}

bool Process::is_running() const
{
    if (!handle_->running)
        return false;

    DWORD exit_code;
    if (GetExitCodeProcess(handle_->process_handle, &exit_code))
        return exit_code == STILL_ACTIVE;
    return false;
}

std::optional<int> Process::try_wait()
{
    if (!handle_->running)
        return handle_->exit_code;

    DWORD result = WaitForSingleObject(handle_->process_handle, 0);
    if (result == WAIT_OBJECT_0)
    {
        DWORD exit_code;
        if (GetExitCodeProcess(handle_->process_handle, &exit_code))
        {
            handle_->exit_code = static_cast<int>(exit_code);
            handle_->running = false;
            return handle_->exit_code;
        }
    }

    return std::nullopt;
}

int Process::wait()
{
    if (!handle_->running)
        return handle_->exit_code;

    WaitForSingleObject(handle_->process_handle, INFINITE);

    DWORD exit_code;
    if (GetExitCodeProcess(handle_->process_handle, &exit_code))
        handle_->exit_code = static_cast<int>(exit_code);
    else
        handle_->exit_code = -1;

    handle_->running = false;
    return handle_->exit_code;
}

void Process::terminate()
{
    if (handle_->running && handle_->process_handle != INVALID_HANDLE_VALUE)
        TerminateProcess(handle_->process_handle, 1);
}

void Process::kill()
{
    terminate(); // On Windows, terminate and kill are the same
}

int Process::pid() const
{
    return static_cast<int>(handle_->process_id);
}

// Helper function to find executable in PATH
std::optional<std::string> find_executable(const std::string& name)
{
    namespace fs = std::filesystem;

    // If it's an absolute path and exists, return it
    fs::path exe_path(name);
    if (exe_path.is_absolute() && fs::exists(exe_path))
        return name;

    // Try with extensions on Windows - prefer .cmd/.bat over no extension
    // This is important for npm-installed tools which have both "claude" and "claude.cmd"
    std::vector<std::string> extensions = {".cmd", ".bat", ".exe", ""};

    // Check in current directory first
    for (const auto& ext : extensions)
    {
        fs::path test_path = name + ext;
        if (fs::exists(test_path))
            return test_path.string();
    }

    // Get PATH environment variable
    const char* path_env = std::getenv("PATH");
    if (!path_env)
        return std::nullopt;

    std::string path_str(path_env);
    size_t start = 0;
    size_t end;

    // Split PATH by semicolon on Windows
    while ((end = path_str.find(';', start)) != std::string::npos)
    {
        std::string dir = path_str.substr(start, end - start);

        for (const auto& ext : extensions)
        {
            fs::path test_path = fs::path(dir) / (name + ext);
            if (fs::exists(test_path))
                return test_path.string();
        }

        start = end + 1;
    }

    // Check last directory
    if (start < path_str.length())
    {
        std::string dir = path_str.substr(start);
        for (const auto& ext : extensions)
        {
            fs::path test_path = fs::path(dir) / (name + ext);
            if (fs::exists(test_path))
                return test_path.string();
        }
    }

    return std::nullopt;
}

} // namespace subprocess
} // namespace claude

#endif // _WIN32
