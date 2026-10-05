# Phase 4: Control Surface & UI - Research

**Researched:** 2026-10-05
**Domain:** JUCE 9 headless GUI testing (Catch2), ADM-OSC send/receive timing and queries, trajectory reverse semantics, embedded-font provenance, offscreen demo-app screenshots
**Confidence:** HIGH (every load-bearing claim below was either read in the source this session or reproduced in a scratch build against JUCE 9.0.0; the few exceptions are tagged `[ASSUMED]`)

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions

### How the map gets checked (EXTR-05, criterion 4)
- **D-01:** **Both automated tests and a demo app.** Automated UI tests run every time: they build
  `SpatialMapComponent` headless, simulate a mouse drag, and assert the reported position, distance
  ring and elevation opacity. This needs a UI test target that links `SpatialCoreUI` (today
  `SpatialCoreTests` deliberately does not — ROADMAP "Starting position"). The demo app gives
  screenshots for one human look.
- **D-02:** **The demo app lives in this repo, off by default.** An `examples/` JUCE app, built only
  behind a CMake option that defaults OFF, so OSD and other consumers never build it. It also serves
  as the worked example of wiring OSC, trajectory and map into `RenderEngine` (see D-12), and Phase 6's
  consumer harness may reuse it.
- **D-03:** **One screenshot review by the user.** Claude runs the demo app and shows before/after-drag
  screenshots (positions, distance ring, elevation fading). The user approves once, in plain
  language. The user runs nothing (standing preference: Claude runs every check).
- **D-04:** **No elevation drag in v1.** The map drag stays azimuth + distance, as OSD has it today
  (`SpatialMapComponent::Listener::objectPositionChanged (index, azimuthDeg, distance)`). Elevation is
  shown as opacity and set by OSC, trajectories or the plugin's own controls.

### Fonts without installed fonts (DATA-02, criterion 4)
- **D-05:** **Prove it with an automated test plus a code rule, not on this Mac.** This Mac has DM Sans,
  JetBrains Mono and Roboto installed (`~/Library/Fonts`, `/Library/Fonts`), so a screenshot here
  proves nothing. A test asserts every SML typeface comes from the embedded `SpatialCoreUIFontData`,
  not a system lookup. A check (test or grep gate) asserts no UI code requests an SML font by family
  name. No clean-user or font-removal run.
- **D-06:** **The map uses SML fonts on its own.** Today `SpatialMapComponent` falls back to the system
  font when the SML look-and-feel is not set (`src/UI/SpatialMapComponent.cpp:247-248, 290-291,
  553-554`). Change it so the map always loads the embedded fonts itself, whatever the look-and-feel.
  OSD, which does set `SMLLookAndFeel`, must look identical.

### OSC broadcast timing and still objects (EXTR-04, criterion 2)
- **D-07:** **The sender keeps its own clock.** `ADMOSCSender::tick()` today sends on every second call
  and assumes a 60 Hz caller timer (`src/OSC/ADMOSCSender.cpp:43-46`). It changes to send when 1/30 s
  has elapsed, independent of the caller's rate. OSD calling at 60 Hz must see the same behaviour.
  30 Hz stays (roadmap + OSD), even though ADM-OSC's typical rate is about 50 Hz.
- **D-08:** **Late joiners follow the ADM-OSC standard: answer position queries, plus send everything
  once on connect.** ADM-OSC has no heartbeat. A device joining late sends an address with no
  arguments (e.g. `/adm/obj/4/xyz`) and the receiver replies to the sender's IP on the return port
  (spec default 4002). SpatialCore will:
  (a) reply to no-argument position queries `/adm/obj/N/{azim,elev,dist,aed,xyz}` with the current
  position. Today `ADMOSCReceiver` ignores messages with no arguments (`message.size() >= 1` checks);
  (b) send every enabled object once on connect and when an object is re-enabled;
  (c) otherwise stay silent while positions are still, so criterion 2 holds as written.
  The receiver has no position store (it forwards to a Listener), so the planner decides where the
  current position for a reply comes from (e.g. a consumer-supplied callback). Keep DR-16: no concrete
  processor pointer.
- **D-09:** **Fix the "never sent at zero" bug.** The sender's `prevAz/prevEl/prevDist` start at 0
  (`include/SpatialCore/OSC/ADMOSCSender.h:31-33`), so an object whose first position is within the
  dead-band of (0°, 0°, 0) is never sent. The first send for each object always goes out.
- **D-10:** **Proof is an automated test with a fake clock.** It counts messages per second while
  moving (30 Hz) and confirms zero while still, plus the connect, re-enable and query cases. No real
  network packet capture is required.

### "Reaches the renderer" bar (EXTR-04 / EXTR-05, criteria 1, 3, 4)
- **D-11:** **The sound must actually move.** For each route — OSC message, trajectory, map drag — an
  automated test moves an object (e.g. to the left), renders audio through `RenderEngine`, and checks
  the output changes the right way (the left channel gets louder). A test that the position value
  merely arrived is not enough.
- **D-12:** **No new library connector.** Wiring OSC / trajectory / map into `RenderEngine` stays each
  plugin's own code, as today. Tests wire it the way a consumer would, and the demo app (D-02) is the
  worked example for plugin authors.
- **D-13:** **Reverse = the same path in the opposite direction.** For each of the 13 shapes, reverse
  traces the forward path backwards, checked point by point. `Random` (case 10) has no fixed path, so
  it is checked only for "it moves and stays in range". Today zero tests cover `reverse`
  (`tests/Trajectory/TrajectoryTests.cpp`).
- **D-14:** **A wrong reverse is fixed, with an OSD release note.** If a shape's reverse is wrong, fix it
  with no legacy switch, the same as the Phase 2 VBIP (P2-D14) and Phase 3 Simple-mode (03 D-02)
  precedent. Add the release-note item to the OSD-side follow-ups in PROJECT.md External Dependencies.
  — **Reversibility:** costly — once OSD ships the corrected motion, reverting changes users' sound a
  second time.
  The D-07 / D-09 sender changes do not change OSD's sound. They change what OSD sends on the wire
  (an initial send, query replies). List them in the same OSD release-note item.

### Gaps closed after the v1.0.0 milestone audit (2026-10-05, `.planning/v1.0.0-MILESTONE-AUDIT.md`)
- **D-15:** **Fix the `-Wunused-parameter` warnings in `ADMOSCReceiver.h`.** The audit lists five such
  warnings (named but unused Listener parameters at `include/SpatialCore/OSC/ADMOSCReceiver.h:45,51`)
  in every OSD translation unit. Phase 4 already edits this file for D-08, so silence them there (e.g.
  unnamed parameters). No behaviour change. Folded in by user choice.
- **D-16:** **Phase 4's VERIFICATION must record DATA-02 explicitly and tick it in REQUIREMENTS.md.** The
  audit marks DATA-02 "unsatisfied" only because no phase VERIFICATION records it and its checkbox is
  `[ ]`, even though the fonts are compiled in. The D-05 test is the evidence. EXTR-04 and EXTR-05 are
  ticked the same way when verified.
- **D-17:** **The new UI test target joins the local gate.** CI's test step runs zero tests (audit
  cross-phase blocker, owned by Phase 6 / CI-01), so the local Debug + Release suites are the only gate.
  The UI test target from D-01 must run in that same local gate command, not just build. Phase 4 does
  not fix CI.
- **D-18:** **"Sound moves" tests (D-11) drive `RenderEngine` like a current consumer:** set
  `engineComputesGains` and `engineDerivesDispatch`. That keeps them off the opt-out silent pass-through
  path the audit names under INTG-02. The audit's other open flows (a format switch during an HRTF
  crossfade; one test with all three engine flags on a binaural block) stay with Phase 5/6, not here.

### Claude's Discretion
- UI test target name and structure (e.g. `SpatialCoreUITests` linking `SpatialCoreUI`), and how to run
  JUCE components headless (`ScopedJuceInitialiser_GUI`, simulated `MouseEvent`s or
  `MouseInputSource`, rendering to a `juce::Image`).
- Demo app name, CMake option name (default OFF), and how Claude drives it for screenshots.
- The sender's clock source and how a test injects a fake one.
- How the query reply gets the current position (D-08), and the return-port configuration.
- Exact level-change threshold for "the sound moved" tests, and which output format(s) they use (stereo
  is simplest; at least one).

