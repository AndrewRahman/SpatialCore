---
phase: 02-algorithm-format-verification
plan: 07
subsystem: spatial-audio-dsp
tags: [docs, ambisonics, ambix, sn3d, vbip, vbap, ear, itu-r-bs2127, dbap, mdap, skill, dr-3, openspatialdelay, phase-gate]
status: complete

requires:
  - phase: 02-algorithm-format-verification
    provides: "02-03: tie-break issue AndrewRahman/SpatialCore#22"
  - phase: 02-algorithm-format-verification
    provides: "02-04: algorithm-layer non-finite guards, engine hold-last-good, D-06b largest-min-gain fallback"
  - phase: 02-algorithm-format-verification
    provides: "02-05: textbook VBIP, VBIP/DBAP/MDAP docblocks (DAFx-98, #20, 12.04 dB, WASPAA 1999)"
  - phase: 02-algorithm-format-verification
    provides: "02-06: evalSH convention docblock, single SH evaluator, single decoder"
provides:
  - "README.md: AmbiX row (ACN, SN3D, no Condon-Shortley phase); VBAP below-horizon EAR; textbook VBIP; DBAP 12.04 dB; MDAP WASPAA 1999"
  - "docs/integration-guide.md: new '## Ambisonics and panning conventions' section (SH convention, observable panning behaviour, non-finite positions)"
  - ".claude/skills/spatial-audio-dsp/SKILL.md: corrected VBAP tie-break, below-horizon, VBIP, Ambisonics convention, DBAP, MDAP, SH reference and anti-pattern text"
  - "Comment-only deprecation note on nearestSpeaker3DFallback in SpatialMath.h (body and signature unchanged, no attribute)"
  - "DR-3 result: OpenSpatialDelay 30391cd compiles and links against this branch (OpenSpatialDelay + OpenSpatialDelayTests)"
  - "Phase gate: 190 cases, only the deferred HUTUBS case fails; all 15 Phase 2 tags run"
affects: [OpenSpatialDelay migration (cross-repo follow-ups below), Phase 6 docs (deferred skill/README drift), next major version (helper removal)]

plan_head_before: 9d956c8a9f4979aeafcb07de39eb3b5019984f6d
plan_head_after: 2b7653b7529d354578fd2cad2feefafdda833374

actuals:
  tokens: 4246
  tasks: 3
  commits: 2

tech-stack:
  added: []
  patterns:
    - "Docs restate code docblocks: every convention sentence in README, the guide and the skill was copied from the evalSH / VBIP / DBAP / MDAP / computeVBAPGains3D source, not paraphrased from memory"
    - "CRLF docs edited byte-safely (read binary, replace, write binary) so diffs touch only intended lines"
    - "Cross-repo DR-3 check: git archive (object reads only) into /tmp, SpatialCore symlinked, JUCE_DIR into this worktree, only the non-installing targets built"

key-files:
  created: []
  modified:
    - README.md
    - docs/integration-guide.md
    - include/SpatialCore/Core/SpatialMath.h
    - .claude/skills/spatial-audio-dsp/SKILL.md
    - .planning/phases/02-algorithm-format-verification/deferred-items.md

key-decisions:
  - "The integration guide says every algorithm returns finite gains or silence, and names only VBAP, VBIP, MDAP, KNN and DirectBinaural as silent for a non-finite direction (DBAP, ConstantPower and Ambisonics return finite non-silent gains), and that only the 2D VBAP path wraps a huge azimuth -- the plan's shorter wording would have over-claimed what the code does"
  - "The skill's section 4 below-horizon paragraph describes the EAR path where it actually runs (VBAP/VBIP/MDAP on height speaker layouts); the 16-speaker virtual binaural array that section describes is not in SpatialCore's code, logged as deferred rather than rewritten (plan: change nothing else)"
  - "DR-3 was run twice: a full build before the doc edits (to surface setup failures early) and an incremental build after both commits, which recompiled PluginProcessor.cpp and every OSD test TU against the final SpatialMath.h; both exit 0"

patterns-established:
  - "The OSD DR-3 recipe (git archive + symlinked SpatialCore + JUCE_DIR + targets OpenSpatialDelay/OpenSpatialDelayTests) is now proven, retiring RESEARCH assumption A4"

