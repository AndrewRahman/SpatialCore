---
phase: 02-algorithm-format-verification
verified: 2026-10-04T01:08:24Z
status: passed
score: 93/93 must-haves verified
covered_files:
  - .claude/skills/spatial-audio-dsp/SKILL.md
  - .gitattributes
  - .gitignore
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
  - .planning/phases/02-algorithm-format-verification/02-10-PLAN.md
  - .planning/phases/02-algorithm-format-verification/02-10-SUMMARY.md
  - CMakeLists.txt
  - README.md
  - docs/integration-guide.md
  - include/SpatialCore/Algorithms/DBAPAlgorithm.h
  - include/SpatialCore/Algorithms/MDAPAlgorithm.h
  - include/SpatialCore/Algorithms/VBIPAlgorithm.h
  - include/SpatialCore/Core/SpatialMath.h
  - include/SpatialCore/Engine/RenderEngine.h
  - include/SpatialCore/IO/AmbisonicsCodec.h
  - include/SpatialCore/IO/SpeakerLayout.h
  - src/Algorithms/AmbisonicsAlgorithm.cpp
  - src/Algorithms/ConstantPowerAlgorithm.cpp
  - src/Algorithms/DBAPAlgorithm.cpp
  - src/Algorithms/DirectBinauralAlgorithm.cpp
  - src/Algorithms/KNNAlgorithm.cpp
  - src/Algorithms/MDAPAlgorithm.cpp
  - src/Algorithms/VBAPAlgorithm.cpp
  - src/Algorithms/VBIPAlgorithm.cpp
  - src/Core/FloatSemanticsGuard.h
  - src/Core/SpatialMath.cpp
  - src/Engine/RenderEngine.cpp
  - src/IO/AmbisonicsCodec.cpp
  - src/IO/SpeakerLayout.cpp
  - src/OSC/ADMOSCReceiver.cpp
  - tests/Algorithms/PanningLawTests.cpp
  - tests/Algorithms/SpatializationAlgorithmTests.cpp
  - tests/CMakeLists.txt
  - tests/Core/VBAPTripletSelectionTests.cpp
  - tests/Engine/RenderEngineTests.cpp
  - tests/IO/AmbisonicsCodecTests.cpp
  - tests/IO/SpeakerLayoutTests.cpp
  - tests/TestNumerics.h
  - tests/reference/.gitignore
  - tests/reference/EarReference.h
  - tests/reference/PanningReference.h
  - tests/reference/README.md
  - tests/reference/ShReference.h
  - tests/reference/gen_ear_reference.py
  - tests/reference/gen_panning_reference.py
  - tests/reference/gen_sh_reference.py
  - tests/reference/layouts_from_cpp.py