### Deferred Ideas (OUT OF SCOPE)
- Elevation drag on the spatial map (D-04): a new capability, later phase.
- A shared library connector from OSC / trajectory / map to `RenderEngine` (D-12): later, possibly when OpenSpatialPanner shows the glue repeating.
- Full ADM-OSC query support for non-position messages (gain, name, etc.): later.
- Raising the broadcast rate toward ADM-OSC's typical ~50 Hz: not v1 (roadmap and OSD use 30 Hz).
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| EXTR-04 | ADM-OSC and the trajectory engine drive real object motion | Sound-moves rig measured (Binaural L/R ratio 3.31 at az +90; Quad channel routing); real-UDP receiver test path verified; sender schedule algorithm simulated (30.0/s at 120/60/50/40/30 Hz callers); reverse semantics audited for all 13 shapes (11 exact, Bounce and Line deliberate, Random seeded) |
| EXTR-05 | UI components render and interact for real | Headless harness built and run in a scratch JUCE 9.0.0 build: `ScopedJuceInitialiser_GUI` via a Catch2 listener, synthesized `MouseEvent`, offscreen `Image` snapshots, pixel numbers for elevation opacity; DR-16 grep clean |
| DATA-02 | `SMLLookAndFeel` has the font BinaryData it needs | Embedded bytes == `fonts/*.ttf` for all 8 resources (probe); spy-LookAndFeel provenance test is red today (2 calls), green after the D-06 change (0 calls); no family-name request exists in `src/UI` or `include/SpatialCore/UI` (grep, 0 hits) |
</phase_requirements>

## Project Constraints (from CLAUDE.md)

- **Facade boundary (SC-13):** consumers drive rendering through `RenderEngine` only; tests wire OSC / trajectory / map the way a consumer would and must not dispatch algorithms or build `LayoutContext`s. Stereo-variant gains (`objGainL`/`objGainR`) stay consumer-side, so the sound-moves tests use Binaural and a surround layout, not `Stereo`.
- **Lock-free audio path:** nothing in this phase may add allocation, locks or logging to anything reached from `processBlock`. Sender/receiver/UI code runs on the message thread; do not touch `RenderEngine`'s audio path.
- **NEVER modify the `SpatializationAlgorithm` interface** without a major bump (not touched in this phase). Adding a defaulted virtual to `ADMOSCReceiver::Listener` is a different class and is additive.
- **ALWAYS maintain backward compatibility with existing plugins:** OSD must look identical (D-06); `ADMOSCSender::tick(5 args)` must keep compiling and behaving (D-07); `SMLLookAndFeel`'s public typeface members stay.
- **ALWAYS run the full test suite before tagging a release;** CI runs zero tests, so the local Debug and Release suites are the gate (D-17).
- **`SpatialCore` vs `SpatialCoreUI` targets:** `SpatialCoreTests` deliberately does not link `SpatialCoreUI`; the new UI test target is the only place that does.
- Project skills read: `.claude/skills/adm-osc-integration/adm-osc-integration.md` (documents "SML Plugins | Send + Receive | Default receive: 4002, send: 4003", line 200; relevant to the return-port pitfall below).

## Summary

Phase 4 is verification plus five small, contained code changes, not new capability. The three routes (OSC, trajectory, map drag) already produce positions; what is missing is proof that each moves the **rendered audio**, proof for the map and fonts, and the sender/receiver changes D-07 to D-09 and D-15. Everything D-01 needs is feasible on this Mac: I built a scratch copy of the tree against the cached JUCE 9.0.0 source, linked a Catch2 executable against `SpatialCoreUI`, and ran a headless drag, offscreen pixel capture, a font-provenance spy, a real UDP loopback into `ADMOSCReceiver`, and a `juce_add_gui_app` demo that wrote a PNG and quit in about one second. The scratch tree is in `/tmp/sc-p4-probe` (not in the worktree).

Five findings change the plan relative to CONTEXT.md and need the planner's attention. (1) **D-08(a) cannot reply "to the sender's IP" with `juce::OSCReceiver`**: its read loop calls `socket->read (oscBuffer.getData(), bufferSize, false)` and discards the sender address (JUCE 9.0.0 `juce_OSCReceiver.cpp:480`), so the reply must go to the destination the plugin is already configured to send to, via a new defaulted Listener callback. This also removes a UDP-reflection risk. (2) **D-13 is literally false for Bounce and Line**, and both are deliberate: OSD#100 was filed because reverse "has no effect" on Bounce, Line and Random, and the current code makes reverse a mirrored diagonal (Bounce) and a half-period shift (Line). Eleven of the twelve deterministic shapes satisfy `reverse(p) == forward(1-p)` exactly; those two do not and need explicit, different assertions. (3) **The `1/30 s elapsed` rule in D-07 is fragile:** the naive "send when `now - last >= 1/30`" rule yields 21.0 messages/s from a 60 Hz caller (simulated), failing "OSD at 60 Hz must see the same behaviour"; a schedule-based rule gives exactly 30.0/s. (4) **D-06 lists three font-fallback sites; there are five** in `SpatialMapComponent.cpp` (247-248, 290-291, 553-554, 581-582, 597-598), plus unconditional default-font use in `PresetBrowser.cpp:148`. (5) **Receiver input hardening is needed** and is in the file D-08 already edits: a string argument to `/enabled` becomes value 0 (and a Debug assertion), NaN in `/aed` is indistinguishable from the single-axis sentinel, and a 1e10 azimuth reaching `wrapAzimuth()` never returns (verified, SIGALRM after 3 s).

**Primary recommendation:** add `SpatialCoreUITests` (Catch2 + a `ScopedJuceInitialiser_GUI` event listener), extend the existing OSC/Trajectory suites, add one shared "route to RenderEngine to per-channel RMS" rig, make the five source changes (sender schedule + first-send + connect/re-enable send, receiver query hook + hardening + unnamed params, map-owned embedded fonts), and build the demo as a `juce_add_gui_app` with an offscreen `--screenshots` mode.

## Architectural Responsibility Map

This is an audio-plugin library, not a web stack, so tiers are threads and layers.

| Capability | Primary Tier | Secondary Tier | Rationale |
|------------|-------------|----------------|-----------|
| UDP receive and OSC decode | JUCE OSC network thread | Message thread (MessageLoopCallback dispatch) | `ADMOSCReceiver` uses `MessageLoopCallback`, so listener callbacks run on the message thread; verified: no delivery until the message loop is pumped |
| Position store and NaN-axis merge | Consumer glue (message thread) | — | Receiver has no store by design (`ADMOSCReceiver.cpp:50-57` comment); D-12 keeps glue consumer-side |
| Position to gains to audio | Audio thread (`RenderEngine::renderBlock`) | — | Consumer sets `engineComputesGains` + `engineDerivesDispatch` (D-18) |
| Trajectory animation | Message-thread timer (about 60 Hz) | Audio thread reads `getFinalAz()` (RTSF-02, Phase 5) | `TrajectoryEngine.h:30` documents the split; the race itself is out of scope |
| Map drag, hit-test, pixel/polar conversion | Message thread (UI) | — | `SpatialMapComponent::mouseDown/mouseDrag` call `Listener` synchronously |
| OSC broadcast and query reply | Message-thread timer calling `ADMOSCSender::tick` | — | Sender owns the 30 Hz clock (D-07); replies flush on the same gate |
| SML fonts | `SpatialCoreUIFontData` BinaryData (build time) | `SMLLookAndFeel` and, after D-06, the map | Embedded; no family-name lookup anywhere |
| Screenshots | Offscreen `Component::createComponentSnapshot` | — | Needs no window, display permission or screen recording |

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| JUCE | 9.0.0 | `juce_gui_basics`, `juce_graphics`, `juce_osc`, `juce_events` | Already pinned: `GIT_TAG        9.0.0` (`CMakeLists.txt:32`). A cached JUCE 9.0.0 source tree exists at `/private/tmp/sc-kemar-only-validate/_deps/juce-src` (`JUCE_MAJOR_VERSION 9`) `[VERIFIED: juce_StandardHeader.h read this session]` |
| Catch2 | v3.7.1 | Test framework, `Catch2WithMain`, `EventListenerBase` for GUI init | Already pinned: `GIT_TAG v3.7.1` (`tests/CMakeLists.txt:5`) |
| CMake `juce_add_gui_app` | JUCE 9.0.0 | Demo app target | Verified working in the scratch tree: built and ran |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| `juce::PNGImageFormat` | JUCE 9.0.0 | Write demo screenshots | Demo `--screenshots` mode |
| `juce::DatagramSocket` / `OSCSender` | JUCE 9.0.0 | Real loopback in tests | Existing pattern in `tests/OSC/ADMOSCSenderTests.cpp` (ports 9700+) |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| `ScopedJuceInitialiser_GUI` in a Catch2 listener | A custom `main()` | Listener works with `Catch2WithMain` unchanged (verified); a custom main adds a second `main` to maintain |
| Offscreen `createComponentSnapshot` | `screencapture` of a real window | Real capture needs Screen Recording permission and a visible window; offscreen verified to need neither |
| Real-UDP receiver test | `testProcessOSCMessage` only | `testProcessOSCMessage` (guarded by `JUCE_UNIT_TESTS`) skips the socket and message-loop hop; keep it for grammar tests, add one real-UDP test for criterion 1 |