requirements-completed: [VERIFY-01, EXTR-01, EXTR-03]

coverage:
  - id: D1
    description: "README and the integration guide state the AmbiX convention (ACN, SN3D, no Condon-Shortley phase, +az left), textbook single-band VBIP (#20), EAR below-horizon panning, the minimum-sum tie-break (#22), DBAP 12.04 dB / no blur, MDAP WASPAA 1999, and non-finite handling, matching the code"
    requirement: "VERIFY-01"
    verification:
      - kind: other
        ref: "Task 1 <verify> command (Condon-Shortley in README and guide, WASPAA 1999, BS.2127, section heading, no 'squared gains|tighter focus') exit 0"
        status: pass
      - kind: other
        ref: "grep -c: DAFx-98 1, SpatialCore#20 1, 12.04 1, 'Pulkki 2000' 0 in README.md; guide heading line 179 < 'What to Keep vs Replace' line 230; SpatialCore#[0-9] 3 and evalSH 2 in the guide"
        status: pass
    human_judgment: true
    rationale: "Greps prove the phrases are present; whether each sentence describes the code accurately and clearly is a reading judgment (prohibition 1 is verification: judgment)."
  - id: D2
    description: "nearestSpeaker3DFallback carries a comment-only deprecation note; signature and body unchanged; no deprecation attribute; no src/ reference"
    requirement: "EXTR-01"
    verification:
      - kind: other
        ref: "grep -B12 above the function: 'deprecated' 1; 'used when triplets are empty' 0; '[[deprecated' 0; git diff of SpatialMath.h shows only // lines; grep -rn nearestSpeaker3DFallback src/ empty"
        status: pass
    human_judgment: false
  - id: D3
    description: "The auto-loading spatial-audio-dsp skill teaches the Phase 2 behaviour: min-sum tie-break with #22, EAR below horizon, textbook VBIP (DAFx-98, #20), AmbiX convention, DBAP 12.04 dB, MDAP WASPAA 1999 with the alpha ring and #10, corrected VBIP anti-pattern; none of the five stale F12 phrases remain"
    requirement: "VERIFY-01"
    verification:
      - kind: other
        ref: "Task 2 <verify> command (6 positive greps, 5 negative counts == 0) exit 0; git status --porcelain .claude/skills/ lists only SKILL.md"
        status: pass
    human_judgment: true
    rationale: "Accuracy of the restated algorithm descriptions is a reading judgment; the greps only prove the stale phrases are gone and the required ones present."
  - id: D4
    description: "Phase gate: full suite green except the deferred HUTUBS PP2 checksum; every one of the 15 Phase 2 tags matches and passes; no tracked file outside .planning cites the year-2000 MDAP attribution"
    requirement: "EXTR-03"
    verification:
      - kind: unit
        ref: "./build/tests/SpatialCoreTests \"~HUTUBS PP2 — golden HRIR checksum at az=90deg (own control)\" (306084 assertions, 189 cases, exit 0) plus the 15-tag loop (no TAG FAILED) and both text gates -- Task 3 automated #1 exit 0"
        status: pass
    human_judgment: false
  - id: D5
    description: "DR-3: OpenSpatialDelay 30391cd compiles and links against this branch (targets OpenSpatialDelay and OpenSpatialDelayTests), OSD repository unmodified, no plugin installed"
    requirement: "EXTR-01"
    verification:
      - kind: integration
        ref: "cmake --build /tmp/osd-dr3-check/build --target OpenSpatialDelay OpenSpatialDelayTests -j8 (Task 3 automated #2) exit 0, 0 'error:' lines"
        status: pass
      - kind: other
        ref: "git -C ~/conductor/repos/openspatialdelay-v1 status --porcelain identical before/after (0 lines both)"
        status: pass
    human_judgment: true
    rationale: "02-VALIDATION.md lists DR-3 as manual-only: the human reviews the build result, the SpatialCore-header warnings and the cross-repo follow-ups (Task 3 <human-check>)."

duration: 8min
completed: 2026-10-01
---

# Phase 2 Plan 07: Docs Sweep, DR-3 OSD Build and Phase Gate Summary

