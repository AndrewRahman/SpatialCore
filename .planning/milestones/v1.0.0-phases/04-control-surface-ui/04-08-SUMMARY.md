---
phase: 04-control-surface-ui
plan: 08
subsystem: docs-gate
tags: [local-gate, readme, release-notes, requirements, verification-evidence, adm-osc, ui-tests]

requires:
  - phase: 04-control-surface-ui
    provides: "04-02 sender schedule, 04-03 query replies and receiver hardening, 04-04 trajectory route and reverse pins, 04-05 SpatialCoreUITests, 04-06 embedded font provenance, 04-07 demo app"
provides:
  - "README 'Local test gate': Debug build plus both executables, Release build plus ctest, the ui: listing check, and the statement that CI runs zero tests until Phase 6"
  - "README 'Demo app': SPATIALCORE_BUILD_EXAMPLES, SpatialCoreDemo, --screenshots and --selftest"
  - "PROJECT.md External Dependencies row 'OSD Phase 4 follow-ups' (the OSD release-note list, D-14, D-20, D-21, D-22)"
  - "CLAUDE.md, adm-osc-integration skill and integration guide describing the shipped OSC, font, test and demo behaviour"
  - "EXTR-04 ticked and Complete; EXTR-05 and DATA-02 marked pending human approval, with their evidence recorded below"
affects: [phase-04-verification, osd-migration, phase-06-ci]

plan_head_before: c4ee8a6914b52b9658c0ff68d66175ba32264ee3
plan_head_after: c89aeb6de1a3eaad0861e647931118d03195a1bb

actuals:
  tokens: 1955
  tasks: 2
  commits: 2

tech-stack:
  added: []
  patterns:
    - "Gate before tick: a requirement box is ticked only after the full Debug and Release gate passes and the evidence section is on disk (D-16, D-17)"

key-files:
  created: []
  modified:
    - README.md
    - CLAUDE.md
    - .claude/skills/adm-osc-integration/adm-osc-integration.md
    - docs/integration-guide.md
    - .planning/PROJECT.md
    - .planning/REQUIREMENTS.md
    - .planning/STATE.md
    - .planning/ROADMAP.md

key-decisions:
  - "The gate listing check greps 'ui:' not 'ui: ' because CTest drops the space after the prefix (Plan 04-05 deviation); the README and this plan's verify use 'ui:'"
  - "EXTR-04 is ticked on all-automated evidence; EXTR-05 (D-03 screenshots) and DATA-02 (D-22 title) stay unticked and marked pending human approval until the Phase 4 end-of-phase review"
  - "requirements.mark-complete is passed EXTR-04 only"

requirements-completed: [EXTR-04, EXTR-05, DATA-02]

coverage:
  - id: D1
    description: "The documented local gate (both Debug executables, Release ctest, ui: listing) passes exactly as the README writes it"
    requirement: "EXTR-04"
    verification:
      - kind: other
        ref: "README gate commands; SpatialCoreTests 303/303, SpatialCoreUITests 13/13, Release ctest 316/316, 13 ui: tests listed"
        status: pass
    human_judgment: false
  - id: D2
    description: "The Phase 4 code rules pass in the same run: D-15 warnings 0, D-05 font greps clean, DR-16 grep clean, no demo target in the default build"
    requirement: "DATA-02"
    verification:
      - kind: other
        ref: "python3 tests/tools/check_unused_params.py; the four greps in the Task 1 verify command"
        status: pass
    human_judgment: false
  - id: D3
    description: "PROJECT.md has the OSD Phase 4 follow-ups row; CLAUDE.md, the skill and the integration guide match the shipped behaviour"
    verification: []
    human_judgment: true
    rationale: "Prose accuracy against the 04-02 to 04-07 SUMMARYs is checked by reading; the plan's grep checks only prove the named strings exist"
  - id: D4
    description: "EXTR-05 (D-03 screenshots) and DATA-02 (D-22 title) wait for the user's end-of-phase approval"
    requirement: "EXTR-05"
    verification: []
    human_judgment: true
    rationale: "D-03 and D-22 are human looks at an image; automated tests cover the same facts but the user rules on appearance"

duration: 16 min
completed: 2026-10-05
status: complete
---

# Phase 4 Plan 08: Phase 4 Gate, Docs and Requirement Records Summary

