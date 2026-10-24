---
phase: 04-control-surface-ui
plan: 07
subsystem: ui
tags: [demo-app, juce-gui-app, screenshots, adm-osc, trajectory, spatial-map, render-engine, cmake-option]

requires:
  - phase: 04-control-surface-ui
    provides: "ADM-OSC receive and query reply (04-03), 30 Hz sender with queueReply (04-02), trajectory reverse pins (04-04), SpatialCoreUITests and headless GUI listener (04-05), embedded map fonts (04-06)"
provides:
  - "SPATIALCORE_BUILD_EXAMPLES option (default OFF, top-level builds only) and the SpatialCoreDemo juce_add_gui_app target in examples/"
  - "SpatialCoreDemo --screenshots <dir>: headless before-drag.png and after-drag.png (480x480) plus a report of object 1 position and rendered L/R before and after the drag"
  - "DemoComponent: the worked consumer example of ADM-OSC in (NaN merge, queries answered via ADMOSCSender::queueReply), a 60 Hz timer ticking TrajectoryEngine and ADMOSCSender, map drag, and an atomics-only audio callback rendering with engineComputesGains and engineDerivesDispatch"
  - "SpatialCoreDemo --selftest: proves osc, query, trajectory and map through RenderEngine on loopback 9790/9791"
affects: [04-08, phase-06-harness]

plan_head_before: d2056768a035d26e28bc560cb9cc28515f5881d2
plan_head_after: 71a3358ef3682ba68dcafa4e76d0dc320d717f74

actuals:
  tokens: 10400
  tasks: 2
  commits: 2

tech-stack:
  added: []
  patterns:
    - "Off-by-default example target: option(... OFF) plus add_subdirectory guarded by PROJECT_IS_TOP_LEVEL, so a consumer that add_subdirectory()s SpatialCore never sees it"
    - "Consumer glue for RenderEngine: message thread writes per-object std::atomic positions (from OSC, trajectory timer and map drag); the audio callback only loads them and calls renderBlock with both opt-in flags set"
    - "Headless JUCE app modes: work runs in initialise(), the return value is set before quit(), snapshots come from createComponentSnapshot with no window or audio device"

key-files:
  created:
    - examples/CMakeLists.txt
    - examples/demo/Main.cpp
    - examples/demo/DemoComponent.h
    - examples/demo/DemoComponent.cpp
  modified:
    - CMakeLists.txt

key-decisions:
  - "The demo owns its glue and keeps TrajectoryEngine off the audio thread: the 60 Hz timer copies getFinalAz/El/Dist into atomics, and the demo sets TrajectoryState::reverse itself because getState() always reports false (Pitfall 3, D-12)"
  - "Origin model: OSC input and map drags move the trajectory origin, so a running trajectory carries on from where the object was dropped; a fixed (None) object follows OSC at once"
  - "The tracer's human-check (D-03) is deferred to the end-of-phase review (Plan 04-08), as the plan text and human_verify_mode=end-of-phase direct; the tracer's automated verify was re-run and passed before expansion"

requirements-completed: [EXTR-05, EXTR-04]

coverage:
  - id: D1
    description: "SpatialCoreDemo exists only behind SPATIALCORE_BUILD_EXAMPLES (default OFF, top-level only); the default build/ tree has no demo target and no file under src/ or include/ changed"
    requirement: "EXTR-04"
    verification:
      - kind: other
        ref: "cmake --build build --target help | grep -c SpatialCoreDemo (prints 0); git diff --name-only d205676..HEAD -- src include (prints nothing)"
        status: pass
    human_judgment: false
  - id: D2
    description: "--screenshots <dir> writes before-drag.png and after-drag.png headless (480x480 PNG) and reports after-drag az=90 with L/R 2.82 (>= 2.0)"
    requirement: "EXTR-05"
    verification:
      - kind: other
        ref: "build/demo/SpatialCoreDemo --screenshots .context/sc-shots; awk on build/demo/shots.log (after-drag L/R >= 2.0)"
        status: pass
    human_judgment: false
  - id: D3
    description: "The screenshots show the map correctly: three objects (one above and bright, one below and faded, one at ear level), distance rings, SML fonts, and the dragged object at its new position"
    requirement: "EXTR-05"
    verification: []
    human_judgment: true
    rationale: "D-03: one plain-language approval of the two screenshots by the user at the end-of-phase review (Plan 04-08); automated pixel tests already cover the same facts"
  - id: D4
    description: "The demo is the worked example of all three control routes and the query reply reaching RenderEngine, proven by its own self-test, with a lock-free, allocation-free audio callback"
    requirement: "EXTR-04"
    verification:
      - kind: e2e
        ref: "build/demo/SpatialCoreDemo --selftest: SELFTEST PASS osc, query, trajectory, map; SELFTEST RESULT PASS"
        status: pass
      - kind: other
        ref: "awk over ::renderAudio and ::audioDeviceIOCallbackWithContext | grep -cE 'ScopedLock|new |push_back|resize|DBG|Logger' prints 0 (76-line range)"
        status: pass
    human_judgment: false

