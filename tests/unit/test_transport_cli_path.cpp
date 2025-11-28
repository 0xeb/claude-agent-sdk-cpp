#include <claude/errors.hpp>
#include <claude/transport.hpp>
#include <claude/types.hpp>
#include <gtest/gtest.h>

using namespace claude;

TEST(TransportCliPathTest, InvalidCliPathRaisesError)
{
    ClaudeOptions opts;
#ifdef _WIN32
    opts.cli_path = "Z:/definitely/not/a/real/path/claude.exe";
#else
    opts.cli_path = "/this/path/does/not/exist/claude";
#endif

    auto transport = create_subprocess_transport(opts);
    EXPECT_THROW(transport->connect(), CLINotFoundError);
}

TEST(TransportCliPathTest, RequireExplicitCliWithoutPathFailsFast)
{
    ClaudeOptions opts;
    opts.require_explicit_cli = true;
    // No cli_path and no env override; should fail immediately without PATH probing.
    auto transport = create_subprocess_transport(opts);
    EXPECT_THROW(transport->connect(), CLINotFoundError);
}