**The local gate (Debug SpatialCoreTests 303 and SpatialCoreUITests 13, Release ctest 316 with 13 `ui:` tests) is documented in the README and passes as written; the OSD Phase 4 release-note row, the shipped-behaviour docs and the requirement records are in, with EXTR-04 ticked and EXTR-05 and DATA-02 held for the user's two end-of-phase approvals**

## Performance

- **Duration:** 16 min
- **Started:** 2026-10-05T09:44:16Z
- **Completed:** 2026-10-05T09:59:58Z
- **Tasks:** 2
- **Files modified:** 7 documentation and planning files (plus this SUMMARY and the workflow-tool updates to STATE.md and ROADMAP.md)
- **BASE:** `c4ee8a6914b52b9658c0ff68d66175ba32264ee3` (HEAD when this plan started)

## Accomplishments

- The README no longer points at the wrong binary path (`./build/SpatialCoreTests`). "Local test gate" lists the Debug and Release commands, the `ui:` listing check, why every target is built before `ctest`, and that CI runs zero tests until Phase 6 (D-17). "Demo app" documents the off-by-default build and the two headless modes.
- The gate was run twice: once while writing and once as the authoritative single command at the end, both with exit 0. No tracked source file changed between the runs (only docs).
- `.planning/PROJECT.md` has the "OSD Phase 4 follow-ups" row (list below).
- `CLAUDE.md`, the adm-osc-integration skill and `docs/integration-guide.md` describe the self-clocked sender, query replies, receiver hardening, map-owned fonts, the two test executables, the demo and the local gate.
- STATE.md's deferred row for the `ADMOSCReceiver.h` unused-parameter warnings is Resolved (Phase 4, 04-03).
- Requirement records follow D-16 (see the VERIFICATION evidence section).

## Gate output (Task 1)

Command: the plan's verify command with one change (`grep -c 'ui:'`, see Deviations). Exit 0.

| Step | Result |
|---|---|
| `cmake --build build -j8` (Debug, every target) | exit 0, 0 `warning:` lines in the log (incremental, trees were already built) |
| `./build/tests/SpatialCoreTests` | `All tests passed (323707 assertions in 303 test cases)` |
| `./build/tests/SpatialCoreUITests` | `All tests passed (3035 assertions in 13 test cases)` |
| `cmake --build build-release -j8` (Release, every target) | exit 0, 0 `warning:` lines in the log (incremental, trees were already built) |
| `ctest --test-dir build-release/tests --output-on-failure` | `100% tests passed, 0 tests failed out of 316` (303 core + 13 `ui:`) |
| `ctest --test-dir build-release/tests -N \| grep -c 'ui:'` | 13 |
| `python3 tests/tools/check_unused_params.py` | `ADMOSCReceiver.h unused-parameter warnings: 0` |
| D-05 font-name and `withName` greps over `src/UI` and `include/SpatialCore/UI` | no hits |
| `createSystemTypefaceFor` not taking `SpatialCoreUIFontData::` data | 0 |
| DR-16 concrete-processor grep over the UI sources | no hits |
| `cmake --build build --target help \| grep -c SpatialCoreDemo` | 0 |
| README has `SpatialCoreUITests` (2) and `ctest --test-dir build-release/tests` | yes; `grep -c "./build/SpatialCoreTests" README.md` is 0 |

Debug and Release were run one after the other, never together (they share UDP ports). The Debug `SpatialCoreTests` run still ends with the known `Leaked objects detected: 3 instance(s) of class FFT` line (SharedFFTCache, already a STATE.md deferred item); the exit code is 0.

## OSD release-note list (the "OSD Phase 4 follow-ups" row, D-14)

1. Wire-visible sender changes: `ADMOSCSender` keeps its own 30 Hz clock, so OSD's 60 Hz timer gives the same rate as before (D-07); the first position of every object is always sent, even at zero (D-09); every enabled object is sent once on connect and on re-enable (D-08b).
2. SpatialCore can answer ADM-OSC position queries (a message with no arguments). The reply goes to the plugin's configured send destination, not to the address of the querying device, a stated deviation from the ADM-OSC text (D-08a, D-20). To answer, OSD overrides `ADMOSCReceiver::Listener::admPositionQueried` and calls `ADMOSCSender::queueReply`. Until it does, queries are ignored and OSD compiles unchanged.
3. The receiver ignores wrong-typed and non-finite values, clamps elevation, distance and partial cartesian axes, and wraps an out-of-range azimuth (a sender writing 190 now gets -170 forwarded). Compliant senders are unaffected (D-21).
4. The SAVE PRESET overlay title renders in DM Sans Bold under OSD's per-component look-and-feel, where OSD's own copy rendered the system font (D-22). Before and after captures: `build/ui-capture-base/overlay-sml-component.png` and `build/ui-capture-after/overlay-sml-component.png` (gitignored). Subject to the user's review; removed from the row if rejected.
5. Map visuals and trajectory motion are unchanged: the map render is byte-identical under the SML look-and-feel (D-06), and no trajectory shape changed (Bounce and Line keep their reverse, D-19).
6. At migration delete OSD's own OSC send gate and its map font fallback.

