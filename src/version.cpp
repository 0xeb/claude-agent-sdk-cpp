#include <claude/version.hpp>
#include <sstream>

namespace claude
{

std::string version_string()
{
    std::ostringstream oss;
    oss << VERSION_MAJOR << "." << VERSION_MINOR << "." << VERSION_PATCH;
    return oss.str();
}

} // namespace claude
