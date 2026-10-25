# Phase 4: Control Surface & UI - Pattern Map

**Mapped:** 2026-10-05
**Files analyzed:** 20 new/modified
**Analogs found:** 17 / 20 (3 have no in-repo analog; use RESEARCH.md verified snippets)

All analog paths verified git-tracked (`git ls-files`). No `.gsd` mirror paths used. Line numbers are from the worktree at HEAD 4f9a030.

## File Classification

| New/Modified File | Role | Data Flow | Closest Analog | Match |
|---|---|---|---|---|
| `include/SpatialCore/OSC/ADMOSCSender.h` (mod) | service | event-driven (timer-gated send) | itself (current 5-arg `tick`) | exact |
| `src/OSC/ADMOSCSender.cpp` (mod) | service | event-driven | itself, lines 34-67 | exact |
| `include/SpatialCore/OSC/ADMOSCReceiver.h` (mod) | listener iface | request-response | itself, `Listener` at lines 31-52 | exact |
| `src/OSC/ADMOSCReceiver.cpp` (mod) | service | request-response | itself (not re-read; see RESEARCH Pattern 4, Pitfalls 5, 6) | exact |
| `src/UI/SpatialMapComponent.cpp` (mod) | component | request-response (paint) | itself, `lf ? makeFont : FontOptions` at 247-248, 290-291, 553-554, 581-582, 597-598 | exact |
| `src/UI/PresetBrowser.cpp` (mod) | component | paint | `SMLLookAndFeel.cpp:21-27` typeface creation | role-match |
| `src/Trajectory/TrajectoryEngine.cpp` (mod, only if a reverse is wrong; D-19 says none expected) | utility | transform | itself | exact |
| `tests/OSC/ADMOSCSenderTests.cpp` (ext) | test | event-driven | itself (loopback `CapturingListener`) | exact |
| `tests/OSC/ADMOSCReceiverTests.cpp` (ext) | test | request-response | itself (`RecordingListener`, `createTestReceiver`) | exact |
| `tests/Trajectory/TrajectoryTests.cpp` (ext) | test | transform | itself, "Reverse flips phase" lines 204-209 | exact |
| `tests/Support/RouteRenderRig.h` (new) | test util | transform | `tests/Engine/RenderEngineTests.cpp` `SourceFixture` (50-70), `makeSc16Context` (442-448) | role-match |
| `tests/Engine/ControlRouteTests.cpp` (new) | test | request-response | `tests/Engine/RenderEngineTests.cpp` simple Woodworth case (166-195) | role-match |
| `tests/UI/UITestMain.cpp` (new) | test infra | event-driven | none | no analog |
| `tests/UI/UITestSupport.h` (new) | test util | n/a | none | no analog |
| `tests/UI/SpatialMapComponentTests.cpp` (new) | test | request-response | none in repo (RESEARCH "Synthesized drag", "Pixel numbers") | no analog |
| `tests/UI/FontProvenanceTests.cpp` (new) | test | n/a | none in repo (RESEARCH spy LNF) | no analog |
| `tests/UI/MapRouteTests.cpp` (new) | test | request-response | `ControlRouteTests.cpp` (new, same phase) | role-match |
| `tests/CMakeLists.txt` (mod) | config | n/a | itself | exact |
| `CMakeLists.txt` (mod: demo option) | config | n/a | `option(SPATIALCORE_BUILD_TESTS ...)` at ~line 351 | role-match |
| `examples/CMakeLists.txt`, `examples/demo/*` (new) | app | event-driven | none in repo | no analog |

## Pattern Assignments

### `src/OSC/ADMOSCSender.cpp` + `.h` (service, event-driven)

