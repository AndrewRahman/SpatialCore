---
phase: 04-control-surface-ui
verified: 2026-10-05T14:10:00Z
status: passed
score: 4/4 roadmap truths verified; both human approvals resolved at the 2026-10-05 review (D-16)
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
covered_digest: "v2:sha256:3f2049421c53d1e6cdf87e5e3910e495cfef700a9fb493a0e7b0dbc9c8b59dad"
behavior_unverified: 0
overrides_applied: 0
re_verification:
  previous_status: human_needed
  previous_score: 4/4 (two human approvals pending)
  gaps_closed: []
  gaps_remaining: []
  regressions: []
gaps: []
deferred: []
advisory: []
behavior_unverified_items: []
coincidental_reliance_items: []
unverified_prohibitions: []
human_verification: []
---

# Phase 4: Control Surface & UI Verification Report

**Phase Goal:** An object's position can be driven by OSC, animated by a trajectory, and dragged on screen, each route reaching the renderer.
**Verified:** 2026-10-05
**Status:** passed
**Re-verification:** Yes. The earlier report (status `human_needed`) went stale when the D-22 revert landed in `52ca7a6` and the review records followed. Evidence for truths 1 to 3 was re-confirmed by a fresh run; the D-22 revert, the font-provenance exemption and the requirement records were re-checked against the code.

All automated evidence passes, no gaps were found, and both D-16 human items are resolved (D-03 approved, D-22 rejected and reverted along the documented path).

## Goal Achievement

The local gate was re-run in this session rather than trusting the SUMMARY or the orchestrator's note. Debug and Release were run one after the other, as they share UDP ports.

### Observable Truths (ROADMAP success criteria)

| # | Truth | Status | Evidence |
|---|-------|--------|----------|
| 1 | An external ADM-OSC sender moves a rendered object via `/adm/obj/N/{azim,elev,dist,aed,xyz}` | VERIFIED | `tests/Engine/ControlRouteTests.cpp`: a plain `juce::OSCSender` over real loopback UDP moves a `RenderEngine` object; rendered channel levels change with azimuth (L/R ratio >= 2 at +90, <= 0.5 at -90). Separate cases cover `/azim`, `/elev`, `/dist`, `/aed` (Binaural and Quad speaker pick) and `/xyz`, each asserting a rendered channel level, not just that the value arrived. `RouteRenderRig.h` sets `engineComputesGains` and `engineDerivesDispatch` (D-18). Passes in the full Debug and Release runs below. None of these files changed since the last verification |
| 2 | SpatialCore broadcasts positions at 30 Hz and stops re-sending while positions are static | VERIFIED | `ADMOSCSender::consumeSendSlot` (`src/OSC/ADMOSCSender.cpp`) is a self-clocked 1/30 s schedule with an injected clock overload, stall resync (no burst) and a dead-band. Tests: 60 Hz caller gives 30 msg/s and none while still; 30 to 120 Hz callers with jitter; first send at exactly (0,0,0) (D-09); connect and re-enable send once (D-08b); stall gives no burst. Part of the full suites that passed |
| 3 | Every trajectory shape animates forward and reverse | VERIFIED | `tests/Trajectory/TrajectoryTests.cpp`: ten shapes retrace the forward path; the 13-shape tick test steps every shape forward and reverse through `tick()`; Random is seeded and stays in range both ways; Orbit through `TrajectoryEngine` moves rendered level L/R 3.311 forward and 0.302 reversed (route test). Bounce and Line are the recorded D-19 exceptions (below), pinned by named tests that pass |
| 4 | A host plugin embeds `SpatialMapComponent`, drags an object, and sees position, distance ring and elevation opacity update, with SML fonts rendering on a machine with no SML font installed | VERIFIED | Drag, pixel and route tests (`ui:` cases, 13 of them) pass: a synthesized drag reports az 90, d 0.8; the dot sits at d x 180 px; rings are brighter than the void; an object at -80 elevation is dimmer than one at +80; a map drag to az +90 gives L/R 2.82 through `RenderEngine`. Fonts: the map owns three embedded JetBrains Mono typefaces and `SMLLookAndFeel` builds seven from `SpatialCoreUIFontData`; spy tests show 0 default-look-and-feel font requests for the map with and without SML. The D-03 demo screenshots were approved by the user at the 2026-10-05 review. The SAVE PRESET overlay title is the one stated exemption to "no system font" (D-22, below) |