**Installation:** none. No new external packages (see audit).

**Version verification:** versions above are the repo's own pins, read from `CMakeLists.txt` and `tests/CMakeLists.txt`; no registry lookup applies (FetchContent git tags).

## Package Legitimacy Audit

| Package | Registry | Age | Downloads | Source Repo | Verdict | Disposition |
|---------|----------|-----|-----------|-------------|---------|-------------|
| (none new) | — | — | — | — | — | Phase 4 installs no external packages. JUCE 9.0.0 and Catch2 v3.7.1 are existing FetchContent pins; the demo app and UI tests use only JUCE modules already in the tree |

**Packages removed due to [SLOP] verdict:** none
**Packages flagged as suspicious [SUS]:** none

## Architecture Patterns

### System Architecture Diagram

```
                       (tests wire this the way a consumer does; D-12 = no library connector)

 ADM-OSC device ──UDP──▶ juce::OSCReceiver thread ──post──▶ MESSAGE THREAD
   /adm/obj/N/aed|xyz|…        (drops sender IP)               │
                                                        ADMOSCReceiver::oscMessageReceived
                                                          │ args present ─▶ Listener::admPositionReceived(i, az, el, d)  [NaN = axis not sent]
                                                          │ NO args     ─▶ Listener::admPositionQueried(i, kind)   (new, D-08a)
                                                          ▼
 TrajectoryEngine::tick(i, input, dt) ──▶ getFinalAz/El/Dist ─┐
 SpatialMapComponent::mouseDrag ─▶ Listener::objectPositionChanged(i, az, dist) ─┤
                                                          ▼
                                         CONSUMER GLUE (per-object state; NaN-merge; D-12)
                                                          │ RenderSources.objects[i] = {az, el, dist, enabled}
                          RenderBlockContext{engineComputesGains=true, engineDerivesDispatch=true}  (D-18)
                                                          ▼
                                         AUDIO THREAD: RenderEngine::renderBlock ─▶ out channels
                                                          ▼
                                     test asserts per-channel RMS moved the right way (D-11)

 CONSUMER timer (60 Hz) ─▶ ADMOSCSender::tick(az[], el[], dist[], enabled[], n [, nowSeconds])
        schedule gate (30 Hz) ─▶ dead-band + first-send + connect/re-enable send ─▶ UDP /adm/obj/N/aed
        pending query replies (coalesced per object+kind) flush on the same gate ─▶ configured host:port
```

### Recommended Project Structure
```
tests/
├── CMakeLists.txt                 # add SpatialCoreUITests beside SpatialCoreTests (before include(CTest))
├── Support/RouteRenderRig.h       # NEW shared header: one object -> RenderEngine -> per-channel RMS
├── Engine/ControlRouteTests.cpp   # NEW in SpatialCoreTests: OSC route + trajectory route (sound moves)
├── OSC/ADMOSCSenderTests.cpp      # EXTEND: rate, static, first-send, connect, re-enable, query reply
├── OSC/ADMOSCReceiverTests.cpp    # EXTEND: queries, wrong-type args, non-finite; + one real-UDP test
├── Trajectory/TrajectoryTests.cpp # EXTEND: reverse sweep, tick() sweep, Random seeded
└── UI/                            # NEW target SpatialCoreUITests
    ├── UITestMain.cpp             # Catch2 listener owning ScopedJuceInitialiser_GUI
    ├── UITestSupport.h            # makeMouseEvent, snapshot, SpyLookAndFeel
    ├── SpatialMapComponentTests.cpp
    ├── FontProvenanceTests.cpp
    └── MapRouteTests.cpp          # map drag -> RenderEngine sound-moves
examples/
├── CMakeLists.txt                 # juce_add_gui_app(SpatialCoreDemo)
└── demo/ (Main.cpp, DemoComponent.{h,cpp})
```
New source files must be added by hand to the target source list (the existing list is hand-maintained, `tests/CMakeLists.txt`).

### Pattern 1: Headless JUCE GUI harness (verified)
**What:** Initialise JUCE GUI once per test process from a Catch2 event listener, construct components without a window, synthesize `MouseEvent`s, and render with `paintEntireComponent` or `createComponentSnapshot` into an `Image`.
**When to use:** every `SpatialCoreUITests` case.
**Evidence:** ran in `/tmp/sc-p4-probe/src/build/tests/ProbeUITests` (Debug, macOS arm64, JUCE 9.0.0); outputs quoted in Code Examples. No `NSApplication` setup and no message-loop pump was needed for components, listeners or painting; a message-loop pump is only needed for `MessageLoopCallback` delivery (Pattern 5). I ran this from a logged-in terminal session; behaviour with no WindowServer session (SSH, headless CI) was not tested `[ASSUMED]` (A1).

### Pattern 2: Sound-moves rig (verified numbers)
**What:** One object through `RenderEngine` with both opt-in flags set, 4 blocks of 64 samples at 48 kHz, RMS per channel of the last block.
**Measured (Debug, mono = `0.5*sin(0.2*n)`, dist 0.5, el 0):**

| Format | az | Result (RMS) |
|--------|----|--------------|
| `OutputFormat::Binaural` (Simple Woodworth, no profile requested) | +90 | L=0.1557 R=0.0470 (L/R = 3.31) |
| | +45 | L=0.1557 R=0.0668 |
| | 0 | L=0.1557 R=0.1557 |
| | -45 | L=0.0668 R=0.1557 |
| | -90 | L=0.0470 R=0.1557 |
| `OutputFormat::Quad` | +45 | ch0=0.3503, ch1=ch2=ch3=0.0000 |
| | -45 | ch1=0.3503, others 0.0000 |
| | +135 | ch2=0.3503, others 0.0000 |

Recommended assertions (margins from the measurements): Binaural az +90 gives `L/R >= 2.0`; az -90 gives `L/R <= 0.5`; az 0 gives `|L/R - 1| < 0.05`. Quad: the loudest channel index equals the speaker nearest the azimuth and every other channel is below 1% of it. Use Binaural and Quad, not `Stereo` (its gains are consumer-side).

### Pattern 3: Sender schedule clock (simulated)
**What:** The sender holds `nextDue_`; it sends when `now >= nextDue_`, then `nextDue_ += 1/30`; if `now - nextDue_ >= 1/30` it resyncs to `now + 1/30` (no burst after a gap). First call sends and arms the schedule.
**Simulation (10 s per row, jitter uniform +-2 ms):**

| Caller rate | Naive `now-last >= 1/30` | Schedule rule |
|-------------|--------------------------|---------------|
| 120 Hz | 24.6 to 26.3 /s | 30.0 /s |
| 60 Hz | 21.0 to 23.2 /s | 30.0 /s |
| 50 Hz | 25.0 /s | 30.0 /s |
| 40 Hz | 20.0 /s | 30.0 /s |
| 30 Hz | 16.1 to 19.2 /s | 29.9 /s |
| 25 Hz | 25.0 /s | 25.0 /s (capped by the caller) |

Test shape: inject `nowSeconds` through a new overload `tick (az, el, dist, enabled, n, double nowSeconds)`; keep the 5-argument overload delegating with `juce::Time::getMillisecondCounterHiRes() * 0.001`. A parameter, not a `std::function`, keeps the fake clock trivial and the header ABI small.

### Pattern 4: Receiver query hook (D-08a)
**What:** Add a **non-pure** `virtual void admPositionQueried (int objectIndex, PositionQuery kind) {}` to `ADMOSCReceiver::Listener` with unnamed parameters, and add `message.size() == 0` branches for `/azim /elev /dist /aed /xyz`. The consumer answers by calling a sender method; the sender coalesces one pending reply per (object, kind) and flushes it on the next schedule slot to its **configured** host/port. Reply address mirrors the query (`/xyz` in, `/adm/obj/N/xyz x y z` out; `/azim` in, one float out), matching the spec's own example `/adm/obj/4/xyz -0.9 0.15 0.0`. Inverse of the receiver's conversion (`ADMOSCReceiver.cpp:12-21`, `azDeg = std::atan2 (-x, y)`) is `x = -d*cos(el)*sin(az)`, `y = d*cos(el)*cos(az)`, `z = d*sin(el)`.

