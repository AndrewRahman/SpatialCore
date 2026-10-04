---
phase: 03-binaural-defects-hrtf-packaging
plan: 08
subsystem: build
tags: [libmysofa, cmake, fetchcontent, sofa, parser-hardening]

requires:
  - phase: 03-binaural-defects-hrtf-packaging
    provides: golden fingerprints, IR-length and ITD characterisation pins (03-02, 03-03), embedded profiles (03-05), loudness table (03-01)
provides:
  - SpatialCore pins libmysofa v1.3.5 (6cc5b15a73e9bd97810d03767082edda7f315881)
  - 03-08-MYSOFA-DELTA.md, the before/after record of every SOFA-derived pinned value, with the user's decision
  - CMake fix that publishes libmysofa's generated mysofa_export.h to SpatialCore
affects: [03-09, OSD libmysofa pin, Phase 5 RTSF]

actuals:
  tokens: 4000
  tasks: 3
  commits: 2
plan_head_before: 8e11ac0932c0fc43636ba62c0bf5fa28cc359476
plan_head_after: c1c2e42c1db2276610d97426dd2c10df08180241
commits: 2

tech-stack:
  added: [libmysofa v1.3.5 (replaces v1.3.2)]
  patterns: ["ask the mysofa-static target for BINARY_DIR instead of assuming where its generated header lives, so a consumer-supplied copy also works"]

key-files:
  created: [.planning/phases/03-binaural-defects-hrtf-packaging/03-08-MYSOFA-DELTA.md]
  modified: [CMakeLists.txt]

key-decisions:
  - "2026-10-04, user decision keep-upgrade (D-17): SpatialCore pins libmysofa v1.3.5; no pinned value moved, so nothing was re-baselined"
  - "mysofa_export.h include folder is taken from get_target_property(mysofa-static BINARY_DIR), not a hardcoded path, so OSD-supplied copies keep working"

patterns-established:
  - "Measure a dependency bump in a disposable worktree and build tree; the main tree's pin moves only after the user decides"

requirements-completed: [DATA-01, EXTR-02]

coverage:
  - id: D1
    description: "libmysofa v1.3.5 evaluated against v1.3.2: every SOFA-derived pinned value measured before and after in Debug and Release, table written, 0 of 140 moved"
    requirement: "DATA-01"
    verification:
      - kind: other
        ref: ".planning/phases/03-binaural-defects-hrtf-packaging/03-08-MYSOFA-DELTA.md"
        status: pass
    human_judgment: false
  - id: D2
    description: "SpatialCore builds and pins libmysofa v1.3.5; full Debug suite and Release ctest pass with no test value edited"
    requirement: "EXTR-02"
    verification:
      - kind: unit
        ref: "./build/tests/SpatialCoreTests (253 cases, exit 0)"
        status: pass
      - kind: unit
        ref: "ctest --test-dir build-release/tests (253/253)"
        status: pass
    human_judgment: false
  - id: D3
    description: "The version choice was put to the user with the exact before/after table and answered keep-upgrade"
    verification: []
    human_judgment: true
    rationale: "A user decision (D-17); recorded with its date, not something a test asserts"

duration: 39min
completed: 2026-10-04
status: complete
---

# Phase 3 Plan 08: libmysofa v1.3.5 Summary

**libmysofa moved from v1.3.2 to v1.3.5 (parser hardening for user-supplied SOFA files) with 0 of 140 pinned values changed, one CMake include-path fix, and the user's keep-upgrade decision on record.**

## Performance

- **Duration:** about 39 min across the measurement session and this continuation
- **Completed:** 2026-10-04T15:42Z
- **Tasks:** 3 (tracer, decision checkpoint, apply)
- **Files modified:** 2 (`CMakeLists.txt`, the delta note)

## Accomplishments

