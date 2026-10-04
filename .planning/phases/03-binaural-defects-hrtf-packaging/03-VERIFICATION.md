---
phase: 03-binaural-defects-hrtf-packaging
verified: 2026-10-04T20:16:21Z
status: passed
score: 5/5 roadmap success criteria verified (4/4 requirement IDs satisfied)
covered_files:
  - .claude/skills/spatial-audio-dsp/SKILL.md
  - .claude/skills/spatialcore-architecture/spatialcore-architecture.md
  - .planning/phases/03-binaural-defects-hrtf-packaging/03-01-PLAN.md
  - .planning/phases/03-binaural-defects-hrtf-packaging/03-01-SUMMARY.md
  - .planning/phases/03-binaural-defects-hrtf-packaging/03-02-PLAN.md
  - .planning/phases/03-binaural-defects-hrtf-packaging/03-02-SUMMARY.md
  - .planning/phases/03-binaural-defects-hrtf-packaging/03-03-PLAN.md
  - .planning/phases/03-binaural-defects-hrtf-packaging/03-03-SUMMARY.md
  - .planning/phases/03-binaural-defects-hrtf-packaging/03-04-PLAN.md
  - .planning/phases/03-binaural-defects-hrtf-packaging/03-04-SUMMARY.md
  - .planning/phases/03-binaural-defects-hrtf-packaging/03-05-PLAN.md
  - .planning/phases/03-binaural-defects-hrtf-packaging/03-05-SUMMARY.md
  - .planning/phases/03-binaural-defects-hrtf-packaging/03-06-PLAN.md
  - .planning/phases/03-binaural-defects-hrtf-packaging/03-06-SUMMARY.md
  - .planning/phases/03-binaural-defects-hrtf-packaging/03-07-PLAN.md
  - .planning/phases/03-binaural-defects-hrtf-packaging/03-07-SUMMARY.md
  - .planning/phases/03-binaural-defects-hrtf-packaging/03-08-PLAN.md
  - .planning/phases/03-binaural-defects-hrtf-packaging/03-08-SUMMARY.md
  - .planning/phases/03-binaural-defects-hrtf-packaging/03-09-PLAN.md
  - .planning/phases/03-binaural-defects-hrtf-packaging/03-09-SUMMARY.md
  - .planning/phases/03-binaural-defects-hrtf-packaging/03-10-PLAN.md
  - .planning/phases/03-binaural-defects-hrtf-packaging/03-10-SUMMARY.md
  - .planning/phases/03-binaural-defects-hrtf-packaging/03-11-PLAN.md
  - .planning/phases/03-binaural-defects-hrtf-packaging/03-11-SUMMARY.md
  - CLAUDE.md
  - CMakeLists.txt
  - README.md
  - docs/development-roadmap.md
  - docs/integration-guide.md
  - include/SpatialCore/Binaural/BinauralRenderer.h
  - include/SpatialCore/Binaural/HRTFDatabase.h
  - include/SpatialCore/Binaural/HRTFProfile.h
  - include/SpatialCore/Binaural/HRTFProfileResolver.h
  - include/SpatialCore/Binaural/PartitionedConvolver.h
  - include/SpatialCore/Core/SimpleBinauralCues.h
  - include/SpatialCore/Engine/RenderEngine.h
  - include/SpatialCore/SpatialCore.h
  - src/Binaural/BinauralRenderer.cpp
  - src/Binaural/EmbeddedHRTF.cpp
  - src/Binaural/HRTFDatabase.cpp
  - src/Binaural/HRTFProfileResolver.cpp
  - src/Binaural/PartitionedConvolver.cpp
  - src/Engine/RenderEngine.cpp
  - tests/Algorithms/PanningLawTests.cpp
  - tests/Binaural/BernschuetzKU100Tests.cpp
  - tests/Binaural/BinauralCueTests.cpp
  - tests/Binaural/BinauralMetrics.h
  - tests/Binaural/BinauralRendererTests.cpp
  - tests/Binaural/BinauralTestUtilities.h
  - tests/Binaural/CipicSubject003Tests.cpp
  - tests/Binaural/HRTFEmbeddedTests.cpp
  - tests/Binaural/HutubsPP2Tests.cpp
  - tests/Binaural/MitKemarLargePinnaTests.cpp
  - tests/Binaural/PartitionedConvolverTests.cpp
  - tests/Binaural/ProfileLoudnessTests.cpp
  - tests/Binaural/SadieD2KU100Tests.cpp
  - tests/CMakeLists.txt
  - tests/Engine/ProfileSwitchTests.cpp
