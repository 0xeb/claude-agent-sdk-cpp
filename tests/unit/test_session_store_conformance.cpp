// Runs the shipped SessionStore conformance suite against the shipped stores.
//
// claude/testing/session_store_conformance.hpp has been part of the public API
// since v0.2.82, so third-party SessionStore implementations can verify
// themselves against the same 14 contracts the SDK's own stores must satisfy.
// Nothing invoked it -- the harness shipped, and both FileSessionStore and
// InMemorySessionStore went out with no coverage at all.
//
// Each contract gets a freshly-built store via the factory, so contracts cannot
// leak state into one another.

#include <claude/sessions/file_session_store.hpp>
#include <claude/sessions/in_memory_session_store.hpp>
#include <claude/testing/session_store_conformance.hpp>

#include <gtest/gtest.h>

#include <atomic>
#include <filesystem>
#include <memory>
#include <string>

namespace
{

/// Report every non-passing contract by name. A bare
/// ASSERT_TRUE(result.all_passed()) would say "false" and leave you to guess
/// which of the 14 broke.
void expect_all_contracts_pass(const claude::testing::ConformanceResult& result,
                               const std::string& store_name)
{
    for (const auto& contract : result.contracts)
    {
        EXPECT_TRUE(contract.passed || contract.skipped)
            << store_name << ": contract '" << contract.name << "' failed: " << contract.message;
    }

    // Guards against a harness that silently exercises nothing -- all-skipped
    // would otherwise sail through the loop above.
    EXPECT_GT(result.passed, 0) << store_name << ": no contract actually ran";
    EXPECT_EQ(result.failed, 0) << store_name << ": " << result.failed << " contract(s) failed";
}

/// Unique temp directory per store instance, so FileSessionStore contracts are
/// isolated from each other and from previous runs.
std::filesystem::path fresh_store_root()
{
    static std::atomic<int> counter{0};
    const auto root = std::filesystem::temp_directory_path() /
                      ("claude_sdk_conformance_" + std::to_string(counter.fetch_add(1)));
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    return root;
}

} // namespace

TEST(SessionStoreConformanceTest, InMemorySessionStoreSatisfiesAllContracts)
{
    const auto result = claude::testing::run_session_store_conformance(
        [] { return std::make_shared<claude::sessions::InMemorySessionStore>(); });

    expect_all_contracts_pass(result, "InMemorySessionStore");
}

TEST(SessionStoreConformanceTest, FileSessionStoreSatisfiesAllContracts)
{
    const auto result = claude::testing::run_session_store_conformance(
        [] { return std::make_shared<claude::sessions::FileSessionStore>(fresh_store_root()); });

    expect_all_contracts_pass(result, "FileSessionStore");
}

// The harness itself has never been observed failing, so this pins the shape of
// its output: a conformance runner that reports success unconditionally would
// pass both tests above and prove nothing.
TEST(SessionStoreConformanceTest, HarnessReportsPerContractResults)
{
    const auto result = claude::testing::run_session_store_conformance(
        [] { return std::make_shared<claude::sessions::InMemorySessionStore>(); });

    EXPECT_FALSE(result.contracts.empty()) << "harness reported no contracts at all";
    EXPECT_EQ(result.contracts.size(),
              static_cast<std::size_t>(result.passed + result.failed + result.skipped))
        << "contract list and pass/fail/skip tallies disagree";

    for (const auto& contract : result.contracts)
        EXPECT_FALSE(contract.name.empty()) << "a contract was reported with no name";
}