covered_digest: "v2:sha256:5beecbecb6d3b6024f50c51434b4d5ff9a7d56b00bdb890befa71d539a77ab33"
behavior_unverified: 0
overrides_applied: 6
overrides:
  - must_have: "RenderEngine has no private SH decoder: activateLayout fills ambiDecodeMatrix through AmbisonicsCodec::getDecodeMatrix at order 3, and each of the 15 speaker layouts gets a decode within 1e-6 of the pre-change private function (02-06)"
    reason: "The 1e-6 bound was never achievable on arm64 Release (G-02-10). Plan 02-10 replaced it with the derived 2.5e-5 bound plus a double-precision anchor. The user accepted the 2.5e-5 bound in place of 1e-6 in UAT test 11."
    accepted_by: "Andrew Rahman (02-UAT.md test 11, answer: yes)"
    accepted_at: "2026-10-04T00:45:00Z"
  - must_have: "No VBAP, VBIP or MDAP source file contains a nearest-speaker branch; a hand-built LayoutContext with empty triplets on a height layout trips a Debug-only assert (D-01)"
    reason: "IN-14: JUCE's assert path logs and allocates on the audio thread (DR-1). The invariant is enforced on the message thread by activateLayout's D-02a abort, and a hand-built empty context now pans by 2D VBAP, pinned by the IN-14 [robust] test."
    accepted_by: 'Andrew Rahman (session 2026-10-04, answer: "accept all five")'
    accepted_at: "2026-10-04T01:10:00Z"
  - must_have: "When no triplet contains a finite direction, computeVBAPGains3D uses the triplet with the largest minimum gain, clamps negatives to 0 and renormalises; an empty triplet list returns silence; a Debug-only assert marks the path (D-06b, D-06c)"
    reason: "WR-03: the audio-thread assert is removed, and a candidate that clamps to zero now gives unity on the nearest speaker instead of silence. Hand-built partial lists only; unreachable through RenderEngine ([ear][coverage]). D-06(b)/(c) amended in CONTEXT."
    accepted_by: 'Andrew Rahman (session 2026-10-04, answer: "accept all five")'
    accepted_at: "2026-10-04T01:10:00Z"
  - must_have: "The docs describe below-horizon panning on height layouts as the ITU-R BS.2127 (EAR) lower-hemisphere construction, the no-triplet case as the largest-minimum-gain triplet, and non-finite positions as silence (algorithm layer) or last-good (engine); no doc describes a nearest-speaker fallback"
    reason: "Follows from accepting WR-03: the guide and skill describe the new snap accurately, which the docs-match-code prohibition requires."
    accepted_by: 'Andrew Rahman (session 2026-10-04, answer: "accept all five")'
    accepted_at: "2026-10-04T01:10:00Z"
  - must_have: "VBAPTriplet, LayoutContext, SpatializationAlgorithm and every signature OpenSpatialDelay compiles against are unchanged: the SpeakerLayout.h diff is comment lines only and [consumer-surface] passes (DR-3)"
    reason: "WR-02 and IN-03 are additive and source-compatible (no data member changed; void-to-bool return). [consumer-surface] passes and OSD 30391cd builds against HEAD."
    accepted_by: 'Andrew Rahman (session 2026-10-04, answer: "accept all five")'
    accepted_at: "2026-10-04T01:10:00Z"
  - must_have: "README.md, docs/integration-guide.md and SKILL.md keep CRLF on every line"
    reason: "IN-13: the guide and SKILL.md were flipped to LF in 8899bcf; restoring CRLF would hide the edits a second time, so LF is pinned in .gitattributes."
    accepted_by: 'Andrew Rahman (session 2026-10-04, answer: "accept all five")'
    accepted_at: "2026-10-04T01:10:00Z"
re_verification:
  previous_status: gaps_found
  previous_score: 88/93
  gaps_closed:
    - "02-01 Debug-only assert on hand-built empty triplets (IN-14): user accepted; override applied"
    - "02-04 Debug-only assert and clamp-to-silence on the no-enclosing-triplet path (WR-03): user accepted; override applied; D-06(b)/(c) amended in 02-CONTEXT.md"
    - "02-07 no doc describes a nearest-speaker fallback (follows WR-03): user accepted; override applied"
    - "02-08 VBAPTriplet / OSD signatures unchanged (WR-02, IN-03): user accepted; override applied"
    - "02-09 three docs keep CRLF (IN-13): user accepted; override applied"
    - "Advisory, Phase 1 criterion 4 issue-reference drift: resolved by 7bd1010; 0 unqualified references remain"
  gaps_remaining: []
  regressions: []