**Score:** 4/4 roadmap truths verified, 0 present-but-behavior-unverified.

Behavior-dependent truths (re-enable forces a send, reverse retraces, drag moves sound) each have a passing named test, so none rest on symbol presence alone.

### Requirements Coverage

Every ID in the plan frontmatter (EXTR-04, EXTR-05, DATA-02) is accounted for. REQUIREMENTS.md maps no other ID to Phase 4, so there are no orphans.

| Requirement | Source Plans | Description | Status | Evidence |
|-------------|--------------|-------------|--------|----------|
| EXTR-04 | 04-01, 04-02, 04-03, 04-04, 04-07, 04-08 | ADM-OSC and the trajectory engine drive real object motion | SATISFIED | Truths 1, 2 and 3. REQUIREMENTS.md shows `[x]` and "Complete (Phase 4)". Query replies, receiver hardening and the D-15 warning cleanup (`check_unused_params.py` reports 0) are in place |
| EXTR-05 | 04-05, 04-06, 04-07, 04-08 | UI components render and interact for real; no concrete processor pointer (DR-16) | SATISFIED | Truth 4. `SpatialCoreUITests` (13 tests) is the only target linking `SpatialCoreUI`. D-03 screenshots approved at the review. REQUIREMENTS.md `[x]`, traceability row "Complete (Phase 4)" (confirmed in the working tree) |
| DATA-02 | 04-06, 04-08 | `SMLLookAndFeel` has the font BinaryData it needs | SATISFIED | Evidence table below. REQUIREMENTS.md `[x]`, traceability row "Complete (Phase 4)", with the D-22 rejection and the exemption stated in the entry |

### DATA-02 evidence (D-16), updated for the D-22 revert

| Evidence | Result |
|----------|--------|
| Embedded data | `juce_add_binary_data(SpatialCoreUIFontData ...)` lists 8 TTFs under `fonts/`; linked PUBLIC into `SpatialCoreUI` (CMakeLists.txt) |
| Bytes | `ui:Fonts: every embedded font resource equals its source file` passes (8 resources, bytes equal the files) |
| SMLLookAndFeel | `ui:Fonts: SMLLookAndFeel loads all seven embedded typefaces` passes |
| Map owns its fonts (D-06) | `SpatialMapComponent.cpp` loads JetBrains Mono Regular, Medium and Bold from embedded data; spy shows 0 default-look-and-feel requests |
| Overlay title (D-22) | Not embedded: the user rejected the DM Sans Bold change. The title is back to the BASE `juce::FontOptions (13.0f).withStyle ("Bold")` (`PresetBrowser.cpp:148`), which resolves through the default look-and-feel exactly as before this phase. Pinned as a documented exemption by `ui:Fonts: the save-preset title keeps its original single font request` (1 request) |
| Code rule | `ui:Fonts: no UI code asks for an SML font by family name` passes with live positive controls; the original title line does not match the rule (no string-literal family name, no `withName`) |
| OSD look unchanged | `ui:Fonts: map render is byte-identical with and without the SML look-and-feel` passes |
| Not proven | No run on a machine without the fonts (D-05); the claim rests on provenance (code rule, spy, bytes). The overlay title is outside that claim by user ruling |

### D-22 revert check (requested)