## VERIFICATION evidence

This section is written for the Phase 4 verifier to lift. It was written before any REQUIREMENTS.md change (D-16).

### DATA-02: `SMLLookAndFeel` has the font BinaryData it needs (status: verified by tests, pending the D-22 human review)

| Evidence | Result |
|---|---|
| `[ui][fonts][bytes]` (`ui:Fonts: every embedded font resource equals its source file`) | `SpatialCoreUIFontData::namedResourceListSize == 8`; every resource's bytes equal `fonts/<original filename>`. Passes in Debug and Release |
| `[ui][fonts][spy]` (`ui:Fonts: the map asks the default look-and-feel for no font, with or without SML` and `ui:Fonts: the save-preset title asks for no system font`) | A spy as default look-and-feel sees 0 typeface requests from the map (with and without SML) and from the overlay title. At BASE of 04-06 the counts were 2 (map without SML) and 1 (overlay title) |
| `[ui][fonts][rule]` (`ui:Fonts: no UI code asks for an SML font by family name`) | No UI source line requests a font by family name; every `createSystemTypefaceFor` takes `SpatialCoreUIFontData::` data; positive-control strings prove the regex is live |
| `[ui][fonts][identical]` (`ui:Fonts: map render is byte-identical with and without the SML look-and-feel`) | The map render is pixel-identical with and without the SML look-and-feel (D-06) |
| `[ui][fonts][sml]` (`ui:Fonts: SMLLookAndFeel loads all seven embedded typefaces`) | All seven `SMLLookAndFeel` typeface members load |
| D-05 grep gate (this run) | No family-name request, no `withName`, no `setTypefaceName`, no `getDefaultSansSerifFontName` in `src/UI` or `include/SpatialCore/UI`; 0 non-embedded `createSystemTypefaceFor` |

No run on a machine without the fonts was made (D-05): this Mac has DM Sans, JetBrains Mono and Roboto installed, so a screenshot cannot prove DATA-02. The claim rests on provenance (the four tags and the grep gate), not on a clean-machine capture.

Waiting on: the D-22 review of the SAVE PRESET title (the one visible change). If the user rejects it, the documented revert in the plan applies (remove `PresetSaveOverlay::titleTypeface_`, its constructor load and its paint line so the BASE title font at `PresetBrowser.cpp:148` returns; the overlay case of `[ui][fonts][spy]` gets a documented exemption asserting the BASE count of 1 request; item (4) leaves the OSD row; record the ruling in STATE.md; re-run the full gate; then tick DATA-02, because its proof rests on the map, the embedded bytes and the code rule).

### EXTR-04: ADM-OSC and the trajectory engine drive real object motion (status: Complete, all-automated proof)

