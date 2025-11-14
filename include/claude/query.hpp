#ifndef CLAUDE_QUERY_HPP
#define CLAUDE_QUERY_HPP

#include <claude/types.hpp>
#include <memory>
#include <vector>

namespace claude
{

// Forward declaration
namespace transport
{
class SubprocessCLITransport;
}

// Query result that provides iteration over messages
class QueryResult
{
  public:
    // Iterator for messages
    class Iterator
    {
      public:
        using iterator_category = std::input_iterator_tag;
        using value_type = Message;
        using difference_type = std::ptrdiff_t;
        using pointer = const Message*;
        using reference = const Message&;

        Iterator();
        Iterator(std::shared_ptr<std::vector<Message>> messages, size_t index);

        reference operator*() const;
        pointer operator->() const;
        Iterator& operator++();
        Iterator operator++(int);

        bool operator==(const Iterator& other) const;
        bool operator!=(const Iterator& other) const;

      private:
        std::shared_ptr<std::vector<Message>> messages_;
        size_t index_;
    };

    QueryResult();
    explicit QueryResult(std::vector<Message>&& messages);

    Iterator begin() const;
    Iterator end() const;

    // Get all messages as vector
    const std::vector<Message>& messages() const;

  private:
    std::shared_ptr<std::vector<Message>> messages_;
};

// Main query function
QueryResult query(const std::string& prompt, const ClaudeOptions& options = ClaudeOptions{});

} // namespace claude

#endif // CLAUDE_QUERY_HPP
