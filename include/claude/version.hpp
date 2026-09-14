#ifndef CLAUDE_VERSION_HPP
#define CLAUDE_VERSION_HPP

#include <string>

namespace claude
{

constexpr int VERSION_MAJOR = 0;
constexpr int VERSION_MINOR = 2;
constexpr int VERSION_PATCH = 82;
// Python SDK version: 0.2.82 (keep C++ in sync)

std::string version_string();

} // namespace claude

#endif // CLAUDE_VERSION_HPP
