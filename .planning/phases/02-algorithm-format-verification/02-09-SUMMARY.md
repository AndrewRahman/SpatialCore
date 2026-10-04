---
phase: 02-algorithm-format-verification
plan: 09
subsystem: docs
tags: [vbap, ear, itu-r-bs2127, docs, skill, gap-closure, dr-3, phase-gate]
status: complete
gap_ids: [G-02-2]

requires:
  - phase: 02-algorithm-format-verification
    provides: plan 02-08 pair-pan wedge code, band oracle, [g02-2] tracer, computeVBAPGains3D comments
provides:
  - integration guide, README and auto-loading skill describe the EAR-exact below-horizon band, the 5.1.4 rear-gap exception, and the #22 tie scoped to above the horizon
  - WR-01 recorded as fixed (open count 7)
  - validation map rows for 02-08 and 02-09
  - green phase gate (full suite minus the deferred HUTUBS PP2 case, 17 Phase 2 tags)
  - DR-3 result for OpenSpatialDelay 30391cd against this branch
  - cross-repo follow-up list superseding 02-07's
affects: [OpenSpatialDelay release notes and docs, issue #22 comment (suggested, not posted), end-of-phase UAT]

plan_head_before: 1237af444ce19646c80448da3f928cfbdf227711
plan_head_after: a322673f23816f945b09a4370b26b74c66aa05b7

actuals:
  tokens: 3355
  tasks: 2
  commits: 2

tech-stack:
  added: []
  patterns:
    - "byte-safe CRLF doc edit: read bytes, replace named bullets/lines, write bytes; gated phrases protected from re-wrapping"

key-files:
  created: []
  modified:
    - docs/integration-guide.md
    - README.md
    - .claude/skills/spatial-audio-dsp/SKILL.md
    - .planning/phases/02-algorithm-format-verification/02-REVIEW-DISPOSITION.md
    - .planning/phases/02-algorithm-format-verification/02-VALIDATION.md

key-decisions:
  - "The 5.1.x region depth (about -59 degrees) is not a 02-08 measurement; it follows from the wedge/cap boundary geometry, so the plan's figure was kept and its basis is stated here"
  - "WR-01 closed by scoping #22 to above the horizon (the band tie no longer exists after 02-08), not by the widening the review proposed"

requirements-completed: [EXTR-01]

coverage:
  - id: D1
    description: "Guide, README and skill state the EAR-exact band (one pan region per ear-level pair, 7.1.4 az 60 = 0.7071 / 0.7071), the 5.1.4 exception, and #22 above the horizon only"
    requirement: EXTR-01
    verification:
      - kind: other
        ref: "Task 1 <verify> grep and CRLF gate in 02-09-PLAN.md"
        status: pass
    human_judgment: false
  - id: D2
    description: "WR-01 fixed in frontmatter and table, open count 7; validation rows 02-08-T1/T2 and 02-09-T1/T2 added"
    requirement: EXTR-01
    verification:
      - kind: other
        ref: "grep gates in 02-09-PLAN.md Task 2 <verify>"
        status: pass
    human_judgment: false
  - id: D3
    description: "Phase gate: full suite minus HUTUBS PP2 passes (304835 assertions, 192 cases) and all 17 Phase 2 tags match and pass"
    verification:
      - kind: unit
        ref: "./build/tests/SpatialCoreTests per-tag sweep (table below)"
        status: pass
    human_judgment: false
  - id: D4
    description: "OpenSpatialDelay 30391cd compiles and links against this branch; OSD repo status and plugin folders unchanged"
    verification:
      - kind: integration
        ref: "cmake --build /tmp/osd-dr3-check/build --target OpenSpatialDelay OpenSpatialDelayTests"
        status: pass
    human_judgment: false
  - id: D5
    description: "Keep 5.1.4's documented rear-gap difference from EAR (a judgment about sound)"
    verification: []
    human_judgment: true
    rationale: "The orchestrator chose to keep the difference on the diagnosis's recommendation; the user has not yet confirmed it and no test can judge it."

duration: 5min
completed: 2026-10-03
---

# Phase 2 Plan 09: Docs match the EAR-exact band, phase gate and DR-3 Summary

**The integration guide, README and auto-loading skill now say what 02-08's code does (each ear-level speaker pair is one pan region down to -30 degrees, matching PyPI ear 2.1.0 except behind the listener on 5.1.4, the #22 tie only above the horizon), WR-01 is fixed, the phase gate is green, and OpenSpatialDelay 30391cd still builds against this branch.**

## What you will hear

On a height speaker setup such as 7.1.4, a sound placed slightly below ear height now stays exactly where it was at ear height, all the way down to 30 degrees below. The test case: a sound 60 degrees to the left puts 0.7071 of its level on each of the two speakers it sits between, and that stays 0.7071 / 0.7071 at every step from 0 down to -30 degrees. This is what the international reference renderer (EAR, ITU-R BS.2127) does. Before, the sound leaned by up to about 1.2 dB toward one of the two speakers, and which one was picked more or less at random. The one deliberate difference from the reference is directly behind the listener on a 5.1.4 setup, where the reference also feeds the two rear ceiling speakers and SpatialCore keeps the sound on the two rear ear-level speakers (so that sounds above ear height stay exactly as they are today).

**Your one question (yes or no):** on a 5.1.4 setup, for a sound directly behind you, EAR also sends part of it to the two rear ceiling speakers, while SpatialCore keeps it on the two rear speakers at ear height so sounds above ear height are unchanged. Is it OK to keep that one difference, written down in the integration guide? A "no" means 5.1.4's rear gap must follow EAR too, which would change the sound above ear height on 5.1.4 and needs a new decision before any code changes. Nothing was blocked on this answer; it is harvested into end-of-phase UAT.

## Performance

- **Duration:** about 5 min
- **Started:** 2026-10-03T10:22Z
- **Completed:** 2026-10-03T10:28Z
- **Tasks:** 2
- **Files modified:** 5

## Accomplishments

- **Guide, README, skill.** Both guide bullets and the README VBAP bullet were replaced, plus step 4 and the two below-horizon paragraphs in the skill, exactly as the plan worded them, byte-safe (CRLF on every line in all three files: 282/282, 144/144, 583/583; line counts unchanged except the guide's growth from 270 to 282). Every new sentence restates a 02-08 test or docblock: the tracer (`[g02-2]`, 0.7071 / 0.7071), the `[ear][golden][band]` pins and generator caveat (matches PyPI ear 2.1.0, 5.1.4 exception, U+135 / U-135, |azimuth| above 110), and the `computeVBAPGains3D` / `appendLowerHemisphereTriplets` comments (selection order regular, cap, wedge; wedge stored with nadir share 0; trapezoid triangulations tie).
- **Scope held.** `git diff` touches only the named passages: guide +24/-12 lines in two bullets, README one line, skill three lines (step 4 and two paragraphs). Delaunay wording, the 16-speaker virtual-array text and the determinant-threshold line are untouched (they stay in deferred-items.md). The guide still has `Condon-Shortley` and exactly three `SpatialCore#` references (#11, #20, #22); the skill's DAFx, 12.04 and WASPAA 1999 gates hold.
- **WR-01 fixed.** Frontmatter `disposition: fixed`, table row `fixed` with the source "02-08 + 02-09: the band no longer ties; guide and skill scope #22 to above the horizon", `open: 7`, WR-02 to IN-04 still `open`.
- **Validation map.** Rows 02-08-T1, 02-08-T2, 02-09-T1, 02-09-T2 added after 02-07-T3 with the file's pending marker.

## Number check against 02-08 (plan ground rule)

02-08-SUMMARY.md reports no measured 5.1.x region depth, so nothing contradicts the plan's "about -59 degrees". It also follows from the geometry the `SpeakerLayout.cpp` docblock describes: the wedge region ends where the nadir-cap triangle begins, at the great-circle arc between the two -30 degree copies. For two ear-level speakers `D` degrees apart the arc's lowest point is at tan(el) = tan(-30)/cos(D/2). On 5.1.x the rear pair M+110 / M-110 is 140 degrees apart, so el = -atan(0.5774/0.3420) = -59.3 degrees. For the 60-degree pair of 7.1.4 the same formula gives -33.7, consistent with the tracer holding the pan through -30. This is a derivation, not a 02-08 measurement; the guide says "about -59 degrees".

## Phase gate (Task 2, Step 3)

Full suite excluding the deferred HUTUBS PP2 case: exit 0, "All tests passed (304835 assertions in 192 test cases)". The post-run "Leaked objects detected: 1 instance(s) of class FFT" notice is the pre-existing teardown notice already recorded in deferred-items.md (process exit still 0).

| Tag | Result |
|---|---|
| `[tracer]` | pass, 114 assertions / 3 cases |
| `[vbap3d-identity]` | pass, 262104 / 1 |
| `[d03]` | pass, 3 / 1 |
| `[consumer-surface]` | pass, 13 / 1 |
| `[robust]` | pass, 5276 / 4 |
| `[sanitize]` | pass, 23 / 5 |
| `[ear]` | pass, 19215 / 11 |
| `[vbip]` | pass, 7206 / 3 |
| `[panning-law]` | pass, 7191 / 10 |
| `[sn3d]` | pass, 153 / 3 |
| `[roundtrip]` | pass, 6 / 1 |
| `[decode-guard]` | pass, 20 / 1 |
| `[ambi-pin]` | pass, 80 / 2 |
| `[format-resolve]` | pass, 695 / 1 |
| `[counts]` | pass, 39 / 3 |
| `[band]` | pass, 828 / 3 |
| `[g02-2]` | pass, 203 / 1 |

Repo-wide text gates: `git grep 'Pulkki 2000' -- ':!.planning'` has no match; nothing under `src/` names `nearestSpeaker3DFallback`.

## DR-3: OpenSpatialDelay 30391cd build against this branch

**Result: COMPILED AND LINKED.** `OpenSpatialDelay` and `OpenSpatialDelayTests` built with exit 0 and no `error:` lines (log: `/tmp/osd-dr3-check/build.log`), then an immediate re-run of the build command also exited 0.

- Recipe as in 02-07: `git archive -o` into `/tmp/osd-dr3-check`, SpatialCore replaced by a symlink to this worktree, `-DJUCE_DIR=<worktree>/build/_deps/juce-src`, configure exit 0 (41 s). Only `OpenSpatialDelay` and `OpenSpatialDelayTests` were built; `all`, `_VST3`, `_AU` were not, and OSD's tests were not run.
- **OSD repository status** (`git status --porcelain` in `~/conductor/repos/openspatialdelay-v1`): empty before and empty after (identical).
- **Plugin folders:** a listing digest of `~/Library/Audio/Plug-Ins/{VST3,Components}` and `/Library/Audio/Plug-Ins/{VST3,Components}` taken before the build equals the one after. The 18 `OpenSpatialDelay*` bundles present are older installs (May to July 2026); none is new.
- **Warnings inside `SpatialCore/include`:** the same five `-Wunused-parameter` in `OSC/ADMOSCReceiver.h` (lines 45:50, 45:83, 46:52, 51:66, 51:86) as 02-07's table. Not new: `git log d43cb15..HEAD -- include/SpatialCore/OSC/ADMOSCReceiver.h` is empty. No warning from any header Phase 2 changed.

## Cross-repo follow-ups (supersedes the list in 02-07-SUMMARY.md; not done here)

These belong to the OpenSpatialDelay repository. None was performed.

1. **Four live OSD files still describe VBIP as squared gains** and should describe textbook VBIP (D-14: the square root of the VBAP gains, renormalised, wider than VBAP; Pernaux, Boussard & Jot, DAFx-98):
   - `docs/wiki/glossary.md:127-128`
   - `SPECIFICATION.md:491`
   - `docs/wiki/output-formats.md:88`
   - `.claude/skills/spatial-audio-dsp/SKILL.md:554`

   (`Archive/` and `docs/VERSION_HISTORY.md` are historical; leave them.)
2. **OSD release notes must announce five audible changes:**
   - VBIP is wider and up to 3 dB louder between speakers (D-14, F1).
   - Below-horizon panning on height layouts uses ITU-R BS.2127 (EAR) instead of a nearest-speaker snap (D-04).
   - 4OA-6OA levels are corrected (D-08, F15).
   - Non-finite positions are held or silenced (D-06, D-19).
   - Fifth, the below-horizon band is now EAR-exact (G-02-2): between the horizon and -30 degrees a source keeps its horizon pan; MDAP near the horizon on height layouts changes slightly because its spread ring dips into that band; and behind the listener on 5.1.4 SpatialCore deliberately differs from EAR.

   Relative to OSD v1.0.0, the second and fifth items describe one move from the snap to EAR, so the release note may combine them as long as both facts appear.
3. **At migration, delete OSD's own global-namespace copies** of `evalSH`, `computeVBAPGains2D/3D` and the nearest-speaker helper (F7, F15).
4. **SpatialCore's deprecated nearest-speaker helper** is removed at the next major version.
5. **Suggested comment for AndrewRahman/SpatialCore#22 (NOT posted; posting is an outward action for the user to approve):** "Since 02-08 the -30 to 0 band evidence bullet is historical: each band region is now one pan region (the neighbouring ear-level pair's horizon pan) with no tie. The above-horizon coplanar tie this issue tracks is unchanged." `gh` was not used to write anything.

Saved sessions still need no data migration: algorithm indices and `OutputFormat` values are unchanged.

## Task Commits

1. **Task 1: guide, README and skill describe the EAR-exact band, the 5.1.4 exception and #22 above the horizon only** - `b4f0046` (docs)
2. **Task 2: WR-01 disposition and validation rows** - `a322673` (docs). The phase gate and DR-3 build produce no tracked file; their results are recorded above.

**Plan metadata:** the docs(02-09) commit that adds this SUMMARY and the STATE/ROADMAP updates.

## Files Created/Modified

- `docs/integration-guide.md` - below-horizon bullet and 3D VBAP bullet replaced (CRLF kept)
- `README.md` - VBAP bullet only (CRLF kept)
- `.claude/skills/spatial-audio-dsp/SKILL.md` - step 4, the "Below the horizon on layouts with no lower speakers" paragraph, the "Below-horizon handling" paragraph (CRLF kept)
- `.planning/phases/02-algorithm-format-verification/02-REVIEW-DISPOSITION.md` - WR-01 fixed, open 7
- `.planning/phases/02-algorithm-format-verification/02-VALIDATION.md` - four new rows

## Decisions Made

- The -59 degree depth in the guide is a geometric derivation (see "Number check"), kept as the plan worded it.
- WR-01 is resolved by scoping #22 to above the horizon, because the band tie no longer exists after 02-08.

## Deviations from Plan

None - plan executed exactly as written.

**Total deviations:** 0 auto-fixed.

## Issues Encountered

- The full-suite run prints the pre-existing JUCE FFT leaked-object notice after "All tests passed" (exit 0); already in deferred-items.md.
- A first read of the plan's zsh plugin-folder baseline glob failed (no match for one `/Library` pattern), so the "before" plugin list was empty; the folder-listing digest taken before the build was used for the before/after comparison instead, and the existing bundles were confirmed as older installs by modification date.

## Known Stubs

None.

## Threat Flags

None. Docs and planning files only; no new network, auth or file-access surface.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- Gap G-02-2 is closed in code (02-08) and docs (02-09). Remaining for the phase: end-of-phase UAT (including the one yes/no question above) and the OSD-side follow-ups, which the user carries.

## Self-Check: PASSED

- All five modified files exist; commits `b4f0046` and `a322673` are in `git log`.
- Task 1 `<verify>` exits 0 with no "CRLF BROKEN"; acceptance checks hold (only the three docs changed in Task 1, Condon-Shortley count 1, `SpatialCore#` count 3, skill DAFx / 12.04 / WASPAA 1999 present).
- Task 2 gates: full suite and all 17 tags exit 0, text gates empty, WR-01 `fixed` with `open: 7` and exactly 7 `disposition: open` entries, `| 02-09-T2 |` present, OSD build exit 0, OSD status identical.

---
*Phase: 02-algorithm-format-verification*
*Completed: 2026-10-03*