### Pattern 5: Real-UDP receiver test (verified in both executables)
`ADMOSCReceiver` uses `MessageLoopCallback`. Delivery needs (a) a live `MessageManager` and (b) the loop pumped: `juce::MessageManager::getInstance()->runDispatchLoopUntil (ms)`, which only exists when `JUCE_MODAL_LOOPS_PERMITTED=1` (JUCE 9.0.0 `juce_MessageManager.h:99-105`; default 0 at `juce_PlatformDefs.h:325-328`). Results: before pumping `n=0`; after `runDispatchLoopUntil (300)`, `n=1 idx=2 az=45.0 el=10.0 d=0.70`. In a non-GUI executable (SpatialCoreTests-style, no `juce_gui_basics`) the same test **segfaults** unless the test holds a `juce::ScopedJuceInitialiser_GUI` (asserts at `juce_MessageListener.cpp:50`, `juce_MessageManager_mac.mm:435`, then SIGSEGV; with the initialiser: passes, 0 assertions). `ScopedJuceInitialiser_GUI` lives in `juce_events`, so no GUI module is needed. Add `JUCE_MODAL_LOOPS_PERMITTED=1` to every target that pumps.

### Pattern 6: Map-owned embedded fonts (prototyped and compared)
`SpatialMapComponent` creates its own JetBrains Mono Regular/Medium/Bold typefaces from `SpatialCoreUIFontData::JetBrains_Mono*_ttf` in its constructor and uses them at all five sites, deleting the `lf` branch and the per-paint `dynamic_cast`. Prototype results (scratch copy only): the SML-look-and-feel render is **byte-identical before and after** (`cmp` equal, OSD looks identical, D-06), the no-look-and-feel render is byte-identical to the SML render, and the spy sees 0 default-look-and-feel font requests in both modes (before: 2).

### Pattern 7: Offscreen demo screenshots (verified)
`juce_add_gui_app` demo, run by executing the binary directly; `initialise()` builds the component, calls `createComponentSnapshot`, writes a PNG with `PNGImageFormat`, then `quit()`. Built in about 10 s incremental, ran in 0.99 s, wrote a valid 480x480 PNG. For D-03 the demo supports `--screenshots <dir>` and writes `before-drag.png` and `after-drag.png` by driving the same synthesized mouse events as the tests.

### Anti-Patterns to Avoid
- **`now - last >= 1/30` as the gate:** 21.0/s at a 60 Hz caller (Pattern 3).
- **Asserting that a position value arrived** instead of that a channel level moved (D-11).
- **Using `Stereo` for sound-moves tests:** its gains are consumer-side; the test would pass for the wrong reason.
- **Building the gate with `--target SpatialCoreTests` only:** once a second Catch2 target exists, an unbuilt target registers a failing `<target>_NOT_BUILT` placeholder (verified in the generated include file); the old Phase 3 command would then fail. Build all targets.
- **Real-network packet capture for the 30 Hz proof:** D-10 says fake clock; counting over loopback is acceptable but must wait for delivery.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| OSC packet parse | A custom UDP parser | `juce::OSCReceiver` / `OSCSender` | JUCE catches `OSCFormatError` for malformed packets (`juce_OSCReceiver.cpp:431-453`); reply-to-sender-IP is the one thing it cannot do, and the answer is not a custom parser (see Open Question 2) |
| Synthesized mouse | A fake component event shim | `juce::MouseEvent` ctor with `Desktop::getInstance().getMainMouseSource()` | Verified: drag to the left at 0.8 radius reports `az=90.000 d=0.800` |
| Image output | Custom PNG writer | `juce::PNGImageFormat` | Verified |
| Typeface loading | Custom TTF parser | `juce::Typeface::createSystemTypefaceFor (data, size)` | Already how `SMLLookAndFeel` does it (`SMLLookAndFeel.cpp:21-27`) |
| Font provenance | Inspecting glyph bytes | A spy `LookAndFeel` plus embedded-bytes-equal-disk test | JUCE 9 resolves default fonts through the **default** look-and-feel (see Pitfall 4); a spy there sees every implicit lookup |

**Key insight:** the work in this phase is mostly test plumbing around code that exists. The risk is in assertions that pass for the wrong reason (position arrived, naive clock, Stereo, system-font fallback), not in missing libraries.

## Common Pitfalls

### Pitfall 1: Naive 1/30 s gate under-sends
**What goes wrong:** sends 21.0/s at a 60 Hz caller (simulated), 16 to 19/s at 30 Hz.
**Why:** exact multiples of the tick period never exceed `1/30` after float rounding or jitter, so every third tick sends.
**How to avoid:** schedule rule (Pattern 3); fake-clock tests at 120, 60, 50, 30 Hz with and without jitter assert `30 +- 1` per second.
**Warning signs:** a test that only calls `tick` at exactly 1/60 s intervals passes a wrong rule.

### Pitfall 2: D-13's point-by-point rule is false for Bounce and Line (deliberate behaviour)
**What:** measured `max|reverse(p) - forward(1-p)|` over p in 0..1: shapes 2-7, 9, 11-13 are exactly 0.0000; Bounce (1) is 180.0 and Line (8) is 90.0 (base az 30, el 10, dist 0.5).
**Why:** `computeTrajectory` starts with `phase = 1.0f - phase` (`TrajectoryEngine.cpp:169-170`). A ping-pong path (Bounce triangle wave, Line cosine) is symmetric under `phase -> 1-phase`, so that flip is invisible. OSD#100 (closed; read via `gh issue view`) reports "The Forward/Reverse direction toggle has no effect on Bounce, Line, and Random trajectories", and the fix was: Bounce negates the azimuth offset (`float azSign = reverse ? -1.0f : 1.0f;` line 186) producing the mirrored diagonal, Line adds half a period (`phase = std::fmod (phase + 0.5f, 1.0f);` line 328), Random flips the sign of `randomTime_` in `tick()` (line 65).
**How to avoid:** the reverse test needs three assertion kinds: (a) 10 shapes (2-7, 9, 11-13): `reverse(p) == forward(1-p)` within 1e-4; (b) Line: `reverse(p) == forward(p + 0.5)` (opposite velocity, same set of points); (c) Bounce: azimuth offset from base is negated and elevation offset equal (`rev.az - base == -(fwd.az - base)`), documented as the OSD#100 behaviour. Do **not** "fix" Bounce or Line under D-14 without a user ruling (Open Question 1).
**Existing coverage:** one reverse test exists, Orbit only: `TEST_CASE ("Reverse flips phase", ...)` at `TrajectoryTests.cpp:204-209` (CONTEXT says "zero"; it is one).

### Pitfall 3: `TrajectoryEngine::getState().reverse` is always false
**What:** `ts.reverse     = false;  // caller must check param separately` (`TrajectoryEngine.cpp:112`); the map's trail samples with `ts.reverse` (`SpatialMapComponent.cpp:322`) and brightens the segment nearest `ts.phase` (lines 350-352).
**Impact:** a consumer that passes `getState()` straight to `setTrajectoryState` (the demo app would) shows a trail and glow focus for the forward direction while the dot runs reverse. For the 10 symmetric shapes the trail points are the same set, so only the glow focus is mirrored; Bounce and Line trails are wrong. OSD overrides it: `PluginProcessor.cpp:4145` sets `result.reverse` from its parameter (OSD repo, read this session). So OSD is fine and the glitch is an API trap for new consumers.
**How to avoid:** the demo sets `ts.reverse` itself exactly as OSD does; a library change (store `reverse` per object in `tick`) is optional and must not change the map's look for OSD (D-06).

### Pitfall 4: JUCE 9 resolves implicit fonts through the DEFAULT look-and-feel
**What:** `Font::getTypefacePtr` uses an explicit typeface when `FontOptions (typeface)` was given (`juce_Font.cpp:199-235`, explicit typeface taken at line 218: `options.getTypeface()`), else asks `TypefaceCache`, which calls `juce_getTypefaceForFont`, wired to `LookAndFeel::getDefaultLookAndFeel().getTypefaceForFont (font)` (`juce_LookAndFeel.cpp:38-40`). A component-level `setLookAndFeel (&sml)` does **not** affect it.
**Impact:** every font created as `juce::FontOptions (10.0f)` without a typeface uses the system sans (`<Sans-Serif>`) unless `SMLLookAndFeel` is installed as the **default**. Probe: map painted with a spy default look-and-feel and no SML look-and-feel made 2 calls named `<Sans-Serif>`.
**Sites (verbatim):** map: `247-248` `if (lf) g.setFont (makeFont (lf->jetbrainsRegular, 9.0f));` / `else    g.setFont (juce::FontOptions (9.0f));`, `290-291`, `553-554`, **`581-582`**, **`597-598`** (the last two are not in D-06). Elsewhere: `PresetBrowser.cpp:148` `auto titleFont = juce::Font (juce::FontOptions (13.0f).withStyle ("Bold"));` (unconditional, no typeface), `OSCSectionComponent.cpp:35,49` and `GlobalTapDrawer.cpp:90` (null-typeface fallbacks, reachable only if the consumer passes none), `SMLLookAndFeel.cpp:67,392` (dead fallbacks when a load failed).
**How to avoid:** D-06 sweep covers all five map sites; the spy test also paints the other widgets to flag `PresetBrowser.cpp:148` (Open Question 6). Call `juce::Typeface::clearTypefaceCache()` before counting, because the cache hides repeat lookups.

