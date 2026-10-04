---
gsd_state_version: "1.0"
milestone: v1.0.0
current_phase: 02
current_phase_name: Algorithm & Format Verification
status: verifying
stopped_at: Completed 02-10-PLAN.md
last_updated: "2026-10-03T21:55:22.596Z"
last_activity: 2026-10-04
last_activity_desc: Phase 02 code review resolved to clean (25 findings fixed)
state_head: dc945acd93f3fecbcac6d4c5cff9803dbb5eb635
progress:
  total_phases: 6
  completed_phases: 1
  total_plans: 15
  completed_plans: 15
milestone_name: milestone
---

# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-08-09)

**Core value:** A Spatial Media Lab plugin author gets production-grade spatial rendering by linking one library, so the only audio code they write is their own effect.
**Milestone:** v1 — OpenSpatialDelay ships on SpatialCore as a submodule with zero regressions
**Current focus:** Phase 02 — Algorithm & Format Verification

## Current Position

Phase: 02 (Algorithm & Format Verification) — EXECUTING
Plan: 10 of 10 (02-10 complete; all plans executed)
Status: Phase complete — ready for verification
Last activity: 2026-10-04 — Phase 02 code review resolved: 4 fix/re-review rounds, all 25 findings fixed, final review `clean` (`02-REVIEW.md`, ledger `02-REVIEW-DISPOSITION.md` 0 open). Debug 200/201 (only the pre-existing `HutubsPP2Tests.cpp:47`), Release 201/201, OpenSpatialDelay builds against the branch. Phase verification still has the human UAT items in `02-UAT.md`.

**Count contract settled 2026-08-11:** the canonical counts are **8** algorithms, **5** SOFA HRTF
profiles, **23** output formats, **15** speaker layouts, **JUCE 9.0.0**. The previously-locked D-04
pair of 25 formats / 14 layouts was wrong: it originated as a mapper miscount in
`.planning/codebase/ARCHITECTURE.md:107` and `STRUCTURE.md:119-120` during the 2026-08-10 remap,
which commit `f0b14f4` copied into REQUIREMENTS.md and ROADMAP.md under a "verified from the tree"
label it never earned. The same remap commit's `TESTING.md:56` already said "23-format table". The
tree has read 23/15 since `149d50d` (2026-07-05), and the code ships
`static constexpr int NUM_OUTPUT_FORMATS = 23;` at `include/SpatialCore/IO/OutputFormatRegistry.h:9`.
Corrected across CONTEXT/REQUIREMENTS/ROADMAP/codebase-map in `cf265e1`. Treat any `25` or `14` in a
count context as a defect.

**Branch note:** planning now tracks `gsd-remap` = `origin/spatialcore-v2-extraction` + `origin/main`.
The 2026-08-09 pass was planned against a branch missing 42 commits of code, so its codebase map
described a tree that no longer matched reality. Do not plan against `origin/main` alone.

Progress: [██████████] 100%

## Performance Metrics

**Velocity:**

- Total plans completed: 5
- Average duration: —
- Total execution time: —

**By Phase:**

| Phase | Plans | Total | Avg/Plan |
|-------|-------|-------|----------|
| 01 | 5 | - | - |

**Recent Trend:**

- Last 5 plans: none yet
- Trend: —

*Updated after each plan completion*
**Per-Plan Metrics:**

| Plan | Duration | Tasks | Files |
|------|----------|-------|-------|
| Phase 01 P01 | 25min | 2 tasks | 6 files |
| Phase 01 P02 | 12min | 2 tasks | 17 files |
| Phase 01 P03 | 6min | 3 tasks | 5 files |
| Phase 01 P04 | 18min | 2 tasks | 2 files |
| Phase 01 P05 | 6min | 2 tasks | 8 files |
| Phase 02 P01 | 7min | 3 tasks | 11 files |
| Phase 02 P03 | 2min | 2 tasks | 1 files |
| Phase 02 P02 | 4min | 3 tasks | 9 files |
| Phase 02 P04 | 13min | 3 tasks | 8 files |
| Phase 02 P05 | 10min | 3 tasks | 9 files |
| Phase 02 P06 | 7min | 3 tasks | 8 files |
| Phase 02 P07 | 8min | 3 tasks | 5 files |
| Phase 02 P08 | 5 min | 2 tasks | 9 files |
| Phase 02 P09 | 5 min | 2 tasks | 5 files |
| Phase 02 P10 | wall 4h50m (incl. usage-limit pause) | 2 tasks | 5 files |

