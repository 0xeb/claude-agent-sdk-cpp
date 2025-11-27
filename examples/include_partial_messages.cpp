#include <claude/claude.hpp>
#include <iostream>

// Mirror of Python's include_partial_messages.py example name
int main()
{
    using namespace claude;

    ClaudeOptions opts;
    opts.permission_mode = "bypassPermissions";
    opts.include_partial_messages = true;

    ClaudeClient client(opts);
    client.connect();

    client.send_query("Write a haiku about streams.");

    std::cout << "Claude: ";
    std::cout.flush();

    for (const auto& msg : client.receive_messages())
    {
        if (is_stream_event(msg))
        {
            const auto& ev = std::get<StreamEvent>(msg);
            if (ev.event == "content_block_delta" && ev.data.contains("delta"))
            {
                const auto& d = ev.data["delta"];
                if (d.contains("type") && d["type"] == "text_delta" && d.contains("text"))
                {
                    std::cout << d["text"].get<std::string>();
                    std::cout.flush();
                }
            }
        }
        else if (is_result_message(msg))
        {
            std::cout << "\n";
            break;
        }
    }

    client.disconnect();
    return 0;
}