### Pitfall 5: Debug builds assert on wrong-typed OSC arguments; Release silently coerces
`OSCArgument::getInt32()` on a non-int does `jassertfalse; return 0;` (`juce_OSCArgument.cpp:54-60`). `ADMOSCReceiver` does `message[0].isFloat32() ? ...getFloat32() : ...getInt32()` for `/enabled /trajectory /direction /input` and `/osd/global/*`. Probe: `/osd/obj/1/enabled` with a string argument printed `JUCE Assertion failure in juce_OSCArgument.cpp:59` and forwarded `param 0 enabled 0.000000` (an attacker-chosen state change in Release, no assertion). Fix with explicit `isInt32()` / `isFloat32()` checks and ignore anything else.

### Pitfall 6: Non-finite and out-of-range floats pass straight through
Probe (Debug): `/aed` with NaN azimuth forwards `pos 0 nan 10.0 0.5` (NaN is the "axis not sent" sentinel, so a forged or buggy full update silently becomes a partial one); `/aed 1e30 1e30 1e30` forwards unchanged. `wrapAzimuth` is `while (az > 180.0f) az -= 360.0f;` (`TrajectoryEngine.h:13`): a standalone test built from that exact loop returned for 720 and 1e7 but **did not return for 1e10 or 1e30** (killed by `alarm(3)`, exit 142). A consumer that stores an OSC azimuth as a trajectory origin hangs its timer thread. `RenderEngine` already sanitizes non-finite values (`sanitizeSources`, D-06a) but not huge finite ones. Fix at the receiver boundary: reject non-finite, clamp elevation to [-90, 90] and distance to [0, 1], wrap azimuth with a bounded operation. Compliant senders are unaffected (ADM-OSC ranges: azim -180..180, elev -90..90, dist 0..1).

### Pitfall 7: ctest names collide and unbuilt targets fail
Two `catch_discover_tests` targets in one directory must not share test names; give the UI target `TEST_PREFIX "ui: "`. Build the whole project in the gate (`cmake --build build-release -j8`), because an unbuilt Catch2 target registers a failing `*_NOT_BUILT` test (verified).

### Pitfall 8: Reply to a loopback destination on the plugin's own receive port
SML plugins receive on 4002 and send on 4003 by project convention (`adm-osc-integration.md:200`), while the ADM-OSC spec's return port is also 4002 and the default sender port in code is `int  sendPort  = 4003;` (`ADMOSCSender.h:27`). A reply to `127.0.0.1:4002` lands on the plugin's own receiver (benign, it has arguments so it is not re-answered, but it is self-feedback). Reuse `oscPortsConflict` (`OSCPortValidation.cpp`) before connecting.

### Pitfall 9: Component/Typeface state and timing in UI tests
`Desktop::getInstance().getMainMouseSource()` and component painting crash without `ScopedJuceInitialiser_GUI`. Each `SMLLookAndFeel` instance creates fresh typefaces (probe: `second instance same ptr? 0`), so pointer identity across instances is not a valid assertion. The probe's typeface names (`DM Sans`, `JetBrains Mono`, `Roboto`) equal the installed system family names (this Mac has them), so a name check proves nothing.

### Pitfall 10: Drag edge cases
`pixelToSpatial` clamps distance to [0, 1] (`SpatialMapComponent.cpp:97`), so a drag outside the outer ring reports `d = 1.0`. Dragging to exactly the centre evaluates `atan2 (-0.0f, -0.0f)`, which is -pi by IEEE rules, so azimuth reads -180 at distance 0 `[ASSUMED]` (A4); harmless audibly but assert only distance there. `mouseDrag` never changes elevation; assert it is preserved (D-04), verified `elevation preserved: 30.00`.

### Pitfall 11: CI builds every target on Linux
`ci.yml` runs `cmake --build build --config Release -j$(nproc)` and installs X11/freetype dev packages, so `SpatialCoreUITests` compiles on Linux in CI (not run: `ctest` at the build root finds zero tests, audit blocker, Phase 6). Keep test code free of macOS-only APIs. The demo stays OFF by default and guarded by `PROJECT_IS_TOP_LEVEL`.

## Code Examples

### Catch2 listener owning the GUI initialiser (verified, built and run)
```cpp
// tests/UI/UITestMain.cpp
#include <catch2/reporters/catch_reporter_event_listener.hpp>
#include <catch2/reporters/catch_reporter_registrars.hpp>
#include <juce_gui_basics/juce_gui_basics.h>

namespace {
struct JuceGuiListener : Catch::EventListenerBase
{
    using Catch::EventListenerBase::EventListenerBase;
    std::unique_ptr<juce::ScopedJuceInitialiser_GUI> init;
    void testRunStarting (Catch::TestRunInfo const&) override { init = std::make_unique<juce::ScopedJuceInitialiser_GUI>(); }
    void testRunEnded (Catch::TestRunStats const&) override { init.reset(); }
};
}
CATCH_REGISTER_LISTENER (JuceGuiListener)
```

### Synthesized drag and measured result (verified)
```cpp
juce::MouseEvent makeEvent (juce::Component& c, juce::Point<float> pos, juce::Point<float> down, bool dragged)
{
    return juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), pos, juce::ModifierKeys(),
        juce::MouseInputSource::defaultPressure, juce::MouseInputSource::defaultOrientation,
        juce::MouseInputSource::defaultRotation, juce::MouseInputSource::defaultTiltX,
        juce::MouseInputSource::defaultTiltY, &c, &c, juce::Time::getCurrentTime(), down,
        juce::Time::getCurrentTime(), 1, dragged);
}
// map(12), setSize(400,400), setObjectState(0, 0, 30, 0.5, true); mouseDown on getObjectScreenPos(0);
// mouseDrag to { 200 - 0.8f * (400 * 0.45f), 200 }  ->  listener: az=90.000 d=0.800, elevation still 30.00
```
Geometry (`SpatialMapComponent.cpp:78-84`): `radius = min(w,h) * 0.45f`, `x = cx - sin(az) * dist * radius`. Positive azimuth is the **left** of the screen and of the audio image.

### Pixel numbers for "elevation opacity" (verified)
Map 400x400, object 0 at az +90 el +80 d 0.5 and object 1 at az -90 el -80 d 0.5, `paintEntireComponent` into an ARGB `Image`, pixel at (object x + 4, object y):

| Object | Pixel | Meaning |
|--------|-------|---------|
| el +80 (above) | `ffed5e5e` = `objectColours[0]` exactly | fill alpha 1.0 (`isAbove ? 1.0f : 0.3f`, `SpatialMapComponent.cpp:545`) |
| el -80 (below) | `ff5c4027` (92,64,39) | fill alpha 0.3 over the void colour; darker |

Assert relative brightness (below sum of RGB under 0.6 x above), not exact values, so label glyph antialiasing cannot flake it. The four static rings are drawn at `r = 0.25, 0.5, 0.75, 1.0` (loop at line 238); assert an object at `dist d` sits at pixel distance `d * 0.45 * min(w,h)` from the centre (within 0.5 px) and a ring pixel is brighter than the void.

### Spy look-and-feel for font provenance (verified)
```cpp
struct SpyLNF : juce::LookAndFeel_V4 {
    int calls = 0; juce::StringArray names;
    juce::Typeface::Ptr getTypefaceForFont (const juce::Font& f) override
    { ++calls; names.add (f.getTypefaceName()); return juce::LookAndFeel_V4::getTypefaceForFont (f); }
};
// SpyLNF spy; juce::LookAndFeel::setDefaultLookAndFeel (&spy);
// juce::Typeface::clearTypefaceCache();  // BEFORE counting, the cache hides repeats
// paint the map with and without SMLLookAndFeel; then:  CHECK (spy.calls == 0);
// teardown: map.setLookAndFeel (nullptr); juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
```
Measured before the D-06 change: `no SML LNF: calls=2 names=<Sans-Serif>,<Sans-Serif>`; with SML: `calls=0`. After the prototype: 0 and 0.

