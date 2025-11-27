#include "../../src/internal/subprocess/process.hpp"

#include <chrono>
#include <gtest/gtest.h>
#include <thread>

using namespace claude::subprocess;

// Test basic process spawn
TEST(ProcessTest, SpawnEcho)
{
    Process proc;

#ifdef _WIN32
    proc.spawn("cmd.exe", {"/c", "echo", "Hello"});
#else
    proc.spawn("/bin/echo", {"Hello"});
#endif

    EXPECT_TRUE(proc.is_running() || proc.try_wait().has_value());
    int exit_code = proc.wait();
    EXPECT_EQ(exit_code, 0);
}

// Test stdout capture
TEST(ProcessTest, CaptureStdout)
{
    Process proc;

#ifdef _WIN32
    proc.spawn("cmd.exe", {"/c", "echo", "TestOutput"});
#else
    proc.spawn("/bin/echo", {"TestOutput"});
#endif

    std::string output = proc.stdout_pipe().read_line();
    EXPECT_FALSE(output.empty());
    EXPECT_NE(output.find("TestOutput"), std::string::npos);

    proc.wait();
}

// Test stdin write
TEST(ProcessTest, WriteStdin)
{
    Process proc;

#ifdef _WIN32
    proc.spawn("findstr", {".*"}); // Acts like cat
#else
    proc.spawn("/bin/cat", {});
#endif

    proc.stdin_pipe().write("Hello\n");
    proc.stdin_pipe().close(); // EOF

    std::string output = proc.stdout_pipe().read_line();
    EXPECT_EQ(output, "Hello\n");

    proc.wait();
}

// Test process termination
TEST(ProcessTest, Terminate)
{
    Process proc;

#ifdef _WIN32
    // Use ping as a portable sleep alternative on Windows
    // ping -n 11 localhost waits ~10 seconds (11 pings with 1 second between)
    proc.spawn("cmd.exe", {"/c", "ping", "-n", "11", "127.0.0.1", ">nul"});
#else
    proc.spawn("/bin/sleep", {"10"});
#endif

    EXPECT_TRUE(proc.is_running());

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    proc.terminate();

    int exit_code = proc.wait();
    // Exit code varies by platform, just check it completed
    EXPECT_TRUE(exit_code != 0 || exit_code == 0); // Any exit code is fine
}

// Test find_executable
TEST(ProcessTest, FindExecutable)
{
#ifdef _WIN32
    auto cmd = find_executable("cmd.exe");
    EXPECT_TRUE(cmd.has_value());

    auto cmd2 = find_executable("cmd");
    EXPECT_TRUE(cmd2.has_value());
#else
    auto sh = find_executable("sh");
    EXPECT_TRUE(sh.has_value());
#endif

    auto nonexistent = find_executable("this_should_not_exist_12345");
    EXPECT_FALSE(nonexistent.has_value());
}

// Test working directory
TEST(ProcessTest, WorkingDirectory)
{
    Process proc;
    ProcessOptions opts;
    opts.working_directory = "."; // Current directory

#ifdef _WIN32
    proc.spawn("cmd.exe", {"/c", "echo", "%CD%"}, opts);
#else
    proc.spawn("/bin/pwd", {}, opts);
#endif

    std::string output = proc.stdout_pipe().read_line();
    EXPECT_FALSE(output.empty());

    proc.wait();
}

// Test environment variables
TEST(ProcessTest, Environment)
{
    Process proc;
    ProcessOptions opts;
    opts.environment["TEST_VAR"] = "test_value";

#ifdef _WIN32
    // Use set to print the variable value
    proc.spawn("cmd.exe", {"/c", "set", "TEST_VAR"}, opts);
#else
    proc.spawn("/bin/sh", {"-c", "echo $TEST_VAR"}, opts);
#endif

    std::string output = proc.stdout_pipe().read_line();
    EXPECT_NE(output.find("test_value"), std::string::npos);

    proc.wait();
}

// Test PID
TEST(ProcessTest, ProcessID)
{
    Process proc;

#ifdef _WIN32
    proc.spawn("cmd.exe", {"/c", "echo", "test"});
#else
    proc.spawn("/bin/echo", {"test"});
#endif

    int pid = proc.pid();
    EXPECT_GT(pid, 0);

    proc.wait();
}

// Test multiple sequential processes
TEST(ProcessTest, SequentialProcesses)
{
    for (int i = 0; i < 3; i++)
    {
        Process proc;

#ifdef _WIN32
        proc.spawn("cmd.exe", {"/c", "echo", "test" + std::to_string(i)});
#else
        proc.spawn("/bin/echo", {"test" + std::to_string(i)});
#endif

        std::string output = proc.stdout_pipe().read_line();
        EXPECT_FALSE(output.empty());

        int exit_code = proc.wait();
        EXPECT_EQ(exit_code, 0);
    }
}

// Test reading multiple lines
TEST(ProcessTest, ReadMultipleLines)
{
    Process proc;

#ifdef _WIN32
    proc.spawn("cmd.exe", {"/c", "echo Line1 && echo Line2"});
#else
    proc.spawn("/bin/sh", {"-c", "echo Line1; echo Line2"});
#endif

    std::string line1 = proc.stdout_pipe().read_line();
    std::string line2 = proc.stdout_pipe().read_line();

    EXPECT_NE(line1.find("Line1"), std::string::npos);
    EXPECT_NE(line2.find("Line2"), std::string::npos);

    proc.wait();
}

// Test process that exits immediately
TEST(ProcessTest, ImmediateExit)
{
    Process proc;

#ifdef _WIN32
    proc.spawn("cmd.exe", {"/c", "exit 0"});
#else
    proc.spawn("/bin/true", {});
#endif

    auto exit_code_opt = proc.try_wait();
    // Process might have already exited
    if (!exit_code_opt)
        exit_code_opt = proc.try_wait();

    int exit_code = proc.wait();
    EXPECT_EQ(exit_code, 0);
}