**README, the integration guide and the auto-loading spatial-audio-dsp skill now describe the Phase 2 code: the AmbiX convention (ACN, SN3D, no Condon-Shortley phase), textbook single-band VBIP, EAR below-horizon panning, the minimum-sum tie-break (#22), DBAP at 12.04 dB, and MDAP cited as WASPAA 1999. The nearest-speaker helper is comment-deprecated. OpenSpatialDelay 30391cd compiles and links against the branch, and the phase gate is green apart from the one deferred HUTUBS case.**

## Performance

- **Duration:** about 8 min (wall clock; the OSD full build ran in the background during Task 1)
- **Started:** 2026-10-01T07:23:59Z
- **Completed:** 2026-10-01T07:32:32Z
- **Tasks:** 3 of 3
- **Files modified:** 4 tracked docs/header files, plus `deferred-items.md`

## Accomplishments

- **README (Task 1).**
  - The Ambisonics Output row now reads "AmbiX: ACN channel order, SN3D, no Condon-Shortley phase".
  - VBAP gains the EAR below-horizon note.
  - VBIP is textbook (DAFx-98), wider than VBAP, and single-band with #20 cited.
  - DBAP gains "effective rolloff 12.04 dB, no spatial blur".
  - MDAP is cited as "Pulkki, WASPAA 1999".
  - The Ambisonics bullet adds "no Condon-Shortley phase".
- **Integration guide (Task 1).** A new `## Ambisonics and panning conventions` section sits just before `## What to Keep vs Replace`. It has three parts:
  1. The `evalSH` docblock convention, quoted verbatim, plus single-evaluator status and the 4OA-6OA level fix (#11).
  2. Panning behaviour a consumer can observe:
     - VBIP law;
     - the EAR construction and how a source moves from 0 to -30 to -90 degrees;
     - no elevated-speaker gain at or below -1 degree for VBAP and VBIP;
     - binaural and Ambisonics keep the true elevation;
     - the minimum-sum tie-break and #22;
     - the D-06b fallback.
  3. Non-finite positions:
     - the engine holds each field independently, with defaults 0 / 0 / 0.5;
     - the algorithm layer protects consumers that call `computeGains` directly, as OSD does;
     - the guards live in `.cpp` files, so a consumer's fast-math does not disable them.
- **`SpatialMath.h` (Task 1).** The stale "v1.0 ... used when triplets are empty" comment is replaced by a comment-only deprecation note:
  - It says the helper is unused since D-01 (D-04 triplets replace it), is kept because removing a public inline function is a major-version change, and is scheduled for removal at the next major.
  - It deliberately carries no attribute (F9).
  - Only `//` lines changed.
- **Skill (Task 2).** The skill now matches the code in eleven places:
  - §1.3 VBAP: the tie-break is the minimum sum (#22), and the fallback text describes EAR, the largest-min-gain case and non-finite silence.
  - §1.3 VBIP: the steps are sqrt then renormalise, with rE aiming, "wider", DAFx-98 sec. 2.2.2, and the single-band note citing 700 Hz and #20.
  - §1.5: the convention paragraph and evalSH as the single evaluator.
  - §1.6 DBAP: step 3 now says 1/d^2 used as amplitude, a = 2, R = 12.04 dB, d^2 clamped at 0.001. It also states no blur, no user rolloff and no hull projection. The old step 3 claimed "6 dB per doubling", which the code does not do.
  - §1.7 MDAP: cited as WASPAA 1999, with the alpha ring formula, the main direction plus 8 auxiliary directions, amplitude summing, and #10.
  - §4: the below-horizon paragraph now describes EAR.
  - §7.1, §7.2, §7.4: the cos/sin rule, the SN3D sum, and no Condon-Shortley phase / +az left / AmbiX.
  - §10: the VBIP anti-pattern row is corrected.
- **DR-3 (Task 3).** OpenSpatialDelay 30391cd compiles and links against this branch (details below).
- **Phase gate (Task 3).** Green except the deferred HUTUBS case, and all 15 Phase 2 tags run (table below).

## DR-3: OpenSpatialDelay 30391cd build against this branch

**Result: COMPILED AND LINKED.** Both `OpenSpatialDelay` (SharedCode static lib) and `OpenSpatialDelayTests` built with exit 0 and 0 `error:` lines.

- **Recipe, exactly as the plan specified:**
  - `git archive 30391cd | tar -x -C /tmp/osd-dr3-check`.
  - `SpatialCore/` is replaced by a symlink to this worktree.
  - Configure: `cmake -S /tmp/osd-dr3-check -B /tmp/osd-dr3-check/build -DCMAKE_BUILD_TYPE=Debug -DJUCE_DIR=<worktree>/build/_deps/juce-src`. It exits 0 in 48.7 s; the only warning is JUCE's "bundle ID contains spaces".
  - Build: `cmake --build ... --target OpenSpatialDelay OpenSpatialDelayTests -j8`.
  - `all`, `_VST3` and `_AU` were never built.
- **Two runs:**
  1. A full build before any doc edit. Log at `/tmp/osd-dr3-check/build-initial.log`; exit 0.
  2. An incremental build after both task commits. Log at `/tmp/osd-dr3-check/build.log`; exit 0. The `SpatialMath.h` comment change recompiled 7 SpatialCore sources, OSD's `PluginProcessor.cpp` and `PluginEditor.cpp` (in both targets), and all 12 OSD test TUs, so the final header was compiled inside OSD translation units under OSD's `-ffast-math`.
- **RESEARCH assumption A4 is retired:** the recipe works as sketched. The only addition was `JUCE_DIR`, because the archive's JUCE submodule directory is empty.
- **Repository and system untouched:**
  - `git -C ~/conductor/repos/openspatialdelay-v1 status --porcelain` was empty before and after.
  - No new plugin bundle exists. The plugin folders hold only older `v2.0.0-dev.*` and unversioned installs dated May to July.
- **OSD tests were not run**, as the plan said: its regression goldens are expected to differ by the four approved audible changes.

### Warnings from SpatialCore headers inside OSD translation units

Five distinct warnings, all `-Wunused-parameter`, all in one header:

| Header:line | Parameter |
|---|---|
| `include/SpatialCore/OSC/ADMOSCReceiver.h:45:50` | `objectIndex` |
| `include/SpatialCore/OSC/ADMOSCReceiver.h:45:83` | `paramName` |
| `include/SpatialCore/OSC/ADMOSCReceiver.h:46:52` | `value` |
| `include/SpatialCore/OSC/ADMOSCReceiver.h:51:66` | `propertyName` |
| `include/SpatialCore/OSC/ADMOSCReceiver.h:51:86` | `value` |

These are the default-empty virtual bodies of `admObjectParamReceived` and `admGlobalParamReceived`. **Pre-existing:** `git log d43cb15..HEAD -- include/SpatialCore/OSC/ADMOSCReceiver.h` is empty. **No warning comes from any header Phase 2 changed**, including `SpatialMath.h`, `SpeakerLayout.h`, `RenderEngine.h` and the algorithm headers. That confirms the no-attribute choice on the deprecated helper added nothing to consumer builds. I logged the five warnings in `deferred-items.md`.

The other OSD warnings are in OSD's own sources:
- shadowing in `PluginEditor.cpp`;
- `-Wfloat-equal` at `PluginProcessor.cpp:3057`;
- a JUCE deprecation in `HarnessCore.cpp`;
- **unused static functions `nearestSpeaker3DFallback`, `computeVBAPGains2D` and `computeVBAPGains3D` at `PluginProcessor.cpp:2115/2327/2431`**. These are OSD's own global-namespace copies, which follow-up 3 below deletes.

## Phase gate

**Full suite: 190 test cases, 189 pass, 1 fails.**
- The failure is `HUTUBS PP2 — golden HRIR checksum at az=90deg (own control)`, the same case as the baseline, deferred to Phase 3.
- With that case excluded by name, the run reports 306,084 assertions in 189 cases and exits 0.
- The JUCE "Leaked objects: FFT" line at exit is the known false positive in `deferred-items.md`.

Per-tag sweep (Catch2 exits 2 on no match; every tag matched):

| Tag | Cases | Assertions | Result |
|---|---|---|---|
| `[tracer]` | 3 | 114 | pass |
| `[vbap3d-identity]` | 1 | 262,104 | pass |
| `[d03]` | 1 | 3 | pass |
| `[consumer-surface]` | 1 | 13 | pass |
| `[robust]` | 4 | 5,276 | pass |
| `[sanitize]` | 5 | 23 | pass |
| `[ear]` | 8 | 20,480 | pass |
| `[vbip]` | 3 | 7,206 | pass |
| `[panning-law]` | 10 | 7,175 | pass |
| `[sn3d]` | 3 | 153 | pass |
| `[roundtrip]` | 1 | 6 | pass |
| `[decode-guard]` | 1 | 20 | pass |
| `[ambi-pin]` | 2 | 80 | pass |
| `[format-resolve]` | 1 | 695 | pass |
| `[counts]` | 3 | 39 | pass |

Repo-wide text gates:
- `git grep -n 'Pulkki 2000' -- ':!.planning'` prints nothing.
- `grep -rn nearestSpeaker3DFallback src/` prints nothing.
- Task 3's first `<automated>` command, run verbatim, exits 0.

## Cross-repo follow-ups (not done here)

These are caused by Phase 2 but belong to the OpenSpatialDelay repository. None was performed.

1. **OSD glossary lines 127-128** still say VBIP squares the gains (D-14). They should describe textbook VBIP: the square root of the VBAP gains, renormalised, wider than VBAP (Pernaux, Boussard & Jot, DAFx-98).
2. **OSD release notes must announce four audible changes:**
   - **VBIP:** wider, and up to 3 dB louder between speakers (0 dB on a speaker) (D-14, F1).
   - **Below-horizon panning on height layouts:** now uses ITU-R BS.2127 (EAR) instead of a nearest-speaker snap (D-04).
   - **4OA-6OA levels corrected:** 22 channels change level. This fixes the same SN3D defect that OSD v1.0.0 ships in its own `evalSH` (D-08, F15).
   - **Non-finite positions:** held at the last good value by the engine, or silenced by the algorithm layer (D-06, D-19).
3. **At migration, delete OSD's own global-namespace copies** of `evalSH`, `computeVBAPGains2D/3D` and `nearestSpeaker3DFallback` in `Source/PluginProcessor.cpp`, so OSD gets SpatialCore's fixed versions (F7, F15). The DR-3 build shows three of them already compile as unused static functions (`:2115`, `:2327`, `:2431`). OSD's local `evalSH` is still used, at `:2262`, for its order-3 decode.
4. **SpatialCore's deprecated `nearestSpeaker3DFallback`** is removed at the next major version (comment-deprecated in this plan).

Saved sessions need no data migration: algorithm indices and `OutputFormat` values are unchanged.

## Task Commits

1. **Task 1: README, integration guide, helper deprecation comment** - `3f88997` (docs)
2. **Task 2: spatial-audio-dsp skill corrections** - `2b7653b` (docs)
3. **Task 3: DR-3 OSD build and phase gate** - no commit; it produces no tracked file (the build lives in `/tmp/osd-dr3-check`). Its results are recorded here.

**Plan metadata:** the docs(02-07) commit that adds this SUMMARY and `deferred-items.md`.

## Files Created/Modified

- `README.md` (CRLF kept) - Ambisonics row; VBAP, VBIP, DBAP, MDAP and Ambisonics bullets
- `docs/integration-guide.md` (CRLF kept) - new conventions section (51 lines)
- `include/SpatialCore/Core/SpatialMath.h` - comment-only deprecation note above `nearestSpeaker3DFallback`
- `.claude/skills/spatial-audio-dsp/SKILL.md` (CRLF kept) - §1.3, §1.5-1.7, §4, §7.1/7.2/7.4, §10
- `.planning/phases/02-algorithm-format-verification/deferred-items.md` - three items found here

## Decisions Made

See `key-decisions`. The first one is a deliberate narrowing of the plan's wording.

The plan's guide text said the algorithm layer "returns silence for a non-finite direction and wraps huge azimuths". The code does not do exactly that:
- **Silence:** only VBAP, VBIP, MDAP, KNN and DirectBinaural return silence for a non-finite direction. DBAP, for example, gets equal finite gains, because `std::max (0.001f, NaN)` returns 0.001. The 02-04 robustness contract is "finite gains or silence".
- **Wrapping:** only `computeVBAPGains2D` wraps a huge azimuth. The 3D path takes sin/cos directly and is never wrapped (02-04 key decision).

The guide states what the code does, so it does not break the plan's own prohibition on describing behaviour the code does not have.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Accuracy] §1.6 DBAP step 3 claimed "6 dB per doubling"**
- **Found during:** Task 2
- **Issue:** The plan said to *add* the 12.04 dB text to §1.6. Its existing step 3 said "Weight = 1 / distance² (inverse-square law, 6 dB per doubling)", which contradicts the code: 1/d² used as amplitude is a = 2, which is 12.04 dB. Adding the new text and leaving the old would have put the skill at odds with itself.
- **Fix:** Rewrote step 3 inside §1.6 to say 1/d², clamped at 0.001, used as amplitude, a = 2, R = 12.04 dB, the paper's default being 6 dB.
- **Files modified:** `.claude/skills/spatial-audio-dsp/SKILL.md`
- **Committed in:** `2b7653b`

**2. [Rule 1 - Accuracy] MDAP step text corrected along with the ring formula**
- **Found during:** Task 2
- **Issue:** The old steps said only the 8 auxiliary sources are panned and "summed". The code also pans the main direction D, amplitude-sums the 9 vectors, then power-normalises.
- **Fix:** The steps now name D plus 8 auxiliary directions at alpha, amplitude-sum them, and normalise.
- **Committed in:** `2b7653b`

---

**Total deviations:** 2 auto-fixed (both doc accuracy, inside sections the plan already edits).
**Impact on plan:** None beyond making the edited sections internally consistent with the code. No other skill section was touched.

## Issues Encountered

- **The DR-3 recipe needed nothing beyond the plan.** The archive's JUCE submodule directory is empty, and `-DJUCE_DIR` covers it as planned. SpatialCore's own FetchContent pieces (libmysofa, Catch2) resolved during configure.
- **Out-of-scope drift found and logged in `deferred-items.md`, not fixed:**
  - Skill §4 describes a 16-speaker virtual binaural array (and "30 pre-computed triangulations") that SpatialCore's code does not contain. `VirtualSpeaker` is unused, and Binaural activates a 0-speaker layout. `README.md` repeats the claim.
  - Skill §1.3 says the triplets come from a "Delaunay triangulation". The builder actually keeps every non-degenerate triple.
  - The `ADMOSCReceiver.h` unused-parameter warnings listed above.

## Known Stubs

None.

## Threat Flags

None. T-02-17 is mitigated: the OSD status was identical before and after. T-02-18 is mitigated: only the non-installing targets were built, and no new bundle appeared. T-02-19 is mitigated: every doc sentence restates a code docblock, and the positive and negative grep gates pass.

## User Setup Required

None.

## Next Phase Readiness

- Phase 2's seven plans are complete. VERIFY-01 is closed in code (02-06) and in docs (this plan).
- **Human check for end-of-phase UAT (Task 3 `<human-check>`):** read the DR-3 section, the SpatialCore-header warning table and the four cross-repo follow-ups above. Confirm that the build result is acceptable, that the `ADMOSCReceiver.h` warnings are fine to defer, and that these are the follow-ups to carry into the OSD repository.

## Self-Check: PASSED

- FOUND: README.md, docs/integration-guide.md, include/SpatialCore/Core/SpatialMath.h, .claude/skills/spatial-audio-dsp/SKILL.md, deferred-items.md
- FOUND commits: `3f88997`, `2b7653b`; `git rev-list --count 9d956c8..HEAD` = 2 before the metadata commit
- Task 1 and Task 2 `<verify>` re-run: exit 0. Task 3 automated #1 exit 0 (no TAG FAILED); automated #2 exit 0; OSD status unchanged.
- `/tmp/osd-dr3-check/build/CMakeCache.txt` exists; `build.log` ends `BUILD EXIT 0`.

---
*Phase: 02-algorithm-format-verification*
*Completed: 2026-10-01*