- Measured v1.3.5 against v1.3.2 in Debug and Release: golden fingerprints (45 values), IR lengths and positions, the 80-value ITD table, embedded IR lengths and the loudness numbers are all identical; 0 of 140 pinned values and 0 of 19 loudness numbers moved. No golden, constant or tolerance was edited.
- Found that v1.3.5 does not compile here as it stands (`mysofa_export.h` is generated in libmysofa's own build folder and not published) and fixed it with a 3-line CMake change that asks `mysofa-static` for its build directory; works with a consumer-supplied copy and is harmless on v1.3.2.
- Recorded a first piece of evidence for the hardening: of 400 damaged copies of `hutubs_pp2.sofa`, one made v1.3.2 hang until killed and v1.3.5 rejected it cleanly; the other 399 behaved the same in both.
- The user answered `keep-upgrade` on 2026-10-04; `CMakeLists.txt` pins `GIT_TAG v1.3.5`, `build/_deps/mysofa-src` is `6cc5b15`, and the guard comment names v1.3.5.

## Task Commits

1. **Task 1: Tracer, measure v1.3.5 in a disposable copy and write the table** - `dc4381f` (docs)
2. **Task 2: User decision** - no commit; answered `keep-upgrade` on 2026-10-04 (recorded in the delta note's Decision section)
3. **Task 3: Pin v1.3.5, add the include-path fix, record the decision, prove both suites** - `c1c2e42` (build)

**Plan metadata:** the docs commit that follows this file.

BASE for this plan: `8e11ac0`.

## Files Created/Modified

- `CMakeLists.txt` - `GIT_TAG v1.3.5`, rewritten guard comment, `get_target_property(... mysofa-static BINARY_DIR)` plus a private include directory
- `.planning/phases/03-binaural-defects-hrtf-packaging/03-08-MYSOFA-DELTA.md` - the before/after tables, findings, and the dated Decision section

## Decisions Made

- Decision (2026-10-04, user): `keep-upgrade`. Nothing moved, so `upgrade-and-update-numbers` would have been identical.
- The include folder is discovered from the target rather than hardcoded, so the OSD guard path (`if(NOT TARGET mysofa-static)`) still works.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] v1.3.5 needs `mysofa_export.h` on the include path**
- **Found during:** Task 1 (measurement build)
- **Issue:** `fatal error: 'mysofa_export.h' file not found` in `HRTFDatabase.cpp`, Debug and Release
- **Fix:** the 3-line `BINARY_DIR` include fix, proven in the throwaway worktree in Task 1 and applied to the main tree in Task 3
- **Files modified:** `CMakeLists.txt`
- **Verification:** Debug 253 of 253 and Release 253 of 253 on v1.3.5
- **Committed in:** `c1c2e42`

---

**Total deviations:** 1 auto-fixed (1 blocking)
**Impact on plan:** Required for the upgrade to build at all; no scope creep. The plan's Task 3 text did not mention it because the problem was only discovered by the Task 1 measurement.

## Issues Encountered

None beyond the build fix. Debug runs print the known `juce_LeakedObjectDetector.h` FFT assertion at exit (3 instances, tracked since Plan 03-01, predates this plan); exit status is 0 and all tests pass.

## Verification Results

- Debug: `All tests passed (309062 assertions in 253 test cases)`, exit 0, against libmysofa `6cc5b15a73e9bd97810d03767082edda7f315881`.
- Release: `100% tests passed out of 253` via ctest.
- `git diff 8e11ac0..HEAD -- tests` is empty: no test value was edited.

## Deferred Items / Follow-ups

- **OSD libmysofa pin bump (not done, by design):** OpenSpatialDelay pins its own libmysofa v1.3.2 under the name `libmysofa`, so the guard in SpatialCore's `CMakeLists.txt` skips SpatialCore's fetch there and OSD ships the old parser. Bump OSD's own pin in the OSD repo; also confirm OSD's build gets `mysofa_export.h` (the new include-path line handles it from the SpatialCore side via the target's `BINARY_DIR`, which should be verified in an OSD scratch build).
- The damaged-file sample (400 mutations) is evidence, not a security audit; deeper hardening review remains Phase 5 (RTSF) scope.

## User Setup Required

None - no external service configuration required.

## Known Stubs

None.

## Threat Flags

None. T-03-18 (silent re-baseline) mitigated: nothing re-baselined and the user saw the table before any pin change. T-03-19 (unhardened parser on shared-folder files) mitigated by adopting v1.3.5. T-03-SC mitigated: tag resolution and fetched `rev-parse HEAD` both equal `6cc5b15a73e9bd97810d03767082edda7f315881`.

## Next Phase Readiness

- Ready for Plan 03-09. The suite is green on v1.3.5 in both build types.
- Remaining concern: the OSD pin follow-up above.

## Self-Check: PASSED

- FOUND: `.planning/phases/03-binaural-defects-hrtf-packaging/03-08-MYSOFA-DELTA.md`, `CMakeLists.txt` (`GIT_TAG v1.3.5`)
- FOUND commits: `dc4381f`, `c1c2e42`
- `git -C build/_deps/mysofa-src describe --tags` prints `v1.3.5`; `git worktree list` no longer lists `/tmp/sc-mysofa135-src`

---
*Phase: 03-binaural-defects-hrtf-packaging*
*Completed: 2026-10-04*