advisory:
  - finding: "The ROADMAP criterion 2 amendment and the 02-CONTEXT D-06 amendment give the wrong reason why the snap cannot happen through RenderEngine"
    category: other
    reason: "ROADMAP.md:77 says '(the layout build aborts on missing coverage)', and 02-CONTEXT.md D-06 amendment says RenderEngine's 'layout build enforces coverage with the D-02(a) abort'. The D-02a abort (RenderEngine.cpp:729-733) fires only when a height layout yields no regular triplets, or a flat layout yields some. It does not test coverage, and a layout with a partial hole would not abort. The amendments' conclusion is still true, because RenderEngine only builds the 15 shipped layouts (OutputFormat is a closed enum) and [ear][coverage] shows every sampled finite direction on all 8 height layouts is enclosed. Suggested wording: 'holds for every layout RenderEngine builds: the shipped layouts are fully covered ([ear][coverage]), and the layout build aborts if a height layout yields no triplets (D-02a)'. Planning text only; no code or must-have is affected."
    evidence_status: "code read (RenderEngine.cpp:722-733); [coverage] passes"
  - finding: "The DR-1 amendment in 02-CONTEXT.md says 'no audio-path diagnostic remains', which is true only for Phase 2's own diagnostics"
    category: other
    reason: "Phase 2's audio-path asserts are gone: no jassert remains in src/Algorithms or src/Core/SpatialMath.cpp. Older audio-path jasserts still exist: RenderEngine.cpp:261 (oversized block), BinauralRenderer.cpp:162 and :207, and PartitionedConvolver.cpp:127-128. They are Phase 3 and Phase 5 scope. Suggested wording: 'no Phase 2 audio-path diagnostic remains'. Planning text only."
    evidence_status: "grep -rn jassert src"
  - finding: "The five new overrides record accepted_at 2026-10-04T01:10:00Z, a few minutes after the commit that recorded them (3a74c4d, 01:07:00Z)"
    category: other
    reason: "Bookkeeping only. The acceptance itself is recorded in the coordinator's message. Optionally change accepted_at to 2026-10-04T01:07:00Z."
    evidence_status: "git log of 3a74c4d"
  - finding: "WR-08: DBAP clamps finite distance to [-1000, 1000], against the 02-04 prohibition's 'MUST NOT change the output for any finite position' wording"
    category: other
    reason: "This only changes output for |distance| > 1000, outside the normalised 0..1 range. The shift is about 2/d relative, which is inaudible. The user has seen WR-08 (backlog 999.2(b))."
    evidence_status: "code read (src/Algorithms/DBAPAlgorithm.cpp:21-45)"
  - finding: "Debug runs print 'Leaked objects detected: 1 instance(s) of class FFT' at exit"
    category: other
    reason: "This comes from the pre-existing process-global SharedFFTCache singleton. It is printed even when every test passes, and no Binaural or FFT file changed in Phase 2."
    evidence_status: "observed in the verifier's Debug runs"
human_verification: []
---

# Phase 2: Algorithm & Format Verification Verification Report

**Phase Goal:** Every algorithm and every advertised format is confirmed correct by a test, and the Ambisonics convention is written down.
**Verified:** 2026-10-04T01:08:24Z (HEAD 3a74c4d)
**Status:** passed
**Re-verification:** Yes. This pass follows the user's acceptance ("accept all five", 2026-10-04) of the five contract gaps found at 0add435.

## Summary

The phase goal is achieved. All 93 must-haves hold: 87 verified directly and 6 by user-accepted overrides. Each of the 5 new overrides matches its must-have word for word. The CONTEXT amendment of D-06(b)/(c) describes the code correctly. The ROADMAP criterion 2 amendment reaches the correct conclusion. The Debug suite rebuilt after 7bd1010 fails only at `HutubsPP2Tests.cpp:47`, and Release passes in full. 7bd1010 resolved the issue-reference advisory, so it is dropped.

Two sentences in the new amendments overstate the code: the ROADMAP/CONTEXT coverage reason and the CONTEXT DR-1 note. Both are planning text, not code, so they are recorded as advisories with replacement wording and do not block.

## What changed since the last pass (0add435 to 3a74c4d)

