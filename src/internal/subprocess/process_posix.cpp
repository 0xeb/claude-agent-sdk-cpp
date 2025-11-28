// POSIX implementation of subprocess process management
// For Linux and macOS

#include "process.hpp"

#include <algorithm>
#include <claude/errors.hpp>
#include <cstdlib>
#include <cstring>
#include <errno.h>
#include <fcntl.h>
#include <filesystem>
#include <signal.h>
#include <sstream>
#include <stdexcept>
#include <sys/select.h>
#include <sys/wait.h>
#include <unistd.h>

namespace claude
{
namespace subprocess
{

// ============================================================================
// ProcessHandle - POSIX implementation
// ============================================================================

struct ProcessHandle
{
    pid_t pid = 0;
    bool running = false;
    int exit_code = -1;

    // Pipe file descriptors (stored for cleanup)
    int stdin_fd = -1;
    int stdout_fd = -1;
    int stderr_fd = -1;
};

// ============================================================================
// PipeHandle - POSIX implementation
// ============================================================================

struct PipeHandle
{
    int fd = -1;

    ~PipeHandle()
    {
        if (fd >= 0)
            ::close(fd);
    }
};

// ============================================================================
// Helper functions
// ============================================================================

static std::string get_errno_message()
{
    return std::strerror(errno);
}

static void set_nonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1)
        throw std::runtime_error("fcntl F_GETFL failed: " + get_errno_message());
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
        throw std::runtime_error("fcntl F_SETFL failed: " + get_errno_message());
}

// ============================================================================
// ReadPipe implementation
// ============================================================================

ReadPipe::ReadPipe() : handle_(std::make_unique<PipeHandle>()) {}

ReadPipe::~ReadPipe()
{
    close();
}

ReadPipe::ReadPipe(ReadPipe&&) noexcept = default;
ReadPipe& ReadPipe::operator=(ReadPipe&&) noexcept = default;

size_t ReadPipe::read(char* buffer, size_t size)
{
    if (!is_open())
        throw std::runtime_error("Pipe is not open");

    ssize_t bytes_read = ::read(handle_->fd, buffer, size);
    if (bytes_read < 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return 0; // No data available (non-blocking)
        throw std::runtime_error("Read failed: " + get_errno_message());
    }

    return static_cast<size_t>(bytes_read);
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

    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(handle_->fd, &read_fds);

    struct timeval timeout;
    timeout.tv_sec = timeout_ms / 1000;
    timeout.tv_usec = (timeout_ms % 1000) * 1000;

    int result = select(handle_->fd + 1, &read_fds, nullptr, nullptr, &timeout);
    if (result < 0)
        throw std::runtime_error("select failed: " + get_errno_message());

    return result > 0 && FD_ISSET(handle_->fd, &read_fds);
}

void ReadPipe::close()
{
    if (handle_ && handle_->fd >= 0)
    {
        ::close(handle_->fd);
        handle_->fd = -1;
    }
}

bool ReadPipe::is_open() const
{
    return handle_ && handle_->fd >= 0;
}

// ============================================================================
// WritePipe implementation
// ============================================================================

WritePipe::WritePipe() : handle_(std::make_unique<PipeHandle>()) {}

WritePipe::~WritePipe()
{
    close();
}

WritePipe::WritePipe(WritePipe&&) noexcept = default;
WritePipe& WritePipe::operator=(WritePipe&&) noexcept = default;

size_t WritePipe::write(const char* data, size_t size)
{
    if (!is_open())
        throw std::runtime_error("Pipe is not open");

    ssize_t bytes_written = ::write(handle_->fd, data, size);
    if (bytes_written < 0)
    {
        if (errno == EPIPE)
            throw std::runtime_error("Broken pipe (process closed stdin)");
        throw std::runtime_error("Write failed: " + get_errno_message());
    }

    return static_cast<size_t>(bytes_written);
}

size_t WritePipe::write(const std::string& data)
{
    return write(data.data(), data.size());
}

void WritePipe::flush()
{
    // On POSIX, write() is unbuffered for pipes, but we can use fsync if needed
    // For pipes, fsync is not supported, so this is a no-op
}

void WritePipe::close()
{
    if (handle_ && handle_->fd >= 0)
    {
        ::close(handle_->fd);
        handle_->fd = -1;
    }
}

bool WritePipe::is_open() const
{
    return handle_ && handle_->fd >= 0;
}

// ============================================================================
// Process implementation
// ============================================================================

Process::Process() : handle_(std::make_unique<ProcessHandle>()) {}

