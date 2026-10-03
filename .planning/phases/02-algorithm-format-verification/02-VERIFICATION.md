---
phase: 02-algorithm-format-verification
verified: 2026-10-03T10:41:21Z
status: human_needed
score: 77/78 must-haves verified
covered_files:
  - .claude/skills/spatial-audio-dsp/SKILL.md
  - .planning/phases/02-algorithm-format-verification/02-01-PLAN.md
  - .planning/phases/02-algorithm-format-verification/02-01-SUMMARY.md
  - .planning/phases/02-algorithm-format-verification/02-02-PLAN.md
  - .planning/phases/02-algorithm-format-verification/02-02-SUMMARY.md
  - .planning/phases/02-algorithm-format-verification/02-03-PLAN.md
  - .planning/phases/02-algorithm-format-verification/02-03-SUMMARY.md
  - .planning/phases/02-algorithm-format-verification/02-04-PLAN.md
  - .planning/phases/02-algorithm-format-verification/02-04-SUMMARY.md
  - .planning/phases/02-algorithm-format-verification/02-05-PLAN.md
  - .planning/phases/02-algorithm-format-verification/02-05-SUMMARY.md
  - .planning/phases/02-algorithm-format-verification/02-06-PLAN.md
  - .planning/phases/02-algorithm-format-verification/02-06-SUMMARY.md
  - .planning/phases/02-algorithm-format-verification/02-07-PLAN.md
  - .planning/phases/02-algorithm-format-verification/02-07-SUMMARY.md
  - .planning/phases/02-algorithm-format-verification/02-08-PLAN.md
  - .planning/phases/02-algorithm-format-verification/02-08-SUMMARY.md
  - .planning/phases/02-algorithm-format-verification/02-09-PLAN.md
  - .planning/phases/02-algorithm-format-verification/02-09-SUMMARY.md
  - README.md
  - docs/integration-guide.md
  - include/SpatialCore/Algorithms/DBAPAlgorithm.h
  - include/SpatialCore/Algorithms/MDAPAlgorithm.h
  - include/SpatialCore/Algorithms/VBIPAlgorithm.h
  - include/SpatialCore/Core/SpatialMath.h
  - include/SpatialCore/Engine/RenderEngine.h
  - include/SpatialCore/IO/AmbisonicsCodec.h
  - include/SpatialCore/IO/SpeakerLayout.h
  - src/Algorithms/DirectBinauralAlgorithm.cpp
  - src/Algorithms/KNNAlgorithm.cpp
  - src/Algorithms/MDAPAlgorithm.cpp
  - src/Algorithms/VBAPAlgorithm.cpp
  - src/Algorithms/VBIPAlgorithm.cpp
  - src/Core/SpatialMath.cpp
  - src/Engine/RenderEngine.cpp
  - src/IO/AmbisonicsCodec.cpp
  - src/IO/SpeakerLayout.cpp
  - tests/Algorithms/PanningLawTests.cpp
  - tests/Algorithms/SpatializationAlgorithmTests.cpp
  - tests/CMakeLists.txt
  - tests/Core/VBAPTripletSelectionTests.cpp
  - tests/Engine/RenderEngineTests.cpp
  - tests/IO/AmbisonicsCodecTests.cpp
  - tests/IO/SpeakerLayoutTests.cpp
  - tests/reference/.gitignore
  - tests/reference/EarReference.h
  - tests/reference/PanningReference.h
  - tests/reference/README.md
  - tests/reference/ShReference.h
  - tests/reference/gen_ear_reference.py
  - tests/reference/gen_panning_reference.py
  - tests/reference/gen_sh_reference.py
  - tests/reference/layouts_from_cpp.py
covered_digest: "v2:sha256:2104f066c10a7ec608d384e6f98f2790fa34085ec02a04fa2ccc8430c9d42bf8"
behavior_unverified: 1
overrides_applied: 0
re_verification:
  previous_status: human_needed
  previous_score: 57/60
  gaps_closed:
    - "G-02-2: below-horizon 0 to -30 degree band on height layouts now matches EAR (UAT test 2, severity major)"
    - "02-07 prohibition 'docs must not describe code differently' (previously partly failed): now holds, docs restate the 02-08 tests"
    - "WR-01 (guide scoped the coplanar tie to 'above the horizon' while the band tied too): the band tie no longer exists, so the scoping is now correct"
  gaps_remaining: []
  regressions: []