| Commit | Content | Verified |
|---|---|---|
| 7bd1010 | Comment-only: `#22` becomes `AndrewRahman/SpatialCore#22` at PanningLawTests.cpp:67 and :877; `SpatialCore#11` becomes `AndrewRahman/SpatialCore#11` at AmbisonicsCodecTests.cpp:172 | Diff read: 3 comment lines, no code. `grep -rhoE '[[:alnum:]/-]*#[0-9]+' include/ src/ tests/ \| grep -cv 'OpenSpatialDelay#\|AndrewRahman/SpatialCore#'` returns **0** (was 3). |
| 3a74c4d | Five overrides added to this file, D-06(b)/(c) and DR-1 amended in 02-CONTEXT.md, ROADMAP criterion 2 amendment added | See the next two sections |

No file under `src/` or `include/` changed. The source diff between 0add435 and HEAD is the 3 test comment lines.

## Override check (Step 3b)

| # | Override `must_have` | Plan must-have it matches | Match |
|---|---|---|---|
| 1 | 02-06 decode within 1e-6 | 02-06 truth 7 | Verbatim; carried from the last pass (UAT test 11) |
| 2 | "No VBAP, VBIP or MDAP source file contains a nearest-speaker branch; … Debug-only assert (D-01)" | 02-01 truth 6 | Verbatim |
| 3 | "When no triplet contains a finite direction, … a Debug-only assert marks the path (D-06b, D-06c)" | 02-04 truth 4 | Verbatim |
| 4 | "The docs describe below-horizon panning … no doc describes a nearest-speaker fallback" | 02-07 truth 3 | Verbatim except the trailing "(D-01, D-04, D-06, D-19)", well above the 80% token threshold |
| 5 | "VBAPTriplet, LayoutContext, SpatializationAlgorithm … [consumer-surface] passes (DR-3)" | 02-08 truth 9 | Verbatim |
| 6 | "README.md, docs/integration-guide.md and SKILL.md keep CRLF on every line" | 02-09 truth 4 | Verbatim |

Each reason matches the code as it stands:

- **Override 2.** `src/Algorithms/{VBAP,VBIP,MDAP}Algorithm.cpp` contain no `jassert`. The IN-14 test is at `VBAPTripletSelectionTests.cpp:821`.
- **Override 3.** `SpatialMath.cpp` contains no `jassert`. The nearest-speaker block is at 407-437. `[coverage]` passes.
- **Override 4.** The snap is described at guide :231-233 and skill :48.
- **Override 5.** The surface is additive. `[consumer-surface]` passes, and OSD built in the last pass. Nothing under `include/` or `src/` has changed since.
- **Override 6.** Line endings unchanged: the guide has 0 CRLF lines and SKILL.md 0, and `.gitattributes` pins them to LF.

## Amendment accuracy (02-CONTEXT.md, ROADMAP.md)

| Text | Accurate? | Evidence |
|---|---|---|
| D-06 amendment: "(b) still picks the largest-min-gain triplet, but if that candidate clamps to all zeros the nearest speaker now gets unity instead of silence" | ✓ Yes | `SpatialMath.cpp` fallback path, then the `usedFallback` nearest block (407-437). Pinned by the WR-03 test (`VBAPTripletSelectionTests.cpp:985`). |
| D-06 amendment: "(c) is withdrawn: … no assert runs in computeGains (VBAP/VBIP/MDAP or computeVBAPGains3D)" | ✓ Yes | `grep -rn jassert src/Algorithms src/Core/SpatialMath.cpp` finds only a comment. `[robust]` prints 0 "JUCE Assertion" lines in Debug. |
| D-06 amendment: "reachable only with a hand-built partial triplet list, never through RenderEngine" | ✓ Yes | `[coverage]`/`[ear]`: no finite direction reaches the fallback on any of the 8 shipped height layouts |
| D-06 amendment: "whose layout build enforces coverage with the D-02(a) abort" | ⚠️ Overstated | D-02a (`RenderEngine.cpp:729-733`) aborts only when the regular list is empty on a height layout or non-empty on a flat one. It does not check coverage; the shipped layouts' coverage is shown by `[ear][coverage]`. Advisory 1. |
| DR-1 amendment: "D-06(c) withdrawn, so no audio-path diagnostic remains" | ⚠️ True only for Phase 2's diagnostic | Pre-existing audio-path jasserts remain: `RenderEngine.cpp:261`, `BinauralRenderer.cpp:162` and `:207`, `PartitionedConvolver.cpp:127-128` (Phase 3/5 scope). Advisory 2. |
| ROADMAP criterion 2 amendment: "holds for every layout RenderEngine builds … A hand-built partial triplet list may snap to the nearest speaker without a diagnostic, because an audio-thread assert allocates" | ✓ Conclusion correct; ⚠️ the parenthetical "(the layout build aborts on missing coverage)" is overstated | Same as the CONTEXT coverage row. Advisory 1. |

