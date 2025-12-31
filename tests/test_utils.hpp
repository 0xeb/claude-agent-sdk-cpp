#pragma once

#include "../src/internal/subprocess/process.hpp"

#include <cstdlib>
#include <gtest/gtest.h>
#include <string>

namespace claude::test
{

inline bool is_ci_environment()
{
    const char* ci_vars[] = {
        "CI",                 // Generic (GitHub Actions, GitLab CI, etc.)
        "GITHUB_ACTIONS",     // GitHub Actions
        "GITLAB_CI",          // GitLab CI
        "TRAVIS",             // Travis CI
        "CIRCLECI",           // CircleCI
        "JENKINS_URL",        // Jenkins
        "BUILDKITE",          // Buildkite
        "TF_BUILD",           // Azure Pipelines
        "APPVEYOR",           // AppVeyor
        "CODEBUILD_BUILD_ID", // AWS CodeBuild
    };

    for (const char* var : ci_vars)
    {
        const char* value = std::getenv(var);
        if (value != nullptr && value[0] != '\0')
            return true;
    }
    return false;
}

inline bool has_env_flag(const char* name)
{
    const char* value = std::getenv(name);
    return value != nullptr && value[0] != '\0' && std::string(value) != "0";
}

inline bool is_claude_cli_available()
{
    if (const char* cli_path = std::getenv("CLAUDE_CLI_PATH");
        cli_path != nullptr && cli_path[0] != '\0')
        return true;

    if (claude::subprocess::find_executable("claude").has_value())
        return true;
#ifdef _WIN32
    if (claude::subprocess::find_executable("claude.exe").has_value())
        return true;
#endif
    return false;
}

inline bool should_run_live_tests()
{
    if (is_ci_environment())
        return false;

    if (!has_env_flag("CLAUDE_AGENT_SDK_RUN_LIVE_TESTS"))
        return false;

    return is_claude_cli_available();
}

} // namespace claude::test

#define SKIP_IN_CI()                                                                               \
    do                                                                                             \
    {                                                                                              \
        if (!claude::test::should_run_live_tests())                                                \
        {                                                                                          \
            GTEST_SKIP() << "Skipped live CLI/API test (set "                                      \
                            "CLAUDE_AGENT_SDK_RUN_LIVE_TESTS=1 and ensure `claude` "               \
                            "is in PATH or set CLAUDE_CLI_PATH)";                                  \
        }                                                                                          \
    } while (0)
