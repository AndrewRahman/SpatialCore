# Phase 4: Control Surface & UI - Context

**Gathered:** 2026-10-05
**Status:** Ready for planning

<domain>
## Phase Boundary

An object's position can be driven three ways — an ADM-OSC message, a trajectory animation, and a
drag on `SpatialMapComponent` — and each route is proven to move the **rendered audio**, not just a
number. SpatialCore broadcasts positions at 30 Hz and goes quiet while objects are still. Every
trajectory shape animates forward and reverse. SML fonts render from the embedded copy on a machine
that has none installed. Requirements: EXTR-04, EXTR-05, DATA-02 (ROADMAP Phase 4 criteria 1-4).

Not in scope: new control capabilities (elevation drag, a shared OSC/trajectory/map→engine
connector, full ADM-OSC query support beyond positions). See Deferred.

</domain>

<decisions>
## Implementation Decisions

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

### Claude's Discretion
- UI test target name and structure (e.g. `SpatialCoreUITests` linking `SpatialCoreUI`), and how to run
  JUCE components headless (`ScopedJuceInitialiser_GUI`, simulated `MouseEvent`s or
  `MouseInputSource`, rendering to a `juce::Image`).
- Demo app name, CMake option name (default OFF), and how Claude drives it for screenshots.
- The sender's clock source and how a test injects a fake one.
- How the query reply gets the current position (D-08), and the return-port configuration.
- Exact level-change threshold for "the sound moved" tests, and which output format(s) they use (stereo
  is simplest; at least one).

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Phase scope and requirements
- `.planning/ROADMAP.md` §"Phase 4: Control Surface & UI" — goal, 4 success criteria, starting position
- `.planning/REQUIREMENTS.md` — EXTR-04, EXTR-05, DATA-02 acceptance; DR-16 (abstract Listener, no processor pointer)
- `.planning/PROJECT.md` — External Dependencies (OSD release-note follow-ups), Key Decisions table
- `.planning/phases/03-binaural-defects-hrtf-packaging/03-CONTEXT.md` — D-02 "one behaviour, no legacy flag" precedent

### ADM-OSC standard (external)
- https://immersive-audio-live.github.io/ADM-OSC/ — query = message with no arguments; receiver replies to sender IP on return port (default 4002); listen 4001 by default; typical position rate about 50 Hz
- https://github.com/immersive-audio-live/ADM-OSC — spec repo, quick reference and JSON schema

### Code
- `include/SpatialCore/OSC/ADMOSCSender.h`, `src/OSC/ADMOSCSender.cpp` — 30 Hz gate, dead-band, zero-init prev arrays
- `include/SpatialCore/OSC/ADMOSCReceiver.h`, `src/OSC/ADMOSCReceiver.cpp` — address grammar, NaN single-axis convention, Listener
- `include/SpatialCore/Trajectory/TrajectoryEngine.h`, `src/Trajectory/TrajectoryEngine.cpp` — 13 shapes (cases 1-13), `reverse` handling at `:65, :167-186, :326-327`
- `include/SpatialCore/UI/SpatialMapComponent.h`, `src/UI/SpatialMapComponent.cpp` — Listener, `mouseDrag` at `:616`, font fallbacks
- `include/SpatialCore/UI/SMLLookAndFeel.h`, `src/UI/SMLLookAndFeel.cpp` — embedded typefaces, `getTypefaceForFont`
- `CMakeLists.txt:258-348` — `SpatialCoreUI` target and `SpatialCoreUIFontData`; `:351` test option
- `include/SpatialCore/Engine/RenderEngine.h` — the consumer render facade the "sound moves" tests drive

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- `tests/OSC/ADMOSCReceiverTests.cpp` and `ADMOSCSenderTests.cpp` — existing harness for feeding and capturing OSC messages. Extend these for D-08 to D-10.
- `tests/Trajectory/TrajectoryTests.cpp` — characterization pattern per shape (`computeTrajectory` is static and pure). Reverse tests (D-13) slot in beside it.
- `tests/Engine/RenderEngineTests.cpp` — existing `RenderEngine` render setup to reuse for D-11.
- `SpatialMapComponent` already has screenshot hooks (`setLabelAllEnabledObjectsForScreenshot`, `setDrawFullTrajectoryForScreenshot`).

### Established Patterns
- Receiver forwards single-axis updates with NaN for the other axes; the consumer merges them.
- UI is a separate `SpatialCoreUI` target; `SpatialCoreTests` does not link it, so UI tests need their own target.
- Defect fixes that change sound ship with an OSD release note, no legacy flag.

### Integration Points
- `TrajectoryEngine.cpp:112` sets `ts.reverse = false` with "caller must check param separately". The map's trajectory preview may not reflect reverse. The researcher should check this when verifying D-13.
- `SpatialMapComponent::setTrajectoryEngine` takes a `const TrajectoryEngine*` (map reads final positions). That read crosses threads, which is RTSF-02 and belongs to Phase 5, not here.

</code_context>

<specifics>
## Specific Ideas

- The user wants to follow "whatever the standard is" for OSC connections: ADM-OSC query/response, not a custom heartbeat.
- The user's only manual involvement is one plain-language screenshot approval (D-03).

</specifics>

<deferred>
## Deferred Ideas

- Elevation drag on the spatial map (D-04): a new capability, later phase.
- A shared library connector from OSC / trajectory / map to `RenderEngine` (D-12): later, possibly when OpenSpatialPanner shows the glue repeating.
- Full ADM-OSC query support for non-position messages (gain, name, etc.): later.
- Raising the broadcast rate toward ADM-OSC's typical ~50 Hz: not v1 (roadmap and OSD use 30 Hz).

</deferred>

---

*Phase: 04-control-surface-ui*
*Context gathered: 2026-10-05*
