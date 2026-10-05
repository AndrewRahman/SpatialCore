---
phase: 04-control-surface-ui
verified: 2026-10-05T10:40:00Z
status: human_needed
score: 4/4 roadmap truths verified by automated evidence (2 human approvals still pending, D-16)
covered_files:
  - ".planning/phases/04-control-surface-ui/04-01-PLAN.md"
  - ".planning/phases/04-control-surface-ui/04-01-SUMMARY.md"
  - ".planning/phases/04-control-surface-ui/04-02-PLAN.md"
  - ".planning/phases/04-control-surface-ui/04-02-SUMMARY.md"
  - ".planning/phases/04-control-surface-ui/04-03-PLAN.md"
  - ".planning/phases/04-control-surface-ui/04-03-SUMMARY.md"
  - ".planning/phases/04-control-surface-ui/04-04-PLAN.md"
  - ".planning/phases/04-control-surface-ui/04-04-SUMMARY.md"
  - ".planning/phases/04-control-surface-ui/04-05-PLAN.md"
  - ".planning/phases/04-control-surface-ui/04-05-SUMMARY.md"
  - ".planning/phases/04-control-surface-ui/04-06-PLAN.md"
  - ".planning/phases/04-control-surface-ui/04-06-SUMMARY.md"
  - ".planning/phases/04-control-surface-ui/04-07-PLAN.md"
  - ".planning/phases/04-control-surface-ui/04-07-SUMMARY.md"
  - ".planning/phases/04-control-surface-ui/04-08-PLAN.md"
  - ".planning/phases/04-control-surface-ui/04-08-SUMMARY.md"
  - "CMakeLists.txt"
  - "examples/CMakeLists.txt"
  - "examples/demo/DemoComponent.cpp"
  - "examples/demo/DemoComponent.h"
  - "examples/demo/Main.cpp"
  - "include/SpatialCore/OSC/ADMOSCReceiver.h"
  - "include/SpatialCore/OSC/ADMOSCSender.h"
  - "include/SpatialCore/UI/PresetBrowser.h"
  - "include/SpatialCore/UI/SpatialMapComponent.h"
  - "src/Core/FloatSemanticsGuard.h"
  - "src/OSC/ADMOSCReceiver.cpp"
  - "src/OSC/ADMOSCSender.cpp"
  - "src/UI/PresetBrowser.cpp"
  - "src/UI/SpatialMapComponent.cpp"
  - "tests/CMakeLists.txt"
  - "tests/Engine/ControlRouteTests.cpp"
  - "tests/OSC/ADMOSCReceiverTests.cpp"
  - "tests/OSC/ADMOSCSenderTests.cpp"
  - "tests/Support/RouteRenderRig.h"
  - "tests/Trajectory/TrajectoryTests.cpp"
  - "tests/UI/FontProvenanceTests.cpp"
  - "tests/UI/MapRouteTests.cpp"
  - "tests/UI/SpatialMapComponentTests.cpp"
  - "tests/UI/UITestMain.cpp"
  - "tests/UI/UITestSupport.h"
  - "tests/tools/check_unused_params.py"
covered_digest: "v2:sha256:eb8cf9c16262569475a45a393f32e3b72540b66995f4e42f74ad19ba3d370bcc"
behavior_unverified: 0
overrides_applied: 0
re_verification: null
gaps: []
deferred: []
advisory: []
behavior_unverified_items: []
coincidental_reliance_items: []
unverified_prohibitions: []
human_verification:
  - test: "D-03 (EXTR-05): look at the two map pictures, before and after the drag"
    expected: "Open .context/sc-shots/before-drag.png and .context/sc-shots/after-drag.png side by side. In the first, object 1 (the big red dot, labelled +30 degrees) sits at the top of the map, straight ahead (F). In the second, the same dot has moved to the far left (L), about two-thirds of the way out. Objects 2 (orange, right) and 3 (green, lower left) have not moved. The concentric rings and the F/L/R/B letters are unchanged, and the text is readable. If that is what you see, say yes."
    why_human: "Whether the map looks right is a judgement about appearance. The tests prove the dot's position, ring placement and brightness in pixels, but the plan reserved the final look for you (D-03)."
  - test: "D-22 (DATA-02): look at the SAVE PRESET box, before and after"
    expected: "Open build/ui-capture-base/overlay-sml-component.png (before) and build/ui-capture-after/overlay-sml-component.png (after). Both are a small dark box with the title SAVE PRESET at the top. In the after picture the title is drawn in the SpatialCore bundled font (DM Sans Bold), so the letters look slightly rounder and a touch bolder than in the before picture. Nothing else about the box changes. Say yes to keep that change, or no to put the old title font back (the revert steps are in 04-08-SUMMARY.md)."
    why_human: "The title font is the one change you can see on a plugin that already uses SpatialCore. Whether it is acceptable is your call, and the tests only prove which font is requested."
