#pragma once

#include <cstdlib>
#include <gtest/gtest.h>

namespace claude::test {

/**
 * Detects if running in a CI environment.
 * Checks common CI environment variables.
 */
inline bool is_ci_environment()
{
    // Common CI environment variables
    const char* ci_vars[] = {
        "CI",              // Generic (GitHub Actions, GitLab CI, etc.)
        "GITHUB_ACTIONS",  // GitHub Actions
        "GITLAB_CI",       // GitLab CI
        "TRAVIS",          // Travis CI
        "CIRCLECI",        // CircleCI
        "JENKINS_URL",     // Jenkins
        "BUILDKITE",       // Buildkite
        "TF_BUILD",        // Azure Pipelines
        "APPVEYOR",        // AppVeyor
        "CODEBUILD_BUILD_ID", // AWS CodeBuild
    };

    for (const char* var : ci_vars)
    {
        const char* value = std::getenv(var);
        if (value != nullptr && value[0] != '\0')
        {
            return true;
        }
    }
    return false;
}

} // namespace claude::test

/**
 * Macro to skip tests in CI environments.
 * Use at the beginning of integration tests that require live Claude CLI.
 *
 * Example:
 *   TEST(MyTest, LiveApiCall) {
 *       SKIP_IN_CI();
 *       // ... test code that calls live API ...
 *   }
 */
#define SKIP_IN_CI() \
    do { \
        if (claude::test::is_ci_environment()) { \
            GTEST_SKIP() << "Skipped in CI environment (live API test)"; \
        } \
    } while (0)
