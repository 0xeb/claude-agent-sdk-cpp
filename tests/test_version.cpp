#include <claude/version.hpp>
#include <gtest/gtest.h>

TEST(VersionTest, VersionString)
{
    std::string version = claude::version_string();
    EXPECT_FALSE(version.empty());
    std::string expected = std::to_string(claude::VERSION_MAJOR) + "." +
                           std::to_string(claude::VERSION_MINOR) + "." +
                           std::to_string(claude::VERSION_PATCH);
    EXPECT_EQ(version, expected);
}

TEST(VersionTest, VersionConstants)
{
    // Ensure version constants are non-negative and consistent with string
    EXPECT_GE(claude::VERSION_MAJOR, 0);
    EXPECT_GE(claude::VERSION_MINOR, 0);
    EXPECT_GE(claude::VERSION_PATCH, 0);
}