**Analog:** itself. Current code to replace (cpp 34-67):
```cpp
void ADMOSCSender::tick(const float* azimuthsDeg, const float* elevationsDeg,
                         const float* distances, const bool* enabled, int numObjects)
{
    if (! connected) return;
    ++tickCounter;                       // D-07: replace with schedule (nextDue_ += 1/30; resync if >=1/30 behind)
    if ((tickCounter & 1) != 0) return;
    for (int i = 0; i < numObjects && i < MAX_SOURCES; ++i)
    {
        if (! enabled[i]) continue;
        float dAz = std::abs(azimuthsDeg[i] - prevAz[i]); ...
        if (dAz > 0.1f || dEl > 0.1f || dDist > 0.001f)
        { sendPosition(i, ...); prevAz[i] = ...; }
    }
}
```
Header members to change (h 31-33): `tickCounter`, `prevAz/prevEl/prevDist[MAX_SOURCES] = {}`. Add per-object `hasSent[]`/`wasEnabled[]` (D-09 first send, D-08b re-enable), `nextDue_`, pending-query coalescing, and a `tick(..., double nowSeconds)` overload; the 5-arg overload delegates with `juce::Time::getMillisecondCounterHiRes() * 0.001` (RESEARCH Pattern 3). Keep the guard idiom `if (! connected || objectIndex < 0 || objectIndex >= MAX_SOURCES) return;` (cpp 24-25) and the 1-based address build `"/adm/obj/" + juce::String(objectIndex + 1) + "/aed"` (cpp 28). Query replies mirror the query's property and go to configured `sendHost/sendPort` (D-20). Run `oscPortsConflict` (`src/OSC/OSCPortValidation.cpp`) before connecting (Pitfall 8).

### `include/SpatialCore/OSC/ADMOSCReceiver.h` + `src/OSC/ADMOSCReceiver.cpp` (listener, request-response)

**Analog:** itself. Existing Listener style (h 31-52): one pure `admPositionReceived`, then defaulted virtuals with NAMED unused params (the D-15 warnings). New pattern:
```cpp
enum class PositionQuery { azim, elev, dist, aed, xyz };
virtual void admPositionQueried (int /*objectIndex*/, PositionQuery /*kind*/) {}   // non-pure: additive, existing consumers compile
virtual void admObjectParamReceived (int /*objectIndex*/, const juce::String& /*paramName*/, float /*value*/) {}
virtual void admGlobalParamReceived (const juce::String& /*propertyName*/, float /*value*/) {}
```
Numeric extraction and finiteness (D-21), from RESEARCH sketch:
```cpp
auto numeric = [] (const juce::OSCArgument& a, float& out)
{ if (a.isFloat32()) { out = a.getFloat32(); return true; }
  if (a.isInt32())   { out = (float) a.getInt32();   return true; }
  return false; };
```
Add a `message.size() == 0` branch before the existing `size() >= 1` branches. Reject non-finite, clamp el to [-90,90], dist to [0,1], bounded azimuth wrap (use `std::fmod`, not the `while` loop in `TrajectoryEngine.h:13`). xyz inverse: `x = -d*cos(el)*sin(az)`, `y = d*cos(el)*cos(az)`, `z = d*sin(el)` (receiver forward conversion is `ADMOSCReceiver.cpp:12-21`). Planner must read the .cpp itself before editing (not excerpted here).

### `src/UI/SpatialMapComponent.cpp` (component, paint)

**Analog:** itself. The pattern to remove (repeated 5x), e.g. 247-248:
```cpp
if (lf) g.setFont (makeFont (lf->jetbrainsRegular, 9.0f));
else    g.setFont (juce::FontOptions (9.0f));
```
Bold variant at 597-598 uses `lf->jetbrainsBold` / `.withStyle ("Bold")`; 581-582 uses `jetbrainsRegular` 11pt. Replace with map-owned typefaces created in the constructor via `juce::Typeface::createSystemTypefaceFor (SpatialCoreUIFontData::JetBrains_MonoRegular_ttf, ..._size)` (same loader as `src/UI/SMLLookAndFeel.cpp:21-27`) and delete the per-paint `dynamic_cast`. Required result: byte-identical render under SML LNF (RESEARCH Pattern 6).

### `src/UI/PresetBrowser.cpp` (component, paint)

Line 148: `auto titleFont = juce::Font (juce::FontOptions (13.0f).withStyle ("Bold"));` is unconditional default-font use. Replace with an embedded DM Sans Bold typeface via `FontOptions (typeface)` (explicit typeface bypasses default LNF lookup, RESEARCH Pitfall 4). Use `SMLLookAndFeel.cpp` typeface creation (21-27) as the loader.

### `tests/OSC/ADMOSCSenderTests.cpp` (test, event-driven) - extend

