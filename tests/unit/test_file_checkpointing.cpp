#include "internal/transport/subprocess_env.hpp"

#include <gtest/gtest.h>

using namespace claude;

TEST(FileCheckpointingTest, AddsEnvVarWhenEnabled)
{
    ClaudeOptions opts;
    opts.enable_file_checkpointing = true;

    subprocess::ProcessOptions proc_opts;
    internal::apply_sdk_environment(proc_opts, opts, "sdk-cpp");

    auto it = proc_opts.environment.find("CLAUDE_CODE_ENABLE_SDK_FILE_CHECKPOINTING");
    ASSERT_NE(it, proc_opts.environment.end());
    EXPECT_EQ(it->second, "true");
}

TEST(FileCheckpointingTest, DoesNotAddEnvVarWhenDisabled)
{
    ClaudeOptions opts;
    opts.enable_file_checkpointing = false;

    subprocess::ProcessOptions proc_opts;
    internal::apply_sdk_environment(proc_opts, opts, "sdk-cpp");

    auto it = proc_opts.environment.find("CLAUDE_CODE_ENABLE_SDK_FILE_CHECKPOINTING");
    EXPECT_EQ(it, proc_opts.environment.end());
}
