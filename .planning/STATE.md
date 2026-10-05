---
gsd_state_version: "1.0"
milestone: v1.0.0
current_phase: 04
current_phase_name: Control Surface & UI
status: verifying
stopped_at: Completed 04-08-PLAN.md
last_updated: "2026-10-05T10:00:17.246Z"
last_activity: 2026-10-05
last_activity_desc: Phase 04 execution started
state_head: c89aeb6de1a3eaad0861e647931118d03195a1bb
progress:
  total_phases: 6
  completed_phases: 3
  total_plans: 34
  completed_plans: 34
milestone_name: milestone
---

# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-10-04)

**Core value:** A Spatial Media Lab plugin author gets production-grade spatial rendering by linking one library, so the only audio code they write is their own effect.
**Milestone:** v1 — OpenSpatialDelay ships on SpatialCore as a submodule with zero regressions
**Current focus:** Phase 04 — Control Surface & UI

## Current Position

Phase: 04 (Control Surface & UI) — EXECUTING
Plan: 8 of 8
Status: Phase complete — ready for verification
Last activity: 2026-10-05 — Phase 04 execution started

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

Progress: [█████░░░░░] 50%

## Performance Metrics

**Velocity:**

- Total plans completed: 26
- Average duration: —
- Total execution time: —

**By Phase:**