### Embedded bytes equal the source fonts (verified; header reachable from the test target)
```cpp
#include "SpatialCoreUIFontData.h"   // propagates PUBLIC via SpatialCoreUI -> SpatialCoreUIFontData
// namedResourceListSize == 8; for each name: size equals fonts/<orig> and memcmp == 0
```
Eight resources, all equal: `DM_SansRegular_ttf` 48280, `DM_SansMedium_ttf` 48308, `DM_SansSemiBold_ttf` 48280, `DM_SansBold_ttf` 48200, `JetBrains_MonoRegular_ttf` 112172, `JetBrains_MonoMedium_ttf` 112204, `JetBrains_MonoBold_ttf` 112092, `RobotoMedium_ttf` 488584 (bytes). `DM_SansSemiBold` is embedded but `SMLLookAndFeel` loads only seven typefaces (no SemiBold member). The fonts are plain TTF (first bytes `00010000`), not Git LFS (`.gitattributes` tracks only `HRTF/*.sofa`).

### Grep gate for family-name requests (baseline passes today)
```bash
grep -rnE '"[^"]*(DM Sans|JetBrains|Roboto)[^"]*"|withName|setTypefaceName|getDefaultSansSerifFontName|Font *\( *"|FontOptions *\( *"|findAllTypefaceNames|getTypefaceName' \
  src/UI include/SpatialCore/UI ; test $? -eq 1     # 0 hits = pass
```
Run today: exit 1 (no matches).

### Receiver sketch: types, finiteness, query (planner to refine)
```cpp
// ADMOSCReceiver.h, Listener additions (unnamed params also close D-15, the five warnings at lines 45-46 and 51)
enum class PositionQuery { azim, elev, dist, aed, xyz };
virtual void admPositionQueried (int /*objectIndex*/, PositionQuery /*kind*/) {}
virtual void admObjectParamReceived (int /*objectIndex*/, const juce::String& /*paramName*/, float /*value*/) {}
virtual void admGlobalParamReceived (const juce::String& /*propertyName*/, float /*value*/) {}

// ADMOSCReceiver.cpp, before the existing branches (sketch)
if (message.size() == 0) { /* map property -> PositionQuery; call admPositionQueried; return */ }
auto numeric = [] (const juce::OSCArgument& a, float& out)
{ if (a.isFloat32()) { out = a.getFloat32(); return true; }
  if (a.isInt32())   { out = (float) a.getInt32();   return true; }
  return false; };                                   // strings/blobs/colours are ignored, never getInt32()'d
```

### Real UDP receiver test (verified in a GUI-less executable)
```cpp
juce::ScopedJuceInitialiser_GUI gui;                       // required, else SIGSEGV
ADMOSCReceiver rx; Rec l; rx.addListener (&l);
REQUIRE (rx.connect (9812));
ADMOSCSender tx; REQUIRE (tx.connect ("127.0.0.1", 9812));
tx.sendPosition (2, 45.0f, 10.0f, 0.7f);
juce::MessageManager::getInstance()->runDispatchLoopUntil (300);   // needs JUCE_MODAL_LOOPS_PERMITTED=1
CHECK (l.n == 1);   // idx=2 az=45.0 el=10.0 d=0.70
```
Use a distinct port per test case (existing tests use 9700+; the probe used 9811 and 9812).

### Trajectory route numbers (verified)
Orbit (`shape = 9`, speed 1.0, origin az 0, dist 0.5, `dt = 1/60`): after 15 ticks forward `az=90.00`; reverse `az=-90.00`. So forward quarter-turn puts the source left, reverse puts it right: deterministic inputs for the trajectory sound-moves test. `TrajectoryEngine::rng_` is a public `juce::Random` (`TrajectoryEngine.h:117`); call `engine.rng_.setSeed (n)` before ticking shape 10 or the Random test is nondeterministic.

### CMake for the UI test target and demo (verified pattern; names are discretionary)
```cmake
# tests/CMakeLists.txt, before include(CTest)
add_executable(SpatialCoreUITests
    UI/UITestMain.cpp UI/SpatialMapComponentTests.cpp UI/FontProvenanceTests.cpp UI/MapRouteTests.cpp)
target_link_libraries(SpatialCoreUITests PRIVATE Catch2::Catch2WithMain SpatialCoreUI SpatialCore)
target_compile_definitions(SpatialCoreUITests PRIVATE JUCE_UNIT_TESTS=1 JUCE_MODAL_LOOPS_PERMITTED=1)
catch_discover_tests(SpatialCoreUITests TEST_PREFIX "ui: ")
# also add JUCE_MODAL_LOOPS_PERMITTED=1 to SpatialCoreTests if it hosts the real-UDP receiver test

# top-level CMakeLists.txt, after the SpatialCoreUI target
option(SPATIALCORE_BUILD_EXAMPLES "Build the SpatialCore demo app" OFF)
if(PROJECT_IS_TOP_LEVEL AND SPATIALCORE_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
# examples/CMakeLists.txt
juce_add_gui_app(SpatialCoreDemo PRODUCT_NAME "SpatialCore Demo")
target_sources(SpatialCoreDemo PRIVATE demo/Main.cpp)
target_compile_definitions(SpatialCoreDemo PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
    JUCE_APPLICATION_NAME_STRING="SpatialCoreDemo" JUCE_APPLICATION_VERSION_STRING="0.1.0")
target_link_libraries(SpatialCoreDemo PRIVATE SpatialCoreUI SpatialCore)
```
In top-level mode `SpatialCoreUI` links `juce_gui_basics` and `juce_graphics` PUBLIC (`CMakeLists.txt` `_spatialcoreui_juce_modules` block), so test and demo targets inherit them. Each such executable recompiles the JUCE module sources (about 35 s clean, about 10 s incremental in the probe).

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| `Font` by family-name string | `FontOptions (Typeface::Ptr)`; JUCE 9 only consults the default look-and-feel for typefaceless fonts | JUCE 8 `FontOptions` | Explicit typefaces bypass every lookup, so provenance is testable with a spy |
| `Component::setLookAndFeel` assumed to drive fonts | Font resolution uses `LookAndFeel::getDefaultLookAndFeel()` | JUCE 8/9 | Component-level SML look-and-feel does not cover typefaceless fonts |
| Phase 3 gate `--target SpatialCoreTests && ctest` | Build all targets, then `ctest` | This phase (second Catch2 target) | Unbuilt target = failing `_NOT_BUILT` test |

**Deprecated/outdated:**
- `MouseInputSource::defaultPressure`-style old names such as `invalidPressure` are `[[deprecated]]` in JUCE 9 (`juce_MouseInputSource.h:251-267`); use the `default*` constants shown above.

## Assumptions Log

| # | Claim | Section | Risk if Wrong |
|---|-------|---------|---------------|
| A1 | The headless harness also works with no WindowServer session (SSH, launchd, CI); I only ran it from a logged-in terminal | Pattern 1 | The local gate would fail when run over SSH. Low impact: the gate is run in the developer's session; CI runs zero tests |
| A2 | `SpatialCoreUITests` compiles on the Linux CI image (X11 packages are installed by `ci.yml`, but I could not build there) | Pitfall 11 | CI turns red on a new target; fix is local to the test sources |
| A3 | Real ADM-OSC senders may send position axes as OSC integers (e.g. whole numbers from Max) or inside OSC bundles; the receiver ignores both (`isFloat32()` guard; `oscBundleReceived` is a no-op default at `juce_OSCReceiver.h:132`) | Open Questions 4 | An external sender could silently fail to move objects. The spec says float32; unverified in the wild |
| A4 | `atan2 (-0.0f, -0.0f)` is -pi per IEEE 754/C99, so a drag to the exact centre reports az -180 | Pitfall 10 | Cosmetic; assertion avoided |
| A5 | Replying only to the configured destination (not the packet source) satisfies the user's wish to follow the ADM-OSC standard closely enough; the spec text says "sender's IP ... return port (default: 4002)" | Open Question 2 | User may want the strict behaviour, which needs a different receive path |

## Open Questions