duration: 21min
completed: 2026-10-05
status: complete
---

# Phase 4 Plan 07: Demo App Summary

**Off-by-default SpatialCoreDemo JUCE app that writes the before/after-drag map screenshots headless and is a self-tested worked example of ADM-OSC (with query reply), trajectories and map drag driving RenderEngine through atomics**

## Performance

- **Duration:** 21 min
- **Started:** 2026-10-05T09:20:00Z
- **Completed:** 2026-10-05T09:40:10Z
- **Tasks:** 2
- **Files modified:** 5 (4 created under `examples/`, 1 modified: root `CMakeLists.txt`)
- **BASE:** `d2056768a035d26e28bc560cb9cc28515f5881d2` (HEAD when this plan started)

## Accomplishments

- `SPATIALCORE_BUILD_EXAMPLES` (OFF) in the root `CMakeLists.txt`, with `add_subdirectory(examples)` only in a top-level build. The default `build/` tree (and `build-release/`) cache the option as OFF and list no `SpatialCoreDemo` target.
- `--screenshots <dir>` builds the map with no window and no audio device, writes `before-drag.png` and `after-drag.png`, and prints the object 1 position and rendered L/R before and after the drag.
- `DemoComponent` wires the three routes the way a plugin does: ADM-OSC receive with the NaN merge and queries answered through `ADMOSCSender::queueReply`; a 60 Hz timer that ticks `TrajectoryEngine` (setting `reverse` itself) and `ADMOSCSender::tick`; map drag; and an audio callback that reads only atomics and renders Binaural with `engineComputesGains` and `engineDerivesDispatch`. Interactive mode opens the default audio device and connects ADM-OSC on 4002 in / 4003 out (`--osc-in`, `--osc-out`), checked with `oscPortsConflict` first. A small row (shape, speed, reverse) edits the selected object's trajectory.
- `--selftest` passes all four checks and exits 0.

### Screenshots (for the end-of-phase review, D-03)

Both are 480x480 PNGs, written to `.context/sc-shots/` (gitignored, which is also the `.context/` copy the build notes ask for):

- `/Users/andrewrahman/conductor/workspaces/SpatialCore/gwangju-v1/.context/sc-shots/before-drag.png` : the black top-down map with five concentric distance rings (labelled 0.25, 0.50, 0.75, 1.00 on the right and 1m, 5m, 11m, 20m on the left), F/B/L/R edge labels and a faint star field. Object 1 (red, large, labelled "+30 deg") sits straight ahead at about half radius; object 2 (small orange ring) is on the right-rear at 0.7 radius, drawn small and dim because it is 60 degrees below the listener; object 3 (green) is on the left-rear near the centre at 0.3 radius. Labels are in the embedded monospace face.
- `/Users/andrewrahman/conductor/workspaces/SpatialCore/gwangju-v1/.context/sc-shots/after-drag.png` : the same scene with object 1 moved to the left edge at 0.8 radius (azimuth 90), still labelled "+30 deg" because the map edits azimuth and distance only (D-04). Objects 2 and 3 are unchanged.

Honest note for the reviewer: object 2's "faded" look is subtle (a small dim ring) rather than a strong fade. The plan's expected text ("a second object drawn faded") is met, but a human may want to judge it.

### Report and self-test output

`build/demo/shots.log`:
```
before-drag: object 1 az=0.0 el=30.0 d=0.50 L/R=1.00
after-drag: object 1 az=90.0 el=30.0 d=0.80 L/R=2.82
```

`build/demo/selftest.log`:
```
SELFTEST PASS osc az=90.0 L/R=3.31
SELFTEST PASS query xyz=(-0.50, -0.00, 0.00)
SELFTEST PASS trajectory forward az=90.0 L/R=3.31 reverse az=-90.0 L/R=0.30
SELFTEST PASS map az=90.0 L/R=3.31
SELFTEST RESULT PASS
```

## Task Commits

1. **Task 1: Tracer, off-by-default demo target that writes before/after-drag screenshots** - `43881bb` (feat)
2. **Task 2: Interactive worked example and a self-test of all three routes** - `71a3358` (feat)

**Plan metadata:** recorded in the docs commit that follows this file (docs: complete plan).

Tracer gate: after Task 1 the automated verify (build, screenshots, ratio, default build has no demo target) was re-run end to end and passed, so expansion went ahead. The `human-check` is the D-03 look at the end-of-phase review, per the plan and `human_verify_mode: end-of-phase`.

## Files Created/Modified

