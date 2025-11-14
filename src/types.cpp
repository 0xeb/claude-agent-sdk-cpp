#include <claude/types.hpp>

namespace claude
{

std::string get_text_content(const std::vector<ContentBlock>& content)
{
    std::string result;

    for (const auto& block : content)
    {
        if (auto* text_block = std::get_if<TextBlock>(&block))
        {
            result += text_block->text;
        }
    }

    return result;
}

} // namespace claude
