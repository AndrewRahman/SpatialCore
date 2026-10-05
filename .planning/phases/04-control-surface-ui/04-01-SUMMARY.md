---
phase: 04-control-surface-ui
plan: 01
subsystem: testing
tags: [adm-osc, juce_osc, render-engine, catch2, udp, route-tests]

requires:
  - phase: 03-binaural-hrtf
    provides: "RenderEngine opt-in flags engineComputesGains (SC-13) and engineDerivesDispatch (SC-16); Simple binaural cue path"
provides:
  - "spatialcore::test::RouteRenderRig, the shared consumer-style RenderEngine rig for control-route tests"
  - "[route][osc][udp][tracer] real-UDP route test and five [route][osc] per-address tests"
  - "Configured Debug (build/) and Release (build-release/) trees for the rest of Phase 4"
affects: [04-02, 04-03, 04-04, 04-05]

plan_head_before: 17b688f64220ca0dd81dec01deec2e3eef849970
plan_head_after: 65bd6254ef1dad2d615432b9ea735a6250969d85

actuals:
  tokens: 4000
  tasks: 2
  commits: 2

tech-stack:
  added: []
  patterns:
    - "Control routes are proven by rendered channel level, never by a position value arriving (D-11)"
    - "OSC-to-engine glue lives in the test, the way a plugin writes it (D-12)"
    - "Every sound-moves render sets engineComputesGains and engineDerivesDispatch and hand-sets no dispatch field (D-18)"

key-files:
  created:
    - tests/Support/RouteRenderRig.h
    - tests/Engine/ControlRouteTests.cpp
  modified:
    - tests/CMakeLists.txt

key-decisions:
  - "JUCE_MODAL_LOOPS_PERMITTED=1 added to SpatialCoreTests only, so MessageManager::runDispatchLoopUntil can deliver MessageLoopCallback OSC messages in a test; no library target changes"
  - "Single-axis address tests (azim, elev, dist) also assert the NaN merge kept the other axes, since that merge is the consumer rule in ADMOSCReceiver.h"

patterns-established:
  - "RouteRenderRig: setObject / mergeObject (NaN-aware) / renderRms (last of 4 blocks) / leftRightRatio / loudestChannel"
  - "SyncRoute (test-local): receiver plus glue plus rig for socket-free address tests via testProcessOSCMessage"

requirements-completed: [EXTR-04]

coverage:
  - id: D1
    description: "An external OSC sender over real UDP (port 9720) moves a rendered object: left channel louder for azimuth +90, right louder for -90, through consumer glue and RenderEngine"
    requirement: EXTR-04
    verification:
      - kind: integration
        ref: "tests/Engine/ControlRouteTests.cpp#OSC route: an external ADM-OSC sender over UDP moves a rendered object left then right"
        status: pass
    human_judgment: false
  - id: D2
    description: "/azim, /elev, /dist, /aed and /xyz each move the rendered audio the right way on Binaural, /aed also on Quad, with the NaN merge keeping unsent axes"
    requirement: EXTR-04
    verification:
      - kind: integration
        ref: "tests/Engine/ControlRouteTests.cpp#[route][osc] (five cases)"
        status: pass
    human_judgment: false
  - id: D3
    description: "No file under src/ or include/ changed; both build trees configured; full Debug suite and Release ctest pass"
    verification:
      - kind: other
        ref: "git diff --name-only 17b688f HEAD -- src include (empty); ./build/tests/SpatialCoreTests; ctest --test-dir build-release/tests"
        status: pass
    human_judgment: false

duration: 16min
completed: 2026-10-05
status: complete
---

# Phase 4 Plan 01: ADM-OSC Route Tracer Summary

**Real-UDP ADM-OSC from an external juce::OSCSender moves a RenderEngine-rendered object (L/R 3.31 at azimuth +90, 0.30 at -90), with all five position addresses proven by channel level through a shared RouteRenderRig**

## Performance

- **Duration:** about 16 min wall (the Debug tree build ran in the background while the rig was written)
- **Completed:** 2026-10-05T08:07Z
- **Tasks:** 2
- **Files modified:** 3 (2 created, 1 modified), all under `tests/`

## Accomplishments

- Tracer: a plain `juce::OSCSender` stands in for a desk, sends `/adm/obj/1/aed` over loopback UDP to `ADMOSCReceiver` on port 9720, test-local glue merges the position into the rig, and `RenderEngine` renders with `engineComputesGains` and `engineDerivesDispatch` set. ROADMAP criterion 1 holds as written.
- Per-address proof (D-11) for `/azim`, `/elev`, `/dist`, `/aed` (Binaural and Quad) and `/xyz`, each asserting a rendered channel-level change, never merely that a value arrived.
- `RouteRenderRig` exists for Plans 04-04 (trajectory route) and 04-05 (map route).
- `build/` (Debug, `compile_commands.json`) and `build-release/` (Release) are configured and built; both are gitignored.

## Measured values (BASE = `17b688f`)

All at 48 kHz, 4 blocks of 64, RMS of the last block, both flags set. The tracer ratios match the research measurement exactly.

| Address | Setup | Measured | Threshold |
|---|---|---|---|
| `/aed` and `/azim` (Binaural) | az +90, el 0, dist 0.5 | L/R 3.311 | >= 2.0 |
| `/aed` and `/azim` (Binaural) | az -90 | L/R 0.302 | <= 0.5 |
| `/elev` (Binaural) | az +90, then elev 75 | L/R 3.311 to 1.363 | < 1.6 and > 1.0 |
| `/dist` (Binaural) | az +90, dist 0.2 to 0.9 | left RMS 0.3336 to 0.0910 (0.273x) | < 0.5x |
| `/xyz` (Binaural) | x -0.5 / x +0.5 | L/R 3.311 / 0.302 | >= 2.0 / <= 0.5 |
| `/aed` (Quad) | az +45 | ch0 0.1557, ch1-3 exactly 0 | ch0 loudest, others < 1% |
| `/aed` (Quad) | az -45 | ch1 0.1557, ch0, ch2, ch3 exactly 0 | ch1 loudest, others < 1% |

