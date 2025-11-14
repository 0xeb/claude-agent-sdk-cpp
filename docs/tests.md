# Tests

- The test suite uses GoogleTest and builds a single runner `test_claude`.
- Tests that require the Claude CLI are auto-disabled when the `claude` CLI is not found on PATH.

## Enable Tests

Configure with tests enabled:

```bash
cmake -S claude-agent-sdk-cpp -B build -G Ninja -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE=Release -DCLAUDE_BUILD_TESTS=ON
cmake --build build -j 4
```

## Running

- Run all tests: `ctest --test-dir build -j 4 --output-on-failure`
- Run only core conformance tests: `ctest --test-dir build -L conformance -j 4 --output-on-failure`

Notes:
- Integration tests that exercise optional fastmcpp components are labeled `integration`.
- Some tests are automatically disabled if external dependencies are missing (e.g., Claude CLI).

