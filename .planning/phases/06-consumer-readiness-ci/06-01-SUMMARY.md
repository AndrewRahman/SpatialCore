---
phase: 06-consumer-readiness-ci
plan: 01
subsystem: build-system
tags: [cmake, ctest, catch2, umbrella-headers, juce, consumer-readiness]

requires:
  - phase: 05-verification
    provides: "SpatialCoreTests and SpatialCoreUITests executables and the local test gate"
provides:
  - "Root-level CTestTestfile.cmake, so ctest --test-dir <tree> finds the whole suite (CI-01 root cause)"
  - "SPATIALCORE_BUILD_TESTS defaulting to PROJECT_IS_TOP_LEVEL (no surprise test targets in a consumer build)"
  - "include/SpatialCore/SpatialCoreUI.h, the UI umbrella (ten UI headers)"
  - "Complete DSP umbrella plus an [umbrella] source-scan rule that keeps both umbrellas complete"
affects: [06-04-harness, 06-05-ci, 06-07-coverage]

plan_head_before: 7eb625b8681047c0afbf41e3a1ae7a31579fadc1
plan_head_after: d74fa5654a8648ccb1fd72077081942cfa15c27c

actuals:
  tokens: 3300
  tasks: 2
  commits: 2

tech-stack:
  added: []
  patterns:
    - "Top-level-only enable_testing() before add_subdirectory(tests)"
    - "Header-completeness enforced by a Catch2 test that scans include/ via SPATIALCORE_SOURCE_DIR"
    - "Each umbrella is the first include of its own test translation unit"

key-files:
  created:
    - include/SpatialCore/SpatialCoreUI.h
    - tests/Core/UmbrellaTests.cpp
    - tests/UI/UmbrellaUITests.cpp
  modified:
    - CMakeLists.txt
    - include/SpatialCore/SpatialCore.h
    - tests/CMakeLists.txt

key-decisions:
  - "SPATIALCORE_BUILD_TESTS defaults to ${PROJECT_IS_TOP_LEVEL}; a consumer sets it ON to get the tests"
  - "Only one allow-list entry (Engine/TripleBufferIndex.h); the DSP umbrella stays UI-free"

requirements-completed: [INTG-01, CI-01]

coverage:
  - id: D1
    description: "ctest at the build root finds and runs the whole suite (BASE found 0 tests); root total equals tests/ total"
    requirement: CI-01
    verification:
      - kind: integration
        ref: "ctest --test-dir build-release --no-tests=error --output-on-failure (406/406 passed)"
        status: pass
    human_judgment: false
  - id: D2
    description: "A consumer add_subdirectory()ing SpatialCore gets no test or Catch2 target unless it sets SPATIALCORE_BUILD_TESTS=ON"
    requirement: INTG-01
    verification:
      - kind: integration
        ref: "scratch consumer configure, cmake --build <dir> --target help: default 0 test targets, ON lists SpatialCoreTests and SpatialCoreUITests"
        status: pass
    human_judgment: false
  - id: D3
    description: "Two complete umbrella headers (SpatialCore.h and new SpatialCoreUI.h), kept complete by a source-scan rule"
    requirement: INTG-01
    verification:
      - kind: unit
        ref: "tests/Core/UmbrellaTests.cpp#Umbrella: every public header is in exactly one umbrella or on the allow-list"
        status: pass
      - kind: unit
        ref: "tests/UI/UmbrellaUITests.cpp#Umbrella: the UI umbrella alone builds and paints the map with the SML look-and-feel"
        status: pass
    human_judgment: false

duration: 69min
completed: 2026-10-17
status: complete
---

# Phase 6 Plan 01: Test visibility, consumed-mode default and umbrella headers Summary

**Top-level `enable_testing()` makes root `ctest` find all 406 tests (BASE found 0), `SPATIALCORE_BUILD_TESTS` now defaults to `PROJECT_IS_TOP_LEVEL`, and a new `SpatialCoreUI.h` plus a complete `SpatialCore.h` are held complete by an `[umbrella]` source-scan test.**

## Performance

- **Duration:** 69 min wall clock (includes first-time FetchContent configure and three full builds)
- **Started:** 2026-10-17T12:16:10Z
- **Completed:** 2026-10-17T13:25:10Z
- **Tasks:** 2
- **Files modified:** 6 (3 created, 3 modified)
- **BASE:** `7eb625b8681047c0afbf41e3a1ae7a31579fadc1`

## Accomplishments

- CI-01 root cause fixed in CMake. BASE evidence recorded below; after the change the root and `tests/` totals are equal.
- INTG-01 no-surprises: a consumer build generates no `SpatialCoreTests`, `SpatialCoreUITests`, `SpatialCoreRTGuard` or Catch2 target by default, and still gets them with `-DSPATIALCORE_BUILD_TESTS=ON`.
- Consumers now have two complete umbrellas. The DSP one gained `OSC/ADMPositionQuery.h` and `OSC/OSCPortValidation.h` (audit W2) and lost its stale `Phase 8 Plan 08-06` comment (W3).

## Evidence

**Red at BASE (Task 1):**

| Command | Result at BASE | After change |
|---|---|---|
| `ctest --test-dir build-release -N \| tail -1` | `Total Tests: 0` | `Total Tests: 403` (406 after Task 2) |
| `ctest --test-dir build-release/tests -N \| tail -1` | `Total Tests: 403` | `Total Tests: 403` (406 after Task 2) |
| `build-release/CTestTestfile.cmake` | missing | exists |
| `ctest --test-dir build-release --no-tests=error` (a real run) | `No tests were found!!!`, exit 8 | 403/403 passed, exit 0 |
| `ctest ... --no-tests=error -N` | exit 0 | exit 0 |