## Accumulated Context

### Decisions

Full log in PROJECT.md Key Decisions. Affecting current work:

- DR-5 **locked, reconciliation decided**: DR-5 says SpatialCore embeds HRTF profiles as BinaryData; the tree loads them from disk. Resolved 2026-08-10: embed as DR-5 says for v1, and add a shared-folder lookup ahead of it so the embedding can later be reduced to a single fallback profile without code changes. Phase 3 owns it.
- DR-1 **locked**: no allocation, locks, or logging in anything reachable from `processBlock`. Owns Phase 5.
- DR-7 **locked**: `SpatializationAlgorithm` is frozen. Contract ambiguity is a defect, not a preference.
- **User ruling 2026-08-10:** OpenSpatialDelay is the v1 consumer, not OpenSpatialPanner. OSD is the tree SpatialCore was extracted from and ships publicly at v1.0.0, so it is the only oracle that can prove zero regressions. Verified: OSD does not yet consume SpatialCore — the migration is unstarted.
- **Discharged:** the 2026-08-09 ruling that `src/` is "partial until audited" has been satisfied by evidence. The suite builds and passes 144 TEST_CASEs / 1585 assertions across 16 files. Phases no longer open with a re-implementation audit.
- [Phase ?]: Count contract (23 output formats / 15 speaker layouts / 8 algorithms) frozen in code via static_assert + Catch2 [counts] backstop; CLAUDE.md I/O row corrected to match
- [Phase ?]: BUG-03 resolved: 39 bare issue citations across 17 files qualified to Spatial-Media-Lab/OpenSpatialDelay#N; all 15 distinct numbers proven to resolve via gh issue view; the live #2 cross-tracker collision is closed
- [Phase ?]: Added missing Constant Power bullet to README.md's algorithm list (Rule 2 deviation) — heading said 8 but list named only 7, matching AllAlgorithms.h's own static_assert guidance naming README.md as a required update site
- [Phase ?]: docs/integration-guide.md submodule URL corrected to AndrewRahman/SpatialCore.git (resolving dev remote); Spatial-Media-Lab/SpatialCore demoted to labelled post-proof-destination prose per D-17
- [Phase ?]: OQ-2 stays closed: shipped tanh soft ceiling is outputLimiter()'s sole governing contract (SpatialMath.h docblock); the scaffold plan's hard-clamp sketch was stamped historical (Tier B pattern) rather than amended, per D-07/D-01
- [Phase ?]: OQ-5 (org repo migration timing) retired to decision DR-18: development stays on the personal remote until the pipeline is proven, migration gated on proof not a date
- [Phase ?]: Phase 01 complete: all Tier B documents stamped historical (zero renumbering); PROJECT.md and REQUIREMENTS.md corrected so they no longer mis-steer future phases with stale counts or a phantom DSP/ path
- [Phase 02]: Plan 02-03 Task 1 resolved file-as-drafted (user delegated to orchestrator); coplanar-quad tie-break filed as AndrewRahman/SpatialCore#22, not fixed in Phase 2 (D-18)
- [Phase 02]: Plan 02-02 Task 1 package gate resolved approved-recreate (user delegated to orchestrator, PyPI provenance verified); .context/venv rebuilt from pins, which also removed an unpinned pypdf 6.19.0
- [Phase 02]: Offline oracles checked in under tests/reference/ (scipy SN3D, ear 2.1.0 nadir cap, textbook VBAP/VBIP/DBAP, MDAP port labelled not-an-oracle); headers regenerate byte-identically and are never run in CI
- [Phase 02]: Plan 02-04: D-19(i) bounded std::remainder wrap applied to computeVBAPGains2D only; computeVBAPGains3D silences non-finite az/el but leaves finite angles unwrapped so finite output stays bit-identical to d43cb15 (D-06b)
- [Phase 02]: Plan 02-04: RenderEngine sanitiser holds azimuth, elevation and distance per field (ADM-OSC sends NaN for unset axes) and never wraps or clamps a finite value, per the plan rather than RESEARCH Pattern 4's pair-hold with wrap/clamp
- [Phase 02]: Plan 02-04: [robust] power check temporarily accepts VBIP's pre-D-14 law (gains sum to 1, F1); Plan 02-05 should remove the commented carve-out once VBIP is unit-power
- [Phase 02]: Plan 02-05: VBIP is textbook VBIP (sqrt of VBAP gains, renormalised to unit power, DAFx-98 sec. 2.2.2); single band, dual-band tracked in AndrewRahman/SpatialCore#20 (D-14, D-15)
- [Phase 02]: Plan 02-05: the coplanar-tie mirror filter counts only rivals of the minimum-sum enclosing triplet; the any-two reading skipped every point on 7.1.6/9.1.6/SML13.1 because coplanar triangles always tie
- [Phase 02]: Plan 02-05: 02-04's temporary VBIP carve-out in the [robust] power check is removed; VBIP is held to power 0 or 1
- [Phase 02]: Plan 02-06: D-08 is constants only -- 22 SN3D substitutions in evalSH; post-fix max literal error 2.4e-7, addition-theorem deviation 2.8e-6 (was 19.54); AmbisonicsCodec::evaluateSH is a forwarder, so evalSH is the single SH implementation
- [Phase 02]: Plan 02-06: D-09 proven pure refactor -- activateLayout uses AmbisonicsCodec::getDecodeMatrix (3, ...); max |diff| vs the verbatim d43cb15 decoder is 0 on all 15 layouts; rows past the speaker count are now cleared (old code left them stale)
- [Phase 02]: Plan 02-06: SpatialCore#11 referenced as closed in commit 2c4e38d message only (no gh action); the docs half of the ACN/SN3D/no-Condon-Shortley statement lands in Plan 02-07
- [Phase 02]: Plan 02-07: docs (README, integration guide, spatial-audio-dsp skill) restate the Phase 2 code -- AmbiX convention, textbook single-band VBIP (#20), EAR below horizon, min-sum tie-break (#22), DBAP 12.04 dB, MDAP WASPAA 1999; the guide names only VBAP/VBIP/MDAP/KNN/DirectBinaural as silent for non-finite input and only the 2D VBAP path as wrapping
- [Phase 02]: Plan 02-07: DR-3 passed -- OpenSpatialDelay 30391cd compiles and links against the branch (OpenSpatialDelay + OpenSpatialDelayTests, git archive + symlinked SpatialCore + JUCE_DIR); only SpatialCore-header warnings are 5 pre-existing -Wunused-parameter in ADMOSCReceiver.h; RESEARCH A4 retired
- [Phase 02]: Plan 02-07: nearestSpeaker3DFallback is comment-deprecated (no attribute, F9), removal at next major; OSD follow-ups (glossary 127-128, release notes for 4 audible changes, delete OSD's local SH/VBAP copies) recorded in 02-07-SUMMARY, not performed
- [Phase 02]: 02-08: pair-pan wedge encoded in existing VBAPTriplet fields (nadirVertex>=0, nadirMask==0, nadirGain==0); no public member added
- [Phase 02]: 02-08: 5.1.4 rear gap (|az|>110) not special-cased; same wedge as other pairs, EAR difference documented and not pinned
- [Phase 02]: 02-09: 5.1.x wedge region depth (about -59 degrees) is a geometric derivation tan(el)=tan(-30)/cos(D/2), not a 02-08 measurement; WR-01 closed by scoping #22 to above the horizon
- [Phase 02]: 02-10: [ambi-pin] bound is kAmbiPinTolerance = 2.5e-5 (derived from conditioning, 2.3x below the +0.1% epsilon failure), anchored by a double-precision decode at 4.0e-5; library and reference untouched
- [Phase 02]: 02-10: Release gate runs ctest on build-release/tests, because ctest from the build root finds no tests and exits 0 (CI has the same blind spot, deferred to Phase 6)

### Pending Todos

- ~~File issues for the two untracked defects~~ **Done 2026-08-10:** SpatialCore#18 (TrajectoryEngine cross-thread position reads) and SpatialCore#19 (second audio-thread allocation in `BinauralRenderer::updateSourceHRIR`). Both labelled `bug`, both owned by Phase 5.

### Blockers/Concerns

- **OQ-1** — algorithm count. **CLOSED: 8**, verified from the tree.
- **OQ-2** — `outputLimiter()`. **CLOSED 2026-08-10: keep the shipped tanh soft ceiling**; amend the SPEC to match. OSD ships v1.0.0 with that curve, so it is the known sound. Hard-clamp mode deferred to v2 as LIMIT-01.
- **OQ-3** — HRTF profile count. **CLOSED: 5**, verified from the tree.
- **OQ-4** (Phase 6) — test coverage target. **Blocked, not merely undefined**: no coverage tooling exists in the build, so there is nothing to measure against. Wire up instrumentation, read the real number, then set the target. The old "~5%" figure was never measured.
- **OQ-5** (not a v1 phase) — org repo migration pending; `docs/integration-guide.md` publishes a submodule URL that does not resolve.
- **OQ-6** (Phase 3) — HRTF packaging. **CLOSED 2026-08-10: lookup chain now, embedded default for v1.** Target architecture is the user's — profiles on disk once per machine in a shared folder, all SML plugins reference it; better than embedding chiefly because it enables user-supplied SOFA files. But v1 still embeds all 5, because the shared folder needs a signed installer that does not exist and OSD ships drag-and-drop. Phase 3 builds the resolution chain (shared folder → embedded → loud error) behind `SPATIALCORE_EMBED_ALL_HRTF=ON`; flipping it OFF later is a build flag, not a redesign. Tracked as SUITE-01.

Also open: v1 cannot close without OpenSpatialDelay-side work (REQ-osd-consume-spatialcore-submodule) that has no SpatialCore phase by design.

## Deferred Items

| Category | Item | Status | Deferred At |
|----------|------|--------|-------------|
| *(none)* | | | |

## Session Continuity

Last session: 2026-10-03T21:55:19.174Z
Stopped at: Completed 02-10-PLAN.md
Resume file: None
Next command: `/gsd-execute-phase 1`

**Scope grew during Phase 1 planning — three findings worth carrying forward:**

1. **BUG-03 is ~10× its filed size.** REQUIREMENTS.md named 4 sites; RESEARCH.md corrected that to
   19; the planner's repo-wide sweep found **39 occurrences across 38 lines in 17 files, spanning 15
   distinct issue numbers**, including `tests/` which earlier sweeps never scanned. Material: **`#2`
   resolves in BOTH trackers to different issues** — `Spatial-Media-Lab/OpenSpatialDelay#2` is "User
   Presets folder missing after build", `AndrewRahman/SpatialCore#2` is "[OpenSpatialPanner] Preset
   system" (OPEN). SpatialCore's tracker runs to #19, so every low-numbered bare citation is already
   ambiguous and that one already points at the wrong issue. BUG-03 is not cosmetic.

2. **`docs/integration-guide.md:164` says "7 spatialization algorithms"** — a stale count that
   appeared in no prior fix table. Folded into plan 01-03.

3. **Two items are already partly done:** `docs/development-roadmap.md:3` already carries a
   supersession note, and `CLAUDE.md`'s `Engine/` row (`:20`) plus its HRTF-loading description
   (`:74`) were already fixed by `2e3b090` / `87cb7a3`. Plans verify these rather than redo them,
   and plan 01-05 corrects the requirement text that still lists them as outstanding.
