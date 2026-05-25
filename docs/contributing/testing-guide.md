# QXEngine Core — Contributing: Testing Guide

**Repository:** https://github.com/qxengine/qxengine-core
**Owner:** Masa Bayu
**Created:** 2026-05-24

---

## Overview

Every subsystem in QXEngine Core has a corresponding unit test file and
at least one integration test. Tests are written with GoogleTest and
built via `tests/CMakeLists.txt`. All tests must pass on every supported
platform before a pull request can be merged.

---

## Test Philosophy

- **Test behaviour, not implementation.** Tests call the public C ABI
  only. Internal structs, static functions, and private headers are
  never accessed directly from tests.
- **One assertion per test.** Each `TEST` or `TEST_F` block tests
  exactly one observable behaviour. Split multi-behaviour tests into
  separate test cases.
- **Tests must be deterministic.** No random data, no time-dependent
  assertions, no reliance on external files or network access.
- **Tests must be isolated.** Each test creates and destroys its own
  handles. No shared mutable state between tests.

---

## Test Structure

```
tests/
├── CMakeLists.txt                         ← test build configuration
├── unit/
│   ├── test_qx_leaf.cpp                   ← QXLeaf unit tests
│   ├── test_qx_segment.cpp                ← QXSegment unit tests
│   ├── test_qx_memloc.cpp                 ← QXMemloc unit tests
│   ├── test_qx_pressure.cpp               ← QXPressure unit tests
│   ├── test_qx_law_enforcer.cpp           ← QXLawEnforcer unit tests
│   ├── test_qx_snapshot.cpp               ← QXSnapshotHistory unit tests
│   ├── test_qx_manifest.cpp               ← QXManifest unit tests
│   └── test_qx_telemetry.cpp              ← QXTelemetry unit tests
└── integration/
    ├── test_full_evaluation_cycle.cpp     ← end-to-end engine cycle
    └── test_manifest_validation.cpp       ← full MFT rule enforcement
```

---

## Running Tests

**Build and run all tests:**

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DQX_BUILD_TESTS=ON
cmake --build build --config Debug
cd build && ctest --output-on-failure
```

**Run a single test binary:**

```bash
./build/tests/unit/test_qx_law_enforcer
```

**Run a single test case by name:**

```bash
./build/tests/unit/test_qx_law_enforcer \
    --gtest_filter="QXLawEnforcerTest.Law5FailsOnLowKnowledgeScore"
```

**Run only integration tests:**

```bash
ctest --output-on-failure -L integration
```

---

## Unit Test Rules

- Every unit test file must open with the standard file header comment
  block as defined in `coding-standards.md`.
- Every unit test file must be strictly under 500 lines. If a test file
  grows beyond this limit, split it by grouping related test cases into
  a new file with a descriptive suffix, e.g.
  `test_qx_law_enforcer_history.cpp`.
- Every test must call `ASSERT_EQ` or `ASSERT_NE` for handle creation
  before using the handle in subsequent assertions.
- Every test must clean up all handles in `TearDown` or at the end of
  the test body. Memory leaks in tests are treated as test failures.
- Tests must not call `exit()`, `abort()`, or `std::terminate()`.

---

## Fixture Conventions

Use a `::testing::Test` fixture whenever two or more test cases share
the same setup and teardown logic:

```cpp
class QXExampleTest : public ::testing::Test {
protected:
    QXExampleHandle handle_ = nullptr;

    void SetUp() override {
        ASSERT_EQ(qx_example_create(&handle_), QX_OK);
        ASSERT_NE(handle_, nullptr);
    }

    void TearDown() override {
        qx_example_destroy(handle_);
        handle_ = nullptr;
    }
};
```

Standalone `TEST()` cases (not `TEST_F`) are used only for tests that
require no shared setup, such as lifecycle and null-handle tests.

---

## Naming Conventions

| Entity | Convention | Example |
|---|---|---|
| Test fixture class | `QX<Subsystem>Test` | `QXLawEnforcerTest` |
| Standalone test suite | `QX<Subsystem><Group>` | `QXLawEnforcerLifecycle` |
| Test case name | `<Condition><ExpectedResult>` | `Law5FailsOnLowKnowledgeScore` |
| Helper function | `make_valid_<type>` | `make_valid_input` |
| Local constants | `k<PascalCase>` | `kSoftBytes` |

Test case names must be self-documenting. A reader must understand what
is being tested and what the expected outcome is from the name alone,
without reading the test body.

---

## Writing a New Unit Test

Follow these steps when adding a unit test for a new subsystem:

1. Create `tests/unit/test_qx_<subsystem>.cpp` with the standard header.
2. Add `qx_add_unit_test(test_qx_<subsystem>)` to `tests/CMakeLists.txt`.
3. Define a fixture with `SetUp` and `TearDown` if two or more tests
   share state.
4. Write helper functions (`make_valid_input`, etc.) before the fixture.
5. Group tests by section using `/* ====== Section ====== */` comments.
6. Cover at minimum: lifecycle (create, destroy, null handles),
   happy-path behaviour, and all documented error return codes.
7. Count lines before committing. If the file exceeds 499 lines, split
   it before opening a pull request.

---

## Integration Test Rules

- Integration tests live in `tests/integration/` and use the full engine
  lifecycle: manifest parse → engine create → operations → engine destroy.
- Integration tests must not mock any subsystem. They exercise the real
  C ABI end-to-end.
- Each integration test has a 60-second timeout enforced by CTest.
- Integration test files follow the same 500-line rule as unit tests.

---

## Coverage Requirements

| Subsystem | Minimum coverage |
|---|---|
| `qx_leaf` | All state transitions, all eviction classes |
| `qx_segment` | Allocation, deallocation, hard limit rejection |
| `qx_memloc` | All nine segment IDs, law input snapshot |
| `qx_pressure` | All four tiers, Android trim, iOS warning |
| `qx_law_enforcer` | All eight laws, history, stats, certification |
| `qx_snapshot` | Capture, history, all five signal helpers |
| `qx_manifest` | All 14 MFT rules, all block and scalar accessors |
| `qx_telemetry` | Record, history, stats, flush, query helpers |

---

## CI Pipeline

Tests are run automatically on every pull request via the CI pipeline.
The pipeline executes the following steps in order:

1. `cmake` configure with `QX_BUILD_TESTS=ON` and `Debug` build type.
2. `cmake --build` for all targets.
3. `ctest --output-on-failure` for all unit tests (30 s timeout each).
4. `ctest --output-on-failure -L integration` for integration tests
   (60 s timeout each).
5. Build failure or any test failure blocks merge.

---

## Related Documents

- [`coding-standards.md`](coding-standards.md) — File and code standards
- [`../api/core.md`](../api/core.md) — Error codes for assertion targets
- [`../architecture/constitutional-laws.md`](../architecture/constitutional-laws.md) — Law test coverage guidance
- [`../architecture/memory-model.md`](../architecture/memory-model.md) — Memory test coverage guidance
