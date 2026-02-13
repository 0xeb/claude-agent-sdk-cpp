#include <chrono>
#include <claude/errors.hpp>
#include <claude/query.hpp>
#include <claude/transport.hpp>
#include <nlohmann/json.hpp>
#include <thread>

namespace claude
{

// QueryResult::Iterator implementation

QueryResult::Iterator::Iterator() : messages_(nullptr), index_(0) {}

QueryResult::Iterator::Iterator(std::shared_ptr<std::vector<Message>> messages, size_t index)
    : messages_(std::move(messages)), index_(index)
{
}

QueryResult::Iterator::reference QueryResult::Iterator::operator*() const
{
    if (!messages_ || index_ >= messages_->size())
        throw std::out_of_range("Iterator out of range");
    return (*messages_)[index_];
}

QueryResult::Iterator::pointer QueryResult::Iterator::operator->() const
{
    return &(operator*());
}

QueryResult::Iterator& QueryResult::Iterator::operator++()
{
    ++index_;
    return *this;
}

QueryResult::Iterator QueryResult::Iterator::operator++(int)
{
    Iterator tmp = *this;
    ++(*this);
    return tmp;
}

bool QueryResult::Iterator::operator==(const Iterator& other) const
{
    // Two end iterators are equal
    if (!messages_ && !other.messages_)
        return true;
    // One is end, one is not
    if (!messages_ || !other.messages_)
        return false;
    // Same position in same container
    return messages_ == other.messages_ && index_ == other.index_;
}

bool QueryResult::Iterator::operator!=(const Iterator& other) const
{
    return !(*this == other);
}

// QueryResult implementation

QueryResult::QueryResult() : messages_(std::make_shared<std::vector<Message>>()) {}

QueryResult::QueryResult(std::vector<Message>&& messages)
    : messages_(std::make_shared<std::vector<Message>>(std::move(messages)))
{
}

QueryResult::Iterator QueryResult::begin() const
{
    return Iterator(messages_, 0);
}

QueryResult::Iterator QueryResult::end() const
{
    return Iterator(messages_, messages_->size());
}

const std::vector<Message>& QueryResult::messages() const
{
    return *messages_;
}

// Main query function

QueryResult query(const std::string& prompt, const ClaudeOptions& options)
{
    if (prompt.empty())
        throw ClaudeError("Prompt cannot be empty");

    try
    {
        // Create one-shot transport with prompt
        auto transport = create_oneshot_transport(prompt, options);

        // Connect to CLI
        transport->connect();

        // v0.1.35: Always use streaming mode - send prompt via stdin as user message
        // then close stdin to signal end of input
        json user_msg = {{"type", "user"},
                         {"message", {{"role", "user"}, {"content", prompt}}},
                         {"parent_tool_use_id", nullptr},
                         {"session_id", ""}};
        transport->write(user_msg.dump() + "\n");
        transport->end_input();

        // Collect all messages until transport closes
        std::vector<Message> all_messages;

        while (transport->has_messages())
        {
            auto messages = transport->read_messages();
            if (messages.empty())
            {
                // No messages available, wait a bit
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }

            for (auto& msg : messages)
            {
                all_messages.push_back(std::move(msg));

                // Check if we got the result message (end of response)
                if (is_result_message(all_messages.back()))
                {
                    // We got the final result, can stop reading
                    goto done;
                }
            }
        }

    done:
        // Close transport
        transport->close();

        // Return result
        return QueryResult(std::move(all_messages));
    }
    catch (const ClaudeError&)
    {
        // Re-throw Claude errors as-is
        throw;
    }
    catch (const std::exception& e)
    {
        // Wrap other exceptions
        throw ClaudeError(std::string("Query failed: ") + e.what());
    }
}

} // namespace claude