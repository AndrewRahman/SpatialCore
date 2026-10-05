---
phase: 04-control-surface-ui
reviewed: 2026-10-05T10:06:33Z
depth: standard
files_reviewed: 30
files_reviewed_list:
  - .claude/skills/adm-osc-integration/adm-osc-integration.md
  - CLAUDE.md
  - CMakeLists.txt
  - README.md
  - docs/integration-guide.md
  - examples/CMakeLists.txt
  - examples/demo/DemoComponent.cpp
  - examples/demo/DemoComponent.h
  - examples/demo/Main.cpp
  - include/SpatialCore/OSC/ADMOSCReceiver.h
  - include/SpatialCore/OSC/ADMOSCSender.h
  - include/SpatialCore/UI/PresetBrowser.h
  - include/SpatialCore/UI/SpatialMapComponent.h
  - src/Core/FloatSemanticsGuard.h
  - src/OSC/ADMOSCReceiver.cpp
  - src/OSC/ADMOSCSender.cpp
  - src/UI/PresetBrowser.cpp
  - src/UI/SpatialMapComponent.cpp
  - tests/CMakeLists.txt
  - tests/Engine/ControlRouteTests.cpp
  - tests/OSC/ADMOSCReceiverTests.cpp
  - tests/OSC/ADMOSCSenderTests.cpp
  - tests/Support/RouteRenderRig.h
  - tests/Trajectory/TrajectoryTests.cpp
  - tests/UI/FontProvenanceTests.cpp
  - tests/UI/MapRouteTests.cpp
  - tests/UI/SpatialMapComponentTests.cpp
  - tests/UI/UITestMain.cpp
  - tests/UI/UITestSupport.h
  - tests/tools/check_unused_params.py
findings:
  critical: 0
  warning: 6
  info: 8
  total: 14
status: issues_found
---

# Phase 4: Code Review Report

**Reviewed:** 2026-10-05T10:06:33Z
**Depth:** standard
**Files Reviewed:** 30
**Status:** issues_found

## Summary

Reviewed the Phase 4 diff (`260f082^..HEAD`) for the ADM-OSC receiver hardening and query path, the self-clocked sender, the map and overlay font ownership, the demo app, the new test targets and the supporting docs and build files.

The threading model of the demo holds up. OSC receive uses `MessageLoopCallback`, so `admPositionQueried` and `ADMOSCSender::queueReply` both run on the message thread. `renderAudio()` only loads atomics, writes buffers sized in `prepareRender()` and calls `renderBlock()`. I found no lock, allocation or logging on the audio path. The receiver hardening is correct for the paths it covers: argument types are checked before the getters are called, `message[0]` is only reached after the `size() == 0` early-out, and `std::remainder` bounds the azimuth in one step.

No BLOCKER-class defect was found. The warnings are:

- A latent NaN poisoning of the sender's dead-band state.
- A project-rule gap: `ADMOSCSender.cpp` relies on `std::isfinite` without the fast-math guard.
- A contract the docs point to that does not exist in the public header (the NaN "axis not sent" rule).
- A cartesian overflow path that yields a wrong elevation.
- Range hardening that stops short of the global and continuous-control properties.
- Test reliability: fixed UDP ports and timing-quiet heuristics.

## Warnings

### WR-01: A single non-finite position permanently silences an object in `ADMOSCSender::tick`

**File:** `src/OSC/ADMOSCSender.cpp:166-177`
**Issue:** `tick()` never validates the positions it is handed. If `azimuthsDeg[i]`, `elevationsDeg[i]` or `distances[i]` is NaN, `sendPosition` puts NaN on the wire. `forceSend_` is cleared and `prevAz/prevEl/prevDist` are then stored as NaN. After that, `std::abs(x - NaN)` is NaN, so `dAz > 0.1f || dEl > 0.1f || dDist > 0.001f` is always false. The object is never sent again until it is disabled and re-enabled, or the sender reconnects. `queueReply` rejects non-finite values, so the two sender paths are inconsistent. A trajectory or consumer glitch that produces one NaN frame becomes a permanent silent drop of position output for that object.
**Fix:** Skip non-finite objects before the dead-band test, and make the comparison NaN-safe:
```cpp
if (! std::isfinite (azimuthsDeg[i]) || ! std::isfinite (elevationsDeg[i]) || ! std::isfinite (distances[i]))
    continue;   // keep the previous reference; do not send NaN
```
Alternatively use `! (dAz <= 0.1f && dEl <= 0.1f && dDist <= 0.001f)` so a NaN reference forces a send. Add a test that ticks NaN and then a finite value and expects the finite value to be sent.