covered_digest: "v2:sha256:88b66e28ef0ea8a42fa031b1db5f27408e8cc26f262c71927ab2fa768c507129"
behavior_unverified: 0
overrides_applied: 0
gaps: []
deferred: []
advisory:
  - finding: "Review WR-01..WR-07 and IN-01..IN-07 (14 items) are all still `open` in 03-REVIEW-DISPOSITION.md. None defeats a roadmap success criterion; they are hardening, documentation and contract items."
    category: other
    reason: "See the Advisory table below. WR-05 (64-sample ITD wrap) is the one with an audible-class effect; it is pre-existing extracted behaviour, deliberately pinned (D-16) and tracked as AndrewRahman/SpatialCore#25."
    evidence_status: "03-REVIEW.md read in full; WR-06 re-checked against src/Engine/RenderEngine.cpp:656-674 and still holds"
  - finding: "The SPATIALCORE_EMBED_ALL_HRTF=OFF (KEMAR-only) build was evidenced only by the 03-05 SUMMARY (a /tmp tree that no longer exists). This verification re-ran the ON build only."
    category: other
    reason: "OFF is the future-installer path (SUITE-01), not a Phase 3 success criterion. Both the default build trees here have the option ON."
    evidence_status: "SUMMARY claim only; not re-run"
human_verification:
  - test: "Say when the two public GitHub messages you already approved should go out. (1) A comment on OpenSpatialDelay issue 234 saying SpatialCore's side of the small-buffer problem is now tested clean and that the issue stays open. (2) A comment closing SpatialCore issue 15 as fixed. Neither has been posted: I checked GitHub and issue 234 has 0 comments and is open, issue 15 is open."
    expected: "You approved both texts as written on 2026-10-04 and asked to hold them until Phase 3 is on `main`. Once you have pushed, they can be posted from the saved text with the commands recorded in 03-11-OUTWARD.md under 'Pending after push'. Nothing needs judging, only a go-ahead on timing."
    why_human: "These are public posts under your name on repositories others read. You deferred them on purpose, so no automated check should send them. Phase 3 is 57 commits ahead of origin and not yet on `main`."
---

# Phase 3: Binaural Defects & HRTF Packaging Verification Report

**Phase Goal:** The binaural path renders correct spatial cues, and a consumer gets HRTF data by linking rather than by hand-copying a directory.
**Verified:** 2026-10-04T20:16:21Z
**Status:** human_needed
**Re-verification:** No, initial verification (no previous 03-VERIFICATION.md)

## Verdict

The phase goal is achieved in the code. All five roadmap success criteria are backed by tests I re-ran against binaries that are newer than every source file (no source or test file is newer than `build-release/tests/SpatialCoreTests`). Status is `human_needed` only because the two outward GitHub actions you approved are still unposted drafts waiting on the push to `main`. No gap, no blocker.

Starting hypothesis was "tasks done, goal missed". It did not hold: the cue bank is called from the Simple path, the resolver is called from the engine's worker thread, the embedded data is linked into the library, and the tests measure what the engine outputs, not what a helper returns.

## Goal Achievement

### Observable Truths (ROADMAP success criteria)

