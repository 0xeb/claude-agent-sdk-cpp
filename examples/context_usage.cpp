// Mirrors: reference/claude-agent-sdk-python ClaudeSDKClient.get_context_usage()
//          (Python commit ac900bd).
//
// Connects a streaming-mode client and polls the current context window usage
// — categories, total/max tokens, percentage, and a list of memory files.
//
// Requires the claude CLI to be installed; without it the example exits 0
// after printing a usage hint so the binary always builds.

#include <claude/claude.hpp>
#include <iostream>

int main()
{
    std::cout << "Claude SDK " << claude::version_string() << " - get_context_usage\n\n";

    claude::ClaudeOptions opts;
    opts.permission_mode = "bypassPermissions";
    opts.model = "claude-sonnet-4-5";

    claude::ClaudeClient client(opts);
    try
    {
        client.connect();
    }
    catch (const claude::CLINotFoundError&)
    {
        std::cout << "claude CLI not detected — set CLAUDE_CLI_PATH or install "
                     "@anthropic-ai/claude-code and re-run.\n";
        return 0;
    }
    catch (const claude::ClaudeError& e)
    {
        std::cerr << "connect failed: " << e.what() << "\n";
        return 1;
    }

    try
    {
        client.send_query("hello");
        auto messages = client.receive_response();

        auto usage = client.get_context_usage();
        std::cout << "model:           " << usage.model << "\n";
        std::cout << "totalTokens:     " << usage.totalTokens << "\n";
        std::cout << "maxTokens:       " << usage.maxTokens << "\n";
        std::cout << "rawMaxTokens:    " << usage.rawMaxTokens << "\n";
        std::cout << "percentage:      " << usage.percentage << "%\n";
        std::cout << "autoCompactOn:   " << (usage.isAutoCompactEnabled ? "yes" : "no") << "\n";
        std::cout << "categories:      " << usage.categories.size() << "\n";
        std::cout << "memoryFiles:     " << usage.memoryFiles.size() << "\n";
        std::cout << "mcpTools:        " << usage.mcpTools.size() << "\n";
    }
    catch (const claude::ClaudeError& e)
    {
        std::cerr << "query/get_context_usage failed: " << e.what() << "\n";
        client.disconnect();
        return 1;
    }

    client.disconnect();
    return 0;
}
