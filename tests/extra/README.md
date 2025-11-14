# Extra Tests

Advanced and specialized test scenarios that go beyond standard unit and integration tests.

## Multithreading Tests

**File:** `test_multithreading.cpp`

Tests concurrent usage of multiple `ClaudeClient` instances across different threads.

### Test Cases

| Test | Threads | Runtime | Status | Description |
|------|---------|---------|--------|-------------|
| `MultipleClientsSequential` | 3 | ~17s | ✅ Active | Baseline: clients created sequentially |
| `MultipleClientsParallel` | 3 | ~7s | ✅ Active | Main test: concurrent client threads |
| `ClientLifetimeInThread` | 1 | ~6s | ✅ Active | Client creation/destruction in thread |
| `DISABLED_ConcurrentQueriesStressTest` | 5 | ~2-5min | ⏸️ Disabled | Stress test with more threads |

### Running the Tests

**Run all active multithreading tests:**
```bash
./build-win/tests/Release/test_claude.exe --gtest_filter="MultithreadingTest.*"
```

**Run with disabled tests included:**
```bash
./build-win/tests/Release/test_claude.exe \
  --gtest_filter="MultithreadingTest.*" \
  --gtest_also_run_disabled_tests
```

**Run a specific test:**
```bash
./build-win/tests/Release/test_claude.exe \
  --gtest_filter="MultithreadingTest.MultipleClientsParallel"
```

### Performance Notes

- Each `ClaudeClient` spawns its own Claude CLI subprocess
- Parallel tests show ~2.6x speedup with 3 threads
- Running many concurrent CLI instances is resource-intensive
- Stress tests are disabled by default due to long runtime

### Implementation Details

The test suite includes:
- **Thread-safe result collection** using mutexes
- **Atomic counters** for tracking concurrent clients
- **Comprehensive error reporting** with per-thread status
- **Varied queries** to ensure independence (2+2, 3*3, 10-5, etc.)

### What These Tests Verify

✅ Multiple clients can run concurrently without interference
✅ Each client manages its own subprocess correctly
✅ Thread-safe construction/destruction
✅ No resource conflicts between concurrent clients
✅ True parallelism is achieved (proven by speedup metrics)

### Limitations

- Claude CLI may have file locking contention on `.claude.json`
- High resource usage with many concurrent clients
- Some system limits may apply to subprocess count
- Stress tests expect 80%+ success rate (not 100% due to system limits)

## Adding New Extra Tests

Extra tests are for:
- Advanced scenarios (multithreading, stress tests, etc.)
- Long-running tests (>30 seconds)
- Edge cases that don't fit unit/integration categories
- Performance benchmarks

To add a new extra test:

1. Create test file in `tests/extra/`
2. Add to `tests/CMakeLists.txt`
3. Document in this README
4. Use `DISABLED_` prefix for slow tests

## See Also

- `tests/unit/` - Component isolation tests
- `tests/integration/` - Real Claude CLI integration tests
- `tests/test_version.cpp` - SDK version tests
