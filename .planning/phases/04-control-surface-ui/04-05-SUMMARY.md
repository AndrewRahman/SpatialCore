---
phase: 04-control-surface-ui
plan: 05
subsystem: testing
tags: [ui, spatial-map, catch2, juce-gui, headless, render-engine, ctest]

requires:
  - phase: 04-control-surface-ui
    provides: "RouteRenderRig (04-01), the configured Debug and Release trees"
provides:
  - "SpatialCoreUITests, the only test executable that links SpatialCoreUI, with ctest names prefixed 'ui:'"
  - "A Catch2 listener that owns juce::ScopedJuceInitialiser_GUI for the run, so components build and paint with no window"
  - "UITestSupport.h helpers: makeMouseEvent, renderToImage, rgbSum, maxChannelDiff, mapPointFor, dragObject"
  - "[ui][route][map][tracer]: a map drag moves rendered audio through a host-style Listener and RenderEngine"
  - "Three [ui][map][drag] and three [ui][map][pixels] cases for SpatialMapComponent"
affects: [04-06, 04-07]

plan_head_before: 49dc045ad7fd6013bff3aa11a09028a62d85be3f
plan_head_after: 1706bcfde7e86ca80bce4ed5174885609f4ebec7

actuals:
  tokens: 4400
  tasks: 2
  commits: 2

tech-stack:
  added: []
  patterns:
    - "UI components are tested headless: constructed with no window, driven by synthesized MouseEvents, painted into an Image"
    - "The JUCE GUI initialiser lives in a Catch2 EventListenerBase (testRunStarting / testRunEnded), so test discovery never starts it"
    - "UI route proof is rendered L/R level through RenderEngine with both opt-in flags, never a callback count (D-11, D-18)"
    - "Pixel assertions use relative brightness (RGB sum ratios and margins), not exact values, so glyph antialiasing cannot flake them"

key-files:
  created:
    - tests/UI/UITestMain.cpp
    - tests/UI/UITestSupport.h
    - tests/UI/MapRouteTests.cpp
    - tests/UI/SpatialMapComponentTests.cpp
  modified:
    - tests/CMakeLists.txt

key-decisions:
  - "SpatialCoreUITests gets JUCE_UNIT_TESTS=1 only, no JUCE_MODAL_LOOPS_PERMITTED, because UI tests never pump the message loop"
  - "The ring-brightness case samples a 70 degree screen ray, clear of the ring labels (30 and 210 degrees), the cardinal marks and the dashed crosshairs"

patterns-established:
  - "MapToRigGlue (test-local): objectPositionChanged copies (index, azimuth, distance) into RouteRenderRig::mergeObject with elevation NaN, so elevation is kept (D-04)"
  - "RecordingListener / MapFixture (test-local): counts and last values for the map's Listener and drag callbacks"

requirements-completed: [EXTR-05]

coverage:
  - id: D1
    description: "Dragging object 0 on SpatialMapComponent to the left at radius 0.8 makes the rendered L/R ratio 2.82 (threshold >= 2.0), and dragging to the right makes it 0.355 (threshold <= 0.5), through a host-style Listener and RenderEngine with both opt-in flags"
    requirement: EXTR-05
    verification:
      - kind: integration
        ref: "tests/UI/MapRouteTests.cpp#Map route: dragging an object to the left makes the left channel louder"
        status: pass
    human_judgment: false
  - id: D2
    description: "A synthesized drag reports azimuth 90.00 and distance 0.800, fires objectSelected once and onDragStarted / onDragEnded once, leaves elevation at 30, clamps a drag beyond the outer ring to distance exactly 1.0, and moves nothing from empty space or a disabled object"
    requirement: EXTR-05
    verification:
      - kind: unit
        ref: "tests/UI/SpatialMapComponentTests.cpp#[ui][map][drag] (three cases)"
        status: pass
    human_judgment: false
  - id: D3
    description: "The painted map shows the dot at the new position and gone from the old one, the dot at d * 0.45 * min(w,h) from the centre for d = 0.25 to 1.0, a ring pixel at least 15 RGB-sum brighter than the void beside it, and a below-horizon source drawn at 195 against 425 above (0.46x, threshold 0.6x)"
    requirement: EXTR-05
    verification:
      - kind: unit
        ref: "tests/UI/SpatialMapComponentTests.cpp#[ui][map][pixels] (three cases)"
        status: pass
    human_judgment: false
  - id: D4
    description: "The UI tests join the local gate: Debug runs both executables and the Release ctest lists seven 'ui:' tests and passes 310 of 310"
    verification:
      - kind: other
        ref: "./build/tests/SpatialCoreTests; ./build/tests/SpatialCoreUITests; ctest --test-dir build-release/tests"
        status: pass
    human_judgment: false
  - id: D5
    description: "No UI header or source holds a concrete processor pointer, and no file under src/ or include/ changed"
    verification:
      - kind: other
        ref: "DR-16 grep over src/UI and include/SpatialCore/UI (no output); git diff --name-only 49dc045 HEAD -- src include (empty)"
        status: pass
    human_judgment: false
  - id: D6
    description: "Behaviour with no logged-in macOS display session (SSH, launchd, CI) is not proven; the gate is run from the developer's session"
    verification: []
    human_judgment: true
    rationale: "RESEARCH A1: the harness ran from a logged-in terminal; a no-WindowServer run was not tested"