---

# Phase 4: Control Surface & UI Verification Report

**Phase Goal:** An object's position can be driven by OSC, animated by a trajectory, and dragged on screen, each route reaching the renderer.
**Verified:** 2026-10-05
**Status:** human_needed
**Re-verification:** No, initial verification

All automated evidence passes and no gaps were found. The status is `human_needed` rather than `passed` only because D-16 reserves two approvals (the D-03 screenshots for EXTR-05 and the D-22 SAVE PRESET title for DATA-02) for the user.

## Goal Achievement

Verification re-ran the local gate in this session instead of trusting the SUMMARY (see Behavioral Spot-Checks). Debug and Release were run one after the other, as they share UDP ports.

### Observable Truths (ROADMAP success criteria)

| # | Truth | Status | Evidence |
|---|-------|--------|----------|
| 1 | An external ADM-OSC sender moves a rendered object via `/adm/obj/N/{azim,elev,dist,aed,xyz}` | VERIFIED | `tests/Engine/ControlRouteTests.cpp`: a plain `juce::OSCSender` over real loopback UDP (port 9720) moves a `RenderEngine` object; the levels change with azimuth (L/R ratio >= 2 at +90, <= 0.5 at -90). Separate cases cover `/azim`, `/elev`, `/dist`, `/aed` (Binaural and Quad speaker pick) and `/xyz`, each asserting a rendered channel level, not just that the value arrived. `RouteRenderRig.h:107-108` sets `engineComputesGains` and `engineDerivesDispatch` (D-18). Re-ran: `[route]` 7 cases, 44 assertions, all passed |
| 2 | SpatialCore broadcasts positions at 30 Hz and stops re-sending while positions are static | VERIFIED | `ADMOSCSender::consumeSendSlot` (src/OSC/ADMOSCSender.cpp) is a self-clocked 1/30 s schedule with an injected clock overload, stall resync (no burst) and a dead-band (0.1 deg / 0.001 dist). Tests: 60 Hz caller gives 30 msg/s and none while still; 30 to 120 Hz callers with jitter; first send at exactly (0,0,0) (D-09); connect and re-enable send once (D-08b); stall gives no burst. Re-ran `[osc][send],[osc][query],[osc][edge]`: 34 cases, 388 assertions, all passed |
| 3 | Every trajectory shape animates forward and reverse | VERIFIED | `tests/Trajectory/TrajectoryTests.cpp`: ten shapes retrace the forward path (`reverse(p) == forward(1-p)`, error under 1e-4 over three bases); the 13-shape tick test steps every shape forward and reverse through `tick()`, requires movement, range and agreement with `computeTrajectory`; Random is seeded (1234), moves and stays in range both ways. Orbit via `TrajectoryEngine` moves rendered level L/R 3.311 forward and 0.302 reversed (route test). Bounce and Line are the recorded D-19 exceptions (below) |
| 4 | A host plugin embeds `SpatialMapComponent`, drags an object, and sees position, distance ring and elevation opacity update, with SML fonts rendering on a machine with no SML font installed | VERIFIED (automated); look pending human approval | Drag: a synthesized drag reports az 90, d 0.8, keeps elevation, clamps distance at 1, ignores empty space and disabled objects. Pixels: the dot moves with the drag, sits at d x 180 px for d 0.25 to 1.0, rings are brighter than the void, an object at -80 elevation is 0.46x as bright as one at +80. Route: a map drag to az +90 gives L/R 2.82 through `RenderEngine`. Fonts: `SpatialCoreUIFontData` (8 TTFs, CMakeLists.txt:302-316) is linked PUBLIC into `SpatialCoreUI`; the map and the overlay title create typefaces only from that data; spy tests show 0 default-look-and-feel font requests; my own greps found no family-name or `withName` request. See the font caveat under Human Verification. I viewed the two demo screenshots and they match the claim (dot moves from front to left, rings and "+30" label drawn) |

