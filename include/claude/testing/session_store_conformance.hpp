#ifndef CLAUDE_TESTING_SESSION_STORE_CONFORMANCE_HPP
#define CLAUDE_TESTING_SESSION_STORE_CONFORMANCE_HPP

#include <claude/sessions/session_store.hpp>
#include <functional>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace claude::testing
{

/// Names of optional SessionStore methods. Mirrors Python's
/// session_store_conformance OptionalMethod literal set.
namespace ConformanceOptional
{
inline const std::string ListSessions = "list_sessions";
inline const std::string ListSessionSummaries = "list_session_summaries";
inline const std::string DeleteSession = "delete";
inline const std::string ListSubkeys = "list_subkeys";
} // namespace ConformanceOptional

/// Options for the conformance suite.
struct ConformanceOptions
{
    /// Optional methods to skip explicitly. Combined with the store's
    /// declared capabilities to determine which contracts are exercised.
    std::set<std::string> skip_optional;
};

/// Single contract result.
struct ConformanceContractResult
{
    std::string name;
    bool passed = false;
    bool skipped = false;
    std::string message;
};

/// Aggregated result for the full suite.
struct ConformanceResult
{
    std::vector<ConformanceContractResult> contracts;
    int passed = 0;
    int failed = 0;
    int skipped = 0;
    bool all_passed() const noexcept
    {
        return failed == 0;
    }
};

/// Factory returning a freshly-isolated SessionStore. Invoked once per
/// contract so contracts cannot leak state into each other.
using StoreFactory = std::function<std::shared_ptr<claude::SessionStore>()>;

/// Run the 14 SessionStore behavioral contracts against the store provided by
/// ``make_store``. Returns a structured ConformanceResult — downstream tests
/// can ASSERT_TRUE(result.all_passed()) or inspect individual contracts.
///
/// Contracts for optional methods (``list_sessions``,
/// ``list_session_summaries``, ``delete``, ``list_subkeys``) are skipped when
/// named in ``options.skip_optional`` or when the store does not advertise
/// that capability.
///
/// Python: claude_agent_sdk/testing/session_store_conformance.py:
/// run_session_store_conformance (line 54).
ConformanceResult run_session_store_conformance(StoreFactory make_store,
                                                const ConformanceOptions& options = {});

} // namespace claude::testing

#endif // CLAUDE_TESTING_SESSION_STORE_CONFORMANCE_HPP