1. **Bounce and Line reverse versus D-13/D-14 (needs a user ruling)**
   - What we know: `reverse(p) == forward(1-p)` holds exactly for 10 deterministic shapes. Bounce reverses to a mirrored diagonal and Line to a half-period shift, both on purpose (OSD#100, closed). Random flips time.
   - What's unclear: whether the user counts the mirrored-diagonal Bounce as "wrong" under D-14 (which would change shipped OSD motion again).
   - Recommendation: keep current behaviour, assert it explicitly (Pitfall 2 kinds a-c), and record it in VERIFICATION as an intentional exception. If the user rules otherwise, D-14 applies with a release note.

2. **D-08(a) reply destination (needs a user ruling because the user asked to "follow the standard")**
   - What we know: the spec says reply "to the sender's IP address, using the return port (default: 4002)" `[CITED: immersive-audio-live.github.io/ADM-OSC]`; `juce::OSCReceiver` never exposes the sender (JUCE 9.0.0 `juce_OSCReceiver.cpp:480`). Hand-parsing OSC from a raw `DatagramSocket` would work but breaks "don't hand-roll".
   - Recommendation: reply to the sender's **configured** host:port through `ADMOSCSender` (Pattern 4). It is the same address the plugin already broadcasts to, needs no new receive path, and cannot be used for UDP reflection to a spoofed source. State the deviation in the plan and the OSD release note.

3. **Where the query's current position comes from**
   - Recommendation: the consumer answers `admPositionQueried` by calling a new `ADMOSCSender::queueReply (index, kind, az, el, dist)`; no position store in the receiver (keeps DR-16).

4. **Receiver hardening scope**
   - Recommendation: include in the same edit as D-08/D-15: type checks, non-finite rejection, clamp/wrap, with tests (Pitfalls 5 and 6). Optional extras the planner may defer: accept int32 for position axes and handle bundles (A3).

5. **Map trail under reverse (CONTEXT asked me to check)**
   - Confirmed (Pitfall 3). Recommendation: demo overrides `ts.reverse` like OSD; no library change in Phase 4 unless the planner wants `getState()` fixed (additive, OSD overrides it anyway).

6. **D-06 sweep breadth**
   - Recommendation: map (5 sites) is required; also give `PresetSaveOverlay` an explicit typeface (`PresetBrowser.cpp:148`) so the spy test over the other widgets is clean. This changes that overlay's title from system sans to DM Sans Bold only where SML is not the default look-and-feel; confirm it is acceptable for OSD's look.

7. **Where the OSC and trajectory sound-moves tests live**
   - Recommendation: OSC and trajectory in `SpatialCoreTests` (no GUI init, fast, `Engine/ControlRouteTests.cpp`), the map route in `SpatialCoreUITests` (it needs `SpatialMapComponent`); both include `tests/Support/RouteRenderRig.h`.

8. **OSD release-note content (D-14)**
   - The note covers: sender sends once on connect and on re-enable, first position always sent, answers position queries to its configured destination, and (if hardening ships) out-of-range/non-numeric OSC values are now ignored or clamped.

## Environment Availability

| Dependency | Required By | Available | Version | Fallback |
|------------|------------|-----------|---------|----------|
| CMake | all builds | yes | 4.4.4 | — |
| Clang / Apple CLT | builds | yes | CLT at `/Library/Developer/CommandLineTools` (`xcodebuild` itself not installed) | — |
| JUCE 9.0.0 source | builds | yes (cached) | `/private/tmp/sc-kemar-only-validate/_deps/juce-src`; fresh worktrees fetch via FetchContent | pass `-DFETCHCONTENT_SOURCE_DIR_JUCE=<path>` offline |
| Catch2 v3.7.1, libmysofa v1.3.5 | tests | yes (cached beside JUCE) | same `_deps` folder | same override flags |
| HRTF SOFA files (real, not LFS stubs) | configure | yes | all five present, 1.1 to 36.6 MB | `git lfs pull` |
| macOS display session | UI tests, demo | yes (logged-in terminal) | — | untested without one (A1) |
| `gh` CLI | issue lookups | yes | — | — |

The worktree has no `build/` or `JUCE/` directory; the worktree's CMake would fetch from GitHub unless pointed at the cached sources. Scratch configure that worked: `cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DFETCHCONTENT_SOURCE_DIR_JUCE=... -DFETCHCONTENT_SOURCE_DIR_MYSOFA=... -DFETCHCONTENT_SOURCE_DIR_CATCH2=... -DFETCHCONTENT_FULLY_DISCONNECTED=ON`.

**Missing dependencies with no fallback:** none.
**Missing dependencies with fallback:** none blocking.

## Validation Architecture

`.planning/config.json` contains only `workflow._auto_chain_active`, so `nyquist_validation` is absent and treated as enabled.

### Test Framework
| Property | Value |
|----------|-------|
| Framework | Catch2 v3.7.1 (FetchContent), CTest via `catch_discover_tests` |
| Config file | `tests/CMakeLists.txt` (hand-maintained source lists; add `SpatialCoreUITests` here) |
| Quick run command | `cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build --target SpatialCoreTests SpatialCoreUITests -j8 && ./build/tests/SpatialCoreTests "[osc],[trajectory],[route]" && ./build/tests/SpatialCoreUITests "[ui]"` |
| Full suite command | Debug: `cmake --build build -j8 && ./build/tests/SpatialCoreTests && ./build/tests/SpatialCoreUITests`. Release: `cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release && cmake --build build-release -j8 && ctest --test-dir build-release/tests --output-on-failure` |
| Estimated runtime | OSC/trajectory tags under 10 s; UI tags under 15 s; new target clean build about 35 s |

The Phase 3 command `--target SpatialCoreTests && ctest ...` must change to the full-build form above (Pitfall 7); the plan must update the gate text so D-17 holds. Add a gate assertion that the UI tests actually ran, e.g. `ctest --test-dir build-release/tests -N | grep -c 'ui: '` is greater than zero.

### Phase Requirements to Test Map
| Req ID / Criterion | Behavior | Test Type | Automated Command | File Exists? |
|--------------------|----------|-----------|-------------------|--------------|
| EXTR-04 / C1 | Real UDP `/adm/obj/N/aed` reaches the Listener (message loop pumped) | integration | `./build/tests/SpatialCoreTests "[osc][udp]"` | Wave 0 (extend `ADMOSCReceiverTests.cpp`) |
| EXTR-04 / C1 | Each of `azim elev dist aed xyz` moves the rendered object (L/R ratio, merged NaN axes) | engine integration | `./build/tests/SpatialCoreTests "[route][osc]"` | Wave 0 `Engine/ControlRouteTests.cpp` |
| EXTR-04 / C1 | Wrong-typed, non-finite and out-of-range args are ignored or clamped; queries call `admPositionQueried` | unit | `./build/tests/SpatialCoreTests "[osc][edge],[osc][query]"` | Wave 0 (extend) |
| EXTR-04 / C2 | 30 +- 1 messages/s at 120/60/50/30 Hz fake-clock callers, with and without jitter | unit | `./build/tests/SpatialCoreTests "[osc][send][rate]"` | Wave 0 (extend `ADMOSCSenderTests.cpp`) |
| EXTR-04 / C2 | Zero messages while still; resumes on change; first send at (0,0,0) goes out (D-09) | unit | `"[osc][send][static]"`, `"[osc][send][first]"` | Wave 0 |
| EXTR-04 / D-08 | Send-all on connect; re-enable sends once; query reply format and coalescing | unit | `"[osc][send][connect]"`, `"[osc][send][query]"` | Wave 0 |
| EXTR-04 / C3 | All 13 shapes animate forward and reverse via `tick()` (moves, in range) | unit | `./build/tests/SpatialCoreTests "[trajectory][tick][reverse]"` | Wave 0 (extend `TrajectoryTests.cpp`) |
| EXTR-04 / D-13 | Reverse identities: 10 shapes `rev(p)==fwd(1-p)`, Line `fwd(p+0.5)`, Bounce mirror, Random seeded range | unit | `"[trajectory][reverse]"` | Wave 0 |
| EXTR-04 / C3 | Trajectory (Orbit fwd then rev) moves rendered audio left then right | engine integration | `"[route][trajectory]"` | Wave 0 |
| EXTR-05 / C4 | Drag reports `az 90.0, d 0.8`; elevation preserved; clamp at 1.0; select fires | UI unit | `./build/tests/SpatialCoreUITests "[ui][map][drag]"` | Wave 0 `UI/SpatialMapComponentTests.cpp` |
| EXTR-05 / C4 | Pixel: dot above vs below brightness, dot radius equals `d*0.45*min(w,h)`, ring pixels lit | UI unit | `"[ui][map][pixels]"` | Wave 0 |
| EXTR-05 / D-11 | Map drag to the left makes the left channel louder through `RenderEngine` | UI + engine | `"[ui][route][map]"` | Wave 0 `UI/MapRouteTests.cpp` |
| EXTR-05 / DR-16 | No `Processor` type in UI headers/sources | grep gate | `! grep -rn "AudioProcessor\|[A-Za-z]*Processor *[*&]" src/UI include/SpatialCore/UI` | n/a (currently only comments match `Processor` text; use a type-pattern) |
| DATA-02 / D-05 | Embedded 8 resources equal `fonts/*.ttf` bytes | UI unit | `"[ui][fonts][bytes]"` | Wave 0 `UI/FontProvenanceTests.cpp` |
| DATA-02 / D-05, D-06 | Spy default look-and-feel sees 0 font requests painting the map with and without SML look-and-feel | UI unit | `"[ui][fonts][spy]"` | Wave 0 (red before the D-06 change) |
| DATA-02 / D-05 | No family-name font request in `src/UI` or `include/SpatialCore/UI` | grep gate | command in Code Examples | n/a (passes today) |
| D-06 | Render with SML look-and-feel is byte-identical to render without (and, once, to the pre-change render) | UI unit | `"[ui][fonts][identical]"` | Wave 0 |
| D-15 | `ADMOSCReceiver.h` builds with no `-Wunused-parameter` under `-Wextra` | build | `cmake --build build --target SpatialCore 2>&1 \| grep -c "unused parameter"` returns 0 | n/a |
| D-03 | Before/after-drag screenshots exist and are viewed | manual-by-Claude | `./build/examples/.../SpatialCoreDemo --screenshots /tmp/sc-shots` | Wave N (demo) |

### Sampling Rate
- **Per task commit:** the quick command with the tag of the area touched (about 10 s).
- **Per wave merge:** full Debug suite (both executables).
- **Phase gate:** full Debug and full Release (`ctest`) green, UI tests confirmed present in the `ctest -N` listing, before `/gsd-verify-work`.

### Wave 0 Gaps
- [ ] `tests/UI/UITestMain.cpp`, `UITestSupport.h` and the `SpatialCoreUITests` CMake target (with `TEST_PREFIX "ui: "`).
- [ ] `tests/Support/RouteRenderRig.h` shared fixture.
- [ ] `tests/Engine/ControlRouteTests.cpp` and the `[osc][udp]` test (add `JUCE_MODAL_LOOPS_PERMITTED=1` to `SpatialCoreTests`).
- [ ] Red-first tests: font spy (fails today), sender rate/first-send, reverse sweep.
- [ ] Update the documented gate command (Pitfall 7).
- Framework install: none (Catch2 and JUCE already pinned).

## Security Domain

`security_enforcement` is not set to `false` in `.planning/config.json`, so it is enabled. The attack surface of this phase is the unauthenticated OSC UDP input; the UI and the demo app have no network input.

### Applicable ASVS Categories (L1)

| ASVS Category | Applies | Standard Control |
|---------------|---------|-----------------|
| V2 Authentication | no | ADM-OSC defines no authentication (accepted risk, see below) |
| V3 Session Management | no | connectionless UDP |
| V4 Access Control | no | single trust domain; no per-user resources |
| V5 Input Validation | **yes** | explicit `isFloat32()/isInt32()` checks (never `getInt32()` on an unchecked argument), reject NaN/inf, clamp ranges, bounded azimuth wrap; JUCE already catches malformed packets (`OSCFormatError`, `juce_OSCReceiver.cpp:431-453`) |
| V6 Cryptography | no | none used |
| V7 Error Handling and Logging | yes (light) | never `jassertfalse`/log from attacker-controlled input; the Debug assertion at `juce_OSCArgument.cpp:59` is the symptom to remove |
| V12 Files | demo only | the demo writes PNGs to a user-supplied path; off by default, never shipped to consumers |
| V14 Configuration | yes (light) | document the bind address (below) |

### Known Threat Patterns for ADM-OSC over UDP

| Pattern | STRIDE | Standard Mitigation |
|---------|--------|---------------------|
| Wrong-typed argument (string where a number is expected) flips state or asserts | Tampering, DoS (Debug) | Type-check every argument; ignore on mismatch (Pitfall 5; probe reproduced value 0 plus assertion) |
| NaN used to forge a partial update; huge finite value hangs a consumer's `wrapAzimuth` | Tampering, DoS | Reject non-finite; clamp/wrap at the receiver boundary (Pitfall 6; verified hang for 1e10 and 1e30) |
| Query flood or reflection/amplification | DoS | Reply only to the configured destination, never to the packet source; coalesce one pending reply per (object, kind) and flush on the 30 Hz gate; reply size is about the query size (small) `[ASSUMED]` |
| Unauthenticated control from the LAN | Spoofing, Tampering | `OSCReceiver::connect (int)` binds all interfaces (`bindToPort (portNumber)`, `juce_OSCReceiver.cpp:345`). Accepted: ADM-OSC devices live on the LAN. Optional later hardening: bind through `connectToSocket` with a chosen address |
| Port feedback loop (reply to own receive port) | DoS | `oscPortsConflict` before connecting (Pitfall 8) |
| Oversized or nested bundle | DoS | JUCE reads into a fixed 65535-byte buffer (`juce_OSCReceiver.cpp:466`) and throws on bad sizes; `ADMOSCReceiver` ignores bundles entirely, so no recursion of ours is exposed |
| Object index parse (`/adm/obj/1abc/aed` accepted as object 1) | Tampering (minor) | Bounds check already present: `if (objNum < 1 || objNum > MAX_SOURCES) return;` (`ADMOSCReceiver.cpp:44`) |

## Sources

### Primary (HIGH confidence)
- JUCE 9.0.0 source, read this session: `juce_Font.cpp:85-135,195-235`, `juce_LookAndFeel.cpp:38-56,127-140`, `juce_OSCReceiver.cpp:335-352,415-480`, `juce_OSCReceiver.h:132`, `juce_OSCArgument.cpp:34-95`, `juce_MouseEvent.h:77-89`, `juce_MouseInputSource.h:236-267`, `juce_MessageManager.h:84-105`, `juce_PlatformDefs.h:320-330`, `juce_Initialisation.h:81-92`, `juce_Component.h:1163-1185`
- Repo files read this session: `CMakeLists.txt`, `tests/CMakeLists.txt`, `include/SpatialCore/OSC/ADMOSCSender.h`, `src/OSC/ADMOSCSender.cpp`, `include/SpatialCore/OSC/ADMOSCReceiver.h`, `src/OSC/ADMOSCReceiver.cpp`, `include/SpatialCore/Trajectory/TrajectoryEngine.h`, `src/Trajectory/TrajectoryEngine.cpp`, `src/UI/SpatialMapComponent.cpp`, `include/SpatialCore/UI/SpatialMapComponent.h`, `src/UI/SMLLookAndFeel.cpp`, `include/SpatialCore/UI/SMLLookAndFeel.h`, `src/UI/PresetBrowser.cpp`, `src/UI/GlobalTapDrawer.cpp`, `include/SpatialCore/Engine/RenderEngine.h` (`RenderSources`, `RenderBlockContext`), `include/SpatialCore/IO/OutputFormat.h`, `tests/Trajectory/TrajectoryTests.cpp`, `tests/OSC/*.cpp`, `tests/Engine/RenderEngineTests.cpp`, `.github/workflows/ci.yml`, `.planning/{REQUIREMENTS,STATE,PROJECT,v1.0.0-MILESTONE-AUDIT}.md`, Phase 3 `03-VALIDATION.md`
- Scratch experiments (not in the worktree): `/tmp/sc-p4-probe/src` (copy of the tree plus `tests/UI/Probe*.cpp`, `examples/demo/Main.cpp`), outputs quoted above
- `gh issue view 100 --repo Spatial-Media-Lab/OpenSpatialDelay` (OSD#100 title, state CLOSED, body)

### Secondary (MEDIUM confidence)
- ADM-OSC specification page `https://immersive-audio-live.github.io/ADM-OSC/` `[CITED]`: query by no-argument message, reply "to the sender's IP address, using the return port (default: 4002)", ports 4001/4002, float32 position types and ranges, about 50 Hz typical rate, no heartbeat
- OSD repo (`/Users/andrewrahman/conductor/repos/openspatialdelay-v1`, `PluginProcessor.cpp:4145`) for how the shipping consumer sets `reverse`

### Tertiary (LOW confidence)
- A3, A4, A5 in the Assumptions Log (unverified in the wild or unprobed)

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH, all versions are existing pins read from the build files.
- Architecture: HIGH, the harness, rig, sender clock simulation, demo build and font prototype were all executed.
- Pitfalls: HIGH, each pitfall is reproduced by a probe or read in JUCE 9.0.0 source, except A1 to A5.

**Research date:** 2026-10-05
**Valid until:** about 2026-11-04 (stable; re-check if JUCE is bumped past 9.0.0, which changes font resolution and `MessageManager` details)