duration: 14min
completed: 2026-10-05
status: complete
---

# Phase 4 Plan 05: UI Test Target and Map Route Summary

**SpatialCoreUITests, the first target to link SpatialCoreUI, drives SpatialMapComponent headless: a synthesized drag moves RenderEngine's L/R ratio from 2.82 to 0.355, and the map's dot, distance rings and elevation opacity are asserted from painted pixels**

## Performance

- **Duration:** about 14 min
- **Completed:** 2026-10-05T09:07Z
- **Tasks:** 2
- **Files modified:** 5 (4 created, 1 modified), all under `tests/`
- **BASE:** `49dc045ad7fd6013bff3aa11a09028a62d85be3f`

## Accomplishments

- A new executable, `SpatialCoreUITests`, links `SpatialCoreUI` and `SpatialCore`. `SpatialCoreTests` still neither compiles `UI/` sources nor links `SpatialCoreUI` (checked by the awk acceptance greps, both 0).
- `UITestMain.cpp` holds a `juce::ScopedJuceInitialiser_GUI` from a Catch2 listener for the whole run. Components construct with no window, take mouse events and paint into an `Image`, with no message-loop pumping.
- Tracer (ROADMAP criterion 4, interaction half): a host-style `MapToRigGlue` copies `objectPositionChanged` into `RouteRenderRig` (elevation kept as NaN, D-04), and the sound follows the drag through `RenderEngine` with `engineComputesGains` and `engineDerivesDispatch` set.
- Six map cases: drag values and callbacks, the outer-ring clamp and centre, empty space and disabled objects, the dot moving with the drag, elevation opacity, and the ring and dot-radius geometry.
- The UI tests are in the local gate: Debug runs both executables and the Release ctest lists and passes seven `ui:` tests.

## Measured values (BASE = `49dc045`)

| Case | Measured | Threshold |
|---|---|---|
| Tracer, drag object 0 to az +90, d 0.8 | L/R 2.8205 | >= 2.0 |
| Tracer, drag object 0 to az -90, d 0.8 | L/R 0.3545 | <= 0.5 |
| Drag report | az 90.0, d 0.800000012 | az 90 +- 0.01, d 0.8 +- 0.001 |
| Elevation after the drag | 30 | unchanged |
| Outside the outer ring (drag to x = 5) | d 1.0 exactly, az 90 | d == 1.0, az 90 +- 0.5 |
| Dot pixel at (x + 4, y), el 30 | `ffed5e5e` = `objectColours[0]` | within 8 per channel |
| Old position after the drag | `ff18191c` | more than 8 per channel from the object colour |
| Above (el +80) / below (el -80) pixel | `ffed5e5e` (RGB sum 425) / `ff5c4027` (RGB sum 195) | below < 0.6 x above (0.46x) |
| Dot offset from centre for d 0.25 / 0.5 / 0.75 / 1.0 | 45 / 90 / 135 / 180 px | d * 180 +- 0.5 |
| Ring brightest vs void (RGB sum), r 90 / 135 / 180 | 107 vs 6, 92 vs 8, 91 vs 7 | margin >= 15 |

The pixel values match the research measurements exactly (`ffed5e5e`, `ff5c4027`).

**ctest `ui:` count:** 7 in the Release listing (the tracer plus six map cases); Release ctest: `100% tests passed, 0 tests failed out of 310` (303 plus 7).

## Task Commits

1. **Task 1: UI test target and a map drag that moves the rendered sound** - `883015b` (test)
2. **Task 2: Map drag and pixel behaviour** - `1706bcf` (test)

**Plan metadata:** committed with this SUMMARY (docs: complete plan)

## Files Created/Modified