- `CMakeLists.txt` - `SPATIALCORE_BUILD_EXAMPLES` option (OFF) and the guarded `add_subdirectory(examples)`
- `examples/CMakeLists.txt` - `juce_add_gui_app(SpatialCoreDemo)`, links `SpatialCoreUI`, `SpatialCore`, `juce::juce_audio_devices`
- `examples/demo/Main.cpp` - application with `--screenshots`, `--selftest` and interactive modes; the return value is set before `quit()`
- `examples/demo/DemoComponent.h` / `.cpp` - the worked example and its offline render, screenshot and self-test routines

## Decisions Made

- The demo copies trajectory output into atomics on the message thread so the audio callback never reads `TrajectoryEngine` (RTSF-02 stays a Phase 5 item), and sets `TrajectoryState::reverse` itself (Pitfall 3).
- OSC input and map drags move the trajectory origin; a fixed object follows OSC immediately.
- Tone amplitude is 0.2 per object so four tones cannot clip; L/R ratios do not depend on level.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Self-test map check dragged the wrong object**
- **Found during:** Task 2 (first `--selftest` run: `SELFTEST FAIL map az=0.0 L/R=1.00`)
- **Issue:** After the earlier checks, objects 1, 2 and 4 all sat at azimuth 0 / distance 0.5, so the mouse-down on object 3 hit another object and object 3 never moved. The fault was in the test scenario, not in the map.
- **Fix:** the map check selects the object first (`objectSelected`); the map hit-tests the selected object first because it is drawn on top.
- **Files modified:** `examples/demo/DemoComponent.cpp`
- **Verification:** `--selftest` passes all four checks; the demo build was re-run.
- **Committed in:** `71a3358` (Task 2 commit)

---

**Total deviations:** 1 auto-fixed (1 bug in new example code, caught before commit)
**Impact on plan:** None on scope. No library file changed.

## Issues Encountered

- The Debug test run still prints the known "Leaked objects detected: ... FFT" line at exit (`SharedFFTCache` static; already in STATE.md Deferred Items). The exit code and test results are unaffected. Debug leak-detector output from the demo itself is the expected noise the plan names.

## Verification

| Gate | Result |
|---|---|
| `SpatialCoreDemo --screenshots .context/sc-shots` | exit 0; both PNGs 480x480; after-drag L/R 2.82 (>= 2.0), az 90.0 |
| `SpatialCoreDemo --selftest` | exit 0; four PASS lines and `SELFTEST RESULT PASS` |
| `cmake --build build --target help \| grep -c SpatialCoreDemo` | 0 (option cached OFF in `build/` and `build-release/`) |
| Audio-callback grep (76-line range, comments excluded) | 0 hits |
| `grep -c oscPortsConflict` / `grep -c queueReply` in `DemoComponent.cpp` | 1 / 1 |
| `git diff --name-only d205676..HEAD -- src include` | empty |
| Debug `SpatialCoreTests` | All tests passed (303 test cases) |
| Debug `SpatialCoreUITests` | All tests passed (13 test cases) |
| Debug `ctest` (build/tests) | 100%, 316/316 |
| Release `ctest` (build-release/tests) | 100%, 316/316 |

Debug and Release suites were run one after the other, never together.

## Known Stubs

None. The demo's tone generator is intentionally a test source (a sine per object), not a placeholder for missing data.

## Threat Flags

None. The only new surface is the demo's own ADM-OSC receiver (interactive mode), which is the library surface hardened in Plan 04-03, and `--screenshots <dir>`, which T-04-15 already covers (it writes only `before-drag.png` and `after-drag.png`, creates the folder if needed, never deletes).

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- Ready for 04-08. At the end-of-phase review Claude shows the two screenshots above with a one-line caption each and asks for one plain-language approval (D-03). EXTR-05 stays pending that approval; Plan 04-08 owns the REQUIREMENTS.md ticks (D-16), so `requirements mark-complete` was not run here.
- To rebuild the demo: configure a separate tree with `-DSPATIALCORE_BUILD_EXAMPLES=ON -DSPATIALCORE_BUILD_TESTS=OFF` (the `build/demo` tree used here is inside the gitignored `build/`).

---
*Phase: 04-control-surface-ui*
*Completed: 2026-10-05*

## Self-Check: PASSED

- FOUND: `CMakeLists.txt`, `examples/CMakeLists.txt`, `examples/demo/Main.cpp`, `examples/demo/DemoComponent.h`, `examples/demo/DemoComponent.cpp`
- FOUND: commits `43881bb` and `71a3358` (`git log --grep="04-07"` returns both); `commits: 2` measured with `git rev-list --count d205676..HEAD`
- FOUND: `.context/sc-shots/before-drag.png` and `.context/sc-shots/after-drag.png`
- All task acceptance criteria re-run and passing (table above)