1. **Source is back to BASE.** `git diff 260f0829b0557073898396045fbc95c2da1fe3b0^ -- include/SpatialCore/UI/PresetBrowser.h src/UI/PresetBrowser.cpp` prints nothing and the stat is empty. `git log 260f0829^..HEAD` over those two files shows only the pair `bb40ec8` (DM Sans change) and `52ca7a6` (revert), so the pair cancels exactly.
2. **Matches the documented path.** `04-08-PLAN.md` (truth at line 30 and the rejection section) and `04-08-SUMMARY.md:153` name: remove `PresetSaveOverlay::titleTypeface_`, its constructor load and its paint line so the BASE title font returns; give the overlay case of `[ui][fonts][spy]` a documented exemption asserting the BASE count of 1 request; remove item (4) from the OSD row; record the ruling in STATE.md; re-run the gate; tick DATA-02. The `52ca7a6` diff does exactly the first two (it also drops the now-unused `SpatialCoreUIFontData.h` include and fixes the scan comment from "map 3 and the overlay 1" to "map 3"). PROJECT.md "OSD Phase 4 follow-ups" now has items (1) to (5) with the old item (4) gone and the later items renumbered (confirmed in the working-tree diff). STATE.md line 168 records the ruling. REQUIREMENTS.md ticks EXTR-05 and DATA-02 in both the entry and the traceability table.
3. **The exemption is sound.** The test paints the overlay under a spy default look-and-feel after clearing the typeface cache and checks `calls == 1`. That is the BASE count recorded in the 04-06 SUMMARY. It cannot pass by accident in either direction: re-embedding a typeface would drop it to 0, and a second system font request would raise it to 2. It is a pin of the user's ruling, not a weakening of the map assertion (the map and SML tests still assert 0). Its comment states why it exists and what a change means.
4. **No consumer-visible change.** The overlay renders as it did before the phase, so no OSD release note is needed for it.

### Recorded exceptions and deviations

- **Bounce and Line reverse (D-19), intentional exceptions to D-13.** Bounce reverse negates the azimuth offset from the base and leaves elevation unchanged; Line reverse is a half-period shift. Deliberate OpenSpatialDelay#100 behaviour, pinned by `Trajectory reverse: Bounce reverse mirrors the azimuth offset (OSD#100)` and `Trajectory reverse: Line reverse is a half-period shift (OSD#100)`. No trajectory path changed.
- **D-20 reply-destination deviation.** Position-query replies go to the sender's configured host and port, not the address the query came from (`juce::OSCReceiver` does not expose a packet's source address). Stated in the `ADMOSCSender::queueReply` doc comment, PROJECT.md "OSD Phase 4 follow-ups" item (2), CLAUDE.md and the integration guide.
- **D-22 overlay title exemption.** See the DATA-02 evidence. The SAVE PRESET title is the single UI font that does not come from `SpatialCoreUIFontData`; user ruling at the 2026-10-05 review.

### Required Artifacts

| Artifact | Expected | Status | Details |
|----------|----------|--------|---------|
| `src/OSC/ADMOSCSender.cpp` | Self-clocked 30 Hz sender, forced first send, query replies | VERIFIED | Substantive; exercised by the OSC cases |
| `src/OSC/ADMOSCReceiver.cpp` | Position grammar, query branch, typed and bounded input | VERIFIED | Unchanged since last verification |
| `tests/Support/RouteRenderRig.h` | Renders through `RenderEngine` with both consumer flags | VERIFIED | Unchanged |
| `tests/Engine/ControlRouteTests.cpp` | Sound-moves proof for OSC and trajectory | VERIFIED | Asserts channel-level ratios |
| `tests/UI/*` and `SpatialCoreUITests` target | UI test target in local gate | VERIFIED | 13 tests listed with the `ui:` prefix in the Release ctest listing |
| `src/UI/SpatialMapComponent.cpp` | Embedded-font ownership | VERIFIED | Unchanged |
| `src/UI/PresetBrowser.cpp`, `include/SpatialCore/UI/PresetBrowser.h` | Original overlay (D-22 reverted) | VERIFIED | Byte-identical to BASE |
| `tests/UI/FontProvenanceTests.cpp` | Font provenance tests with the overlay exemption | VERIFIED | Exemption case present, passing |
| `examples/` demo + `SPATIALCORE_BUILD_EXAMPLES` | Off by default | VERIFIED | Unchanged from the prior check |