**Analog:** itself. Reuse (lines 1-60): `CapturingListener : juce::OSCReceiver::Listener<RealtimeCallback>` with `std::atomic<int> messageCount`, `waitForMessage (counter, n, timeoutMs)`, and a distinct port constant per test (`kTestSenderPort = 9700`; pick new ports 97xx to avoid collisions). Rate/static/first-send/re-enable tests use the injected `nowSeconds` overload and need no network wait for counting if the capture is a loopback listener; assert 30 +- 1 msgs per simulated second at 120/60/50/30 Hz with jitter, 0 while still, first send at (0,0,0), connect-send, re-enable send.

### `tests/OSC/ADMOSCReceiverTests.cpp` (test, request-response) - extend

**Analog:** itself. Reuse `RecordingListener` (lines 17-56, add `admPositionQueried` recorder + call count) and `createTestReceiver (RecordingListener&)` (58-61), driven synchronously by `testProcessOSCMessage` (guarded by `JUCE_UNIT_TESTS`, set in `tests/CMakeLists.txt`). The one real-UDP test needs `juce::ScopedJuceInitialiser_GUI gui;` and `juce::MessageManager::getInstance()->runDispatchLoopUntil (300);`, plus `JUCE_MODAL_LOOPS_PERMITTED=1` on the target (RESEARCH Pattern 5). Cover wrong-typed args, NaN/inf, 1e10 azimuth (Pitfalls 5, 6).

### `tests/Trajectory/TrajectoryTests.cpp` (test, transform) - extend

**Analog:** itself, lines 204-209 and 8-17 (`TrajShape` constants, `computeTrajectory (shape, phase, baseAz, baseEl, baseDist, reverse)`):
```cpp
auto fwd = TrajectoryEngine::computeTrajectory (TrajShape::Orbit, 0.25f, 0.0f, 0.0f, 0.5f);
auto rev = TrajectoryEngine::computeTrajectory (TrajShape::Orbit, 0.75f, 0.0f, 0.0f, 0.5f, true);
CHECK_THAT (fwd.azDeg, WithinAbs (rev.azDeg, 0.01f));
```
Sweep per D-19: shapes 2-7, 9, 11-13 assert `reverse(p) == forward(1-p)` within 1e-4; Line (8) `reverse(p) == forward(fmod(p+0.5,1))`; Bounce (1) azimuth offset negated, elevation offset equal; Random (10) moves and stays in range (seeded, via `tick()`). Note `TrajShape` has 14 constants (None + 13).

### `tests/Support/RouteRenderRig.h` + `tests/Engine/ControlRouteTests.cpp` (test util / test, request-response)

**Analog:** `tests/Engine/RenderEngineTests.cpp`. Copy fixtures:
```cpp
// 50-70: SourceFixture::makeSources() fills RenderSources (monoBuffers[0], tapFadeGainPerSample[0],
//        distGainPerSample[0], objectLive[0], objects[0].{azimuthDeg,elevationDeg,distance,enabled})
// 166-195: RenderEngine engine; engine.prepare (kSampleRate, kBlockSize);
//          engine.setOutputFormat (OutputFormat::Binaural); ... engine.renderBlock (sources, ctx, outPtrs, 2);
// 442-448: makeSc16Context -> ctx.sampleRate; ctx.engineComputesGains = true; ctx.engineDerivesDispatch = true;
```
Per D-18 set BOTH flags; do not set `isBinaural` etc. by hand (the engine derives them). Includes used: `<SpatialCore/Engine/RenderEngine.h>`, `"../TestNumerics.h"`, `"../Binaural/BinauralTestUtilities.h"`. Use Binaural (Simple, no profile) and Quad, not Stereo (CLAUDE.md). Assertions from RESEARCH Pattern 2 (Binaural az +90 L/R >= 2.0; az -90 <= 0.5; az 0 within 5%; Quad loudest channel = nearest speaker, others < 1%). Render 4 blocks of 64 and take RMS of the last. New test files go into the `SpatialCoreTests` source list by hand.

### `tests/UI/*` (test, new target) - no analog

No UI test target exists. Use the verified snippets in RESEARCH "Code Examples": Catch2 `EventListenerBase` owning `ScopedJuceInitialiser_GUI` (`UITestMain.cpp`), `makeMouseEvent` (drag az=90.000 d=0.800, elevation preserved), pixel-brightness assertions (below sum RGB < 0.6 x above; ring distance `d * 0.45 * min(w,h)` +-0.5 px), spy `LookAndFeel_V4` plus `juce::Typeface::clearTypefaceCache()` and embedded-bytes == `fonts/*.ttf` check via `#include "SpatialCoreUIFontData.h"` (8 resources), and the grep gate. `MapRouteTests.cpp` reuses the `RouteRenderRig.h` header (map `objectPositionChanged (index, az, dist)` -> rig -> RMS).

