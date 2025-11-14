#ifndef CLAUDE_SUBPROCESS_PROCESS_HPP
#define CLAUDE_SUBPROCESS_PROCESS_HPP

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace claude
{
namespace subprocess
{

// Forward declarations for platform-specific types
struct ProcessHandle;
struct PipeHandle;

// Pipe for reading from subprocess
class ReadPipe
{
  public:
    ReadPipe();
    ~ReadPipe();

    // No copy, move only
    ReadPipe(const ReadPipe&) = delete;
    ReadPipe& operator=(const ReadPipe&) = delete;
    ReadPipe(ReadPipe&&) noexcept;
    ReadPipe& operator=(ReadPipe&&) noexcept;

    // Read up to size bytes, returns actual bytes read
    // Returns 0 on EOF, throws on error
    size_t read(char* buffer, size_t size);

    // Read a line (up to newline or size)
    std::string read_line(size_t max_size = 4096);

    // Check if data is available without blocking
    bool has_data(int timeout_ms = 0);

    // Close the pipe
    void close();

    // Check if pipe is open
    bool is_open() const;

  private:
    friend class Process;
    std::unique_ptr<PipeHandle> handle_;
};

// Pipe for writing to subprocess
class WritePipe
{
  public:
    WritePipe();
    ~WritePipe();

    // No copy, move only
    WritePipe(const WritePipe&) = delete;
    WritePipe& operator=(const WritePipe&) = delete;
    WritePipe(WritePipe&&) noexcept;
    WritePipe& operator=(WritePipe&&) noexcept;

    // Write data to pipe
    size_t write(const char* data, size_t size);

    // Write string to pipe
    size_t write(const std::string& data);

    // Flush write buffer
    void flush();

    // Close the pipe
    void close();

    // Check if pipe is open
    bool is_open() const;

  private:
    friend class Process;
    std::unique_ptr<PipeHandle> handle_;
};

// Process configuration
struct ProcessOptions
{
    std::string working_directory;
    std::map<std::string, std::string> environment;
    bool redirect_stdin = true;
    bool redirect_stdout = true;
    bool redirect_stderr = false;
};

// Main Process class
class Process
{
  public:
    Process();
    ~Process();

    // No copy, move only
    Process(const Process&) = delete;
    Process& operator=(const Process&) = delete;
    Process(Process&&) noexcept;
    Process& operator=(Process&&) noexcept;

    // Spawn a process
    void spawn(const std::string& executable, const std::vector<std::string>& args,
               const ProcessOptions& options = {});

    // Get pipes (only valid if redirected)
    WritePipe& stdin_pipe();
    ReadPipe& stdout_pipe();
    ReadPipe& stderr_pipe();

    // Process control
    bool is_running() const;
    std::optional<int> try_wait(); // Non-blocking wait, returns exit code if done
    int wait();                    // Blocking wait, returns exit code
    void terminate();              // Graceful termination (SIGTERM/close)
    void kill();                   // Forceful kill (SIGKILL/TerminateProcess)

    // Process ID
    int pid() const;

  private:
    std::unique_ptr<ProcessHandle> handle_;
    std::unique_ptr<WritePipe> stdin_;
    std::unique_ptr<ReadPipe> stdout_;
    std::unique_ptr<ReadPipe> stderr_;
};

// Helper function to find executable in PATH
std::optional<std::string> find_executable(const std::string& name);

} // namespace subprocess
} // namespace claude

#endif // CLAUDE_SUBPROCESS_PROCESS_HPP