- `tests/UI/UITestMain.cpp` - Catch2 `EventListenerBase` owning `ScopedJuceInitialiser_GUI` (`CATCH_REGISTER_LISTENER`)
- `tests/UI/UITestSupport.h` - `makeMouseEvent`, `renderToImage`, `rgbSum`, `maxChannelDiff`, `mapPointFor`, `dragObject`
- `tests/UI/MapRouteTests.cpp` - `MapToRigGlue` and the tracer
- `tests/UI/SpatialMapComponentTests.cpp` - `RecordingListener`, `MapFixture`, three drag and three pixel cases
- `tests/CMakeLists.txt` - `SpatialCoreUITests` target (CRLF preserved) and `catch_discover_tests(SpatialCoreUITests TEST_PREFIX "ui: ")`

## Decisions Made

- No `JUCE_MODAL_LOOPS_PERMITTED` on `SpatialCoreUITests` (the research CMake sketch added it; the plan says UI tests never pump, and none does).
- The ring-brightness case samples the brightest pixel within 1 px of each ring radius on a 70 degree screen ray, against a void pixel 0.125 of the radius closer in. The star field is deterministic, so the case is not flaky.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] The `ui: ` ctest prefix loses its trailing space, so the plan's `grep -c 'ui: '` finds nothing**
- **Found during:** Task 2 (Release gate)
- **Issue:** `catch_discover_tests (... TEST_PREFIX "ui: ")` generates test names `ui:Map route: ...` with no space (Catch2 v3.7.1's discovery scripts pass the prefix through a `-D` argument and an unquoted forward, which drop the trailing whitespace). The plan's gate command `grep -c 'ui: '` therefore prints 0 against a listing that does contain the tests.
- **Fix:** kept the CMake line exactly as the plan specifies (`TEST_PREFIX "ui: "`, acceptance grep prints 1) and checked the listing with `grep -c 'ui:'`, which prints 7. The prefix still does its job: the names are unique across the two executables and every UI test is listed.
- **Files modified:** none
- **Verification:** `ctest --test-dir build-release/tests -N | grep -c 'ui:'` prints 7; `ctest` passes 310 of 310
- **Committed in:** not a source change

---

**Total deviations:** 1 (a gate-command pattern adjusted, no source change)
**Impact on plan:** none on scope. Later plans that grep the listing should use `ui:`.

## Issues Encountered

- The Debug `SpatialCoreTests` run still prints `*** Leaked objects detected: 3 instance(s) of class FFT` and a `juce_LeakedObjectDetector` assertion at exit; this is the known SharedFFTCache false positive (deferred from 02-04, expected noise per the plan).
- A stray `find /` I started while checking the Catch2 prefix script was killed; it changed nothing.

## Verification

- `./build/tests/SpatialCoreUITests "[ui][route][map][tracer]"`: All tests passed (4 assertions in 1 test case)
- `./build/tests/SpatialCoreUITests "[ui][map]"`: All tests passed (47 assertions in 7 test cases); `--list-tests` lists 7
- Full Debug `SpatialCoreTests`: All tests passed (323707 assertions in 303 test cases)
- Full Debug `SpatialCoreUITests`: All tests passed (47 assertions in 7 test cases)
- Release: `cmake --build build-release -j8` clean, `ctest --test-dir build-release/tests`: 100% tests passed out of 310
- Acceptance greps: `SpatialCoreUI` in `tests/CMakeLists.txt` 6, `TEST_PREFIX "ui: "` 1, `UI/` inside the `SpatialCoreTests` source list 0, `SpatialCoreUI` inside `SpatialCoreTests` link list 0, DR-16 grep over `src/UI` and `include/SpatialCore/UI` prints nothing, `git diff --name-only 49dc045 HEAD -- src include` prints nothing

## Known Stubs

None.

## Threat Flags

None. No network or file surface was added; the only input is synthesized mouse events on a component. T-04-12 is mitigated by the DR-16 grep (clean). T-04-13 is mitigated by building every target before `ctest` and by the `ui:` listing check.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- Ready for Plan 04-06 (fonts). `SpatialCoreUITests` is the home for its `FontProvenanceTests.cpp`; add the source to the target's list in `tests/CMakeLists.txt` and reuse `UITestSupport.h`.
- Open assumption (RESEARCH A1): the UI gate has only been run from a logged-in macOS session. A no-WindowServer run (SSH, launchd, CI) is unproven; CI compiles the target on Linux but does not run it.
- A drag to the exact centre reports azimuth -180 by IEEE `atan2 (-0, -0)`; the test asserts distance only (RESEARCH A4).

---
*Phase: 04-control-surface-ui*
*Completed: 2026-10-05*

## Self-Check: PASSED

- FOUND: tests/UI/UITestMain.cpp, tests/UI/UITestSupport.h, tests/UI/MapRouteTests.cpp, tests/UI/SpatialMapComponentTests.cpp
- FOUND: commits 883015b, 1706bcf
