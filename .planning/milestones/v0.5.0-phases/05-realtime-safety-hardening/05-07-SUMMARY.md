---
phase: 05-realtime-safety-hardening
plan: 07
subsystem: testing
tags: [multi-instance, spatial-map, typeface, shared-resource-pointer, threadsanitizer, osd-131, catch2, ui]

requires:
  - phase: 05-realtime-safety-hardening
    provides: "Plan 05-06 (engine side of VERIFY-02), 05-04 build-tsan tree and SPATIALCORE_TEST_TIMEOUT_SCALE use"
  - phase: 04-ui-fonts
    provides: "SpatialMapComponent owns its embedded JetBrains Mono faces (D-05, D-06, IN-05); FontProvenanceTests"
provides:
  - "SpatialMapComponent::getSharedFaceIdentityForTesting() const noexcept and static getSharedFaceConstructionCountForTesting() noexcept: read-only, TEST-ONLY"
  - "[ui][multi][tracer]: sixteen live maps cause exactly one MonoFaces construction, share one face identity, paint 0 differing pixels against the first; the faces are released with the last map and rebuilt once for the next"
  - "[ui][multi][churn]: 200 create/paint/destroy rounds beside four long-lived, repainted maps: no crash, construction count unchanged, long-lived images unchanged"
  - "SpatialCoreUITests clean under ThreadSanitizer (16 cases, 150 assertions, 0 warnings)"
affects: [05-08, VERIFY-02, OpenSpatialDelay migration]

plan_head_before: f8438a309e69826f060afc5e443d1c32f6b4a56f
plan_head_after: 494ebadde5ff43b6c9aea78c5eadc6546f29941f

actuals:
  tokens: 2500
  tasks: 2
  commits: 2

tech-stack:
  added: []
  patterns:
    - "A construction counter incremented in a shared resource's constructor plus an identity accessor lets a test prove sharing without changing behaviour"
    - "D-07: the regression test is shown red against the old behaviour by a scratch mutation, recorded, and restored with git checkout"

key-files:
  created:
    - tests/UI/MultiMapTests.cpp
  modified:
    - include/SpatialCore/UI/SpatialMapComponent.h
    - src/UI/SpatialMapComponent.cpp
    - tests/CMakeLists.txt

key-decisions:
  - "The test-only accessors are public and read-only (a face pointer identity and a relaxed-atomic count), in the same style as the existing getObjectScreenPos helpers, so no friend declaration or test hook macro is needed"
  - "The construction counter lives in the .cpp's anonymous namespace, not in the header, so the header gains no new state and no pixel or layout changes"

patterns-established:
  - "UI multi-instance tests run on the test thread, which is the message thread (GUI initialiser held by UITestMain.cpp); no UI object crosses threads"

requirements-completed: [VERIFY-02]

coverage:
  - id: D1
    description: "OSD#131's typeface-duplication failure mode has a regression test: sixteen live maps give one MonoFaces construction, one face identity and identical images, and the test goes red when the faces are per-instance"
    requirement: VERIFY-02
    verification:
      - kind: unit
        ref: "tests/UI/MultiMapTests.cpp#Multi-map: sixteen live maps share one face set and paint identically (OSD#131)"
        status: pass
    human_judgment: false
  - id: D2
    description: "Maps created, painted and destroyed 200 times beside four live maps never crash, keep one shared face set and leave the long-lived images unchanged"
    requirement: VERIFY-02
    verification:
      - kind: unit
        ref: "tests/UI/MultiMapTests.cpp#Multi-map: maps created, painted and destroyed beside four live maps never crash and keep one face set"
        status: pass
    human_judgment: false
  - id: D3
    description: "The UI suite is race-free under ThreadSanitizer"
    requirement: VERIFY-02
    verification:
      - kind: other
        ref: "SPATIALCORE_TEST_TIMEOUT_SCALE=10 ./build-tsan/tests/SpatialCoreUITests (exit 0, 0 ThreadSanitizer warnings, 16 cases, 150 assertions)"
        status: pass
    human_judgment: false
  - id: D4
    description: "SMLLookAndFeel and every map pixel are unchanged (D-12)"
    requirement: VERIFY-02
    verification:
      - kind: unit
        ref: "tests/UI/FontProvenanceTests.cpp ([ui][fonts], 7 cases, 86 assertions, unchanged and passing)"
        status: pass
    human_judgment: false

duration: ~20min
completed: 2026-10-06
status: complete
---

# Phase 5 Plan 07: Multi-map typeface sharing Summary

**OSD#131's typeface-duplication failure mode now has a regression test: sixteen live SpatialMapComponents cause one construction of the JetBrains Mono face set, share one face identity and paint identical images, and a per-instance face set turns the test red (construction delta 16, 15 of 16 maps with a different face).**

## Performance

- **Duration:** about 20 min (start 2026-10-06T02:22:08Z, end 02:42Z)
- **Tasks:** 2
- **Files modified:** 4 (1 created, 3 modified)
- **BASE:** `f8438a309e69826f060afc5e443d1c32f6b4a56f`

## Accomplishments