### WR-02: `ADMOSCSender.cpp` uses `std::isfinite` without the fast-math guard

**File:** `src/OSC/ADMOSCSender.cpp:1-2, 42`
**Issue:** Phase 4 adds a non-finite rejection (`std::isfinite` in `queueReply`) to this file. `FloatSemanticsGuard.h` states "Include it from every src/ file that relies on a non-finite test", and it updated its own comment to list `ADMOSCReceiver.cpp` for the same reason. `ADMOSCSender.cpp` does not include it. If fast-math or finite-math-only reaches this TU through `CMAKE_CXX_FLAGS` or `add_compile_options`, the test folds to "finite" with no diagnostic. The D-20/T-04-07 input validation then silently disappears, and NaN reaches `sender.send`.
**Fix:**
```cpp
#include <SpatialCore/OSC/ADMOSCSender.h>
#include "../Core/FloatSemanticsGuard.h"   // WR-04/D-21: queueReply (and tick, see WR-01) test isfinite
#include <cmath>
```
Add `ADMOSCSender.cpp` to the list in the guard header's comment.

### WR-03: The NaN "axis not sent" contract is cited as documented on `Listener::admPositionReceived`, but the header does not document it

**File:** `include/SpatialCore/OSC/ADMOSCReceiver.h:42-45` (cited from `examples/demo/DemoComponent.cpp:194`, `tests/Support/RouteRenderRig.h:60`)
**Issue:** The demo and the test rig both say "the consumer rule documented on `ADMOSCReceiver::Listener::admPositionReceived`". The comment on that method only says "Per-object position update ... objectIndex is 0-based". The NaN sentinel rule lives in a comment inside `oscMessageReceived` in the `.cpp` and in `docs/integration-guide.md`. A consumer implementing the pure-virtual `admPositionReceived` from the header alone will treat the NaN as a position. A NaN axis then reaches the engine, where the hold-last-good sanitiser has to catch it. The rule is also the reason the receiver now drops NaN input (D-21).
**Fix:** Document it on the declaration:
```cpp
// Single-axis messages (/azim, /elev, /dist) forward the two axes that were NOT sent as NaN.
// The consumer MUST keep its stored value for any NaN axis (use std::isnan). The receiver never
// forwards a NaN that came from the wire (D-21), so NaN always means "axis not sent".
// azimuthDeg is within [-180, 180], elevationDeg [-90, 90], distance [0, 1].
```

### WR-04: `/xyz` overflows float for large components and returns a wrong elevation; it is also the only position form with no input range limit

**File:** `src/OSC/ADMOSCReceiver.cpp:12-20, 154-164`
**Issue:** `/x /y /z` are clamped with `clampCartesian`, but `/xyz` passes the raw finite triple to `cartesianToPolar`. With `x = 1e30` or any component above about `1.8e19`, `x * x` overflows to `+inf` in float. `r = sqrt(inf) = inf`, so `elDeg = atan2(z, inf) = 0`. A message such as `(1e30, 0, 1e30)` yields az = -90, el = 0, dist = 1. The correct result is el = 45. The result is finite and in range, so the new test (`ADMOSCReceiverTests.cpp:765-769`, which only checks `isfinite` and `dist == 1`) passes while the direction is silently wrong. The hostile-input bound the phase set out to establish (T-04-05) is only partly met for this address.
**Fix:** Either clamp the components as `/x /y /z` do (this is the compliant range), or scale before squaring:
```cpp
const float m = std::max ({ std::abs (x), std::abs (y), std::abs (z) });
if (m > 1.0f) { x /= m; y /= m; z /= m; }   // direction preserved, no overflow; dist then clamps to 1
```
Add a test with `(1e30, 0, 1e30)` that expects el near 45.

### WR-05: Range hardening is partial: `/osd/global/*`, `/doppler`, `/pitch`, `/speed` are forwarded unbounded, and `wrapAzimuth` still loops without bound