behavior_unverified_items:
  - truth: "RenderEngine::activateLayout aborts in every build type when a height layout yields no regular triplets or a flat layout yields some; only a SpatialCore developer editing layoutDefs or the builder can reach it, and the abort is on the message/prepare thread, never on a path reachable from renderBlock (02-01, D-02a; verification: backstop)"
    test: "Temporarily edit a layoutDefs height entry so it builds no regular triplets (or lower a flat speaker to 0.8 degrees with the threshold reverted), build Release, call setOutputFormat for that format"
    expected: "Process aborts inside activateLayout on the calling (message/prepare) thread; renderBlock never reaches it"
    why_human: "Tagged verification: backstop (non-inferable). The abort kills the test process, so no Catch2 test exercises it. Evidence is placement only: one std::abort at src/Engine/RenderEngine.cpp:729 (re-confirmed this run), none in renderBlock. Unchanged by 02-08 and 02-09 (src/Engine has no diff since 306b025)."
coincidental_reliance_items:
  - truth: "Finite inputs are untouched by every guard and every std::isfinite check lives in a .cpp, so OpenSpatialDelay's -ffast-math build cannot fold it away (02-04)"
    reason: undeclared-precondition
    harden: "The guards hold only because the SpatialCore target itself is compiled without -ffast-math / -ffinite-math-only. Turn it into a compile-time #error on __FAST_MATH__ / __FINITE_MATH_ONLY__ in the guarded TUs (review WR-04). Unchanged by this round."
human_verification:
  - test: "Keep 5.1.4's rear-gap difference from EAR? (02-09's one plain-English question, harvested here, not answered.) On a 5.1.4 setup, for a sound directly behind you (beyond 110 degrees either side), EAR also sends part of it to the two rear ceiling speakers, even at ear height. SpatialCore keeps it on the two rear ear-level speakers, so sounds at or above ear height are unchanged from today. This one difference is written down in the integration guide, README and skill."
    expected: "Answer yes or no. Yes: keep it (recommended: matching EAR there would change the sound above ear height on 5.1.4 and reintroduce the 0.71 horizon jump from RESEARCH F4). No: needs a new decision before any code changes."
    why_human: "A judgment about sound, not a fact the code can settle. The code and docs agree with each other (verified: the guide, README and skill all state it, and the ear generator carves out |az| > 110 on 5.1.4); the tests pin that SpatialCore differs there only in that region (review swept 3-degree grid vs ear 2.1.0)."
  - test: "Optional listening check of the corrected band (02-08 coverage D7). In a real OpenSpatialDelay session on 7.1.4, slowly lower a sound from ear height to 30 degrees below at about 60 degrees to the left."
    expected: "It stays put, with no drift toward one speaker and no side flip. The numbers already prove this (0.7071 / 0.7071 on M+030 / M+090 at every elevation, my own probe below); this is only whether you like how it sounds."
    why_human: "Audible character is a listening judgment. You already chose to change the panning (UAT test 2, option B)."
  - test: "Backstop truth: the D-02a Release abort (see behavior_unverified_items)."
    expected: "Optionally break a layoutDefs height entry on a scratch branch, build Release, call setOutputFormat; the process aborts in activateLayout, never from renderBlock."
    why_human: "Tagged verification: backstop. An abort cannot be caught by Catch2. Placement evidence only."
  - test: "Judgment-tier prohibitions: 25 across 9 plans (17 from before plus 8 from 02-08 and 02-09). See the Prohibitions table. I recorded a NON-AUTHORITATIVE judgment for each; all 25 hold on the evidence."
    expected: "Accept or reject. unverified-prohibition: human review recommended."
    why_human: "Judgment-tier items are never silently absorbed into a pass. UAT test 5 already accepted the first 17 (with the 02-07 doc-accuracy item partly failing, now cured)."
---

# Phase 2: Algorithm & Format Verification Verification Report

**Phase Goal:** Every algorithm and every advertised format is confirmed correct by a test, and the Ambisonics convention is written down.
**Verified:** 2026-10-03T10:41:21Z
**Status:** human_needed
**Re-verification:** Yes, after gap closure (plans 02-08 and 02-09, UAT gap G-02-2)

## Summary

Gap G-02-2 is closed, by evidence in code, in tests and in my own probe. All four ROADMAP success criteria still hold. There are no blockers and no regressions. Status is `human_needed` rather than `passed` because four items remain for the user: the one yes/no question about 5.1.4's rear gap, an optional listening check, the backstop abort (a runtime abort no test can exercise), and the flagged judgment-tier prohibitions.

**Test run (my own):**