### Key Link Verification

| From | To | Via | Status | Details |
|------|----|-----|--------|---------|
| Real UDP OSC | Rendered audio | `ADMOSCReceiver` -> consumer glue -> `RouteRenderRig` -> `RenderEngine::renderBlock` | WIRED | Tracer test; the glue stays consumer-side by design (D-12) |
| `TrajectoryEngine::tick` | Rendered audio | Consumer glue -> engine | WIRED | Orbit route test, L/R 3.311 forward and 0.302 reversed |
| Map drag | Rendered audio | `mouseDrag` -> `Listener::objectPositionChanged` -> glue -> engine | WIRED | `Map route:` tracer test |
| `SpatialCoreUIFontData` | Map and `SMLLookAndFeel` typefaces | `createSystemTypefaceFor` on embedded arrays | WIRED | Byte, spy and code-rule tests. The overlay title no longer uses it, by ruling |
| `ADMOSCReceiver::Listener::admPositionQueried` | `ADMOSCSender::queueReply` | Consumer override | WIRED (consumer-side) | Proven over UDP in the query tests; OSD has not yet adopted it (documented follow-up) |

### Data-Flow Trace (Level 4)

| Artifact | Data | Source | Real data | Status |
|----------|------|--------|-----------|--------|
| Map dot and rings | object positions | drag events; pixel-read back in tests | Yes | FLOWING |
| Rendered L/R levels | object position -> engine-computed gains | `RenderEngine` with `engineComputesGains` | Yes (ratios 3.3 / 0.30 / 2.82) | FLOWING |
| Sender messages | consumer arrays -> UDP | `tick` -> `sendPosition` | Yes (captured by a real receiver in tests) | FLOWING |

### Behavioral Spot-Checks

All run in this session, Debug then Release, one at a time.

| Behavior | Command | Result | Status |
|----------|---------|--------|--------|
| Core suite (Debug) | `build/tests/SpatialCoreTests` | All tests passed (323707 assertions in 303 test cases). Ends with the known `Leaked objects ... FFT` line already tracked in STATE.md | PASS |
| UI suite (Debug) | `build/tests/SpatialCoreUITests` | All tests passed (3027 assertions in 13 test cases; 3035 before the revert, the 8 fewer are per-line checks of the rule scan over the lines the revert removed) | PASS |
| Both trees up to date | `cmake --build build`, `cmake --build build-release` | Nothing rebuilt; all targets built | PASS |
| Release gate | `ctest --test-dir build-release/tests` | 100% tests passed out of 316 | PASS |
| UI tests in the gate listing (D-17) | `ctest --test-dir build-release/tests -N \| grep -c 'ui:'` | 13 | PASS |
| Font provenance (Release) | `build-release/tests/SpatialCoreUITests "[fonts]"` | All passed (2980 assertions in 6 test cases), including the exemption case | PASS |
| D-15 unused-parameter warnings | `python3 tests/tools/check_unused_params.py` | 0, exit 0 | PASS |
| Demo self-test | `SpatialCoreDemo --selftest` | Not run: the demo is off by default and not built in these trees. Same behaviours are covered by the suites above | SKIPPED |

### Probe Execution

No probes declared by any plan and none found under `scripts/*/tests/probe-*.sh`. SKIPPED (none to run).

### Anti-Patterns Found