**File:** `src/OSC/ADMOSCReceiver.cpp:176-181, 192-201`; `include/SpatialCore/Trajectory/TrajectoryEngine.h:11-16`
**Issue:** D-21 bounds azimuth, elevation, distance and the cartesian axes. Every other numeric value is forwarded as any finite float or int32, up to about 3e38. That covers the global parameters, `doppler`, `pitch`, `speed`, `enabled`, `trajectory`, `direction` and `input`. `wrapAzimuth` (`while (az > 180) az -= 360`) is unchanged. `1e10 - 360 == 1e10` in float, so any consumer that routes one of these unbounded values (for example `/osd/global/tapazimuth`) into `wrapAzimuth` hangs the message thread. The new test comment says the same of the raw 1e10 azimuth. The receiver is on every network interface and unauthenticated (`docs/integration-guide.md` says so), so this is a remote hang for any consumer that has not clamped. The listener docs do not tell consumers to clamp.
**Fix:** Make `wrapAzimuth` bounded, which fixes it at the root for every caller:
```cpp
inline float wrapAzimuth (float az)
{
    if (az >= -180.0f && az <= 180.0f) return az;
    return std::isfinite (az) ? std::remainder (az, 360.0f) : 0.0f;
}
```
At minimum, document on `admObjectParamReceived` and `admGlobalParamReceived` that values are finite but otherwise unbounded and must be range-limited by the consumer.

### WR-06: New network tests bind fixed UDP ports and judge counts by "quiet period" heuristics

**File:** `tests/OSC/ADMOSCSenderTests.cpp:29, 221, 312, 337-742, 646-817`; `tests/OSC/ADMOSCReceiverTests.cpp:458-475`; `tests/Engine/ControlRouteTests.cpp:90-94`; `examples/demo/DemoComponent.cpp:553`
**Issue:**
- About 20 tests bind loopback ports 9700-9756, plus 9720, 9750, 9751 and 9790/9791 in the demo. Under `ctest -j`, or with two checkouts or worktrees on one machine, the tests collide. JUCE datagram sockets set `SO_REUSEADDR`, so a collision may not fail `connect()`. Instead, packets are split between the two listeners, which gives intermittent count failures.
- `waitUntilQuiet()` returns after 60 ms without a new datagram. On a loaded CI or dev machine, delayed delivery ends the wait early. The exact-count `CHECK`s that follow (`== 6`, `moving >= 87 && <= 93`, `== 3`) then fail.
- The wall-clock rate test asserts `elapsed >= 0.9` and `total <= 30 * elapsed + 5`.
- CLAUDE.md says the local suites are the only gate, so flaky tests directly cost trust in that gate.

**Fix:** Bind to port 0 and read the OS-assigned port. JUCE `OSCReceiver::connect(0)` is not accepted, so instead use a small helper that probes a free port (bind a `juce::DatagramSocket`, take `getBoundPort()`, release it) and pass it to both ends. Replace the quiet-period wait with "wait until count reaches the expected value or timeout, then wait one further window to prove no extras arrive".

## Info

### IN-01: `ADMOSCSender.h` includes the whole receiver header only for one enum

**File:** `include/SpatialCore/OSC/ADMOSCSender.h:4`
**Issue:** `ADMPositionQuery` is the only thing the sender needs from `ADMOSCReceiver.h`. Including it pulls the full receiver (and its private `juce::OSCReceiver` bases) into every TU that only sends. This is a coupling wart, not a bug.
**Fix:** Move `ADMPositionQuery` to a small `OSC/ADMPositionQuery.h` and include that from both.

### IN-02: `kNumQueryKinds` is not tied to the enum

**File:** `include/SpatialCore/OSC/ADMOSCSender.h:78`, `src/OSC/ADMOSCSender.cpp:55`
**Issue:** `kNumQueryKinds = 5` and the `kPropertyNames` table must match `ADMPositionQuery`. Adding an enumerator compiles and then drops replies for it (silently ignored by the `k >= kNumQueryKinds` check).
**Fix:**
```cpp
static_assert (static_cast<int> (ADMPositionQuery::xyz) + 1 == kNumQueryKinds, "update the reply table");
```

### IN-03: Object-number parsing accepts trailing garbage