Process::~Process()
{
    if (is_running())
    {
        terminate();
        wait();
    }
}

Process::Process(Process&&) noexcept = default;
Process& Process::operator=(Process&&) noexcept = default;

void Process::spawn(const std::string& executable, const std::vector<std::string>& args,
                    const ProcessOptions& options)
{
    // Create pipes for stdin
    int stdin_pipe[2] = {-1, -1};
    if (options.redirect_stdin)
    {
        if (pipe(stdin_pipe) != 0)
            throw std::runtime_error("Failed to create stdin pipe: " + get_errno_message());
    }

    // Create pipes for stdout
    int stdout_pipe[2] = {-1, -1};
    if (options.redirect_stdout)
    {
        if (pipe(stdout_pipe) != 0)
        {
            if (stdin_pipe[0] >= 0)
            {
                ::close(stdin_pipe[0]);
                ::close(stdin_pipe[1]);
            }
            throw std::runtime_error("Failed to create stdout pipe: " + get_errno_message());
        }
    }

    // Create pipes for stderr
    int stderr_pipe[2] = {-1, -1};
    if (options.redirect_stderr)
    {
        if (pipe(stderr_pipe) != 0)
        {
            if (stdin_pipe[0] >= 0)
            {
                ::close(stdin_pipe[0]);
                ::close(stdin_pipe[1]);
            }
            if (stdout_pipe[0] >= 0)
            {
                ::close(stdout_pipe[0]);
                ::close(stdout_pipe[1]);
            }
            throw std::runtime_error("Failed to create stderr pipe: " + get_errno_message());
        }
    }

    // Fork the process
    pid_t pid = fork();
    if (pid < 0)
    {
        // Fork failed - clean up pipes
        if (stdin_pipe[0] >= 0)
        {
            ::close(stdin_pipe[0]);
            ::close(stdin_pipe[1]);
        }
        if (stdout_pipe[0] >= 0)
        {
            ::close(stdout_pipe[0]);
            ::close(stdout_pipe[1]);
        }
        if (stderr_pipe[0] >= 0)
        {
            ::close(stderr_pipe[0]);
            ::close(stderr_pipe[1]);
        }
        throw std::runtime_error("Failed to fork process: " + get_errno_message());
    }

    if (pid == 0)
    {
        // Child process

        // Redirect stdin
        if (options.redirect_stdin)
        {
            ::close(stdin_pipe[1]); // Close write end
            if (dup2(stdin_pipe[0], STDIN_FILENO) < 0)
                _exit(127);
            ::close(stdin_pipe[0]);
        }

        // Redirect stdout
        if (options.redirect_stdout)
        {
            ::close(stdout_pipe[0]); // Close read end
            if (dup2(stdout_pipe[1], STDOUT_FILENO) < 0)
                _exit(127);
            ::close(stdout_pipe[1]);
        }

        // Redirect stderr
        if (options.redirect_stderr)
        {
            ::close(stderr_pipe[0]); // Close read end
            if (dup2(stderr_pipe[1], STDERR_FILENO) < 0)
                _exit(127);
            ::close(stderr_pipe[1]);
        }

        // Change working directory
        if (!options.working_directory.empty())
        {
            if (chdir(options.working_directory.c_str()) != 0)
                _exit(127);
        }

        // Set environment variables
        // Optionally strip inherited environment before applying overrides.
        if (!options.inherit_environment)
        {
#if defined(__linux__) && defined(_GNU_SOURCE)
            clearenv();
#else
            // Fallback: best-effort clearing by resetting environ
            extern char** environ;
            if (environ)
                environ[0] = nullptr;
#endif
        }
        for (const auto& [key, value] : options.environment)
            setenv(key.c_str(), value.c_str(), 1); // Overwrite if exists

        // Build argv array
        std::vector<char*> argv;
        argv.push_back(const_cast<char*>(executable.c_str()));
        for (const auto& arg : args)
            argv.push_back(const_cast<char*>(arg.c_str()));
        argv.push_back(nullptr);

        // Execute the program
        execvp(executable.c_str(), argv.data());

        // If execvp returns, it failed
        _exit(127);
    }

    // Parent process

    // Close unused pipe ends and store handles
    if (options.redirect_stdin)
    {
        ::close(stdin_pipe[0]); // Close read end
        stdin_ = std::make_unique<WritePipe>();
        stdin_->handle_->fd = stdin_pipe[1];
    }

    if (options.redirect_stdout)
    {
        ::close(stdout_pipe[1]); // Close write end
        stdout_ = std::make_unique<ReadPipe>();
        stdout_->handle_->fd = stdout_pipe[0];
    }

    if (options.redirect_stderr)
    {
        ::close(stderr_pipe[1]); // Close write end
        stderr_ = std::make_unique<ReadPipe>();
        stderr_->handle_->fd = stderr_pipe[0];
    }

    // Store process information
    handle_->pid = pid;
    handle_->running = true;
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
    if (!handle_ || handle_->pid == 0)
        return false;

    if (!handle_->running)
        return false;

    // Check process status using kill with signal 0
    int result = ::kill(handle_->pid, 0);
    if (result == 0)
        return true; // Process exists

    if (errno == ESRCH)
        return false; // Process doesn't exist

    // For other errors (EPERM), assume process exists
    return true;
}