## Test runs (this pass)

| Run | Result |
|---|---|
| `cmake --build build --target SpatialCoreTests -j8` | Rebuilt (the test sources changed), exit 0 |
| **Debug** `build/tests/SpatialCoreTests` | **201 test cases: 200 passed, 1 failed. 307,474 assertions: 307,473 passed, 1 failed.** The only failure is `tests/Binaural/HutubsPP2Tests.cpp:47`, the allowed pre-existing Phase 3 failure. |
| Release `build-release/tests/SpatialCoreTests` (rebuilt too) | All tests passed (307,474 assertions in 201 test cases) |

The totals are identical to the last pass, as expected for a comment-only test change. The full regression evidence from the last pass is still valid because no source changed since: bit-identical triplets for the 15 shipped layouts (old-vs-new `cmp`), EAR band pins, all three reference headers regenerating byte-identically, the `[ambi-pin]` +0.1% mutation proof (11 failures per build, 411 passes when reverted), the panning-law suite, D-02a abort placement, the fast-math `#error`, and the OSD 30391cd build.

## Goal Achievement

### ROADMAP Success Criteria

| # | Criterion | Status | Evidence |
|---|-----------|--------|----------|
| 1 | AmbisonicsCodec convention confirmed ACN/SN3D, stated in code and docs (SpatialCore#11) | ✓ VERIFIED | `[sn3d]` (49 scipy literals, addition theorem, parity); convention on `evalSH`, `SpatialMath.cpp`, `AmbisonicsCodec.h`, guide, README, skill |
| 2 | 3D triplet fallback no longer exists or fails loudly (amended 2026-10-04: holds for every layout RenderEngine builds) | ✓ VERIFIED | The empty-triplet nearest-speaker branches are deleted (`grep -rn nearestSpeaker src` is empty). On an engine-built layout the empty case aborts (D-02a). `[ear][coverage]`: no finite direction reaches the 3D fallback on shipped layouts. The off-facade snap is the user-accepted amendment. |
| 3 | 23 formats resolve; 15 layouts give channel indices and LFE | ✓ VERIFIED | `[format-resolve]`, `[io][layout]`, `[golden]` (Release 18/18; Debug 17/18, the HUTUBS case only) |
| 4 | Encode/decode round trip within tolerance to order 6 | ✓ VERIFIED | `[roundtrip]` |

### Plan must-have truths (89)

| Plan | Truths | Status |
|---|---|---|
| 02-01 | 11 | 10 ✓ VERIFIED, 1 PASSED (override 2) |
| 02-02 | 8 | 8 ✓ VERIFIED (generators regenerate byte-identically) |
| 02-03 | 3 | 3 ✓ VERIFIED (UAT test 4) |
| 02-04 | 11 | 10 ✓ VERIFIED, 1 PASSED (override 3). The fast-math precondition is enforced by `FloatSemanticsGuard.h`, so there is no coincidental reliance. |
| 02-05 | 10 | 10 ✓ VERIFIED (`[panning-law]`, `[textbook]`) |
| 02-06 | 10 | 9 ✓ VERIFIED, 1 PASSED (override 1) |
| 02-07 | 9 | 8 ✓ VERIFIED, 1 PASSED (override 4) |
| 02-08 | 9 | 8 ✓ VERIFIED, 1 PASSED (override 5) |
| 02-09 | 9 | 8 ✓ VERIFIED, 1 PASSED (override 6) |
| 02-10 | 9 | 9 ✓ VERIFIED (`[ambi-pin]` both builds; mutation proof) |

Per-truth evidence for every row is in the previous pass's table (commit 0add435). No source changed since, so that evidence still applies.

**Score:** 93/93 must-haves verified (87 VERIFIED + 6 PASSED (override)); 0 present-but-behavior-unverified.

### Deferred Items

| # | Item | Addressed In | Evidence |
|---|------|-------------|----------|
| 1 | CI has no arm64/FMA leg; CI's ctest step runs zero tests | Phase 6 | ROADMAP Phase 6 criterion 3 |
| 2 | HUTUBS PP2 golden checksum fails in Debug only | Phase 3 (EXTR-02) | `deferred-items.md` |
| 3 | Listening checks (band; WR-05 gap; WR-08 DBAP) | Backlog 999.1, 999.2 | User deferred in UAT |

### Advisory (New Scope, Unevidenced)

| # | Finding | Category | Why Advisory |
|---|---------|----------|--------------|
| 1 | The ROADMAP/CONTEXT amendments say the layout build "enforces coverage" with the D-02a abort; it only catches an empty or mismatched triplet list | other | Planning wording; the conclusion is true via `[ear][coverage]`. Replacement wording is in the frontmatter. |
| 2 | The DR-1 amendment says "no audio-path diagnostic remains"; older binaural and oversized-block jasserts remain | other | Planning wording; Phase 3/5 scope |
| 3 | The overrides' `accepted_at` (01:10Z) is after their recording commit (01:07Z) | other | Bookkeeping |
| 4 | DBAP finite-distance clamp at ±1000 (WR-08) | other | Outside 0..1; inaudible; user aware |
| 5 | Debug FFT leak line at exit | other | Pre-existing singleton |

The Phase 1 issue-reference advisory is **resolved** (7bd1010; the count is 0) and has been dropped.

### Required Artifacts / Key Links / Data Flow

Unchanged since the last pass: `verify.artifacts` gives 31 of 32, and the one miss is the 02-01 `lowerHemisphere` pattern, which moved into `VBAPTriplet::kind()` (substantive and wired, covered by override 5's WR-02 change). `verify.key-links` gives 24 of 24. The data flow from `setOutputFormat` to `activateLayout` to the triplets and decode matrix to `computeGains` is ✓ FLOWING.

### Requirements Coverage

| Requirement | Status | Evidence |
|---|---|---|
| EXTR-01 | ✓ SATISFIED | Algorithm and panning-law suites pass in both builds; SC2 as amended |
| EXTR-03 | ✓ SATISFIED | SC3, SC4, `[ambi-pin]` |
| VERIFY-01 | ✓ SATISFIED | SC1 |

No orphaned requirements. Tracking note: REQUIREMENTS.md still shows EXTR-01 and EXTR-03 as `[ ]` / "Largely verified by tests" (lines 138, 148, 427, 428). Set both to complete by hand; the `mark-complete` tool cannot flip that status text.

### Anti-Patterns Found

| File | Line | Pattern | Severity | Impact |
|---|---|---|---|---|
| (all non-planning files changed by the phase, including 7bd1010's) | | TBD / FIXME / XXX / TODO / HACK | none | Debt-marker gate clear |

### Human Verification Required

None. `02-UAT.md` is complete (11 passed, 1 skipped and deferred to backlog 999.1, 0 pending). The user resolved the five contract gaps with "accept all five" (2026-10-04), and those decisions are recorded as overrides above.

### Gaps Summary

No gaps remain. All five contract gaps from the previous pass are closed by user-accepted overrides, and the CONTEXT and ROADMAP now record the decision. The two wording overstatements in those amendments (advisories 1 and 2) are optional one-line edits to planning text.

---

_Verified: 2026-10-04T01:08:24Z_
_Verifier: Claude (gsd-verifier)_