The Quad level is 0.1557 here, not the 0.3503 in the research note, because the rig supplies `distanceAttenuation (0.5)` as the surround distance gain; the channel selection and silence of the other channels are what the test asserts.

**Does the Simple binaural path read `distGainPerSample`?** No. `renderSimpleBinauralWoodworth` (`src/Engine/RenderEngine.cpp:1077-1172`) never references it; the reads are in the Ambisonics path (line 1295), the direct-binaural HRTF path (line 918) and the discrete-surround path (line 1355). On the Simple path distance reaches the sound through the engine-computed `objGains`, which is why `/dist` still moves the Binaural level.

## Task Commits

1. **Task 1: Tracer, real UDP /adm/obj/1/aed moves the rendered sound left then right** - `28de9ea` (test)
2. **Task 2: Every position address moves the rendered sound** - `65bd625` (test)

**Plan metadata:** committed with this SUMMARY (docs: complete plan)

## Files Created/Modified

- `tests/Support/RouteRenderRig.h` - header-only consumer-style rig around `RenderEngine`: `setObject`, NaN-aware `mergeObject`, `object`, `renderRms`, `leftRightRatio`, `loudestChannel`
- `tests/Engine/ControlRouteTests.cpp` - `OscToRigGlue`, `pumpUntil`, `SyncRoute`, the real-UDP tracer and five per-address cases
- `tests/CMakeLists.txt` - adds `Engine/ControlRouteTests.cpp` and `JUCE_MODAL_LOOPS_PERMITTED=1` to `SpatialCoreTests`

## Decisions Made

- `JUCE_MODAL_LOOPS_PERMITTED=1` is scoped to `SpatialCoreTests` only (JUCE 9.0.0 `juce_MessageManager.h:99-105`), so the library and its consumers are unaffected.
- The tracer holds one `ScopedJuceInitialiser_GUI` for its whole body, constructed first and destroyed last, and disconnects both sockets before it goes out of scope. Note for Plan 04-03: if its second real-UDP case fails to re-initialise JUCE in the same process, share one initialiser rather than dropping either test.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Most vexing parse in the OSC message helper**
- **Found during:** Task 1 (first compile)
- **Issue:** `juce::OSCMessage m (juce::OSCAddressPattern (juce::String (address)));` parsed as a function declaration, so the build failed.
- **Fix:** Brace initialisation, `juce::OSCMessage m { juce::OSCAddressPattern { juce::String (address) } };`
- **Files modified:** `tests/Engine/ControlRouteTests.cpp`
- **Verification:** compiles; tracer passes
- **Committed in:** `28de9ea`

**2. [Environment] First configure attempt failed with "Unknown CMake command juce_add_binary_data"**
- **Found during:** Task 1 (build trees)
- **Issue:** the first configure left a partial `build/`; `FetchContent` then populated `build/_deps` instead of reading the cached sources.
- **Fix:** removed the partial `build/` and `build-release/` (build output only, untracked) and reconfigured with the `$DEPS` overrides from the ground rules. Both configured cleanly.
- **Committed in:** not a source change

---

**Total deviations:** 1 auto-fixed (1 blocking compile error), plus one environment retry.
**Impact on plan:** none; no scope change and no file outside `tests/` touched.

## Issues Encountered

- The Debug test executable prints `*** Leaked objects detected: 3 instance(s) of class FFT` and a `juce_LeakedObjectDetector` assertion at exit; this is the known SharedFFTCache false positive (deferred item from 02-04). The process still exits 0.

## Verification

- `[route][osc][udp][tracer]`: All tests passed (8 assertions in 1 test case)
- `[route][osc]`: 6 matching test cases, all passed (36 assertions)
- `[osc]` (existing OSC tests): All tests passed (110 assertions in 27 test cases)
- Full Debug suite: All tests passed (309519 assertions in 272 test cases), exit 0 (baseline 266 plus 6 new)
- Release: `ctest --test-dir build-release/tests`: 100% tests passed out of 272
- Acceptance greps: `engineComputesGains = true` 1, `engineDerivesDispatch = true` 1, hand-set dispatch field 0, `Engine/ControlRouteTests.cpp` in CMake 1, `JUCE_MODAL_LOOPS_PERMITTED=1` present, `git diff --name-only 17b688f HEAD -- src include` empty

## Known Stubs

None.

## Threat Flags

None. The only network surface touched is the existing ADMOSCReceiver UDP input, exercised over loopback on port 9720 (T-04-01 accepted, T-04-02 mitigated by the dedicated port and by disconnecting both sockets before the JUCE initialiser ends).

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- Ready for Plan 04-02. `RouteRenderRig` is available for 04-04 and 04-05; the Debug and Release trees are built and reusable.
- Phase 4 plans must keep building every target (`cmake --build build -j8`) before `ctest`, as the plan's ground rules say, once 04-05 adds a second Catch2 executable.

---
*Phase: 04-control-surface-ui*
*Completed: 2026-10-05*

## Self-Check: PASSED

- FOUND: tests/Support/RouteRenderRig.h
- FOUND: tests/Engine/ControlRouteTests.cpp
- FOUND: commits 28de9ea, 65bd625
