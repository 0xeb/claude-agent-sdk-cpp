#include <gtest/gtest.h>
#include <claude/client.hpp>
#include <claude/mcp.hpp>

using namespace claude;
using json = nlohmann::json;

// Refactored per Python SDK parity: permission enforcement tests should use
// E2E with real CLI subprocess (not MockTransport + connect()).
// See Python SDK: e2e-tests/test_sdk_mcp_tools.py