| Evidence | Result |
|---|---|
| Route tags `[route][osc]` (6 cases) and `[route][trajectory][tracer]` (1 case), 7 `[route]` cases | An external `juce::OSCSender` over loopback UDP moves a `RenderEngine` object: L/R 3.311 at azimuth +90, 0.302 at -90; `/azim`, `/elev`, `/dist`, `/aed` (Binaural and Quad) and `/xyz` each change a rendered channel level. Orbit through `TrajectoryEngine` gives L/R 3.311 forward and 0.302 reversed (04-01, 04-04) |
| Sender rate tags `[osc][send][rate]` (13 `[osc][send]` cases) | Injected clock, 10 simulated seconds: 300 messages at 120, 60 and 50 Hz callers, 299 at 30 Hz with jitter, 30 in the second after a 1 s stall, silent while still (04-02) |
| Query and edge tags `[osc][query]` (9 cases) and `[osc][edge]` (15 cases) | A no-argument `/adm/obj/4/xyz` over UDP is answered at the configured destination with `-0.5 0 0`; 1000 queries produce one reply; wrong-typed and non-finite input is ignored; a 1e10 azimuth wraps once instead of hanging; compliant input is bit-identical (04-03) |
| Reverse sweep `[trajectory][reverse]` (5 cases) | Ten shapes (2-7, 9, 11-13) have maximum identity error 0.0 for reverse against forward at 1-p; all 13 shapes run forward and reverse through `tick()`; Random seeded with 1234 stays in range both ways (04-04) |
| Intentional exceptions to D-13 (D-19) | **Bounce (shape 1):** reverse negates the azimuth offset from the base, elevation unchanged (max difference 180 degrees from the generic flip). **Line (shape 8):** reverse is a half-period shift (max difference 90 degrees). Both are the deliberate OSD#100 behaviour, pinned by `Bounce reverse mirrors the azimuth offset` and `Line reverse is a half-period shift`, and no trajectory path changed |
| D-15 | `ADMOSCReceiver.h` unused-parameter warnings 5 at the 04-03 BASE, 0 now (`tests/tools/check_unused_params.py`, exit 0 in this run) |
| Demo self-test | `SpatialCoreDemo --selftest`: osc, query, trajectory, map PASS (04-07) |

### EXTR-05: UI components render and interact for real (status: verified by tests, pending the D-03 human approval)

| Evidence | Result |
|---|---|
| UI target | `SpatialCoreUITests`, the only target linking `SpatialCoreUI`: 13 tests, `ui:` prefix in the ctest listing (13 listed in Release) |
| Drag tags `[ui][map]` (7 cases) | A synthesized drag reports az 90.0, d 0.8, keeps elevation, clamps distance to 1.0 outside the outer ring, ignores empty space and disabled objects |
| Pixel tags | The dot moves with the drag (`ffed5e5e` at the new position, background at the old); an object at -80 elevation renders at 0.46x the brightness of one at +80; the dot sits at d * 180 px for d 0.25, 0.5, 0.75, 1.0; the distance rings are brighter than the void by at least 15 |
| Map route `[ui][route][map][tracer]` | A drag to az +90 gives L/R 2.8205 through `RenderEngine`, to az -90 gives 0.3545 |
| Demo screenshots | `.context/sc-shots/before-drag.png` and `.context/sc-shots/after-drag.png` (480x480, written by `SpatialCoreDemo --screenshots`); report: object 1 az 0 to 90, L/R 1.00 to 2.82 (04-07) |

Waiting on: the D-03 approval of the two demo screenshots, one plain-language yes from the user at the end-of-phase review. If the user rejects them, EXTR-05 stays unticked with its pending note and the objection becomes a gap for `/gsd-plan-phase 4 --gaps`.

### D-20 deviation (record for VERIFICATION)

Query replies go to the sender's configured host and port, not to the address the query came from, because `juce::OSCReceiver` does not expose a packet's source address. The ADM-OSC text says to reply to the querying device. The choice also means a spoofed query cannot reflect traffic at a third party. Stated in the `ADMOSCSender::queueReply` doc comment, the OSD row, CLAUDE.md, the skill and the integration guide.

### Which boxes wait on which approval

| Requirement | State after this plan | Waits on |
|---|---|---|
| EXTR-04 | `[x]`, traceability "Complete (Phase 4)" | nothing |
| EXTR-05 | `[ ]`, "pending human approval" | D-03: the user approves the two screenshots |
| DATA-02 | `[ ]`, "pending human approval" | D-22: the user approves the SAVE PRESET title change |

The end-of-phase review has exactly these two human items.

## Task Commits

1. **Task 1: Tracer, the documented local gate runs both executables in Debug and Release and checks every Phase 4 code rule** - `faa13a4` (docs)
2. **Task 2: OSD release-note row, docs that match the shipped behaviour, requirement ticks and the VERIFICATION evidence** - `c89aeb6` (docs)

**Plan metadata:** the docs commit that follows (SUMMARY, STATE.md, ROADMAP.md, REQUIREMENTS.md as updated by the workflow tools).

Tracer gate: Task 1's verify is the tracer's end-to-end check; it passed before Task 2 began. It has no human-check (D-03 and D-22 are at the end-of-phase review).

## Files Created/Modified