No `TODO`, `FIXME`, `TBD`, `XXX`, `HACK` or `PLACEHOLDER` markers in `src/UI`, `include/SpatialCore/UI` or `tests/UI` (re-grepped; the phase's other sources were clean at the previous verification and are unchanged). The code review (04-REVIEW.md) found 0 critical, 6 warning and 8 info items; none blocks the phase goal. Their disposition is recorded in `04-REVIEW-DISPOSITION.md`. The ones that touch the goal's routes:

| File | Line | Pattern | Severity | Impact |
|------|------|---------|----------|--------|
| `src/OSC/ADMOSCSender.cpp` | 166-177 | WR-01: a NaN position stored as the dead-band reference means that object is never sent again until disabled or reconnected | Warning | Needs a NaN from the consumer. Normal routes are unaffected, but it fails silently on a delivered route and is worth scheduling first |
| `src/OSC/ADMOSCSender.cpp` | 1-2, 42 | WR-02: `std::isfinite` without the fast-math guard header | Warning | Only matters under fast-math |
| `include/SpatialCore/OSC/ADMOSCReceiver.h` | 42-45 | WR-03: the "NaN means axis not sent" rule is not on the Listener doc | Warning | A header-only reader could treat NaN as a position |
| `src/OSC/ADMOSCReceiver.cpp` | 12-20, 154-164 | WR-04: `/xyz` with huge components overflows to elevation 0 | Warning | Hostile or broken input only; second to schedule |
| `src/OSC/ADMOSCReceiver.cpp` / `TrajectoryEngine.h` | 176-201 | WR-05: non-position addresses forwarded unbounded; `wrapAzimuth` loops without bound | Warning | A consumer routing such a value into it can hang its message thread. Position addresses are bounded |
| `tests/OSC/*`, `tests/Engine/ControlRouteTests.cpp`, `examples/demo` | various | WR-06: fixed UDP ports and quiet-period counting | Warning | Possible flakiness under parallel runs; all passed here in Debug and Release |
| various | | IN-01 to IN-08 | Info | See 04-REVIEW.md |

Notes (info only):
- `GlobalTapDrawer.cpp:90` keeps a system-font fallback for a null `dmSansBold`; defensive and outside the criterion-4 scope.
- `.planning/STATE.md:164` (the 04-06 decision line) and `04-VALIDATION.md:60` still describe the overlay as embedded DM Sans / 0 lookups. These are historical planning entries, superseded by the review ruling at STATE.md:168; no code, test or shipped doc carries the stale claim (grepped CLAUDE.md, README.md, docs/, .claude/, src, include, tests, examples).

Judgment-tier prohibitions in the plans (no library connector, no legacy switch, no elevation drag, no concrete processor pointer, no default-on demo, no tick before gate, and similar) were spot-checked at the previous verification and none of the files involved changed since, apart from the overlay revert, which touches none of them. Non-authoritative judge verdict, no violation found.

### Human Verification

None outstanding. Both items were resolved at the 2026-10-05 end-of-phase review (see below).

### Gaps Summary

No gaps. Every roadmap truth is backed by a test that was re-run in this session, with route tests that assert rendered channel levels rather than arrived values. The D-22 revert matches the documented path exactly, the font-provenance exemption is a tight pin of the user's ruling, and the requirement, state and release-note records agree with the code. The six open code-review warnings are hardening and test-reliability items that do not stop an object being moved by OSC, a trajectory or a drag; WR-01 and WR-04 are the two worth scheduling first.

---

_Verified: 2026-10-05_
_Verifier: Claude (gsd-verifier)_

## Human review outcome (2026-10-05)

| Item | Ruling | Effect |
|---|---|---|
| D-03 demo screenshots (EXTR-05) | Approved. The dot moves from the top to az 90 at d 0.8, as `writeDragScreenshots` intends (not the outer ring) | EXTR-05 ticked |
| D-22 SAVE PRESET title (DATA-02) | Rejected: DM Sans Bold looked too small; the user kept the original font | Documented revert applied in `52ca7a6`: `PresetSaveOverlay` is byte-identical to BASE, the `[ui][fonts][spy]` overlay case is a documented exemption pinning its single original request, item (4) removed from the OSD Phase 4 row. DATA-02 ticked on the map, embedded bytes and code rule |

Gate re-run after the revert (by the orchestrator, and independently by this verification): Debug `SpatialCoreTests` 303 cases pass, `SpatialCoreUITests` 13 cases pass; Release `ctest` 316/316 with 13 `ui:` tests listed; `ADMOSCReceiver.h` unused-parameter warnings 0.