The plan expected the `-N` form to exit non-zero at BASE. It does not: `-N` only lists and never trips `--no-tests=error`. The real run does (exit 8). Plan 06-05's CI must therefore run `ctest --no-tests=error` without `-N`; the `-N` listing is only useful with a count check such as `grep -c 'ui:'`.

**Consumed-mode probe (scratch `/tmp/sc-p6-consumed`, configure only):**

- Default: 0 matches for `SpatialCoreTests|SpatialCoreUITests|SpatialCoreRTGuard|Catch2`. Only `SpatialCore`, `SpatialCoreHRTFData`, `SpatialCoreUI`, `SpatialCoreUIFontData` were listed.
- `-DSPATIALCORE_BUILD_TESTS=ON`: `SpatialCoreRTGuard`, `SpatialCoreTests`, `SpatialCoreUITests` listed (2 matches for the two executables).

**Red first for `[umbrella]` (Task 2):**

1. With the test added and the umbrellas unchanged, the translation unit did not compile: `use of undeclared identifier 'oscPortsConflict'`.
2. With those two lines temporarily commented out (restored afterwards), the run exited 42 and reported:
   - `include/SpatialCore/SpatialCoreUI.h must exist (D-02: the UI umbrella)`
   - `SpatialCore/OSC/ADMPositionQuery.h must be included exactly once by SpatialCore/SpatialCore.h (found 0)`
   - `SpatialCore/OSC/OSCPortValidation.h must be included exactly once by SpatialCore/SpatialCore.h (found 0)`
   - all ten `SpatialCore/UI/*.h` headers missing from `SpatialCoreUI.h`

**Green:** `[umbrella]` passes in both executables (DSP: 10 assertions in 2 cases; UI: 2 assertions in 1 case).

**Full gate:**

- Debug `SpatialCoreTests`: exit 0, 389 test cases, 329034 assertions (the expected process-lifetime `FFT` leak report at exit is the only noise).
- Debug `SpatialCoreUITests`: exit 0, 17 cases, 152 assertions.
- Release `ctest --test-dir build-release/tests`: 406/406 passed. Release `ctest --test-dir build-release --no-tests=error`: 406/406 passed. `ui:` listing count 17 on both.

## Task Commits

1. **Task 1 (tracer): ctest at the build root, tests off by default when consumed** - `b7eae4f` (feat)
2. **Task 2: UI umbrella, complete DSP umbrella, source-scan rule** - `d74fa56` (feat)

**Plan metadata:** committed with this SUMMARY (docs).

## Files Created/Modified

- `CMakeLists.txt` - `SPATIALCORE_BUILD_TESTS` default and top-level-only `enable_testing()` (tests block only)
- `include/SpatialCore/SpatialCoreUI.h` - UI umbrella, ten includes
- `include/SpatialCore/SpatialCore.h` - two OSC headers added, header and Engine comments fixed
- `tests/Core/UmbrellaTests.cpp` - `[umbrella]` source-scan rule and DSP-umbrella-first translation unit
- `tests/UI/UmbrellaUITests.cpp` - `[ui][umbrella]` UI-umbrella-first translation unit
- `tests/CMakeLists.txt` - two source-list lines (CRLF preserved, `w/crlf`, 2 insertions)

## Decisions Made

- Followed the plan. The `[umbrella]` test also checks that `oscPortsConflict (9000, 9000, "127.0.0.1")` is true, a small addition beside the plan's one negative check.
- The scan reports every offending header in one failure message instead of stopping at the first.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Acceptance criterion `grep -c "enable_testing()"` printed 2**
- **Found during:** Task 1
- **Issue:** my explanatory comment in `CMakeLists.txt` contained the literal `enable_testing()`, so the count criterion (expected 1) read 2.
- **Fix:** reworded the comment to "enable_testing call".
- **Files modified:** `CMakeLists.txt`
- **Verification:** `grep -c "enable_testing()" CMakeLists.txt` prints 1.
- **Committed in:** `b7eae4f`

### Plan expectation that did not hold (not a code fix)

- `ctest --no-tests=error -N` exits 0 at BASE (see Evidence). The defect shows only on a real run. Task 2's last verify command ends with the `-N` form; it passes but proves only that the listing is non-empty. The real zero-test guard is the earlier non-`-N` run plus the `ui:` count. Flagged for Plan 06-05.

---

**Total deviations:** 1 auto-fixed (1 bug in my own comment). **Impact on plan:** none.

## Issues Encountered

- The session limit interrupted the run during the full gate. On resume the background Debug runs had both finished green; the Release ctest was re-run and passed.
- The pre-commit branch rule for worktrees expects an `agent-*` branch. This run is the sequential executor on the user's branch `AndrewRahman/gsd-discuss-phase-6`, as the orchestrator directed, so only the protected-branch check applied (branch is not protected).

## Known Stubs

None.

## Threat Flags

None. No new network, auth or file-access surface.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- `build/` (Debug, `compile_commands.json`) and `build-release/` (Release) are configured and built; later plans reuse them. Dependencies are in `build/_deps`.
- Plans 06-04 and 06-05 can rely on root-level `ctest` and on both umbrellas. 06-05 must call `ctest --no-tests=error` without `-N`.

## Self-Check: PASSED

- Created files exist: `include/SpatialCore/SpatialCoreUI.h`, `tests/Core/UmbrellaTests.cpp`, `tests/UI/UmbrellaUITests.cpp`.
- Commits `b7eae4f` and `d74fa56` are ancestors of HEAD.
- All acceptance criteria re-run: header counts 10/0/2/0, first-include checks, CRLF `w/crlf` with 2 insertions, `build-release/CTestTestfile.cmake` present.

---
*Phase: 06-consumer-readiness-ci*
*Completed: 2026-10-17*