- `README.md` - "Local test gate" and "Demo app" (CRLF kept)
- `.planning/STATE.md` - one deferred row set to Resolved (04-03), plus the workflow-tool updates below
- `.planning/PROJECT.md` - "OSD Phase 4 follow-ups" row
- `CLAUDE.md` - OSC row, UI row, Tests and Demo bullets
- `.claude/skills/adm-osc-integration/adm-osc-integration.md` - self-clocked send note and "Answering position queries" (CRLF kept)
- `docs/integration-guide.md` - "Wiring OSC, trajectories and the map into RenderEngine" with the security sentence, and a Testing note
- `.planning/REQUIREMENTS.md` - EXTR-04 ticked and Complete; EXTR-05 and DATA-02 marked pending human approval

## Decisions Made

- The gate's listing check uses `grep 'ui:'`. CTest generates names like `ui:Map route: ...` with no space after the prefix, so the plan's `ui: ` pattern finds nothing (found in 04-05; applied here).
- EXTR-04 is ticked and EXTR-05 and DATA-02 are not, as D-16 requires. `requirements.mark-complete` was given EXTR-04 only.
- The integration guide's security paragraph tells consumers to run `oscPortsConflict` before connecting and states that the receiver listens on every interface with no authentication.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] The plan's `ui: ` grep pattern cannot match the CTest listing**
- **Found during:** Task 1 (preparing the README gate commands)
- **Issue:** Plan 04-05 found that `catch_discover_tests (... TEST_PREFIX "ui: ")` yields names with no space after `ui:`. The plan's verify (`grep -c 'ui: '`), its fails_when text and the README listing check as written would print 0 and fail a correct gate.
- **Fix:** The README and this run use `grep 'ui:'` and say the space is dropped. The CMake prefix is unchanged.
- **Files modified:** `README.md`
- **Verification:** `ctest --test-dir build-release/tests -N | grep -c 'ui:'` prints 13 and the whole gate exits 0.
- **Committed in:** `faa13a4`

**2. [Rule 3 - Blocking] README line endings**
- **Found during:** Task 1 (commit diff showed 145 lines deleted)
- **Issue:** `README.md` is CRLF at BASE; the first edit wrote LF and hid the real change in a whole-file diff.
- **Fix:** Restored CRLF and amended the unpushed task commit; the net README diff is 33 insertions and 4 deletions. The skill file was edited the same way (CRLF kept).
- **Files modified:** `README.md`
- **Committed in:** `faa13a4`

---

**Total deviations:** 2 auto-fixed (2 blocking, both mechanical). **Impact on plan:** none on scope; no source file changed.

## Issues Encountered

- None beyond the two items above. The gate ran clean on the first full pass; the second pass (the single plan command) matched it.

## Known Stubs

None. This plan edited only documentation and planning records.

## Threat Flags

None. No code, network or file surface was added. T-04-18 is mitigated by the order followed here (gate, evidence, then ticks, with the two human-dependent boxes left open and the gate requiring `ui:` tests in the listing). T-04-19 is mitigated: the README, CLAUDE.md and the integration guide each say CI runs zero tests until Phase 6.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- Phase 4 has all eight plans executed. The next step is the phase verification and the end-of-phase review, which asks the user for exactly two things: a plain-language yes on the two demo screenshots (D-03, `.context/sc-shots/before-drag.png` and `after-drag.png`) and a yes or no on the SAVE PRESET title in DM Sans Bold (D-22, the before and after captures named in the OSD row).
- After the user approves: tick EXTR-05 and DATA-02 and replace the pending notes with "Complete (Phase 4)". If a review item is rejected, follow the rejection paths in the plan.

---
*Phase: 04-control-surface-ui*
*Completed: 2026-10-05*

## Self-Check: PASSED

- FOUND: `README.md`, `CLAUDE.md`, `docs/integration-guide.md`, `.claude/skills/adm-osc-integration/adm-osc-integration.md`, `.planning/PROJECT.md`, `.planning/REQUIREMENTS.md`
- FOUND: commits `faa13a4` and `c89aeb6` (`git log --grep="04-08"` returns both); `commits: 2` measured with `git rev-list --count c4ee8a6..HEAD` before this SUMMARY was committed
- Task 1 and Task 2 acceptance criteria re-run and passing: README has 0 `./build/SpatialCoreTests` and 2 `SpatialCoreUITests`; STATE.md has 1 `Resolved — Phase 4 (04-03)`; REQUIREMENTS.md has 1 EXTR-04 Complete row, 2 pending rows, 0 ticked EXTR-05 or DATA-02; the skill has 0 `every other tick`; the PROJECT.md row names D-07, D-08, D-09, D-19, D-20, D-21 and D-22