- `cmake --build build --target SpatialCoreTests` exit 0.
- Full binary: **193 cases, 192 passed, 1 failed; 304,838 assertions, 1 failed.** The one failure is `HUTUBS PP2 — golden HRIR checksum at az=90deg (own control)` (`tests/Binaural/HutubsPP2Tests.cpp:47`), pre-existing and Phase 3 scope. `git log d43cb15..HEAD` on `tests/Binaural`, `src/Binaural` and `src/HRTF` is empty.
- With that case excluded (the orchestrator's command): **All tests passed (304835 assertions in 192 test cases)**, matching the orchestrator's figure. The trailing "Leaked objects: FFT" notice is the SharedFFTCache static-destruction false positive recorded in `deferred-items.md` (exit code unaffected).
- Per-tag runs, all pass: `[g02-2]` 203 assertions / 1 case, `[band]` 828 / 3, `[vbap3d-identity]` 262,104 / 1, `[consumer-surface]` 13 / 1, `[ear]` 19,215 / 11, `[io][layout]` 19,119 / 7, `[panning-law]` 7,191 / 10.
- All three reference headers regenerate **byte-identical** from `.context/venv` (ear 2.1.0, scipy, textbook formulas): `gen_ear_reference.py`, `gen_sh_reference.py`, `gen_panning_reference.py`.

## G-02-2 closure (the focus of this re-verification)

**The gap, per 02-UAT.md:** on height layouts a source between 0 and -30 degrees elevation must pan as EAR does: the horizon pan holds (7.1.4 az 60 stays 0.7071 / 0.7071 on M+030 / M+090), no lean, no side flip.

**Code evidence (read, not taken from SUMMARY):**

- `src/IO/SpeakerLayout.cpp` `appendLowerHemisphereTriplets`: the four overlapping trapezoid triangles are no longer emitted (the loop `continue`s on any facet without the nadir vertex). For each nadir-cap facet it records the neighbouring ear-level pair (`capPairs`) and then pushes one pair-pan wedge per pair: the two real speakers plus the virtual nadir, `nadirVertex = 2`, `nadirMask = 0`, `nadirGain = 0`. That is 2n lower-hemisphere triplets for n ear-level speakers.
- `src/Core/SpatialMath.cpp` `computeVBAPGains3D`: `selectionTier` (integer tests only) classifies regular, cap and wedge. The loop is `for (pass = 0; pass < 3 && bestTri < 0; ++pass)`, so regular triplets run first with the original loop body, then caps, then wedges. Tolerance is still `-1e-6f` (one occurrence at line 312). No allocation, lock or logging was added. The output mapping for a zero-share wedge gives the pair's 2D pan, power-normalised.
- Public surface: `git diff 306b025 HEAD -- include/` contains comment lines only (the single non-comment `+` line is blank). `VBAPTriplet`, `LayoutContext` and the `SpatializationAlgorithm` interface are unchanged. `[consumer-surface]` passes.
- `src/Engine/` has no diff since 306b025, so the `activateLayout` guard and abort are untouched.

**My own probe (independent of the Catch2 tests).** A standalone program, compiled against this branch and `build/libSpatialCore.a`, drove `RenderEngine::setOutputFormat` then `VBAPAlgorithm::computeGains`. The probe was in `/tmp/g022probe`, outside the repo. Results:

| Format | Azimuth | Elevations 0, -5, -15, -25, -30 | Gains |
|---|---|---|---|
| 7.1.4 | 60 | identical at every elevation | speaker 0 = 0.7071, speaker 3 = 0.7071, all others below 1e-4 |
| 7.1.4 | -60 | identical | speakers 1 and 4 = 0.7071 |
| 7.1.4 | 45 | identical | 0.9391 / 0.3437 |
| 5.1.4 | 60 | identical | 0.8374 / 0.5466 (matches the pinned ear value for el -15) |
| 5.1.4 | -60, 45 | identical | mirror / 0.9616 / 0.2746 |

Before the fix the earlier verifier measured 0.7210 / 0.6929 at el -5, 0.7472 / 0.6646 at -15 and 0.6768 / 0.7361 at -25. No lean, no side flip, no height speaker used.

**Oracle evidence:**

- `tests/reference/EarReference.h` gained 17 `kEarBand*` entries (26 band cases) produced by `gen_ear_reference.py` from PyPI ear 2.1.0. The regenerate-diff is empty. The only lines removed from the header are the old "band deliberately NOT pinned" comment. The 13 nadir-cap pins are unchanged.
- 5.1.4's band pins stop at |az| 100 (the generator carves out beyond 110). The header's 5.1.4 cases are (60,-15), (-70,-25) and (-100,-20).
- `[ear][golden][band]` matches ear within 1e-5 (measured max 1.79e-7). `[ear][band]` checks an independent double-precision pair pan at 720 x 9 directions on all 8 layouts, plus exactly one enclosing wedge and no regular or cap enclosure. The band continuity bound in `PanningLawTests.cpp` is now 0.01 for the band (-5, -15, -25) and a new band/cap seam scope (-35, -40). It was 0.12.
- `[vbap3d-identity]` (262,104 assertions) is bit-identical to `d43cb15` above the horizon, so issue #22's above-horizon tie is untouched, as intended.

**Docs match code:** the guide, README and skill state the pair-region band, the PyPI ear 2.1.0 match, the 5.1.4 exception and #22 above the horizon only. I confirmed the phrases are present (`one pan region` once in the guide and twice in the skill, `PyPI ear 2.1.0` likewise, `except behind the listener on 5.1.4` in README, `above the horizon on height layouts` in the skill) and that CRLF is intact on every line (guide 282/282, README 144/144, skill 583/583). The guide's "about -59 degrees" depth is a derivation, not a measurement. 02-09-SUMMARY states that openly and I re-derived it: tan(el) = tan(-30)/cos(70) gives -59.3 for a 140-degree pair.

**Verdict: G-02-2 is CLOSED.**

## UAT carry-forward

02-UAT.md still reads `status: diagnosed`, 4 passed, 1 issue (test 2). It has not been updated since the gap closure. The orchestrator should move test 2 to pass.

| UAT test | UAT result | Status now |
|---|---|---|
| 1 DR-3 cross-repo build | pass | Carried, and strengthened: 02-09 re-ran the build on the final code (`/tmp/osd-dr3-check/build.log` ends with `Built target OpenSpatialDelayTests`, 0 `error:` lines; `SpatialCore` is symlinked to this worktree; `git diff 582fa5a HEAD -- src include` is empty, so no source changed after the build; the OSD repo's `git status --porcelain` is empty). The five ADMOSCReceiver.h warnings are pre-existing. Follow-up list updated in 02-09-SUMMARY (4 OSD files, five audible changes), as UAT's amendment required. |
| 2 Below-horizon band | issue (G-02-2) | **Resolved by 02-08 and 02-09**, as shown above. Needs the UAT file updated. |
| 3 WR-03 hand-built contexts | pass (accepted under D-01) | Carried. WR-03 is still open in code and is re-reported by the new review; the acceptance still stands because it is reachable only by bypassing `RenderEngine`. |
| 4 Process gates | pass | Carried. `gh issue view 22` was not re-run: `gh` is unauthenticated in this session (HTTP 401). UAT recorded the body match on 2026-10-01 and #22 is unchanged; 02-09 suggested a comment for #22 but did not post it. |
| 5 Judgment prohibitions | pass | Carried. The one that partly failed (02-07 docs-match-code) now holds. |

## Goal Achievement

### ROADMAP Success Criteria

| # | Criterion | Status | Evidence |
|---|-----------|--------|----------|
| 1 | AmbisonicsCodec channel order and normalisation confirmed ACN/SN3D or FuMa, stated in code and docs (SpatialCore#11) | ✓ VERIFIED | Unchanged by 02-08 and 02-09 (no diff to `AmbisonicsCodec.*` or `[sn3d]` tests since the first verification). Convention docblocks in `SpatialMath.h`, `AmbisonicsCodec.h` and the guide. `[sn3d]` runs the 49 scipy literals within 1e-5, the addition theorem at orders 1-6, and negative-elevation parity. The scipy reference regenerates byte-identically. |
| 2 | The 3D triplet fallback in VBAP/VBIP/MDAP either no longer exists or fails loudly | ✓ VERIFIED | Both nearest-speaker paths stay deleted: `grep -rn nearestSpeaker3DFallback src/` is empty. The engine abort sits at `RenderEngine.cpp:729`. The no-enclosing-triplet case still hits Debug `jassertfalse` plus the largest-min-gain triplet (`SpatialMath.cpp`). The new pair wedges make coverage better, not worse: `[ear][coverage]` still passes (0 uncovered finite directions on all 8 height layouts). |
| 3 | All 23 OutputFormat entries resolve; all 15 layouts return populated channel indices and LFE placement | ✓ VERIFIED | `[io][golden]`, `[format-resolve]`, `[io][layout]` (19,119 assertions / 7 cases this run) all pass. The layout test now asserts the exact 2n extras structure. |
| 4 | Ambisonics encode/decode round-trips within tolerance at every order up to 6 | ✓ VERIFIED | `[roundtrip]` passes, with the previously noted limit: the order-6 decode half is test-local (D-11/D-12, shipped decoder capped at order 3 / 16 speakers). |

### Plan must-have truths

**02-01 to 02-07 (60 truths, unchanged scope).** I re-ran the tags that back them (`[tracer]`, `[vbap3d-identity]`, `[consumer-surface]`, `[ear]`, `[panning-law]`, `[io][layout]` and the full suite) and re-ran all three reference generators. Everything that was VERIFIED before is still VERIFIED, with these changes:

| Truth | Before | Now | Why |
|---|---|---|---|
| 02-07: docs describe below-horizon as EAR, with no nearest-speaker (accuracy caveat) | ✓ VERIFIED (as worded) | ✓ VERIFIED | The 0 to -30 band is now EAR-exact, so the caveat is gone |
| 02-04: below-horizon VBAP matches ear 2.1.0 (as scoped to the nadir cap) | ✓ VERIFIED (as scoped) | ✓ VERIFIED | The band is now pinned too |
| 02-02: numpy/scipy/ear only after human verification | ? UNCERTAIN | ✓ VERIFIED | UAT test 4: the user's own delegation is quoted in 02-02-SUMMARY |
| 02-03: nothing filed until a human approved the text | ? UNCERTAIN | ✓ VERIFIED | UAT test 4: filed issue body identical to the approved draft `02-03-ISSUE-BODY.md` |
| 02-01: D-02a abort, every build type, not reachable from renderBlock | ⚠️ PRESENT_BEHAVIOR_UNVERIFIED (backstop) | ⚠️ PRESENT_BEHAVIOR_UNVERIFIED (backstop) | Unchanged. Placement re-confirmed. See human items. |
| 02-04: finite inputs untouched, isfinite only in .cpp | ✓ VERIFIED (coincidental-reliance) | ✓ VERIFIED (coincidental-reliance) | Unchanged advisory (WR-04). |

**02-08: below-horizon band matches EAR (9 truths)**

| Truth | Status | Evidence |
|---|---|---|
| 7.1.4 az 60 holds 0.7071 / 0.7071 from 0 to -30; az -60 mirrors on speakers 1 and 4; no lean, no side flip | ✓ VERIFIED | `[g02-2]` passes (203 assertions). My own probe reproduces it through `RenderEngine::setOutputFormat` and `VBAPAlgorithm::computeGains`. |
| Exactly 2n lower triplets (n caps with every ear-level bit and `1/sqrt(n)`, n wedges with `nadirMask = 0`, `nadirGain = 0`), no trapezoid triangle remains | ✓ VERIFIED | Builder code read. `[io][layout][ear]` asserts extras == 2n, n caps, n wedges and matching pair sets. |
| Selection order: regular, then cap, then wedge; no min-sum tie below the horizon | ✓ VERIFIED | `selectionTier` plus `pass < 3`. `[ear][band]` asserts exactly one enclosing wedge and no regular or cap enclosure per band direction. |
| Band gains equal an independent double-precision 2D pair pan (within 1e-5) on all 8 layouts, 0.5-degree azimuth, 9 elevations, including 5.1.4's rear gap | ✓ VERIFIED | `[band]` passes (828 assertions). The summary reports max 2.98e-7. |
| 26 band directions pinned against ear 2.1.0 within 1e-5, regenerate-diff empty, no 5.1.4 pin beyond 110 degrees, 13 cap pins unchanged | ✓ VERIFIED | I ran the regenerate-diff myself (identical). The header's removed lines are comments only. 5.1.4 pins are at az 60, -70 and -100. |
| Band and band/cap seam continuity at most 0.01 per 0.1 degree; the old bound was 0.12 | ✓ VERIFIED | `PanningLawTests.cpp:966-967` scopes `-30..0 band` and `band/cap seam` at 0.01f. `[panning-law]` passes. |
| Above the horizon nothing changes (bit-identical to d43cb15; #22 untouched) | ✓ VERIFIED | `[vbap3d-identity]` passes (262,104 assertions). Pass-0 loop body unchanged. |
| Tolerance stays -1e-6f in every pass | ✓ VERIFIED | One `g0 >= -1e-6f` site (`SpatialMath.cpp:312`) serves all passes. |
| Public surface unchanged (comment-only header diff, `[consumer-surface]` passes) | ✓ VERIFIED | Header diff is comments only; `[consumer-surface]` passes; the OSD build compiles. |

**02-09: docs, gate, DR-3 (9 truths)**

| Truth | Status | Evidence |
|---|---|---|
| Integration guide states the pair region, 0.7071 / 0.7071, EAR match except 5.1.4 behind the listener (U+135 / U-135), #22 above the horizon only | ✓ VERIFIED | Text read in the diff; required phrases present. |
| README VBAP bullet states the EAR match except behind the listener on 5.1.4 | ✓ VERIFIED | `grep` confirms. |
| Skill states the same in both below-horizon paragraphs and scopes step 4's tie | ✓ VERIFIED | `one pan region` x2, `PyPI ear 2.1.0` x2, `above the horizon on height layouts` x1. |
| All three docs keep CRLF on every line | ✓ VERIFIED | 282/282, 144/144, 583/583. |
| WR-01 recorded as fixed in 02-REVIEW-DISPOSITION.md | ✓ VERIFIED | Frontmatter and table row both say `fixed`. The count now reads `open: 10, total: 11` because the later review (ec957c1) added WR-05, IN-05 and IN-06 and dropped nothing else, so the "open count 7" in the plan was correct at the time of 02-09 and has since moved. |
| Phase gate: full suite minus HUTUBS passes; every Phase 2 tag including `[band]` and `[g02-2]` passes | ✓ VERIFIED | My run reproduces 304,835 assertions / 192 cases; the tag sweep above passes. |
| DR-3: OSD 30391cd compiles and links against this branch, OSD repo status unchanged, nothing installed | ✓ VERIFIED | Build log evidence above; `git -C ~/conductor/repos/openspatialdelay-v1 status --porcelain` is empty. I did not rebuild OSD (instructions). |
| 02-09-SUMMARY carries the superseding cross-repo follow-ups (4 files, five audible changes, #22 comment suggested but not posted) | ✓ VERIFIED | Section present with all three. |
| The user gets one plain-English yes/no question and Claude runs the checks | ✓ VERIFIED | Question present in 02-09-SUMMARY, harvested as a human item below. |

**Score:** 77/78 truths verified; 1 present-but-behaviour-unverified (the backstop abort), routed to human verification.

### Prohibitions (judgment-tier, non-authoritative LLM judgments; human review recommended)

The 17 earlier items are carried from the first verification and UAT test 5. The previously partly-failing 02-07 item now holds. New items:

| Plan | Prohibition (short) | Verifier judgment |
|---|---|---|
| 02-07 | Docs must not describe code differently from what it computes | **Now holds.** The band is EAR-exact and the tie scope is accurate. Residual minor wording items IN-01 and IN-04 are unchanged and not covered by this prohibition's core. |
| 02-08 | No output change above the horizon; #22 stays separate | Holds: `[vbap3d-identity]` |
| 02-08 | No alloc, lock or log in `computeVBAPGains3D`; wedges built on the layout-build thread | Holds: only integer tests and stack floats added; wedges built in `appendLowerHemisphereTriplets` |
| 02-08 | No public member added, removed or retyped; no change to `SpatializationAlgorithm` or the 4-member `LayoutContext` | Holds: header diff is comments only |
| 02-08 | No special case for 5.1.4's rear gap in code | Holds: the builder has no layout-specific branch; the gap gets the same wedge |
| 02-08 | No hand-edited `EarReference.h`, no value derived from SpatialCore code | Holds: byte-identical regeneration from ear 2.1.0 |
| 02-09 | Docs must not describe behaviour the code lacks | Holds, with the "about -59 degrees" figure a stated derivation I re-checked |
| 02-09 | OSD repo not modified; no installing targets built; no GitHub post; CONTEXT unchanged | Holds: OSD status empty; no `gh` write; I did not touch OSD |
| 02-09 | No change outside the named bullets and paragraphs | Holds: guide +24/-12 in two bullets, README one line, skill three lines |

### Required Artifacts

| Artifact | Status | Details |
|---|---|---|
| `src/IO/SpeakerLayout.cpp` | ✓ VERIFIED | Cap and wedge emission, shared `setInverse`, banner rewritten; WIRED via `RenderEngine.cpp:735` |
| `src/Core/SpatialMath.cpp` | ✓ VERIFIED | `selectionTier` and three-pass selection; WIRED (read by VBAP/VBIP/MDAP through `computeVBAPGains3D`) |
| `include/SpatialCore/IO/SpeakerLayout.h` | ✓ VERIFIED | Comment-only change |
| `tests/IO/SpeakerLayoutTests.cpp`, `tests/Core/VBAPTripletSelectionTests.cpp`, `tests/Algorithms/PanningLawTests.cpp` | ✓ VERIFIED | All registered and run; the three new test groups pass |
| `tests/reference/gen_ear_reference.py`, `EarReference.h`, `README.md` | ✓ VERIFIED | Regenerate byte-identically |
| `docs/integration-guide.md`, `README.md`, `.claude/skills/spatial-audio-dsp/SKILL.md` | ✓ VERIFIED | New text present, CRLF intact |
| `02-REVIEW-DISPOSITION.md`, `02-VALIDATION.md` | ✓ VERIFIED | WR-01 fixed; the four new validation rows exist (still marked pending, a bookkeeping item, not a goal gap) |
| Earlier plans' artifacts (02-01 to 02-07) | ✓ VERIFIED | No regressions; tests above pass |

### Key Link Verification

| From | To | Status | Details |
|---|---|---|---|
| `RenderEngine::activateLayout` | `appendLowerHemisphereTriplets` | ✓ WIRED | Called after the guard, before the swap (`RenderEngine.cpp:735`) |
| `computeVBAPGains3D` | wedge and cap fields | ✓ WIRED | `selectionTier` reads `lowerHemisphere`, `nadirVertex`, `nadirMask`; the output mapping reads `nadirGain` |
| Docs | tests | ✓ WIRED | The guide's 0.7071 / 0.7071 example and the PyPI ear 2.1.0 claim are `[g02-2]` and the `[ear][golden][band]` pins |
| `EarReference.h` | `gen_ear_reference.py` | ✓ WIRED | Regenerate-diff empty |

### Data-Flow Trace (Level 4)

Not applicable to UI. The equivalent trace is `setOutputFormat` then `activateLayout` then `vbapTriplets` then `computeGains` then output gains. My probe exercises the whole chain with real engine-built triplets, not test-built ones, and the gains are non-trivial (0.7071, 0.9391, 0.8374). Status: ✓ FLOWING.

### Behavioral Spot-Checks

| Behaviour | Command | Result | Status |
|---|---|---|---|
| Suite builds | `cmake --build build --target SpatialCoreTests` | exit 0 | ✓ PASS |
| Suite passes except the deferred HUTUBS case | `./build/tests/SpatialCoreTests "~HUTUBS PP2 — golden HRIR checksum at az=90deg (own control)"` | 192 cases / 304,835 assertions pass | ✓ PASS |
| G-02-2 end to end | own probe (7.1.4, 5.1.4) | 0.7071 / 0.7071 at 0, -5, -15, -25, -30; mirrors at -60 | ✓ PASS |
| Reference headers are generated, not typed | three `gen_*_reference.py` vs checked-in headers | 3/3 identical | ✓ PASS |
| Tags `[g02-2]` `[band]` `[vbap3d-identity]` `[consumer-surface]` `[ear]` `[io][layout]` `[panning-law]` | per-tag runs | all pass | ✓ PASS |
| Issue #22 still open | `gh issue view 22` | **SKIPPED**: `gh` returns HTTP 401 this session. UAT already recorded the match. | ? SKIP (no decision rides on it) |

### Probe Execution

No `scripts/*/tests/probe-*.sh` exist and no plan declares one. Step 7c: N/A.

### Requirements Coverage

| Requirement | Source Plans | Description | Status | Evidence |
|---|---|---|---|---|
| EXTR-01 | 02-01, 02-02, 02-03, 02-04, 02-05, 02-07, 02-08, 02-09 | Every public algorithm computes correct gains; the 3D triplet fallback is eliminated or fails loudly | ✓ SATISFIED | Algorithm tests pass; `[panning-law]` covers all 8; both nearest-speaker fallbacks gone; below-horizon VBAP pinned to ear 2.1.0 including the band |
| EXTR-03 | 02-02, 02-06, 02-07 | Layouts, format registry and Ambisonics codec return real data; 23 formats, 15 layouts, encode/decode to order 6 | ✓ SATISFIED | SC3 and SC4 evidence |
| VERIFY-01 | 02-02, 02-06, 02-07 | Ambisonics channel-order and normalisation convention stated (SpatialCore#11) | ✓ SATISFIED | SC1. REQUIREMENTS.md already shows `[x]` and "Complete". #11 itself was never closed on GitHub (only by commit-message reference). |

No orphaned requirements: REQUIREMENTS.md maps only EXTR-01, EXTR-03 and VERIFY-01 to Phase 2 (traceability rows 427, 428, 442), and ROADMAP.md lists the same three. 02-08 and 02-09 declare only EXTR-01.

**Tracking defect (answers 02-09's note that `requirements.mark-complete EXTR-01` found no match).** REQUIREMENTS.md does contain the entry, in the same `- [ ] **EXTR-01**: ...` form as the `[x]` entries (line 138 for EXTR-01, 148 for EXTR-03). So the text format is not the cause. The likely cause is the traceability table: the rows for EXTR-01 and EXTR-03 carry the free-text status "Largely verified by tests" (lines 427, 428) where the completed rows carry "Complete", so the tool's row matcher finds no "Pending" row to flip. Both checkboxes are still `[ ]` and the rows still say "Largely verified by tests", although the evidence satisfies both requirements. I did not edit REQUIREMENTS.md (the orchestrator owns tracking). Suggested edit: set both checkboxes to `[x]` and both traceability statuses to "Complete".

### Anti-Patterns Found

| File | Line | Pattern | Severity | Impact |
|---|---|---|---|---|
| (files changed by 02-08 and 02-09) | | TBD / FIXME / XXX | none found | Debt-marker gate clear |
| `src/IO/SpeakerLayout.cpp` | 293-375 | WR-05: holes when the ear-level ring has an azimuth gap of 180 degrees or more (hull "away from origin" orientation) | ⚠️ Advisory | See below |
| `src/Core/SpatialMath.cpp`, `include/.../SpeakerLayout.h` | 243-256 / 30-52 | WR-02: wedge kind identified only by the implicit `nadirVertex >= 0 && nadirMask == 0` sentinel | ⚠️ Advisory | Hand-built triplets with `nadirMask == 0` would be reclassified; nothing enforces the encoding |
| `src/Core/SpatialMath.cpp` | 337-355 | WR-03: regular-only triplet list has no below-horizon coverage; Debug assert on the audio thread | ⚠️ Advisory (UAT test 3 accepted) | Reachable only by bypassing `RenderEngine` |
| `src/Algorithms/{VBAP,VBIP,MDAP}Algorithm.cpp` | jassert sites | Release fall-through to 2D pan on a height layout with empty triplets | ⚠️ Advisory | Hand-built contexts only, accepted under D-01 |

### Code review cross-check (02-REVIEW.md: 0 critical, 3 warnings, 2 info)

I checked each finding against the must-haves. **None contradicts a must-have.**

- **WR-05** is new. It is real, but every must-have that mentions "2n triplets" or "every height layout" is scoped to the 8 shipped height layouts, and on those it holds (the reviewer swept 200,000 random directions per layout with no hits; `[ear][coverage]` and `[io][layout]` pass in my run). The reachable surface is a consumer-defined `SpeakerLayout` passed to the public `appendLowerHemisphereTriplets`, outside the SC-13 facade, and `RenderEngine` only builds layouts from the closed `OutputFormat` enum. The honest doc defect is that the header promises "2n" and lists only three no-op conditions. Recommend a follow-up (decline-and-document, plus a test), not a blocker. There is no human decision needed beyond the 02-UAT test 3 acceptance, which already covers this class (hand-built contexts, D-01).
- **WR-02** and **WR-03** are re-reports of known items. WR-02 touches no must-have: the 02-08 prohibition says the wedge is encoded in existing fields, and it is. A `Kind` field would conflict with that prohibition and DR-3 unless it is defaulted, so it is a follow-up for a future major or minor version. WR-03 was accepted in UAT test 3.
- **IN-05** (equivalence assumes ear-level speakers at 0 degrees while the code admits up to +/-10 degrees) is true of the code and is already stated in a code comment. No shipped layout has an ear-level speaker off 0 degrees.
- **IN-06** (test duplicates the tier classifier) is a test-hygiene item. It weakens the "exactly one wedge encloses" check only if production's rule drifts, so it is worth folding into the WR-02 follow-up.

### Human Verification Required

#### 1. Keep 5.1.4's rear-gap difference from EAR? (yes or no)

**Test:** On a 5.1.4 setup, for a sound directly behind you, EAR also sends part of it to the two rear ceiling speakers. SpatialCore keeps it on the two rear speakers at ear height, so sounds at or above ear height stay exactly as they are today.
**Expected:** Answer yes or no. Yes keeps it (the recommendation). No needs a new decision before any code changes, because matching EAR there would change the sound above ear height on 5.1.4.
**Why human:** A judgment about sound. It is documented in the guide, README and skill, and nothing is blocked on the answer.

#### 2. Optional listening check of the corrected band

**Test:** In a real session on 7.1.4, slowly lower a sound from ear height to 30 degrees below, 60 degrees to the left.
**Expected:** It stays where it is, with no drift to one speaker and no flip. The numbers already prove it.
**Why human:** Audible character is a listening judgment. Skip it if you are happy with the numbers.

#### 3. Backstop truth: the D-02a abort

**Test:** Optional. Break a `layoutDefs` height entry on a scratch branch, build Release, call `setOutputFormat`.
**Expected:** The process aborts in `activateLayout`, never from `renderBlock`.
**Why human:** Tagged `verification: backstop`. No Catch2 test can survive an abort. Placement (one `std::abort` at `RenderEngine.cpp:729`, none in `renderBlock`) is the evidence.

#### 4. Judgment-tier prohibitions (25)

**Test:** Review the Prohibitions table.
**Expected:** Accept or reject. All 25 hold on the evidence.
**Why human:** These are unverified prohibitions and human review is recommended.

### Gaps Summary

There are no gaps and no blockers. G-02-2 was the only open gap from the previous round and it is closed with a passing test, a regenerate-diffed ear oracle and an independent probe. Nothing regressed: the above-horizon output is still bit-identical, the public headers are unchanged apart from comments, and the OSD build still links.

Follow-ups, none of which blocks the phase:

- Update `02-UAT.md`: test 2 to pass, status to complete.
- Update REQUIREMENTS.md: EXTR-01 and EXTR-03 to `[x]` and "Complete" (see the tracking note above).
- Tick the `02-VALIDATION.md` rows for 02-08 and 02-09 (they are still `pending`).
- Carry the 02-09 OSD follow-ups into the OSD repo.
- Optional hardening: WR-05 (decline layouts whose ear-level ring does not surround the listener), WR-02/IN-06 (explicit triplet kind), WR-03 (single builder entry point), WR-04 (compile-time fast-math check).

---

_Verified: 2026-10-03T10:41:21Z_
_Verifier: Claude (gsd-verifier)_