**Score:** 4/4 roadmap truths verified by automated evidence, 0 present-but-behavior-unverified.

Behavior-dependent truths (re-enable forces a send, reverse retraces, drag moves sound) each have a passing named test, so none rest on symbol presence alone.

### Requirements Coverage

Every ID in the plan frontmatter (EXTR-04 in 04-01 to 04-04, 04-07 and 04-08; EXTR-05 in 04-05, 04-06, 04-07 and 04-08; DATA-02 in 04-06 and 04-08) is accounted for. REQUIREMENTS.md maps no other ID to Phase 4, so there are no orphans.

| Requirement | Source Plans | Description | Status | Evidence |
|-------------|--------------|-------------|--------|----------|
| EXTR-04 | 04-01, 04-02, 04-03, 04-04, 04-07, 04-08 | ADM-OSC and the trajectory engine drive real object motion | SATISFIED | Truths 1, 2 and 3. REQUIREMENTS.md already shows `[x]` and "Complete (Phase 4)", which this verification supports. Query replies, receiver hardening and the D-15 warning cleanup (`check_unused_params.py` reports 0) are also in place |
| EXTR-05 | 04-05, 04-06, 04-07, 04-08 | UI components render and interact for real; no concrete processor pointer (DR-16) | SATISFIED by tests; NEEDS HUMAN (D-03) | Truth 4. `SpatialCoreUITests` (13 tests) is the only target linking `SpatialCoreUI`; my grep for a concrete processor in `src/UI` and `include/SpatialCore/UI` found nothing. REQUIREMENTS.md correctly leaves it `[ ]`, "pending human approval" |
| DATA-02 | 04-06, 04-08 | `SMLLookAndFeel` has the font BinaryData it needs | SATISFIED by tests; NEEDS HUMAN (D-22) | Recorded explicitly below as D-16 requires. REQUIREMENTS.md correctly leaves it `[ ]`, "pending human approval" |

### DATA-02 evidence (recorded explicitly, D-16)

| Evidence | Result |
|----------|--------|
| Embedded data | `juce_add_binary_data(SpatialCoreUIFontData ...)` lists 8 TTFs under `fonts/`; `target_link_libraries(SpatialCoreUI PUBLIC SpatialCoreUIFontData)` (CMakeLists.txt:302-316) |
| Bytes | `ui:Fonts: every embedded font resource equals its source file` passes (8 resources, bytes equal the files) |
| SMLLookAndFeel | `ui:Fonts: SMLLookAndFeel loads all seven embedded typefaces` passes; `SMLLookAndFeel.cpp:21-27` builds all seven from `SpatialCoreUIFontData::` data |
| Map owns its fonts (D-06) | `SpatialMapComponent.cpp:41-43` loads JetBrains Mono Regular, Medium and Bold from embedded data; every draw site uses `makeFont (monoRegular_/monoMedium_/monoBold_ ...)` (lines 251, 293, 555, 582, 597) |
| Overlay title (D-22) | `PresetBrowser.cpp:23` loads DM Sans Bold from embedded data and line 151 uses it |
| Spy | `ui:Fonts: the map asks the default look-and-feel for no font, with or without SML` and `... the save-preset title asks for no system font` both pass (0 requests; the SUMMARY records 2 and 1 at the pre-change base) |
| Code rule | `ui:Fonts: no UI code asks for an SML font by family name` passes; my greps for `withName`, `setTypefaceName`, `getDefaultSansSerifFontName` and the strings "DM Sans", "JetBrains Mono" and "Roboto" over `src/UI` and `include/SpatialCore/UI` found no hits; every `createSystemTypefaceFor` call takes `SpatialCoreUIFontData::` data |
| OSD look unchanged | `ui:Fonts: map render is byte-identical with and without the SML look-and-feel` passes |
| Not proven | No run on a machine without the fonts (D-05: this Mac has all three families installed, so a screenshot cannot prove it). The claim rests on provenance (code rule, spy, bytes), as D-05 chose |