- Two read-only TEST-ONLY accessors on `SpatialMapComponent`, plus a relaxed-atomic construction counter in `MonoFaces::MonoFaces()`. Nothing else in the map changed: `git diff BASE -- src/UI/SpatialMapComponent.cpp` adds an include, the counter, the `fetch_add` line and the getter; `SMLLookAndFeel` is untouched.
- `[ui][multi][tracer]`: sixteen maps alive give a construction delta of exactly 1, one non-null face identity shared by all, 0 maps whose image differs from the first (0 differing pixels in total). Destroying them all and creating one more gives a delta of exactly 1 more, and the same image.
- `[ui][multi][churn]`: 200 rounds of create, paint, destroy beside four long-lived maps that are repainted every round: 0 crashes, the construction count is unchanged across the loop, and every long-lived repaint equals its reference pixel for pixel.
- The whole UI suite is clean under ThreadSanitizer.

## Task Commits

1. **Task 1 (tracer): sixteen live maps share one face set** - `d100a4c` (test)
2. **Task 2: churn beside four live maps** - `494ebad` (test)

**Plan metadata:** committed separately (docs: complete plan).

## D-07 Mutation Result (restored with `git checkout`, tree rebuilt)

| Mutation | Test | Result |
|----------|------|--------|
| `juce::SharedResourcePointer<MonoFaces> monoFaces_;` replaced by `std::unique_ptr<MonoFaces> monoFaces_ { std::make_unique<MonoFaces>() };` (Release) | `[ui][multi]` | **Red**: 1 test case failed, 3 of 10 assertions failed. Construction delta **16** (expected 1); **15** of 16 maps reported a face identity different from the first (all 16 identities distinct); after clearing, delta still **16**. The image checks stayed green (0 differing pixels): per-instance faces paint the same, so the construction count and identity are the proof, as the plan expected. |

After the restore, `git diff d100a4c -- include` is empty and Release `[ui][multi]` passes again (1 case, 10 assertions at that point; 2 cases, 17 assertions after Task 2).

## Verification Results

| Gate | Result |
|------|--------|
| Debug `[ui][multi]` / `[ui][fonts]` | 2 cases, 17 assertions / 7 cases, 86 assertions, all passed |
| Release `[ui][multi]` and `[ui][multi][tracer]` | 2 cases, 17 assertions / 1 case, 10 assertions, all passed (exit 0) |
| build-tsan `SpatialCoreUITests` (scale 10, `halt_on_error=0 second_deadlock_stack=1`, log `/tmp/sc-p5-ui-tsan.log`) | exit 0, **0** `WARNING: ThreadSanitizer`, 16 test cases, 150 assertions, **44 s** run (51 s with the incremental rebuild) |
| Debug `SpatialCoreTests` (full) | 386 test cases, 328,307 assertions, all passed (exit 0) |
| Debug `SpatialCoreUITests` | 16 test cases, 150 assertions, all passed |
| Release `ctest --test-dir build-release/tests` | 402 of 402 passed (115.68 s); `ctest -N | grep -c 'ui:'` prints 16; `grep -c 'ui:.*Multi-map'` prints 2 |
| `tests/CMakeLists.txt` CRLF | `git ls-files --eol` reports `w/crlf`; diff vs BASE is 1 added line |
| `git diff BASE -- src/UI/SMLLookAndFeel.cpp include/SpatialCore/UI/SMLLookAndFeel.h` | empty |

## Decisions Made

- The counter and its getter sit in the `.cpp`, with only the getter declared in the header, so no new member state is added to the class.
- The identity accessor returns the regular face only; the three faces are constructed together, so one is enough to show sharing.

## Deviations from Plan

None - plan executed exactly as written.

(Process note: the churn test counts a paint that produces a wrong-sized image as a failure in addition to the pixel comparison; this is an extra check inside the planned test, not a scope change.)

**Total deviations:** 0.
**Impact on plan:** None.

## Issues Encountered

- A Debug `SpatialCoreTests` run still prints "Leaked objects detected: 7 instance(s) of class FFT" and a JUCE assertion line at exit with exit status 0. This is the known SharedFFTCache life-of-the-process static documented in Plan 05-06, not something this plan touches.

## Known Stubs

None.

## Threat Flags

None. The two new accessors are public, read-only and test-only (T-05-18, accepted); no new endpoint, auth path, file access or schema change. T-05-17 is mitigated and shown red by the D-07 mutation.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- VERIFY-02 (engine side in 05-06, UI side here) is done. Ready for 05-08.
- Not claimed: a real DAW with several plugin windows open, or the CoreGraphics crash itself (this proves sharing of the face set, which is the cause the issue's own analysis names, not the crash path). Windows and Linux font backends are not exercised.

## Self-Check: PASSED

- Created file exists: `tests/UI/MultiMapTests.cpp`.
- Commits `d100a4c` and `494ebad` found; `git rev-list --count f8438a3..HEAD` was 2 before this SUMMARY.
- All task acceptance criteria re-run and passing (see Verification Results); plan-level verification 1 to 3 satisfied.

---
*Phase: 05-realtime-safety-hardening*
*Completed: 2026-10-06*