| # | Truth | Status | Evidence |
|---|-------|--------|----------|
| 1 | A source at elevation +90 is measurably distinguishable from one at 0, and azimuth 0 from 180 (SpatialCore#15) | VERIFIED | `tests/Binaural/BinauralCueTests.cpp` measures the engine's own impulse response (`engineImpulseResponse`, third-octave band difference, bounds 2.0 dB RMS and 4.0 dB max, set before measuring). `[bug01]` re-run Release: 5 cases, 99 assertions pass. Covers the Simple path and all five HRTF profiles, plus `[identity]` (ear-level front half bit-identical to the old formula), `[weights]`, `[cold-start]`. Code: `RenderEngine::renderSimpleBinauralWoodworth` (`src/Engine/RenderEngine.cpp:1030`) runs the rear/up/down cue bank (`Core/SimpleBinauralCues.h`) before the pan gains; design in `prepare()` at `:480`. Figures from 03-11-OUTWARD.md: Simple front vs back 4.49 / 11.59 dB (was 0.00), front vs overhead 2.59 / 7.41 dB; HRTF profiles 3.39-6.30 RMS front vs back, 3.29-4.59 front vs overhead. |
| 2 | Rendering at 32, 64 and 128 sample blocks produces no artifacts (OSD#234) | VERIFIED | `PartitionedConvolver.cpp:233-240` times warm-up and crossfade in samples (warm-up at least irLen, fade at least `kMinCrossfadeSamples` = 2048). `[convolver][blocksize]` matches a double-precision direct convolution at block plans 32/64/128/512/37 and mixed; `[bug02][moving]` moving-source step at 32/64/128 within 1.2x of the 512 value; `[bug02][steady]` engine output vs 512 reference at small and irregular plans. Re-run Release: `[convolver]` 3 cases / 27 assertions, `[bug02]` 2 cases / 45 assertions pass. The comment at the old `PartitionedConvolver.cpp:120-124` site is no longer the live defect. |
| 3 | A freshly cloned consumer links SpatialCore and renders through any of the 5 profiles with no install step and no path configuration; a SOFA file in the shared platform folder is picked up ahead of the embedded copy; a missing folder falls back silently to embedded | VERIFIED | `CMakeLists.txt:62-104` embeds all five `HRTF/*.sofa` as `SpatialCoreHRTFData`, linked PRIVATE into `SpatialCore` (`:195-200`), with a configure-time Git-LFS-pointer guard (`:89-97`). `HRTFDatabase::loadFromBinaryData` reads it via `src/Binaural/EmbeddedHRTF.cpp`. `resolveHRTFProfile` (`src/Binaural/HRTFProfileResolver.cpp`) goes shared folder, embedded copy, reported error; the engine's worker calls it (`RenderEngine.cpp:216`) and the default folder is `/Library/Application Support/Spatial Media Lab/HRTF` (`:285-286`). Tests (re-run Release): `[hrtf-embed]` 4 cases (embedded bytes equal the source file; each profile loads), `[hrtf-resolve]` 10 cases / 759 assertions: no shared folder gives every profile `Embedded` with no problem and non-silent output, a same-name shared file wins and nothing else does, missing/empty/not-a-directory/empty-path all fall back silently with `problem == None`, broken shared file falls back and says so, folder re-checked on every call (D-11). The machine here has no shared folder (`ls` confirms), so the default path is the embedded one. Caveat: no external consumer harness exists yet (INTG-01 is Phase 6); "freshly cloned" is proven through the in-tree test binary, which links the library the way a consumer does. |
| 4 | Switching HRTF profile while audio is running produces no click, pop or dropout | VERIFIED | Engine-owned switch: `setHRTFProfile` (`RenderEngine.cpp:296`) wakes the loader thread, which resolves into the renderer the audio thread is not using and hands it over through a one-slot atomic mailbox (`readyRenderer_` exchange at `:386`, store at `:243`), then a sample-based crossfade of `max(8 blocks, 4096 samples)` (`:770`). `[hrtf-switch]` Release: 19 cases / 327 assertions pass, including `[click]` at 32/64/128/512 (sine step ratio at most 1.5, pink-noise RMS dip at least 0.7), Simple to/from HRTF with `engineSelectsHRTF`, concurrent render thread plus switching (finite, no deadlock), prepare() during a load, a crossfade abandoned by a path change frees its renderer, and engine destruction mid-load returns promptly. The cancellation/ordering parts of this truth are exercised by named tests, so none is left presence-only. |
| 5 | `PartitionedConvolver` and `BinauralRenderer` have dedicated test files | VERIFIED | `tests/Binaural/PartitionedConvolverTests.cpp` (418 lines, 5 cases) and `tests/Binaural/BinauralRendererTests.cpp` (420 lines, 6 cases) exist, are listed in `tests/CMakeLists.txt:29-30`, and their tags pass (`[convolver]`, `[renderer]` 6 cases / 280 assertions). Not stubs: the convolver is compared to a double-precision reference, the renderer cases cover Simple-mode silence, 1/sqrt(irLen) scaling, the KEMAR shelf, reset/invalidate, ITD characterisation and off-thread KEMAR prepare. |

**Score:** 5/5 truths verified, 0 present-but-behavior-unverified.

### Deferred Items

None. Nothing in the later phases' goals covers a failed item here, and no item failed.

### Advisory (New Scope, Unevidenced)

None from this verification beyond the review items listed next.

### Known Advisory Items (code review, all 14 still open)

Per the brief, factored in only where one defeats a success criterion. None does.

| ID | Summary | Touches a success criterion? |
|----|---------|------------------------------|
| WR-01 | Decoded IR length not bounded; a file under the 256 MB cap could force a huge allocation (arithmetic estimate, not measured) | No. SC3 still holds for real files; this is hardening against a hostile file in the shared folder (T-03-09) |
| WR-02 | Loader thread has no try/catch; a throwing load leaves status `Loading` forever | No. SC4 is about normal switches; this is a failure path |
| WR-03 | Size cap is stat-then-read, so FIFOs/symlinks to special files bypass it | No. Needs write access to the shared folder |
| WR-04 | `prepare()` now joins and restarts the loader thread (can block up to 15 s) and is not documented or synchronised with `setHRTFProfile` | No. Contract/documentation gap |
| WR-05 | 64-sample ITD line wraps delays of 64 samples or more; tests pin the defect | Not a roadmap criterion. It is long-standing extracted behaviour, kept on purpose so 44.1/48 kHz sound does not change (D-16), filed as AndrewRahman/SpatialCore#25 (open) and cited from the pinning test. It is the one item that bears on "correct spatial cues" in the broad goal wording, hence worth a deliberate decision in a later phase |
| WR-06 | `engineSelectsHRTF` without `engineComputesGains` renders silence on the Simple path (confirmed still true: gains are only computed under `engineComputesGains`, `RenderEngine.cpp:656-674`) | No. Documented as "set together"; a consumer foot-gun |
| WR-07 | Flag-off consumers hear changes (Simple cue bank, longer warm-up, longer renderer fade) | No. The cue bank change is the BUG-01 fix itself and is documented; the other two are missing from the integration guide's migration note |
| IN-01 to IN-07 | Doc figures (36 MB vs 256 MB cap), an over-strong "no lock or allocation" sentence, a 2 ms idle poll, a stale handle after read failure, a stale libmysofa version in a test comment, a 2.6 vs 2.7 dB figure, test/CMake edge cases | No |

### Required Artifacts

`verify.artifacts` against all 11 PLAN files: every declared artifact passes (exists, substantive). Highlights re-read by hand:

| Artifact | Expected | Status | Details |
|----------|----------|--------|---------|
| `tests/Binaural/PartitionedConvolverTests.cpp` | Dedicated convolver tests | VERIFIED | 418 lines, in CMake list, passing |
| `tests/Binaural/BinauralRendererTests.cpp` | Dedicated renderer tests | VERIFIED | 420 lines, in CMake list, passing |
| `tests/Binaural/BinauralCueTests.cpp` | BUG-01 on all paths | VERIFIED | 330 lines, 5 cases |
| `tests/Binaural/HRTFEmbeddedTests.cpp` | Embed and resolve | VERIFIED | 581 lines, 14 cases |
| `tests/Engine/ProfileSwitchTests.cpp` | Switch, click, threads | VERIFIED | 1542 lines, 19 cases |
| `tests/Binaural/ProfileLoudnessTests.cpp` | Loudness record with tolerance | VERIFIED | Bounds pinned to approved values; 2 cases pass |
| `include/SpatialCore/Binaural/HRTFProfile.h` | Profile table, indices 0-5 | VERIFIED | Numbering matches CLAUDE.md and D-07 |
| `include/SpatialCore/Binaural/HRTFProfileResolver.h` + `src/Binaural/HRTFProfileResolver.cpp` | Resolution chain | VERIFIED | Real chain, not a stub |
| `src/Binaural/EmbeddedHRTF.cpp` | Embedded lookup | VERIFIED | `getNamedResource`, returns null for what was not embedded |
| `include/SpatialCore/Core/SimpleBinauralCues.h` | Cue bank design and weights | VERIFIED | Included by `SpatialCore.h` and `RenderEngine.cpp` |
| `CMakeLists.txt` | `SpatialCoreHRTFData`, `SPATIALCORE_EMBED_ALL_HRTF`, LFS guard, libmysofa v1.3.5 | VERIFIED | Lines 62-104, 195-212 |

### Key Link Verification

`verify.key-links` reported 11 of 24 links as "not verified", every one with the message "from: must be a relative file path", because those plans wrote component names rather than paths in `from`. That is a tooling-format mismatch, not a broken link. I checked the substantive links by hand:

| From | To | Via | Status | Details |
|------|----|-----|--------|---------|
| `setHRTFProfile` | loader thread | `startThread`, `notify` | WIRED | `RenderEngine.cpp:316-322` |
| loader thread | `resolveHRTFProfile` | direct call | WIRED | `RenderEngine.cpp:216` |
| loader thread | audio thread | `readyRenderer_` release store / acq-rel exchange | WIRED | `:243`, `:386`; no lock or file access on the audio side |
| `renderBlock` | `claimReadyRenderer` | both the `engineSelectsHRTF` branch and the legacy branch | WIRED | `:668`, `:683` |
| `renderSimpleBinauralWoodworth` | cue bank | `computeSimpleCueWeights`, `cueBiquadStep` | WIRED | `:1037-1087` |
| `PartitionedConvolver::process` | sample-timed warm-up and fade | `stateSampleCount`, `crossfadeLengthSamples` | WIRED | `PartitionedConvolver.cpp:233-240` |
| `SpatialCore` | `SpatialCoreHRTFData` | `target_link_libraries ... PRIVATE` | WIRED | `CMakeLists.txt:195-200`; test binary resolves embedded data without any path |
| `CMakeLists.txt` libmysofa pin | binaural golden tests | FetchContent | WIRED | `[binaural][golden]` pass under v1.3.5 (full suite 260/260, orchestrator) |

### Data-Flow Trace (Level 4)

| Artifact | Data variable | Source | Produces real data | Status |
|----------|---------------|--------|--------------------|--------|
| Embedded HRTF | `data`/`sizeInBytes` | `SpatialCoreHRTFData::getNamedResource` over the real `.sofa` bytes | Yes: `[hrtf-embed][bytes]` compares embedded bytes to the source file; IR length asserted per profile | FLOWING |
| Resolver output | `HRTFDatabase` contents | shared file or embedded bytes, parsed by libmysofa | Yes: `rendersNonSilentImpulse` per profile; shared file test sees the donor profile's IR length | FLOWING |
| Simple cue bank | filter outputs | `computeSimpleCueWeights` from sanitised source positions | Yes: IR differs from front by the measured dB values above | FLOWING |

### Behavioral Spot-Checks (single tags, Release binary, run by me)

| Behavior | Command | Result | Status |
|----------|---------|--------|--------|
| BUG-01 cues | `SpatialCoreTests "[bug01]"` | 99 assertions, 5 cases pass | PASS |
| BUG-02 small blocks | `"[bug02]"` | 45 assertions, 2 cases pass | PASS |
| Convolver | `"[convolver]"` | 27 assertions, 3 cases pass | PASS |
| Renderer | `"[renderer]"` | 280 assertions, 6 cases pass | PASS |
| Resolver | `"[hrtf-resolve]"` | 759 assertions, 10 cases pass | PASS |
| Embed | `"[hrtf-embed]"` | 145 assertions, 4 cases pass | PASS |
| Profile switching | `"[hrtf-switch]"` | 327 assertions, 19 cases pass (about 35 s) | PASS |
| Loudness | `"[loudness]"` | 26 assertions, 2 cases pass | PASS |

The full suite (260/260 Debug, 260/260 Release at HEAD 7f79716) was taken from the orchestrator and not re-run.

### Probe Execution

Step 7c: SKIPPED. No `scripts/*/tests/probe-*.sh` exists and no plan declares a probe.

### Requirements Coverage

All four phase requirement IDs appear in PLAN frontmatter and in REQUIREMENTS.md. No orphaned requirement maps to Phase 3.

| Requirement | Source plans | Description | Status | Evidence |
|-------------|--------------|-------------|--------|----------|
| EXTR-02 | 03-01, 03-02, 03-03, 03-08, 03-10, 03-11 | Binaural rendering is measured-correct; PartitionedConvolver and BinauralRenderer get dedicated tests | SATISFIED | SC5 above; goldens now tolerance fingerprints (HUTUBS Debug failure closed, 03-02); loudness pinned within 0.5 LU |
| DATA-01 | 03-05, 03-06, 03-07, 03-08, 03-09, 03-10 | HRTF lookup chain with an embedded set that cannot fail | SATISFIED | SC3 and SC4 above; `SPATIALCORE_EMBED_ALL_HRTF` option present, LFS guard present, `loadFromFile`/`loadFromMemory` kept |
| BUG-01 | 03-01, 03-04, 03-10, 03-11 | Elevation and front/back cues (SpatialCore#15) | SATISFIED | SC1 above |
| BUG-02 | 03-01, 03-03, 03-09, 03-10, 03-11 | No artifacts below 256-sample buffers (OSD#234) | SATISFIED | SC2 above; note the OSD issue itself stays open on purpose because OSD's own delay line and pitch shifter are unchecked |

Bookkeeping to do after this verification, not a gap: `.planning/REQUIREMENTS.md` still shows EXTR-02 and DATA-01 as `- [ ]` (BUG-01 and BUG-02 are already `[x]`), and the traceability rows for EXTR-02 and DATA-01 still read "Largely verified by tests" and "OQ-6 resolved". Plan 03-10 deliberately ticked nothing. They can be ticked now.

### Anti-Patterns Found

| File | Line | Pattern | Severity | Impact |
|------|------|---------|----------|--------|
| (phase diff, `15778d4^..HEAD`, src/include/tests/CMake) | - | `TBD`, `FIXME`, `XXX` in added lines | none found | 0 matches |
| (phase diff, src/include) | - | `TODO`, `HACK`, `PLACEHOLDER`, "not yet implemented" in added lines | none found | 0 matches |

No stubs, hollow props or hardcoded empties found on the paths this phase added. The known leak-detector message at process exit ("Leaked objects detected: FFT", exit code 0) comes from the process-global `SharedFFTCache` and is recorded as Phase 5 scope.

### Outward Items (03-11-OUTWARD.md)

Checked against GitHub, not against the SUMMARY.

| Item | Decision (2026-10-04) | Actual state |
|------|----------------------|--------------|
| A. Comment on OpenSpatialDelay#234 (issue stays open) | Approved, posting deferred to after the push | NOT POSTED. Issue is open with 0 comments. Draft only. |
| B. New SpatialCore issue for the ITD wrap | Approved, filed now | POSTED. AndrewRahman/SpatialCore#25, open, title matches, cited from the pinning test |
| C. Close SpatialCore#15 with a comment | Approved, posting deferred to after the push | NOT POSTED. Issue is open. Draft only. |
| D. Loudness tolerance in `[loudness]` | Approved | APPLIED in `tests/Binaural/ProfileLoudnessTests.cpp`, test passes |

A and C are the reason the status is `human_needed` rather than `passed`. They are approved text awaiting timing, not a defect in the phase.

### Human Verification Required

#### 1. Go-ahead to post the two held GitHub messages

**Test:** Tell the orchestrator when Phase 3 is on `main` and it is fine to post: the comment on OpenSpatialDelay issue 234, and the closing comment on SpatialCore issue 15.
**Expected:** Both go out exactly as the text you approved. Issue 234 stays open; issue 15 closes.
**Why human:** Public posts under your name that you chose to hold. The branch is 57 commits ahead of origin and not yet on `main`.

No listening test is requested. The roadmap's own note puts a headphone check of the cue shape on the backlog and says it is not a gate; every criterion here is a measured one and was measured by the tests above.

### Gaps Summary

No gaps. Every roadmap success criterion is met by code that exists, is wired, and passes tests that measure the engine's real output. The 14 open review findings are hardening and documentation work. The one worth a conscious decision later is WR-05 (the 64-sample ITD wrap, issue #25), because it is a pre-existing limit on cue timing for the SADIE profile and for every profile at 88.2 kHz and above; it does not affect the criteria written for this phase.

---

_Verified: 2026-10-04T20:16:21Z_
_Verifier: Claude (gsd-verifier)_

## Re-verification after UAT (2026-10-04)

- **Human item resolved:** the user gave the go-ahead in `/gsd-verify-work 3` (03-UAT.md, 1/1 pass).
  Section A posted to Spatial-Media-Lab/OpenSpatialDelay#234 (still OPEN); section C posted to
  AndrewRahman/SpatialCore#15 (CLOSED - auto-closed by bb8b3d1 on the push to main, so only the
  comment was added). Links are in 03-11-OUTWARD.md "Done".
- **Covered-file change since the first verification:** merge 3d8adcf brought in 6977d64 (OSP WR-09,
  a Debug-only single-writer guard and header note on `RenderEngine::setOutputFormat()`), touching
  `include/SpatialCore/Engine/RenderEngine.h` and `src/Engine/RenderEngine.cpp`. It does not touch any
  Phase 3 path. Release suite re-run on the merged tree: 260/260 pass. The digest was recomputed.
- Status moved from `human_needed` to `passed`.