### `tests/CMakeLists.txt` (config)

**Analog:** itself. Pattern to duplicate for `SpatialCoreUITests`:
```cmake
add_executable(SpatialCoreTests <hand-maintained source list>)
target_link_libraries(SpatialCoreTests PRIVATE Catch2::Catch2WithMain SpatialCore)
target_compile_definitions(SpatialCoreTests PRIVATE SPATIALCORE_HRTF_DIR="..." JUCE_UNIT_TESTS=1)
list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(CTest)
include(Catch)
catch_discover_tests(SpatialCoreTests)
```
Add the UI target (links `SpatialCoreUI`, `Catch2::Catch2WithMain`; `TEST_PREFIX "ui: "` per Pitfall 7) before/with `catch_discover_tests`. Add `JUCE_MODAL_LOOPS_PERMITTED=1` to targets that pump. Gate must build all targets (no `--target SpatialCoreTests` only) so no `_NOT_BUILT` placeholder fails (D-17).

### Root `CMakeLists.txt` demo option (config)

**Analog:** the tests option at the end of the file:
```cmake
option(SPATIALCORE_BUILD_TESTS "Build SpatialCore tests" ON)
if(SPATIALCORE_BUILD_TESTS)
    add_subdirectory(tests)
endif()
```
Copy shape with default OFF, e.g. `option(SPATIALCORE_BUILD_EXAMPLES ... OFF)` and `if(PROJECT_IS_TOP_LEVEL AND ...) add_subdirectory(examples)`. The `PROJECT_IS_TOP_LEVEL` branch idiom is used throughout the file (CMake 258-348). `SpatialCoreUI` already exposes PUBLIC includes, `SpatialCoreUIFontData`, and `JUCE_USE_CURL=0`.

## Shared Patterns

### Facade boundary and consumer flags
**Source:** `tests/Engine/RenderEngineTests.cpp:442-448`. Apply to every "sound moves" test (OSC, trajectory, map): set `engineComputesGains` and `engineDerivesDispatch`.

### OSC loopback harness
**Source:** `tests/OSC/ADMOSCSenderTests.cpp:15-60`. Apply to sender tests and any network-based test: dedicated port per test, atomic counter, polled wait.

### Receiver forwarding with NaN single-axis convention
**Source:** `include/SpatialCore/OSC/ADMOSCReceiver.h:35-37` (comment) and RESEARCH Pitfall 6. NaN stays the "axis not sent" sentinel only for single-axis messages; D-21 rejects NaN in full messages.

### Embedded-font loading
**Source:** `src/UI/SMLLookAndFeel.cpp:21-27`. Apply to `SpatialMapComponent` constructor and `PresetBrowser.cpp:148` fix.

### Thread/audio constraint
Sender, receiver, UI code stays message-thread. Nothing in this phase may touch `RenderEngine`'s audio path (CLAUDE.md lock-free rule).

## No Analog Found

| File | Role | Data Flow | Reason |
|---|---|---|---|
| `tests/UI/UITestMain.cpp`, `UITestSupport.h` | test infra | n/a | no GUI test harness exists; use RESEARCH verified listener and mouse-event snippets |
| `tests/UI/SpatialMapComponentTests.cpp`, `FontProvenanceTests.cpp` | test | request-response | `SpatialCoreTests` deliberately never links `SpatialCoreUI` |
| `examples/CMakeLists.txt`, `examples/demo/*` | app | event-driven | no `examples/` directory; use `juce_add_gui_app` plus `--screenshots <dir>` mode (RESEARCH Pattern 7); demo must set `ts.reverse` itself (Pitfall 3) |

## Metadata

**Analog search scope:** `include/SpatialCore/{OSC,UI,Trajectory,Engine}`, `src/{OSC,UI,Trajectory}`, `tests/`, root and tests CMakeLists
**Files scanned:** about 14 read (partial ranges), tracked file list reviewed
**Pattern extraction date:** 2026-10-05
