# Troubleshooting Guide

This guide helps you resolve common issues when using the Claude SDK for C++.

## Table of Contents

- [Installation Issues](#installation-issues)
- [Build Errors](#build-errors)
- [Runtime Errors](#runtime-errors)
- [CLI Communication Issues](#cli-communication-issues)
- [Platform-Specific Issues](#platform-specific-issues)
- [Performance Issues](#performance-issues)
- [Getting Help](#getting-help)

## Installation Issues

### Claude CLI Not Found

**Error:**
```
CLINotFoundError: Could not find 'claude' executable in PATH
```

**Solutions:**

1. **Install Claude Code CLI:**
   ```bash
   npm install -g @anthropic-ai/claude-code
   ```

2. **Verify installation:**
   ```bash
   claude -v
   # Should show version number
   ```

3. **Check PATH:**
   ```bash
   # Linux/macOS
   which claude
   echo $PATH

   # Windows
   where claude
   echo %PATH%
   ```

4. **Add npm global bin to PATH:**
   ```bash
   # Linux/macOS
   export PATH="$HOME/.npm-global/bin:$PATH"

   # Windows
   setx PATH "%PATH%;%APPDATA%\npm"
   ```

### vcpkg Not Found

**Error:**
```
CMake Error: VCPKG_ROOT not set
```

**Solutions:**

1. **Install vcpkg:**
   ```bash
   git clone https://github.com/Microsoft/vcpkg.git
   cd vcpkg
   ./bootstrap-vcpkg.sh  # or .bat on Windows
   ```

2. **Set environment variable:**
   ```bash
   # Linux/macOS
   export VCPKG_ROOT=/path/to/vcpkg

   # Windows (PowerShell)
   $env:VCPKG_ROOT="C:\path\to\vcpkg"

   # Windows (permanently)
   setx VCPKG_ROOT "C:\path\to\vcpkg"
   ```

3. **Or specify in CMake command:**
   ```bash
   cmake -B build -DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake
   ```

## Build Errors

### nlohmann/json Not Found

**Error:**
```
fatal error: nlohmann/json.hpp: No such file or directory
```

**Solutions:**

1. **Install via vcpkg:**
   ```bash
   $VCPKG_ROOT/vcpkg install nlohmann-json
   ```

2. **Ensure vcpkg toolchain is specified:**
   ```bash
   cmake -B build -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake
   ```

3. **Try using a preset:**
   ```bash
   cmake --preset windows  # or linux/macos
   ```

### Google Test Not Found

**Error:**
```
Could not find GTest
```

**Solution:**
```bash
$VCPKG_ROOT/vcpkg install gtest
cmake -B build -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake
```

### C++17 Not Supported

**Error:**
```
error: 'std::optional' has not been declared
```

**Solutions:**

1. **Update compiler:**
   - MSVC 19.14+ (Visual Studio 2017 Update 7 or later)
   - GCC 7+
   - Clang 5+

2. **Check CMake standard:**
   ```cmake
   set(CMAKE_CXX_STANDARD 17)
   set(CMAKE_CXX_STANDARD_REQUIRED ON)
   ```

3. **Verify compiler version:**
   ```bash
   # GCC
   g++ --version

   # Clang
   clang++ --version

   # MSVC
   cl  # Shows version in banner
   ```

### Link Errors

**Error:**
```
undefined reference to claude::query
```

**Solutions:**

1. **Link against claude_sdk:**
   ```cmake
   target_link_libraries(your_app PRIVATE claude_sdk)
   ```

2. **Check library was built:**
   ```bash
   # Should exist
   ls build-win/Release/claude_sdk.lib  # Windows
   ls build-lnx/libclaude_sdk.a         # Linux
   ```

3. **Rebuild from clean state:**
   ```bash
   rm -rf build-*
   cmake --preset windows
   cmake --build build-win --config Release
   ```

## Runtime Errors

### Process Hangs or Deadlocks

**Symptoms:**
- Program freezes
- No response from Claude
- Must kill process

**Solutions:**

1. **Always call receive_messages() after send_query():**
   ```cpp
   client.send_query("Hello");
   // MUST call receive_messages() or receive_response()
   for (const auto& msg : client.receive_messages()) {
       // Process messages
   }
   ```

2. **Check for is_result_message to know when to stop:**
   ```cpp
   for (const auto& msg : client.receive_messages()) {
       if (claude::is_result_message(msg)) {
           break;  // End of response
       }
   }
   ```

3. **Always disconnect when done:**
   ```cpp
   client.disconnect();  // Clean shutdown
   ```

4. **Use RAII pattern:**
   ```cpp
   {
       claude::ClaudeClient client(opts);
       client.connect();
       // Use client
   }  // Automatically disconnects
   ```

### Process Exits Immediately

**Error:**
```
ProcessError: Command failed with exit code 1
```

**Solutions:**

1. **Check permission mode:**
   ```cpp
   opts.permission_mode = "bypassPermissions";  // For scripts
   ```

2. **Verify working directory exists:**
   ```cpp
   opts.working_directory = "/valid/path";
   ```

3. **Check CLI output:**
   ```bash
   claude --help  # Should work
   claude --print "test"  # Test basic functionality
   ```

4. **Enable verbose mode for debugging:**
   ```cpp
   opts.extra_args["verbose"] = "";  // Becomes --verbose
   ```

### JSON Parsing Errors

**Error:**
```
JSONDecodeError: Failed to parse message
```

**Solutions:**

1. **Update Claude CLI:**
   ```bash
   npm update -g @anthropic-ai/claude-code
   claude -v  # Check version is 2.0.0+
   ```

2. **Check for binary output in pipes:**
   - Ensure working directory is valid
   - Check for stdout/stderr mixing

3. **Enable partial message mode:**
   ```cpp
   opts.include_partial_messages = false;  // More stable parsing
   ```

### Memory Leaks

**Detection:**

**Linux (Valgrind):**
```bash
valgrind --leak-check=full --show-leak-kinds=all ./your_app
```

**Address Sanitizer:**
```bash
cmake -DCMAKE_CXX_FLAGS="-fsanitize=address -g" ..
cmake --build .
./your_app
```

**Common causes:**

1. **Not calling disconnect():**
   ```cpp
   client.disconnect();  // Always clean up
   ```

2. **Exceptions during cleanup:**
   ```cpp
   try {
       client.connect();
       // Use client
   } catch (...) {
       client.disconnect();  // Clean up even on error
       throw;
   }
   client.disconnect();
   ```

## CLI Communication Issues

### Version Check Failures

**Error:**
```
Failed to verify Claude CLI version
```

**Solutions:**

1. **Skip version check (development/testing only):**
   ```bash
   # Linux/macOS
   export CLAUDE_AGENT_SDK_SKIP_VERSION_CHECK=1
   ./your_app

   # Windows (PowerShell)
   $env:CLAUDE_AGENT_SDK_SKIP_VERSION_CHECK=1
   .\your_app.exe

   # Windows (CMD)
   set CLAUDE_AGENT_SDK_SKIP_VERSION_CHECK=1
   your_app.exe
   ```

2. **Or set from code (C++17):**
   ```cpp
   #include <cstdlib>

   // Before creating ClaudeClient
   #ifdef _WIN32
       _putenv("CLAUDE_AGENT_SDK_SKIP_VERSION_CHECK=1");
   #else
       setenv("CLAUDE_AGENT_SDK_SKIP_VERSION_CHECK", "1", 1);
   #endif
   ```

3. **Update Claude CLI:**
   ```bash
   npm update -g @anthropic-ai/claude-code
   claude -v  # Verify it works
   ```

**Note:** Skipping version check should only be used for development/testing. In production, ensure Claude CLI is properly installed and accessible.

### Permission Denied Errors

**Error:**
```
Tool permission denied
```

**Solutions:**

1. **Use bypass mode for scripts:**
   ```cpp
   opts.permission_mode = "bypassPermissions";
   ```

2. **Or handle permissions programmatically:**
   ```cpp
   opts.tool_permission_callback = [](
       const std::string& tool_name,
       const claude::json& input,
       const claude::ToolPermissionContext& context) -> claude::PermissionResult
   {
       return claude::PermissionResultAllow{};
   };
   ```

3. **Check allowed tools list:**
   ```cpp
   opts.allowed_tools = {"Read", "Write", "Bash"};
   // Make sure needed tools are in the list
   ```

### Timeout Errors

**Error:**
```
Control request timed out
```

**Solutions:**

1. **Increase max_turns:**
   ```cpp
   opts.max_turns = 50;  // Default is 25
   ```

2. **Use simpler prompts:**
   - Break complex tasks into steps
   - Reduce context size

3. **Check system resources:**
   - High CPU usage
   - Low memory
   - Network issues (if CLI uses network)

## Platform-Specific Issues

### Windows

**Issue: Long paths not supported**

**Solution:**
Enable long path support in Windows 10+:
```powershell
# Run as Administrator
New-ItemProperty -Path "HKLM:\SYSTEM\CurrentControlSet\Control\FileSystem" `
    -Name "LongPathsEnabled" -Value 1 -PropertyType DWORD -Force
```

**Issue: Antivirus blocking subprocess**

**Solution:**
- Add exception for claude.exe
- Add exception for your application
- Temporarily disable for testing

### Linux

**Issue: Permission denied on subprocess**

**Solution:**
```bash
chmod +x $(which claude)
```

**Issue: Missing libraries**

**Solution:**
```bash
# Install dependencies
sudo apt-get install build-essential cmake

# Check missing libraries
ldd ./your_app
```

### macOS

**Issue: Unsigned binary blocked**

**Solution:**
```bash
# Allow unsigned binaries
xattr -d com.apple.quarantine $(which claude)
```

**Issue: Xcode command line tools missing**

**Solution:**
```bash
xcode-select --install
```

## Performance Issues

### Slow Response Times

**Causes & Solutions:**

1. **Large context:**
   - Use `max_turns` to limit history
   - Break into smaller conversations

2. **Too many tools enabled:**
   - Limit with `allowed_tools`
   - Disable unused tools with `disallowed_tools`

3. **System prompt too long:**
   - Keep system prompts concise
   - Move examples to few-shot in conversation

### High Memory Usage

**Solutions:**

1. **Limit conversation length:**
   ```cpp
   opts.max_turns = 10;
   ```

2. **Use one-shot queries when possible:**
   ```cpp
   // Instead of client for single query
   auto messages = claude::query("Single question", opts);
   ```

3. **Monitor message queue size:**
   - Check if consumer is keeping up
   - Ensure messages are being read

## Getting Help

### Diagnostic Information

When reporting issues, include:

1. **Version information:**
   ```cpp
   std::cout << "SDK Version: " << claude::version_string() << "\n";
   ```

2. **Platform:**
   - OS and version
   - Compiler and version
   - CMake version

3. **CLI information:**
   ```bash
   claude -v
   ```

4. **Minimal reproduction:**
   - Smallest code that shows the issue
   - Steps to reproduce
   - Expected vs actual behavior

### Where to Get Help

- **GitHub Issues:** https://github.com/yourorg/ccsdk/issues
- **Documentation:** Check other docs in this folder
- **Examples:** See `examples/` directory for working code

### Before Opening an Issue

1. ✅ Searched existing issues
2. ✅ Read this troubleshooting guide
3. ✅ Tried on latest version
4. ✅ Created minimal reproduction
5. ✅ Included version information

---

**Related Docs:**
- [Getting Started](getting-started.md)
- [API Reference](api-reference.md)
- [Examples](../examples/)
