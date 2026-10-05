---
phase: 04-control-surface-ui
plan: 04
subsystem: trajectory
tags: [trajectory, reverse, route-test, render-engine, catch2, osd-100]

requires:
  - phase: 04-control-surface-ui
    provides: "04-01 RouteRenderRig and the [route] test pattern; 04-03 hardened OSC input (T-04-06 bounds any azimuth before a consumer stores it as an origin)"
provides:
  - "[route][trajectory][tracer]: an Orbit ticked through TrajectoryEngine and copied into a Binaural RouteRenderRig moves the rendered sound left, then right in reverse (ROADMAP criterion 3 through the renderer)"
  - "[trajectory][reverse] identity sweep for the ten deterministic shapes (D-13)"
  - "Line and Bounce reverse pinned as intentional exceptions to D-13 (D-19, OSD#100), for VERIFICATION"
  - "[trajectory][tick][reverse] all-13-shape forward and reverse sweep through tick(), and a seeded Random case"
affects: [04-08]

plan_head_before: 68a67e9b33739e2708ed6a1af8d1ff8fe13effed
plan_head_after: bdc5b4218526990f51d030a436a9c53ee20b0c1c

actuals:
  tokens: 3100
  tasks: 2
  commits: 2

tech-stack:
  added: []
  patterns:
    - "Trajectory consumer glue in a test: tick a TrajectoryEngine for object 0, copy getFinalAz/El/Dist into RouteRenderRig::setObject, exactly what a plugin timer runs (D-12)"
    - "Per-shape maximum-error CAPTURE after an identity sweep, so a -s run reports the measured error"

key-files:
  created: []
  modified:
    - tests/Engine/ControlRouteTests.cpp
    - tests/Trajectory/TrajectoryTests.cpp

key-decisions:
  - "No library change: the ten D-13 identities hold exactly on this tree, so there is no D-14 defect and no OSD release-note row for trajectories"
  - "Bounce and Line reverse are asserted as they are today (D-19): Line is a half-period shift, Bounce mirrors the azimuth offset"

patterns-established:
  - "Exception cases for a rule are separate named test cases that cite the decision and the OSD issue, so a future change to them fails loudly with the reason attached"

requirements-completed: [EXTR-04]

coverage:
  - id: D1
    description: "A trajectory moves the rendered audio: Orbit forward for 15 ticks puts the source at +90 and the left channel louder (L/R 3.31), reverse puts it at -90 and the right louder (L/R 0.30), 15 more forward ticks return the image to the centre (L/R 1.00)"
    requirement: EXTR-04
    verification:
      - kind: integration
        ref: "tests/Engine/ControlRouteTests.cpp#Trajectory route: Orbit forward moves the rendered object left, reverse moves it right"
        status: pass
    human_judgment: false
  - id: D2
    description: "For Circle, Cross, Figure-8, Heart, Helix, Infinity, Orbit, Spiral, Square and Triangle, reverse traces the forward path backwards point by point (maximum error 0.0 on azimuth, elevation and distance over 3 bases and 101 phases)"
    requirement: EXTR-04
    verification:
      - kind: unit
        ref: "tests/Trajectory/TrajectoryTests.cpp#Trajectory reverse: ten shapes retrace the forward path"
        status: pass
    human_judgment: false
  - id: D3
    description: "Line reverse is the half-period shift and Bounce reverse mirrors the azimuth offset, both asserted as the deliberate OSD#100 behaviour so OSD's trajectory motion is unchanged"
    requirement: EXTR-04
    verification:
      - kind: unit
        ref: "tests/Trajectory/TrajectoryTests.cpp#Trajectory reverse: Line reverse is a half-period shift (OSD#100); #Trajectory reverse: Bounce reverse mirrors the azimuth offset (OSD#100)"
        status: pass
    human_judgment: false
  - id: D4
    description: "All 13 shapes animate forward and reverse through tick(): active, finite, in range at every tick, position changes over 60 ticks, and the 12 deterministic shapes equal computeTrajectory at the engine's phase"
    requirement: EXTR-04
    verification:
      - kind: unit
        ref: "tests/Trajectory/TrajectoryTests.cpp#Trajectory tick: all 13 shapes animate forward and reverse through tick()"
        status: pass
    human_judgment: false
  - id: D5
    description: "Random (seeded 1234) moves and stays in range over 600 ticks forward and reverse; reverse runs its noise time negative"
    requirement: EXTR-04
    verification:
      - kind: unit
        ref: "tests/Trajectory/TrajectoryTests.cpp#Trajectory tick: Random is seeded, moves and stays in range, forward and reverse"
        status: pass
    human_judgment: false

duration: 8min
completed: 2026-10-05
status: complete
---

# Phase 4 Plan 04: Trajectory Route and All-Shape Reverse Summary

**An Orbit ticked through `TrajectoryEngine` and fed to `RenderEngine` moves the sound left (L/R 3.31) and, reversed, right (L/R 0.30); reverse retraces the forward path exactly for ten shapes, Bounce and Line are pinned as the deliberate OSD#100 exceptions, and all 13 shapes are proven to animate both ways, tests only**

## Performance

- **Duration:** 8 min (08:44Z to 08:52Z)
- **Tasks:** 2
- **Files modified:** 2 (both test files, 272 lines added, no source or header touched)
- **BASE:** `68a67e9b33739e2708ed6a1af8d1ff8fe13effed` (HEAD when this plan started)