**File:** `src/OSC/ADMOSCReceiver.cpp:90`
**Issue:** `getIntValue()` reads the leading integer and ignores the rest, so `/adm/obj/2x/azim`, `/adm/obj/ 2/azim` and `/adm/obj/2.7/azim` are all accepted as object 2. Hardening that is otherwise strict about argument types is lax about the address.
**Fix:** Require `segment.containsOnly ("0123456789")` and a non-empty segment before `getIntValue()`.

### IN-04: Duplicated test helpers and unchecked argument getters

**File:** `tests/OSC/ADMOSCReceiverTests.cpp:357-388`, `tests/OSC/ADMOSCSenderTests.cpp:31-98`
**Issue:** `RecordingCapture` is copy-pasted between two files (the receiver test says so). Both capture helpers and `CapturingListener` call `message[i].getFloat32()` without `isFloat32()`. Stray non-float traffic on a shared port asserts in Debug.
**Fix:** Move one `RecordingCapture` into `tests/Support/` and guard each read with `isFloat32()`.

### IN-05: Font loading is per instance and a null typeface falls back silently

**File:** `src/UI/SpatialMapComponent.cpp:40-43`, `src/UI/PresetBrowser.cpp:23`
**Issue:** Each `SpatialMapComponent` parses three JetBrains Mono faces and each `PresetSaveOverlay` parses DM Sans Bold. Creating the overlay on every Save press re-parses the font each time. `createSystemTypefaceFor` can return null, and `FontOptions (nullptr)` then resolves through the default look-and-feel. That is the exact fallback D-06/D-22 exist to prevent, and nothing reports it. The tests assert non-null only for `SMLLookAndFeel`, not for the map or overlay.
**Fix:** Share one process-wide cache of the four typefaces (a function-local static in `src/UI`), and `jassert` on null. Add `CHECK` calls for the map and overlay members, or expose a test accessor.

### IN-06: Demo shares audio state between message and audio paths without a guard

**File:** `examples/demo/DemoComponent.cpp:349-443`, `examples/demo/DemoComponent.h:69-72`
**Issue:**
- (a) `measureLeftRight()` is public and calls `renderAudio()` from the message thread. It shares `engine_`, `tone_`, `tonePhase_` and `enabled_` with the audio callback. The header says "never called while an audio device is running", but nothing enforces it, and this is a worked example consumers will copy.
- (b) `az_`, `el_` and `dist_` are three separate relaxed atomics, so a block can pair a new azimuth with the previous distance. This is benign here but contradicts the rest of the project's torn-block hygiene (SC-16).
- (c) `Timer::callAfterDelay (300, [this] ...)` chains capture `this`. They fire into a destroyed component if the app quits mid-test.

**Fix:** (a) `jassert (! deviceManager_.getCurrentAudioDevice())` at the top of `measureLeftRight`. (b) Pack the triple into one `std::atomic<std::array<float,3>>` or a seqlock, or note the tolerance in the header. (c) Cancel through a `juce::Timer` member or a `WeakReference` guard.

### IN-07: Font provenance rule scans comments and hard-codes a resource count

**File:** `tests/UI/FontProvenanceTests.cpp:175, 216-224, 255-266`
**Issue:** `familyNameRule()` runs on every raw source line, comments included. A comment that merely says `getTypefaceName` or mentions `"DM Sans"` in quotes fails the rule. The test issues one `CHECK` plus one `INFO` per source line (tens of thousands of assertions). `namedResourceListSize == 8` breaks when a font is added, with a message that does not say why.
**Fix:** Strip `//` comments before matching, assert once per file rather than per line, and name the expected count in a message or derive it from the CMake font list.

### IN-08: Magic numbers in the dead-band

**File:** `src/OSC/ADMOSCSender.cpp:170`
**Issue:** `0.1f` degrees and `0.001f` distance are inline literals, and the tests restate them (`ADMOSCSenderTests.cpp` "exactly 0.1 degrees").
**Fix:** `static constexpr float kAngleDeadBandDeg = 0.1f, kDistanceDeadBand = 0.001f;` in the class, referenced from the tests.

---

_Reviewed: 2026-10-05T10:06:33Z_
_Reviewer: Claude (gsd-code-reviewer)_
_Depth: standard_