### Recorded exceptions and deviations

- **Bounce and Line reverse (D-19), intentional exceptions to D-13.** Bounce (shape 1): reverse negates the azimuth offset from the base and leaves elevation unchanged (differs from the generic flip by up to 180 degrees). Line (shape 8): reverse is a half-period shift (differs by up to 90 degrees). Both are the deliberate OpenSpatialDelay#100 behaviour, pinned by `Trajectory reverse: Bounce reverse mirrors the azimuth offset (OSD#100)` and `Trajectory reverse: Line reverse is a half-period shift (OSD#100)`, which I re-ran and which passed. No trajectory path changed, so OSD's motion is unchanged and no D-14 release note applies to them.
- **D-20 reply-destination deviation.** Position-query replies go to the sender's configured host and port, not the address the query came from. `juce::OSCReceiver` does not expose a packet's source address, and the configured address cannot be used to reflect traffic at a third party. This departs from the ADM-OSC text (reply to the querying device). It is stated in the `ADMOSCSender::queueReply` doc comment (ADMOSCSender.h:53), PROJECT.md "OSD Phase 4 follow-ups" row (item 2), CLAUDE.md, the adm-osc-integration skill and the integration guide. A device that queries from a different address than the plugin's configured send target will not see the reply.

### Required Artifacts

| Artifact | Expected | Status | Details |
|----------|----------|--------|---------|
| `src/OSC/ADMOSCSender.cpp` | Self-clocked 30 Hz sender, forced first send, query replies | VERIFIED | Substantive (schedule, dead-band, `queueReply`, `flushReplies`); exercised by 34 passing OSC cases |
| `src/OSC/ADMOSCReceiver.cpp` | Position grammar, query branch, typed and bounded input | VERIFIED | Query branch, `readFloat`/`readNumeric`, bounded azimuth wrap, elevation/distance/axis clamps |
| `tests/Support/RouteRenderRig.h` | Renders through `RenderEngine` with both consumer flags | VERIFIED | Lines 107-108 |
| `tests/Engine/ControlRouteTests.cpp` | Sound-moves proof for OSC and trajectory | VERIFIED | Asserts channel-level ratios |
| `tests/UI/*` and `SpatialCoreUITests` target | UI test target in local gate | VERIFIED | Only target linking `SpatialCoreUI`; 13 tests listed with `ui:` prefix in the ctest listing |
| `src/UI/SpatialMapComponent.cpp`, `src/UI/PresetBrowser.cpp` | Embedded-font ownership | VERIFIED | See DATA-02 evidence |
| `examples/` demo + `SPATIALCORE_BUILD_EXAMPLES` | Off by default | VERIFIED | Option defaults OFF (CMakeLists.txt:355); `cmake --build build --target help` lists no `SpatialCoreDemo` |
| `.context/sc-shots/{before,after}-drag.png` | D-03 screenshots | VERIFIED (exist, viewed) | Gitignored; approval pending |
| `build/ui-capture-{base,after}/overlay-sml-component.png` | D-22 captures | VERIFIED (exist, viewed) | Gitignored; approval pending |

### Key Link Verification

| From | To | Via | Status | Details |
|------|----|-----|--------|---------|
| Real UDP OSC | Rendered audio | `ADMOSCReceiver` -> consumer glue -> `RouteRenderRig` -> `RenderEngine::renderBlock` | WIRED | Tracer test; the glue stays consumer-side by design (D-12). `grep` finds no `RenderEngine` reference in `src/OSC`, `src/UI`, `src/Trajectory` or their headers (prohibition honoured) |
| `TrajectoryEngine::tick` | Rendered audio | Consumer glue -> engine | WIRED | Orbit route test, L/R 3.311 forward and 0.302 reversed |
| Map drag | Rendered audio | `mouseDrag` -> `Listener::objectPositionChanged` -> glue -> engine | WIRED | `Map route:` tracer test, L/R 2.82 at +90 and 0.35 at -90 |
| `SpatialCoreUIFontData` | Map, overlay and `SMLLookAndFeel` typefaces | `createSystemTypefaceFor` on embedded arrays | WIRED | Greps and byte/spy tests |
| `ADMOSCReceiver::Listener::admPositionQueried` | `ADMOSCSender::queueReply` | Consumer override | WIRED (consumer-side) | Proven end to end over UDP in the query tests; OSD has not yet adopted it (documented follow-up) |