## Accomplishments

- ROADMAP criterion 3 holds through the renderer (D-11). With consumer-style glue copying `getFinalAz/El/Dist` into a Binaural `RouteRenderRig`, Orbit speed 1 origin (0, 0, 0.5) after 15 ticks of 1/60 s sits at azimuth 90.0 with L/R 3.311; the same on a fresh engine with reverse set sits at -90.0 with L/R 0.302. Fifteen more forward ticks reach azimuth -179.99994 (straight behind) and L/R 0.999999, so the image is back at the centre.
- D-13 holds on this tree: the ten shapes (2-7, 9, 11-13) have a **maximum identity error of 0.0** on azimuth (angular), elevation and distance over 3 bases and p = 0 to 1 in steps of 0.01, so there is no D-14 defect.
- D-19 is recorded in tests: Line reverse equals forward at `(p + 0.5) mod 1`, and Bounce reverse negates the azimuth offset from the base with equal elevation. Both pass within 1e-4 and both cite OSD#100 in the test comment.
- Every shape 1-13 runs through `tick()` forward and reverse for 60 ticks: active, finite and in range at every tick, position changes, and for the 12 deterministic shapes the ticked position equals `computeTrajectory` at `phase_[0]` with the same reverse flag.
- Random, seeded with `rng_.setSeed (1234)`: forward over 600 ticks spans azimuth -89.3 to 99.2 (`randomTime` +5.00001); reverse spans -105.5 to 71.3 (`randomTime` -5.00001); both stay in range.

## Task Commits

1. **Task 1: Tracer, an Orbit trajectory moves the rendered sound left, and right in reverse** - `f7f6182` (test)
2. **Task 2: Reverse for all 13 shapes, the Line and Bounce exceptions, tick sweep, seeded Random** - `bdc5b42` (test)

**Plan metadata:** committed with this SUMMARY (docs: complete plan)

## Files Created/Modified

- `tests/Engine/ControlRouteTests.cpp` - `TrajectoryToRigGlue` and the `[route][trajectory][tracer]` case (forward, reverse, return to centre)
- `tests/Trajectory/TrajectoryTests.cpp` - file-local `angularDiff`, `inRange`, `Base`; the three `[trajectory][reverse]` cases and the two `[trajectory][tick][reverse]` cases

## For VERIFICATION and Plan 04-08

**Intentional exceptions to D-13 (D-19), asserted by tests:**

| Shape | Reverse behaviour | Measured vs the generic phase flip | Test |
|---|---|---|---|
| Bounce (1) | azimuth offset from the base is negated, elevation unchanged (OSD#100) | max difference 180 degrees | `Bounce reverse mirrors the azimuth offset` |
| Line (8) | half-period shift, because `cos` is even and the phase flip alone would not reverse it (OSD#100) | max difference 90 degrees | `Line reverse is a half-period shift` |

**Maximum identity error per shape (generic reverse vs forward at 1 - p):** Circle 0.0, Cross 0.0, Figure-8 0.0, Heart 0.0, Helix 0.0, Infinity 0.0, Orbit 0.0, Spiral 0.0, Square 0.0, Triangle 0.0.

**OSD release note:** nothing for trajectories. No shape's path changed, and `git diff 68a67e9..HEAD -- src include` is empty.

## Decisions Made

- No library change was needed, so no D-14 decision was taken; OpenSpatialDelay's trajectory motion is bit-for-bit what it was.
- The identity test runs per point as three `CHECK_THAT`s plus a per-shape maximum, so a future regression names the shape, base and phase.

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered

- The Hook that wraps `Read` returned only line 1 of several files on the first read, so I read them with explicit ranges and `sed`. No effect on the result.
- The Debug executable still prints the known `FFT` leaked-object report at exit (pre-existing, noted in 04-02 and 04-03); exit code is 0.
- The plan's filter `[trajectory][reverse]` also selects the two tick cases (they carry both tags), so it lists 5 cases, which is what the acceptance criterion asks for.

## Verification

- `[route][trajectory][tracer]`: All tests passed (8 assertions in 1 test case)
- `[trajectory][reverse]`: 5 test cases, All tests passed (13839 assertions)
- `[trajectory]`: 35 test cases, All tests passed (13966 assertions)
- Full Debug suite: All tests passed (323707 assertions in 303 test cases), exit 0 (baseline 297 plus 6 new)
- Release: `cmake --build build-release -j8` then `ctest --test-dir build-release/tests`: 100% tests passed out of 303 (run after Debug, never together)
- `git diff --name-only 68a67e9..HEAD -- src include` prints nothing
- Line endings: both test files are LF, as at BASE

## Known Stubs

None.

## Threat Flags

None. T-04-10 was mitigated before this plan (Plan 04-03's bounded wrap); this plan adds no input path and leaves `wrapAzimuth` unchanged. T-04-11 is mitigated: Random is seeded with 1234 and was deterministic across repeated runs.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- Ready for Plan 04-05. The two D-19 exceptions and the all-zero identity table above are ready for Plan 04-08's VERIFICATION notes.
- Both build trees are configured and current.

---
*Phase: 04-control-surface-ui*
*Completed: 2026-10-05*

## Self-Check: PASSED

- FOUND: tests/Engine/ControlRouteTests.cpp
- FOUND: tests/Trajectory/TrajectoryTests.cpp
- FOUND: commits f7f6182, bdc5b42