| Phase | Plans | Total | Avg/Plan |
|-------|-------|-------|----------|
| 01 | 5 | - | - |
| 02 | 10 | - | - |
| 3 | 11 | - | - |

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
| Phase 03 P01 | 12 min | 3 tasks | 8 files |
| Phase 03 P02 | 6 min | 2 tasks | 6 files |
| Phase 03 P03 | 8 min | 3 tasks | 6 files |
| Phase 03 P04 | 11 min | 3 tasks | 5 files |
| Phase 03 P05 | 12min | 3 tasks | 8 files |
| Phase 03 P06 | 36 min | 2 tasks | 4 files |
| Phase 03 P07 | 31 min | 2 tasks | 1 files |
| Phase 03 P08 | 39 min | 3 tasks | 2 files |
| Phase 03 P09 | 31 min | 2 tasks | 3 files |
| Phase 03 P10 | 13 min | 3 tasks | 8 files |
| Phase 03 P11 | 60 min | 3 tasks | 3 files |
| Phase 04 P01 | 16 min | 2 tasks | 3 files |
| Phase 04 P02 | 18 min | 2 tasks | 3 files |
| Phase 04 P03 | 14 min | 3 tasks | 8 files |
| Phase 04 P04-04 | 8 min | 2 tasks | 2 files |
| Phase 04 P05 | 14 min | 2 tasks | 5 files |
| Phase 04 P06 | 10 min | 2 tasks | 6 files |
| Phase 04 P07 | 21 min | 2 tasks | 5 files |
| Phase 04 P08 | 16 min | 2 tasks | 7 files |

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
- [Phase 02]: Below-horizon VBAP on height layouts is ITU-R BS.2127 (EAR): n nadir caps plus n pair-pan wedges, ear-level gaps of 179 degrees or more bridged (WR-05), tiers named by `VBAPTriplet::kind()` (WR-02). The coplanar-quad tie above the horizon stays open as SpatialCore#22
- [Phase 02]: D-06 amended 2026-10-04: engine hold-last-good sanitiser per field (az/el/distance); VBAP falls back to the largest-min-gain triplet, then the nearest speaker at unity for hand-built partial triplet lists (WR-03); no asserts in `computeGains` (IN-14); DBAP non-finite distance -> 0.5, finite clamp +-1000 (WR-08)
- [Phase 02]: VBIP is textbook single-band (dual-band SpatialCore#20); one SH evaluator with SN3D orders 4-6 fixed and one decoder; AmbiX convention in code and docs; `getDecodeMatrix` returns `bool` (IN-03). Offline oracles live in `tests/reference/` and never run in CI
- [Phase 02]: fast-math is a build error for SpatialCore sources (`src/Core/FloatSemanticsGuard.h`, WR-04), because the non-finite guards are `std::isfinite` tests
- [Phase 02]: Public API changes are additive only (`VBAPTriplet::kind()`, `getDecodeMatrix` -> `bool`), so the next release is a minor bump; `nearestSpeaker3DFallback` is comment-deprecated, removal at next major. Full log: PROJECT.md Key Decisions (P2-*) and the 02-*-SUMMARY files
- [Phase 03]: 03-01: convolver oracle signals use IR amplitude 0.05 so output scale matches the research measurement behind the unchanged 2e-6 bound
- [Phase 03]: 03-01: ITD line characterised not fixed (D-16): 20-row table pinned, wrap mechanism holds on SADIE and KEMAR at 44.1/48 kHz, Debug equals Release; D-14 loudness recorded (spread 2.48 LU), no tolerance asserted
- [Phase 03]: 03-02: Binaural goldens compare a tolerance fingerprint (length/peak index exact, delays 1e-4 samples, energy/peak 1e-5 relative) instead of an FNV hash; values captured from the tree, Debug and Release agree
- [Phase 03]: 03-03: convolver warm-up ends at max(1 call, irLen samples); fade is max(4 x call size at fade start, 2048) samples, fixed once and advanced by elapsed samples
- [Phase 03]: 03-03: BinauralRenderer scratch is grow-only max(block, 512, IR length), sized in prepare() and setProfile(); the RTSF-01 audio-thread resize guard is left for Phase 5
- [Phase 03]: 03-04: Simple-path Down cue dip tuned -6 to -5.5 dB (research value measured -5.78 dB at 5.5 kHz, outside the -5.2 +- 0.5 design check; now -5.28 dB)
- [Phase 03]: 03-04: non-finite mono input is fed to the Simple cue filters as 0 (output unchanged) so one bad sample cannot poison recursive state
- [Phase 03]: 03-05: shared-file resolver tests run in both embedding modes through a probe profile (3 in ON, 5 in KEMAR-only) — Keeps every D-08/D-09/D-11/D-18 rule exercised in the OFF build instead of skipped
- [Phase 03]: 03-05: SpatialCoreHRTFData linked PRIVATE into SpatialCore; SPATIALCORE_EMBEDS_ALL_HRTF PUBLIC 0/1; no #if in the embedded lookup — getNamedResource returning nullptr is the not-embedded signal
- [Phase 03]: 03-06: the loader decides skip-or-load from its own belief of what is playing (mailbox outcome plus per-renderer meta), never from an audio-thread value — A claim landing between reading active and acting on it would mislabel the active profile
- [Phase 03]: 03-06: crossfade abort and prepare() crossfade reset only run after the first setHRTFProfile call — Keeps legacy escape-hatch consumers byte-for-byte unchanged
- [Phase 03]: 03-07: [hrtf-switch][churn] added because the prescribed [threads] scenario lets only the last switch reach the audio thread; churn makes all 16 switches complete under a paced render thread
- [Phase 03]: 03-07: ThreadSanitizer found no Phase 3 race; the Phase 5 TSAN gate must build with optimization (RelWithDebInfo) because an -O0 instrumented SADIE load exceeds the engine's 15 s stopThread
- [Phase 03]: 03-08: user chose keep-upgrade (2026-10-04); SpatialCore pins libmysofa v1.3.5, 0 of 140 pinned values moved, nothing re-baselined (D-17)
- [Phase 03]: 03-08: mysofa_export.h include folder taken from mysofa-static BINARY_DIR; OSD still pins its own libmysofa v1.3.2 (follow-up in OSD repo)
- [Phase 03]: Renderer crossfade is counted in elapsed samples: max (8 x block at fade start, 4096), fixed when the fade starts; identical to the old fade at 512-sample blocks and above — Below 512-sample blocks the old block-counted fade was shorter than the HRIRs it blends. The step and dip bounds already held at the plan base (03-03 sample-based convolver), so the length is what the tests pin.
- [Phase 03]: RenderBlockContext::engineSelectsHRTF (default false, D-15): the engine claims a ready profile, derives useHRTF from its active renderer and blends the Woodworth and HRTF paths during a Simple <-> HRTF fade; flag-off and no-switch output is bit-identical — A consumer-derived useHRTF can read a different renderer than the engine claims (the SC-16 tear class). Plain paths call exactly the flag-off dispatch so existing consumers are unchanged; the blend was rebuilt outside the engine and matched to 0 deviation, with once-per-block Woodworth proven by mutation.
- [Phase 03]: 03-10: spatial-audio-dsp skill profile table renumbered to match kHRTFProfiles (SADIE 1 ... KEMAR 5); describeHRTFProfileStatus documented as a free function; Windows shared-folder path documented as implemented but unverified
- [Phase 03]: 03-11: OSD#234 comment (A) and SpatialCore#15 close (C) approved but deferred until Phase 3 is pushed to main; B filed as SpatialCore#25; D loudness bounds pinned (0.5 LU, spread 2.98 LU)
- [Phase 03]: Code review --fix (2026-10-05), 3 fix rounds to clean, ledger 14/14 fixed: SOFA files are pre-validated (declared rate, R, M, resampled N, 2^27-float budget) before libmysofa resamples; shared files are read through one fd with fstat S_ISREG and a streamed byte cap; a throwing load settles as Failed/LoadFailed; engineSelectsHRTF now fills objGains itself on binaural blocks (overwrites consumer objGains there); WR-05 64-sample ITD wrap documented only, still SpatialCore#25
- [Phase 04]: 04-01: JUCE_MODAL_LOOPS_PERMITTED=1 scoped to SpatialCoreTests only so a test can pump the message loop for MessageLoopCallback OSC delivery; ADM-OSC route proven by rendered channel level (Binaural Simple path L/R 3.31 at +90, 0.30 at -90; Quad channel select) through RouteRenderRig with both engine flags set
- [Phase 04]: 04-02: ADMOSCSender keeps its own 30 Hz schedule (tick with nowSeconds, stall resync, first/connect/re-enable send once, silent while still); the five-argument tick OSD calls delegates to it on the hi-res wall clock. Measured 300 messages per 10 s at 120/60/50 Hz callers, 299 at 30 Hz with jitter
- [Phase 04]: 04-03: query replies go to the sender's configured host:port, never the packet source (D-20); disconnect() clears pending replies
- [Phase 04]: 04-03: ADMOSCReceiver reads an argument only after its OSC type is known; NaN/inf drop the whole message, azimuth wrapped once, elevation/distance/partial axes clamped (D-21)
- [Phase 04]: 04-04: no trajectory library change; ten shapes retrace forward exactly (max error 0.0), Bounce and Line reverse pinned as intentional OSD#100 exceptions (D-19)
- [Phase 04]: 04-05: SpatialCoreUITests carries JUCE_UNIT_TESTS only (no modal loops); a Catch2 listener owns the GUI initialiser; the ui: ctest prefix drops its trailing space, so gates grep 'ui:'
- [Phase 04]: 04-06: SpatialMapComponent owns embedded JetBrains Mono typefaces and PresetSaveOverlay an embedded DM Sans Bold title (D-05, D-06, D-22); DATA-02 proven by provenance (spy, bytes, code rule)
- [Phase 04]: 04-07: SpatialCoreDemo is off by default (SPATIALCORE_BUILD_EXAMPLES, top-level only); it keeps TrajectoryEngine off the audio thread by copying output into atomics on the message thread and sets TrajectoryState::reverse itself; selftest proves osc, query, trajectory and map through RenderEngine
- [Phase 04]: 04-08: gate listing check greps 'ui:' (CTest drops the space after the ui: prefix); README documents it
- [Phase 04]: 04-08: EXTR-04 ticked on all-automated evidence; EXTR-05 (D-03 screenshots) and DATA-02 (D-22 title) stay unticked, pending human approval at the Phase 4 end-of-phase review

### Pending Todos

- ~~File issues for the two untracked defects~~ **Done 2026-08-10:** SpatialCore#18 (TrajectoryEngine cross-thread position reads) and SpatialCore#19 (second audio-thread allocation in `BinauralRenderer::updateSourceHRIR`). Both labelled `bug`, both owned by Phase 5.

### Blockers/Concerns

- **OQ-1** — algorithm count. **CLOSED: 8**, verified from the tree.
- **OQ-2** — `outputLimiter()`. **CLOSED 2026-08-10: keep the shipped tanh soft ceiling**; amend the SPEC to match. OSD ships v1.0.0 with that curve, so it is the known sound. Hard-clamp mode deferred to v2 as LIMIT-01.
- **OQ-3** — HRTF profile count. **CLOSED: 5**, verified from the tree.
- **OQ-4** (Phase 6) — test coverage target. **Blocked, not merely undefined**: no coverage tooling exists in the build, so there is nothing to measure against. Wire up instrumentation, read the real number, then set the target. The old "~5%" figure was never measured.
- **OQ-5** (not a v1 phase) — org repo migration pending; `docs/integration-guide.md` publishes a submodule URL that does not resolve.
- **OQ-6** (Phase 3) — HRTF packaging. **CLOSED 2026-08-10: lookup chain now, embedded default for v1.** Target architecture is the user's — profiles on disk once per machine in a shared folder, all SML plugins reference it; better than embedding chiefly because it enables user-supplied SOFA files. But v1 still embeds all 5, because the shared folder needs a signed installer that does not exist and OSD ships drag-and-drop. Phase 3 builds the resolution chain (shared folder → embedded → loud error) behind `SPATIALCORE_EMBED_ALL_HRTF=ON`; flipping it OFF later is a build flag, not a redesign. Tracked as SUITE-01.

- **[Phase 2] Branch sync before Phase 3: done 2026-10-04.** `origin/main` (the LFE fixes `3a0d912` + `5140c9a` via `4bed89a`, and SC-16 `b9877e1`..`ab60c25`) was merged into `gsd-remap` as `6055109` and pushed as `main`. Debug 208/209 (only HutubsPP2Tests.cpp:47), Release 209/209, OSD 30391cd builds.
- **[Phase 2] CI "green" is test-vacuous.** CI passed at `879a8fe`, but its ctest step runs from the build root and finds zero tests (no top-level `enable_testing()`); there is also no arm64/FMA leg. Phase gates use `ctest` in `build-release/tests`. Owned by Phase 6.
- **[Phase 3] `HutubsPP2Tests.cpp:47`** — RESOLVED in Phase 3 (03-02 tolerance fingerprints); Debug and Release 266/266.
- **[Phase 3] BUG-01** — DirectBinaural elevation and front/back cues are deliberately unasserted in `PanningLawTests.cpp`; Phase 3 adds them.
- **[Phase 2] Listening checks pending** — ROADMAP backlog 999.1 (OSD 7.1.4 below-horizon hold) and 999.2 (WR-05 gap bridge, WR-08 DBAP broken distance). Need ears, not tests.
- **[Phase 2] OSD-side follow-ups** — release notes for the audible changes, four OSD docs describing VBIP as squared gains, delete OSD's local SH/VBAP copies at migration (`02-09-SUMMARY.md`). Tracked in PROJECT.md External Dependencies.

Also open: v1 cannot close without OpenSpatialDelay-side work (REQ-osd-consume-spatialcore-submodule) that has no SpatialCore phase by design.

## Deferred Items

| Category | Item | Status | Deferred At |
|----------|------|--------|-------------|
| Binaural | `SharedFFTCache` static outlives JUCE's leak detector: "Leaked objects detected: 1 FFT" at test exit (false positive) | Open — Phase 3 or 5 | Phase 02 (02-04) |
| Binaural | HUTUBS PP2 golden checksum fails in Debug only | Resolved — Phase 3 (03-02) | Phase 02 (02-10) |
| Tests | Other tight test-local tolerances may fail on another compiler/build type (none fails today) | Open | Phase 02 (02-10) |
| CI | ctest step runs zero tests; no arm64/FMA Release leg | Open — Phase 6 | Phase 02 (02-10) |
| Docs | spatial-audio-dsp skill §4 virtual 16-speaker array and §1.3 "Delaunay" wording describe code that does not exist | Open — next docs pass | Phase 02 (02-07) |
| Build | `ADMOSCReceiver.h` five `-Wunused-parameter` warnings in OSD builds | Resolved — Phase 4 (04-03) | Phase 02 (02-07) |
| Tests | Milestone audit: no test sets all three engine flags on one binaural block; Binaural→non-binaural switch during an HRTF renderer crossfade is untested | Open — Phase 5 or 6 | v1.0.0 audit |
| Planning | 01-VALIDATION.md and 02-VALIDATION.md still `status: draft` (optional `/gsd-validate-phase 1`, `2`) | Open | v1.0.0 audit |
| Binaural | libmysofa's own HDF5 parse allocates from declared dimensions before SpatialCore's pre-validation; bounded by the 256 MB file cap except zlib inflation | Accepted residual | Phase 03 (review) |
| API | `setLoaderFailureForTesting` is public in `RenderEngine.h` (marked TEST-ONLY; a compile guard would make the class layout differ between SpatialCore and consumers) | Accepted | Phase 03 (review) |

## Session Continuity

Last session: 2026-10-05T10:00:17.180Z
Stopped at: Completed 04-08-PLAN.md
Resume file: None
Next command: `/gsd-discuss-phase 4`