### Data-Flow Trace (Level 4)

| Artifact | Data | Source | Real data | Status |
|----------|------|--------|-----------|--------|
| Map dot and rings | object positions | `setObjects`-style state driven by real drag events; pixel-read back in tests | Yes | FLOWING |
| Rendered L/R levels | object position -> engine-computed gains | `RenderEngine` with `engineComputesGains` | Yes (non-trivial ratios 3.3 / 0.30) | FLOWING |
| Sender messages | consumer arrays -> UDP | `tick` -> `sendPosition` | Yes (captured by a real receiver in tests) | FLOWING |

### Behavioral Spot-Checks

| Behavior | Command | Result | Status |
|----------|---------|--------|--------|
| Core suite (Debug) | `build/tests/SpatialCoreTests` | All tests passed (323707 assertions in 303 test cases); exit 0. Ends with the known `Leaked objects ... FFT` line already tracked in STATE.md | PASS |
| UI suite (Debug) | `build/tests/SpatialCoreUITests` | All tests passed (3035 assertions in 13 test cases) | PASS |
| Both trees up to date | `cmake --build build` and `build-release` | Nothing rebuilt; all targets built | PASS |
| Release gate | `ctest --test-dir build-release/tests` | 100% tests passed out of 316 | PASS |
| UI tests in the gate listing (D-17) | `ctest --test-dir build-release/tests -N \| grep -c 'ui:'` | 13 | PASS |
| Route tests | `SpatialCoreTests '[route]'` | 7 cases, 44 assertions passed | PASS |
| Sender/query/edge tests | `SpatialCoreTests '[osc][edge],[osc][query],[osc][send]'` | 34 cases, 388 assertions passed | PASS |
| D-15 unused-parameter warnings | `python3 tests/tools/check_unused_params.py` | 0, exit 0 | PASS |
| Demo self-test | `SpatialCoreDemo --selftest` | Not run: the demo is not built in these trees (off by default). The SUMMARY records PASS; the same behaviours are covered by the test suites above | SKIPPED |

### Probe Execution

No probes declared by any plan and none found under `scripts/*/tests/probe-*.sh`. SKIPPED (none to run).

### Anti-Patterns Found

No `TODO`, `FIXME`, `TBD`, `XXX`, `HACK` or `PLACEHOLDER` markers in the phase's source, tests or examples. The code review (04-REVIEW.md) found 0 critical, 6 warning and 8 info items, all with disposition `open`. None blocks the phase goal; the ones that touch the goal's routes are listed so the user can decide whether to schedule them.

| File | Line | Pattern | Severity | Impact |
|------|------|---------|----------|--------|
| `src/OSC/ADMOSCSender.cpp` | 166-177 | WR-01: a NaN position stored as the dead-band reference means that object is never sent again until disabled or reconnected | Warning | Needs a NaN from the consumer (a trajectory or glue glitch). Normal routes are unaffected, but it is a silent failure of "positions broadcast" and worth a fix and a test |
| `src/OSC/ADMOSCSender.cpp` | 1-2, 42 | WR-02: uses `std::isfinite` without the fast-math guard header | Warning | Only matters if a consumer builds with fast-math; the input check would then silently vanish |
| `include/SpatialCore/OSC/ADMOSCReceiver.h` | 42-45 | WR-03: the "NaN means axis not sent" rule is cited by the demo and test rig as documented on the Listener, but only the integration guide and a .cpp comment carry it | Warning | A consumer reading only the header could treat NaN as a position |
| `src/OSC/ADMOSCReceiver.cpp` | 12-20, 154-164 | WR-04: `/xyz` with huge components (above about 1.8e19) overflows and returns elevation 0 instead of the right angle | Warning | Hostile or broken input only; the in-range `/xyz` route passes. The wrong direction is finite and in range, so it is silent |
| `src/OSC/ADMOSCReceiver.cpp` / `TrajectoryEngine.h` | 176-201 | WR-05: `/osd/global/*`, `/doppler`, `/pitch`, `/speed` are forwarded unbounded and `wrapAzimuth` still loops without bound | Warning | A consumer that routes an unbounded value into `wrapAzimuth` can hang its message thread. Position addresses, the ones this phase's goal covers, are bounded |
| `tests/OSC/*`, `tests/Engine/ControlRouteTests.cpp`, `examples/demo` | various | WR-06: fixed UDP ports 9700-9791 and "quiet period" counting | Warning | Possible flaky failures under parallel runs or a loaded machine. All passed here in both Debug and Release. This matters because the local suites are the only gate |
| various | | IN-01 to IN-08 | Info | Coupling, a missing `static_assert` on the query table, lax object-number parsing (`/adm/obj/2x/azim` is read as object 2), duplicated test helpers, per-instance font loading with no null report, demo threading notes, a font-rule that scans comments, magic numbers |