std::optional<int> Process::try_wait()
{
    if (!handle_ || handle_->pid == 0)
        return handle_ ? handle_->exit_code : -1;

    if (!handle_->running)
        return handle_->exit_code;

    int status;
    pid_t result = waitpid(handle_->pid, &status, WNOHANG);

    if (result == handle_->pid)
    {
        // Process has exited
        if (WIFEXITED(status))
            handle_->exit_code = WEXITSTATUS(status);
        else if (WIFSIGNALED(status))
            handle_->exit_code = 128 + WTERMSIG(status);
        else
            handle_->exit_code = -1;
        handle_->running = false;
        return handle_->exit_code;
    }
    else if (result == 0)
    {
        // Process is still running
        return std::nullopt;
    }
    else
    {
        // Error occurred
        throw std::runtime_error("waitpid failed: " + get_errno_message());
    }
}

int Process::wait()
{
    if (!handle_ || handle_->pid == 0)
        return handle_ ? handle_->exit_code : -1;

    if (!handle_->running)
        return handle_->exit_code;

    int status;
    pid_t result = waitpid(handle_->pid, &status, 0);

    if (result == handle_->pid)
    {
        if (WIFEXITED(status))
            handle_->exit_code = WEXITSTATUS(status);
        else if (WIFSIGNALED(status))
            handle_->exit_code = 128 + WTERMSIG(status);
        else
            handle_->exit_code = -1;
        handle_->running = false;
        return handle_->exit_code;
    }

    throw std::runtime_error("waitpid failed: " + get_errno_message());
}

void Process::terminate()
{
    if (handle_ && handle_->pid > 0 && handle_->running)
        ::kill(handle_->pid, SIGTERM);
}

void Process::kill()
{
    if (handle_ && handle_->pid > 0 && handle_->running)
        ::kill(handle_->pid, SIGKILL);
}

int Process::pid() const
{
    return handle_ ? static_cast<int>(handle_->pid) : 0;
}

// ============================================================================
// Helper functions
// ============================================================================

std::optional<std::string> find_executable(const std::string& name)
{
    namespace fs = std::filesystem;

    // If it's an absolute path and exists, check if it's executable
    fs::path exe_path(name);
    if (exe_path.is_absolute())
    {
        if (fs::exists(exe_path) && access(exe_path.c_str(), X_OK) == 0)
            return name;
        return std::nullopt;
    }

    // If name contains a path separator, treat as relative path
    if (name.find('/') != std::string::npos)
    {
        if (fs::exists(name) && access(name.c_str(), X_OK) == 0)
            return fs::absolute(name).string();
        return std::nullopt;
    }

    // Search in PATH environment variable
    const char* path_env = std::getenv("PATH");
    if (!path_env)
    {
        // No PATH set - try current directory
        if (fs::exists(name) && access(name.c_str(), X_OK) == 0)
            return fs::absolute(name).string();
        return std::nullopt;
    }

    std::string path_str(path_env);
    size_t start = 0;
    size_t end;

    // Split PATH by colon on POSIX
    while ((end = path_str.find(':', start)) != std::string::npos)
    {
        std::string dir = path_str.substr(start, end - start);
        if (!dir.empty())
        {
            fs::path test_path = fs::path(dir) / name;
            if (fs::exists(test_path) && access(test_path.c_str(), X_OK) == 0)
                return test_path.string();
        }

        start = end + 1;
    }

    // Check last directory
    if (start < path_str.length())
    {
        std::string dir = path_str.substr(start);
        if (!dir.empty())
        {
            fs::path test_path = fs::path(dir) / name;
            if (fs::exists(test_path) && access(test_path.c_str(), X_OK) == 0)
                return test_path.string();
        }
    }

    return std::nullopt;
}

} // namespace subprocess
} // namespace claude