Not in the review, noted here: `GlobalTapDrawer.cpp:90` keeps a system-font fallback (`FontOptions (12.5f).withStyle ("Bold")`) for a null `dmSansBold`. The drawer holds an `SMLLookAndFeel` reference whose constructor loads that typeface, so the branch is defensive and the font-provenance rule does not cover it. It is outside the criterion-4 map and overlay scope (D-22). Info only.

Judgment-tier prohibitions in the plans (no library connector, no legacy switch, no elevation drag, no concrete processor pointer, no default-on demo, no tick before gate, and similar): I spot-checked these against the code (no `RenderEngine` reference in the OSC, UI or Trajectory sources; `SpatialCoreTests` does not link `SpatialCoreUI`; demo option defaults OFF; REQUIREMENTS.md ticks only EXTR-04) and found no violation. This is a non-authoritative judge verdict, not a hard gate. The per-plan prohibitions are `resolved` in the plans, and none is left unflagged as a silent pass for human review beyond the two approvals below.

### Human Verification Required

Two items, both plain-language looks at a picture. Nothing needs to be run.

#### 1. The map before and after a drag (D-03, EXTR-05)

**Test:** Open `.context/sc-shots/before-drag.png` and `.context/sc-shots/after-drag.png`.
**Expected:** The big red dot (object 1, labelled +30) is at the top of the map in the first picture and has moved to the far left in the second. The other two dots stay put, the rings and F/L/R/B letters are unchanged and readable.
**Why human:** Tests prove the positions, rings and brightness in pixels. Whether the picture looks right is your call.

#### 2. The SAVE PRESET title (D-22, DATA-02)

**Test:** Open `build/ui-capture-base/overlay-sml-component.png` (before) and `build/ui-capture-after/overlay-sml-component.png` (after).
**Expected:** The same dark box in both. The title SAVE PRESET looks a little rounder and bolder in the after picture because it now uses the bundled SpatialCore font. Say yes to keep it, or no to restore the previous title font.
**Why human:** It is the one visible change for plugins that already use SpatialCore, so it needs your decision.

After you approve: tick EXTR-05 and DATA-02 in `.planning/REQUIREMENTS.md` and replace their pending notes with "Complete (Phase 4)" (also the traceability rows). If either is rejected, the rejection paths in `04-08-SUMMARY.md` apply (a rejected D-03 becomes a gap for `/gsd-plan-phase 4 --gaps`; a rejected D-22 reverts the overlay title, and DATA-02 is then still ticked because its proof rests on the map, the embedded bytes and the code rule).

### Gaps Summary

No gaps. Every roadmap truth is backed by a test I re-ran, with route tests that assert rendered channel levels rather than arrived values. The phase is waiting only on the two D-16 human approvals. The six open code-review warnings are hardening and test-reliability items, none of which stops an object being moved by OSC, a trajectory or a drag; WR-01 and WR-04 are the two worth scheduling first because they fail silently on a route this phase delivered.

---

_Verified: 2026-10-05_
_Verifier: Claude (gsd-verifier)_
