---
phase: 02-algorithm-format-verification
verified: 2026-10-04T00:50:10Z
status: gaps_found
score: 88/93 must-haves verified
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
covered_digest: "v2:sha256:03dae1ac058c7cda2e5cd3c81f0add794726da6fddcb1e02e3938cf141c559c5"
behavior_unverified: 0
overrides_applied: 1
overrides:
  - must_have: "RenderEngine has no private SH decoder: activateLayout fills ambiDecodeMatrix through AmbisonicsCodec::getDecodeMatrix at order 3, and each of the 15 speaker layouts gets a decode within 1e-6 of the pre-change private function (02-06)"
    reason: "The 1e-6 bound was never achievable on arm64 Release (G-02-10). Plan 02-10 replaced it with the derived 2.5e-5 bound plus a double-precision anchor. The user accepted the 2.5e-5 bound in place of 1e-6 in UAT test 11."
    accepted_by: "Andrew Rahman (02-UAT.md test 11, answer: yes)"
    accepted_at: "2026-10-04T00:45:00Z"
re_verification:
  previous_status: human_needed
  previous_score: 88/88
  gaps_closed:
    - "Both human items from the previous report are resolved in 02-UAT.md (status complete): the 2.5e-5 [ambi-pin] bound was accepted (test 11, 'yes') and the four 02-10 judgment-tier prohibitions were accepted (test 12, 'accept')"
    - "WR-06 (advisory last time): [ambi-pin] now fails on non-finite decode entries (accumulateWorstFinite in tests/TestNumerics.h)"
    - "WR-04 coincidental-reliance (advisory last time): the no-fast-math precondition is now enforced. src/Core/FloatSemanticsGuard.h raises #error under -ffast-math or -ffinite-math-only (checked by the verifier), and every src/ file with an isfinite/isnan test includes it"
  gaps_remaining: []
  regressions:
    - "02-01: 'a hand-built LayoutContext with empty triplets on a height layout trips a Debug-only assert'. The assert was removed by IN-14 (8ae2eed)"
    - "02-04: 'a Debug-only assert marks the [no-enclosing-triplet] path'. The assert was removed by WR-03 (8899bcf), and a nearest-speaker snap was added for the case where the D-06b candidate clamps to zero"
    - "02-07: 'no doc describes a nearest-speaker fallback'. The integration guide and the skill now describe the WR-03 nearest-speaker snap, accurately"
    - "02-08: 'VBAPTriplet ... unchanged: the SpeakerLayout.h diff is comment lines only'. WR-02 (2ae900c) added the public enum VBAPTriplet::Kind and the member function kind()"
    - "02-09: 'README.md, docs/integration-guide.md and SKILL.md keep CRLF on every line'. The guide and the skill are now LF (0 of 306 and 0 of 583 lines are CRLF), and IN-13 (52a2cb5) pinned them to LF in .gitattributes"
gaps:
  - truth: "02-01: No VBAP, VBIP or MDAP source file contains a nearest-speaker branch; a hand-built LayoutContext with empty triplets on a height layout trips a Debug-only assert (D-01)"
    status: failed
    reason: "IN-14 (8ae2eed) deliberately removed the jassert from all three computeGains functions, because JUCE's assert path logs and allocates on the audio thread. A hand-built empty-triplet context on a height layout now pans by 2D VBAP and gives no diagnostic in any build. Nothing failed in function. The pinned contract changed and no human decision on it is recorded. The first clause still holds: grep finds no nearest-speaker code in the three files."
    artifacts:
      - path: "src/Algorithms/VBAPAlgorithm.cpp"
        issue: "lines 13-19: comment explains why there is no assert; the jassert is gone (same in VBIPAlgorithm.cpp and MDAPAlgorithm.cpp)"
      - path: "tests/Core/VBAPTripletSelectionTests.cpp"
        issue: "line 821: [robust] IN-14 test pins the new assert-free 2D-pan behaviour"
    missing:
      - "A recorded human decision: accept the assert removal (add the suggested override) or restore the Debug-only assert"
  - truth: "02-04: When no triplet contains a finite direction, computeVBAPGains3D uses the triplet with the largest minimum gain, clamps negatives to 0 and renormalises; an empty triplet list returns silence; a Debug-only assert marks the path (D-06b, D-06c)"
    status: failed
    reason: "WR-03 (8899bcf) removed the jassertfalse (D-06c), and when the D-06b candidate clamps to all zeros it now gives unity on the nearest speaker. That is the pre-Phase-2 snap D-06(b) says to 'replace', and the fixer's own report says 'This changes behaviour, so it needs a human to confirm it'. No confirmation is recorded: the 2026-10-04 UAT deferred listening checks for WR-05 and WR-08 only (ROADMAP 999.2), not WR-03. The snap is unreachable through RenderEngine ([ear][coverage]: every sampled finite direction is enclosed by the engine-built list). It is reachable with a hand-built partial list: per the WR-03 test, 27,885 of 64,800 below-horizon directions on regular-only lists now snap to one speaker, where before they gave silence plus a Debug assert."
    artifacts:
      - path: "src/Core/SpatialMath.cpp"
        issue: "lines 338-348 (assert removed, comment), 407-437 (new nearest-speaker block, stack-only, no assert)"
      - path: ".planning/phases/02-algorithm-format-verification/02-CONTEXT.md"
        issue: "D-06(b) 'Replace the snap', D-06(c) 'jassertfalse in Debug only' and the DR-1 note ('the one diagnostic permitted') still describe the old design"
    missing:
      - "A recorded human decision on WR-03: accept the nearest-speaker snap and the assert removal for hand-built lists (add the suggested override and amend D-06(b)/(c) in CONTEXT), or revert to the D-06b clamp plus a diagnostic"
  - truth: "02-07: The docs describe below-horizon panning on height layouts as the ITU-R BS.2127 (EAR) lower-hemisphere construction, the no-triplet case as the largest-minimum-gain triplet, and non-finite positions as silence (algorithm layer) or last-good (engine); no doc describes a nearest-speaker fallback (D-01, D-04, D-06, D-19)"
    status: failed
    reason: "This follows from the WR-03 change. docs/integration-guide.md:231-233 and .claude/skills/spatial-audio-dsp/SKILL.md:48 now say 'if that clamps to all zeros, the nearest speaker gets unity'. The docs match the code, so the 'docs must not describe code differently' prohibition holds. The must-have's 'no nearest-speaker fallback' clause does not. It resolves together with the 02-04 gap."
    artifacts:
      - path: "docs/integration-guide.md"
        issue: "lines 231-233"
      - path: ".claude/skills/spatial-audio-dsp/SKILL.md"
        issue: "line 48"
    missing:
      - "Same decision as the 02-04 gap: an override if WR-03 is accepted, or a doc revert if it is reverted"
  - truth: "02-08: VBAPTriplet, LayoutContext, SpatializationAlgorithm and every signature OpenSpatialDelay compiles against are unchanged: the SpeakerLayout.h diff is comment lines only and [consumer-surface] passes (DR-3)"
    status: failed
    reason: "WR-02 (2ae900c) added a public nested enum VBAPTriplet::Kind and a public const member function kind(). IN-03 (b87eac0) changed AmbisonicsCodec::getDecodeMatrix's return type from void to bool, and the fixer's report says 'The public return type changed, so it needs a human to confirm it'. Both changes are additive and source-compatible for call sites. The intent of DR-3 holds: no data member changed, field-by-field construction still works, [consumer-surface] passes (16 assertions, both builds), and OSD 30391cd's OpenSpatialDelay and OpenSpatialDelayTests targets build against HEAD (exit 0, OSD repo status clean). The literal 'unchanged' clause, and 02-08's prohibition on adding a public member function, no longer hold at HEAD."
    artifacts:
      - path: "include/SpatialCore/IO/SpeakerLayout.h"
        issue: "lines 52-63: enum class Kind and kind()"
      - path: "include/SpatialCore/IO/AmbisonicsCodec.h"
        issue: "getDecodeMatrix now returns bool"
    missing:
      - "A recorded human decision: accept the additive API changes (override; consider whether CLAUDE.md's versioning rule wants a minor bump at release), or revert"
  - truth: "02-09: README.md, docs/integration-guide.md and SKILL.md keep CRLF on every line"
    status: failed
    reason: "8899bcf (WR-03) converted the guide and SKILL.md from CRLF to LF as a side effect of the editing tool. IN-13 (52a2cb5) then chose to keep LF and pinned it in .gitattributes (text eol=lf) rather than restore CRLF. Measured now: README 144/144 CRLF, guide 0/306, SKILL.md 0/583 (include/SpatialCore/IO/AmbisonicsCodec.h, also flipped, 0/50). Cosmetic, and deliberate per IN-13. The must-have no longer holds."
    artifacts:
      - path: ".gitattributes"
        issue: "lines 2-7 pin LF for the guide, SKILL.md and AmbisonicsCodec.h"
    missing:
      - "A recorded human decision: accept LF (override) or restore CRLF and drop the .gitattributes rule"
advisory:
  - finding: "WR-08: DBAP now clamps finite distances to [-1000, 1000]. That is a guard acting on a finite value, against the wording of the 02-04 prohibition 'MUST NOT change the output for any finite position'"
    category: other
    reason: "This only changes DBAP output for |distance| > 1000, which is outside the normalised 0..1 range. Above about 1e18 such distances used to give silence. By the 1/d^2 weight analysis the shift is about 2/d relative (about 0.002 at d = 1000), which is inaudible. The user has seen WR-08 (UAT deferred follow-up 999.2(b)). It is recorded so the 02-04 wording can be amended."
    evidence_status: "code read (src/Algorithms/DBAPAlgorithm.cpp:21-45); no test asserts the >1000 regime against the old output"
  - finding: "Phase 1 criterion 4 drift: three issue references are still not in Owner/Repo#N form"
    category: other
    reason: "Carried over unchanged. tests/Algorithms/PanningLawTests.cpp:67 and :877 (bare #22) and tests/IO/AmbisonicsCodecTests.cpp:172 (SpatialCore#11, owner missing). All three name SpatialCore's own tracker. The fix is three one-token comment edits."
    evidence_status: "reproduced by verifier (grep returns 3)"
  - finding: "Debug runs print 'Leaked objects detected: 1 instance(s) of class FFT' at exit"
    category: other
    reason: "This is the process-global SharedFFTCache singleton. It is printed even when every test passes (exit 0 with HUTUBS excluded). No Binaural or FFT file changed in Phase 2. The iteration-2 fix report noted it as 'printed as before'. Not a Phase 2 item."
    evidence_status: "observed in verifier's Debug runs"
human_verification: []
---

# Phase 2: Algorithm & Format Verification Verification Report

**Phase Goal:** Every algorithm and every advertised format is confirmed correct by a test, and the Ambisonics convention is written down.
**Verified:** 2026-10-04T00:50:10Z (HEAD 6134e57)
**Status:** gaps_found
**Re-verification:** Yes. The previous report (2026-10-03T22:30Z, human_needed, 88/88) went stale when the four code-review fix rounds changed library code.

## Summary in plain English

Nothing broke. Every test passes in both build types, apart from the one known Phase 3 failure. The sound through RenderEngine is unchanged on every shipped layout. The four ROADMAP success criteria hold.

But the review fixes changed five things that the phase's own plans had promised. Each change was made on purpose and each is documented. None was signed off by you:

1. **The engine's "safety net" behaviour for hand-built setups changed (WR-03, IN-14).** If a programmer bypasses RenderEngine and hands VBAP an incomplete set of speaker triangles, some directions now snap to the single nearest speaker, and nothing warns in any build. Before, that case was silent but a debug-build warning fired. The plans and the locked decision D-06 said "no nearest-speaker snap" and "a debug-only warning marks this path". The fixer itself wrote that this "needs a human to confirm it", and I found no record that anyone did. This cannot happen through RenderEngine, which is how OpenSpatialDelay uses the library.
2. **The docs now describe that nearest-speaker snap.** They describe it accurately, but a plan promised no doc would.
3. **Two small additions to the public programming interface (WR-02, IN-03).** OpenSpatialDelay still builds against it; I checked.
4. **Two doc files switched line-ending style** (CRLF to LF). This is cosmetic, and a review fix chose to keep it.

To close the phase, you decide for each one: **accept it** (I have written the five "override" entries below, ready to paste in) or **undo it**. My recommendation is to accept all five. The behaviour is better than before in every case: a sound is never silent where it used to be. But accepting overturns a decision you locked in CONTEXT (D-06), so it should be your call, not the fixer's.

## Test runs (my own, this pass, at HEAD 6134e57)

| Run | Result |
|---|---|
| `cmake --build build --target SpatialCoreTests -j8` and the same for `build-release` | exit 0, both up to date. Caches confirmed `CMAKE_BUILD_TYPE` Debug and Release. No file under `src`, `include`, `tests` or `CMakeLists.txt` is newer than either binary. |
| **Debug**, `build/tests/SpatialCoreTests` (full) | **201 test cases: 200 passed, 1 failed. 307,474 assertions: 307,473 passed, 1 failed.** The only failure is `tests/Binaural/HutubsPP2Tests.cpp:47` (got `0x7a35c1f848c2a410`, expected `0x9e3c2875eeade4b7`). That is pre-existing Phase 3 debt, Debug only. Exit 42 (Catch2's failure code). |
| Debug, HUTUBS case excluded | All tests passed (307,471 assertions in 200 test cases), exit 0 |
| **Release**, `build-release/tests/SpatialCoreTests` (full) | **All tests passed (307,474 assertions in 201 test cases)**, exit 0 |
| Release, `ctest --test-dir build-release/tests` | 100% passed, 201 of 201 |
| After the mutation check below (rebuild from the reverted source) | Release full: 201/201 again. Debug full: 200/201 again (HUTUBS only). |

These match the orchestrator's figures (Debug 200/201, Release 201/201). The baseline grew from 193 to 201 cases because the review rounds added tests.

### Phase 2 tag sweep (each run alone, both trees)

Every tag passed in both builds with identical assertion counts:

| Tag | Assertions / cases |
|---|---|
| `[g02-2]` | 203 / 1 |
| `[band]` | 828 / 3 |
| `[vbap3d-identity]` | 262,104 / 1 |
| `[consumer-surface]` | 16 / 1 |
| `[ear]` | 19,431 / 12 |
| `[io][layout]` | 19,290 / 7 |
| `[panning-law]` | 8,117 / 10 |
| `[sn3d]` | 208 / 3 |
| `[roundtrip]` | 12 / 1 |
| `[tracer]` | 114 / 3 |
| `[ambi-pin]` | 411 / 3 |
| `[format-resolve]` | 695 / 1 |
| `[robust]` | 6,340 / 8 |
| `[sanitize]` | 27 / 6 |
| `[gap]` | 45 / 1 |
| `[decode-guard]` | 51 / 2 |
| `[coverage]` | 32 / 1 |
| `[textbook]` | 90 / 2 |

`[golden]` passes in Release (1,233 / 18). In Debug it is 17 of 18, and the one failure is the HUTUBS case.

## The specific regression checks requested

| Check | Method | Result |
|---|---|---|
| **15 shipped layouts bit-identical** (WR-05 gap bridge) | Standalone probe in `/tmp/p02v_bitid`, outside the repo. It compiles `src/IO/SpeakerLayout.cpp` and `include/` from f6ddd2d (the last verified tree) and from HEAD, and dumps every triplet (indices, the raw bytes of the `inv` matrix, `lowerHemisphere`, `nadirVertex`, `nadirMask`, the raw bytes of `nadirGain`) for all 15 layouts after `buildVBAPTripletsForLayout` + `appendLowerHemisphereTriplets`. | **`cmp`: BIT-IDENTICAL** (1,357 lines each). The 6 flat and 1 octaphonic layouts give 0 triplets. The height layouts give 7.1.4 118 + 14, 9.1.6 355 + 18, and so on, unchanged. |
| **EAR band pins** | `[ear]` (includes `[ear][golden][band]`), `[band]`, `[g02-2]` in both builds; regenerate-diff of all three reference generators via `.context/venv` | All pass. `gen_sh`, `gen_ear` and `gen_panning` all regenerate **byte-identical** headers (`cmp`). ShReference.h changed only in a comment (IN-02), and its generator changed in the same way. |
| **Above-horizon unchanged** | `[vbap3d-identity]` | 262,104 assertions pass in both builds. |
| **[ambi-pin]** | Both builds, plus my own mutation check: Tikhonov `epsilon 0.01f` changed to `0.01001f` (+0.1%) at `src/IO/AmbisonicsCodec.cpp:78`, rebuild, run, `git checkout`, rebuild, rerun | Mutated: **11 of 411 assertions fail in both Debug and Release** (5 at RenderEngineTests.cpp:893, 5 at :894, 1 at :901: the per-layout pin, the double-precision anchor, and the all-layout check). Reverted: 411 pass in both. `git status` clean afterwards. The pin still discriminates after the WR-06 and IN-07 to IN-11 test edits. |
| **Panning-law suite** | `[panning-law]` both builds | 8,117 assertions pass. IN-12 makes every Ambisonics law fail on a silent decode (per the fix report; the suite passes). |
| **D-02a abort** | Code read | The only `std::abort()` in `src/` is `RenderEngine.cpp:732`, inside `activateLayout`, which is called only from `setOutputFormat` (line 628). `renderBlock` (121-176) contains no `setOutputFormat`, `activateLayout` or `abort`. The guard lines are unchanged since UAT test 8 observed the SIGABRT. The only edit to `activateLayout` is the getDecodeMatrix bool check, which is placed before the guard. The line moved from 729 to 732. |
| **Docs match code** | grep and read | The guide, README and skill all keep the convention (Condon-Shortley, ACN, SN3D, +Y left), DAFx-98, SpatialCore#20, WASPAA 1999, 12.04, BS.2127, "one pan region" and "PyPI ear 2.1.0". No VBIP text says "squares" or "tighter"; SKILL.md:562 lists squaring only as an anti-pattern. The new WR-03, WR-05 and WR-08 text matches the code: guide 231-236, skill 48, `DBAPAlgorithm.h`. See gap 02-07 for the one clause that now fails. |
| **DR-3: OSD still builds** | `cmake --build /tmp/osd-dr3-check/build --target OpenSpatialDelay OpenSpatialDelayTests` (OSD 30391cd via git archive, SpatialCore symlinked to this worktree) | exit 0. Up to date: its `libSpatialCore.a` was built at 01:48:58, after the last source change at 01:48:15. `git -C ~/conductor/repos/openspatialdelay-v1 status --porcelain` is empty. |
| **Fast-math guard (WR-04)** | `clang++ -ffast-math` / `-ffinite-math-only` on a TU that includes `FloatSemanticsGuard.h` | `#error` fires under both flags, and compiles cleanly without them. All 8 `src/` files that test isfinite/isnan include the guard. |

## Goal Achievement

### ROADMAP Success Criteria

| # | Criterion | Status | Evidence |
|---|-----------|--------|----------|
| 1 | AmbisonicsCodec channel order and normalisation confirmed ACN/SN3D or FuMa, stated in code and docs (SpatialCore#11) | ✓ VERIFIED | `[sn3d]` (208 assertions, 49 scipy literals, addition theorem, parity) passes in both builds, and ShReference.h regenerates byte-identically. The convention is stated on `evalSH`, in `SpatialMath.cpp:12-16`, on `AmbisonicsCodec.h` and in the guide, README and skill. IN-02 reworded "+Y (left)" to "the listener's left (AmbiX +Y; SpatialCore's internal +x)". The meaning is unchanged and clearer. |
| 2 | The 3D triplet fallback in VBAP/VBIP/MDAP either no longer exists or fails loudly instead of silently degrading to nearest-speaker | ✓ VERIFIED (consumer path), with the caveat in gap 02-04 | `grep -rn nearestSpeaker src` is empty. The empty-triplet nearest-speaker branches (D-01) stay deleted. For an engine-built layout the empty case aborts in every build (D-02a, `RenderEngine.cpp:732`, loud), and `[ear][coverage]` shows no finite direction reaches the 3D fallback. **Caveat:** off the facade (a hand-built partial triplet list, which CLAUDE.md SC-13 and UAT test 3 put outside the guarantee), WR-03 reintroduced a silent snap to the nearest speaker for directions whose D-06b candidate clamps to zero. I count SC2 as met because the project scopes it to the consumer path, as the previous verification and UAT test 3 did. The reversal of D-06(b)/(c) itself is gap 02-04. |
| 3 | All 23 OutputFormat entries resolve; all 15 layouts return populated channel indices and LFE placement | ✓ VERIFIED | `[format-resolve]` (695), `[io][layout]` (19,290) and `[golden]` pass. The 15 layouts' triplets are bit-identical to the last verified tree (probe above). |
| 4 | Ambisonics encode/decode round-trips within tolerance at every order up to 6 | ✓ VERIFIED | `[roundtrip]` passes in both builds. The limit is unchanged: the order-6 decode half is test-local (D-11/D-12). |

### Plan must-have truths (89)

Status of every truth against HEAD. Truths that passed in the previous report and whose code and tests did not change in a way that touches them were re-checked through the tag sweep, the full suites and the generator diffs above. Rows below are the ones the review fixes touched, plus every non-VERIFIED row.

| Plan | Truth (short) | Status | Evidence |
|---|---|---|---|
| 02-01 | EAR nadir 1/sqrt(7) at el -90 on 7.1.4; meridian gain 1 | ✓ VERIFIED | `[tracer]` 114 |
| 02-01 | Regular first, bit-identical above the horizon | ✓ VERIFIED | `[vbap3d-identity]`. `kind() == regular` is equivalent to the old `!lowerHemisphere` (`SpeakerLayout.h:56-63`). |
| 02-01 | Lower triplets built by `appendLowerHemisphereTriplets`, virtual vertices never in a real slot | ✓ VERIFIED | Builder read. WR-05 bridge vertices are virtual and downmix onto real targets. |
| 02-01 | One shared 0.0175 rad threshold | ✓ VERIFIED | `SpeakerLayout.cpp:96` |
| **02-01** | **No nearest-speaker branch in VBAP/VBIP/MDAP; a hand-built empty context trips a Debug-only assert (D-01)** | **✗ FAILED** | First clause holds. Second clause: the asserts were removed by IN-14 (8ae2eed). See gaps. |
| 02-01 | D-02a abort, every build type, not reachable from renderBlock (backstop) | ✓ VERIFIED | UAT test 8 observed it. Guard lines unchanged since then. Placement re-read: line 732, `setOutputFormat` only. |
| 02-01 | `setOutputFormat` void; OSD-facing symbols compile | ✓ VERIFIED | `[consumer-surface]`, OSD build exit 0 |
| 02-01 | Tolerance -1e-6f | ✓ VERIFIED | `SpatialMath.cpp` |
| 02-01 | `kLayoutExpectations` tied to `NUM_LAYOUT_DEFS` | ✓ VERIFIED | `[io][layout]` |
| 02-01 | Binaural/Ambisonics never call computeVBAPGains | ✓ VERIFIED | grep |
| 02-02 | 8 oracle truths | ✓ VERIFIED | All three generators regenerate byte-identically. The headers compile in the suite. |
| 02-03 | 3 truths (#22 filed after approval) | ✓ VERIFIED | Carried (UAT test 4). Not touched by the review. |
| 02-04 | Non-finite / huge inputs: finite gains, power 0 or 1, under the watchdog | ✓ VERIFIED | `[robust]` 6,340. DBAP is now never silent (WR-08), which still satisfies "0 or 1". |
| 02-04 | VBAP 2D/3D silence for non-finite; bounded wrap | ✓ VERIFIED | `SpatialMath.cpp:141`, `:273` guards. `[robust]` |
| 02-04 | KNN and DirectBinaural silence for non-finite | ✓ VERIFIED | Unchanged. Guards in place. |
| **02-04** | **No enclosing triplet: largest-min-gain, clamp, renormalise; empty is silent; a Debug-only assert marks the path (D-06b, D-06c)** | **✗ FAILED** | The assert was removed and a nearest-speaker snap was added when the clamp gives zero (WR-03, 8899bcf). Largest-min-gain and empty-is-silent still hold (test at `VBAPTripletSelectionTests.cpp:928`). See gaps. |
| 02-04 | Engine hold-last-good per object | ✓ VERIFIED | `[sanitize]` 27 |
| 02-04 | NaN distance never reaches the NFC-HOA state | ✓ VERIFIED | `[sanitize]` |
| 02-04 | Finite inputs untouched by guards; isfinite only in .cpp, safe from OSD's fast-math | ✓ VERIFIED | **Upgraded from coincidental-reliance.** The precondition is now enforced by `FloatSemanticsGuard.h`'s `#error` (checked). One wording exception, DBAP's finite clamp above 1000, is recorded under advisory. |
| 02-04 | Nadir-cap ear pins; meridian | ✓ VERIFIED | `[ear]` |
| 02-04 | Horizon continuity at most 0.01 | ✓ VERIFIED | `[ear]` |
| 02-04 | Coverage: every finite direction contained, no elevated leak below | ✓ VERIFIED | `[coverage]`, `[ear]`. This is what keeps WR-03's snap off the engine path. |
| 02-04 | Tolerance -1e-6f | ✓ VERIFIED | |
| 02-05 | 10 panning-law truths (textbook VBIP, laws for all 8, unity, continuity, textbook pins, height coverage, DBAP doc 12.04, MDAP WASPAA 1999, DirectBinaural properties) | ✓ VERIFIED | `[panning-law]` 8,117, `[textbook]` 90. VBIP and MDAP lost only the jassert (IN-14, comment only otherwise). DBAP's docblock still states 12.04 dB, no blur, 0.001 clamp, and adds the WR-08 rules. |
| 02-06 | evalSH single implementation; 49 literals; addition theorem; parity; round trip; convention stated | ✓ VERIFIED | `[sn3d]`, `[roundtrip]` |
| 02-06 | Order-3 decode via getDecodeMatrix, within 1e-6 of the pre-change function | PASSED (override) | Override: superseded by 02-10's derived 2.5e-5 bound. Accepted by the user in UAT test 11 on 2026-10-04. `[ambi-pin]` passes at 2.5e-5. |
| 02-06 | getDecodeMatrix writes nothing for >16 or 0 speakers, writes a finite decode for 16 | ✓ VERIFIED | `[decode-guard]` 51. IN-03 now also returns `false` for >16 or a bad order, and `true` with no write for 0. The "writes nothing" behaviour is unchanged. |
| 02-06 | 23 formats resolve; row and channel order pinned | ✓ VERIFIED | `[format-resolve]`, `[ambi-pin]` |
| 02-07 | Docs state the convention; textbook VBIP; DBAP/MDAP citations; nearestSpeaker3DFallback comment-deprecated; tie-break min-sum cites the issue | ✓ VERIFIED | grep table above. `SpatialMath.h:37-45` keeps the deprecated inline helper. |
| **02-07** | **Docs describe below-horizon as EAR … no doc describes a nearest-speaker fallback** | **✗ FAILED** | `docs/integration-guide.md:231-233` and `SKILL.md:48` describe the WR-03 snap, accurately. See gaps. |
| 02-07 | OSD 30391cd compiles and links (DR-3) | ✓ VERIFIED | Rebuilt this pass, exit 0 |
| 02-07 | Full suite passes except HUTUBS; every Phase 2 tag runs | ✓ VERIFIED | Above |
| 02-07 | Cross-repo follow-ups recorded, not performed | ✓ VERIFIED | Carried |
| 02-08 | 7.1.4 az ±60 hold 0.7071/0.7071 from 0 to -30 | ✓ VERIFIED | `[g02-2]` 203 |
| 02-08 | Exactly 2n lower triplets, no trapezoid triangle | ✓ VERIFIED (shipped layouts) | `[io][layout][ear]`. The probe shows the counts unchanged. A consumer layout with a gap of 179 degrees or more now gets 2(n + 2g), documented in the header and pinned by `[gap]`. Before WR-05 it got fewer than 2n (holes), so this extends the truth and does not break it. |
| 02-08 | Regular, then cap, then wedge; exactly one wedge per band direction | ✓ VERIFIED | `selectionTier` via `kind()`. `[band]` |
| 02-08 | Band equals the double-precision pair pan on all 8 layouts | ✓ VERIFIED | `[band]` 828 |
| 02-08 | 26 band pins vs ear 2.1.0, regenerate-diff empty, 13 cap pins unchanged | ✓ VERIFIED | Regenerated this pass, identical |
| 02-08 | Band and seam continuity at most 0.01 | ✓ VERIFIED | `[panning-law]` |
| 02-08 | Above horizon bit-identical (#22 untouched) | ✓ VERIFIED | `[vbap3d-identity]` |
| 02-08 | Tolerance -1e-6f every pass | ✓ VERIFIED | |
| **02-08** | **VBAPTriplet / LayoutContext / SpatializationAlgorithm / OSD signatures unchanged; SpeakerLayout.h diff comment-only (DR-3)** | **✗ FAILED (literal; DR-3 intent holds)** | WR-02 added `VBAPTriplet::Kind` and `kind()`. IN-03 changed getDecodeMatrix void to bool. No data member changed, `[consumer-surface]` passes and OSD builds. See gaps. |
| 02-09 | Guide, README and skill state the band, the 5.1.4 exception and #22 above the horizon only | ✓ VERIFIED | grep: "one pan region", "PyPI ear 2.1.0", "above the horizon", SpatialCore#22 |
| **02-09** | **README, guide and SKILL.md keep CRLF on every line** | **✗ FAILED** | Guide 0/306 and SKILL.md 0/583 lines are CRLF; README 144/144 is. IN-13 pinned LF. See gaps. |
| 02-09 | WR-01 fixed in the disposition ledger | ✓ VERIFIED | The ledger now reads 0 open of 25. WR-01 is `fixed`. |
| 02-09 | Phase gate; DR-3; follow-ups; one yes/no question | ✓ VERIFIED | Above / carried |
| 02-10 | `[ambi-pin]` passes in Release and Debug within 2.5e-5 | ✓ VERIFIED | 411 / 3 in both builds |
| 02-10 | Bound derived, comment records the derivation | ✓ VERIFIED | IN-09 tidied the comment, and IN-07 names the 5 resolving layouts |
| 02-10 | Float vs double anchor within 4.0e-5 | ✓ VERIFIED | Passes. It tripped under my mutation (:894). |
| 02-10 | +0.1% epsilon fails in both builds and passes when reverted | ✓ VERIFIED | **Re-proved this pass:** 11 failed in each build, 411 pass after the revert |
| 02-10 | Library decoder and verbatim reference unchanged as of 02-10 | ✓ VERIFIED | Scoped to 02-10's commits (accepted in UAT test 12). After 02-10, IN-03 touched `AmbisonicsCodec.cpp` (bool return, order guard, and `if (M > MAX_AMBI_CHANNELS)` replaced by `order > MAX_AMBI_ORDER`, which is equivalent). The numeric body is unchanged, and `[ambi-pin]` and the mutation check confirm it. |
| 02-10 | Phase gate both build types; recurrence guard; deferred items; yes/no question | ✓ VERIFIED | Above / carried |

**Score:** 88/93 must-haves verified: 4 ROADMAP criteria plus 89 plan truths, including 1 PASSED (override). 5 FAILED. 0 present-but-behavior-unverified.

The previous report's 88 used a different denominator, which deduplicated and superseded some items. Here every plan truth is counted once and the ROADMAP criteria are counted separately.

### Deferred Items

None of the five gaps is covered by a later phase. Phase 5 (realtime safety) concerns audio-path allocation, not whether these contract changes are accepted. The carried items are the same as last time:

| # | Item | Addressed In | Evidence |
|---|------|-------------|----------|
| 1 | CI has no arm64/FMA leg; CI's ctest step runs zero tests | Phase 6 | ROADMAP Phase 6 criterion 3 |
| 2 | HUTUBS PP2 golden checksum fails in Debug only | Phase 3 (EXTR-02, deferred-items.md) | Not a Phase 2 truth |
| 3 | Listening checks: test 7 (band) and post-review (WR-05 gap, WR-08 DBAP) | Backlog 999.1, 999.2 | User deferred in UAT |

### Advisory (New Scope, Unevidenced)

| # | Finding | Category | Why Advisory |
|---|---------|----------|--------------|
| 1 | DBAP clamps finite distance to ±1000 (WR-08), against the 02-04 "finite inputs untouched" wording | other | Only |d| > 1000, outside 0..1. The shift is about 2/d relative and inaudible. The user has seen WR-08. |
| 2 | Three unqualified issue references (Phase 1 criterion 4) | other | Carried, cosmetic |
| 3 | Debug FFT leak-detector line at exit | other | Pre-existing singleton, not Phase 2 |

### Required Artifacts

`gsd-tools verify.artifacts`: 31 of 32 pass. The one miss is mechanical. 02-01 expects `src/Core/SpatialMath.cpp` to contain `lowerHemisphere`, and WR-02 moved that read into `VBAPTriplet::kind()` (`SpeakerLayout.h:56-63`), which `SpatialMath.cpp:255` and `:362` call. The logic is substantive and wired: `[vbap3d-identity]`, `[tracer]` and `[band]` pass. I count it as ✓ VERIFIED with the pattern relocated. It is part of the WR-02 change already listed under gap 02-08.

| Artifact | Status | Details |
|---|---|---|
| `src/IO/SpeakerLayout.cpp` | ✓ VERIFIED | Gap bridge added; shipped output bit-identical (probe) |
| `src/Core/SpatialMath.cpp` | ✓ VERIFIED | `kind()`-based tiers; WR-03 nearest block (gap 02-04) |
| `src/Core/FloatSemanticsGuard.h` | ✓ VERIFIED (new) | `#error` fires under fast-math; included by all 8 guarded TUs plus ADMOSCReceiver |
| `src/Algorithms/DBAPAlgorithm.cpp`, `ConstantPowerAlgorithm.cpp`, `AmbisonicsAlgorithm.cpp` | ✓ VERIFIED | Explicit non-finite rules; `[robust]` |
| `src/IO/AmbisonicsCodec.cpp` / `.h` | ✓ VERIFIED | bool return; `[decode-guard]`, `[ambi-pin]` |
| `tests/TestNumerics.h` | ✓ VERIFIED (new) | `accumulateWorstFinite`, shared by the ambi-pin and codec tests (WR-06, WR-07) |
| `tests/Core/VBAPTripletSelectionTests.cpp` | ✓ VERIFIED | New WR-03, IN-14 and WR-05/IN-15 cases (lines 821, 985, 1597) |
| Every other 02-01 to 02-10 artifact | ✓ VERIFIED | verify.artifacts pass |

### Key Link Verification

`gsd-tools verify.key-links`: 24 of 24 links verified across the 10 plans. Additionally:

| From | To | Status | Details |
|---|---|---|---|
| `[ambi-pin]` | `AmbisonicsCodec::getDecodeMatrix` via `activateLayout` | ✓ WIRED | The mutation of the library's epsilon changed the test outcome |
| `RenderEngine::activateLayout` | `getDecodeMatrix` return value | ✓ WIRED | `RenderEngine.cpp:713-716`: a false return gives a Debug jassert on the message thread, and the memset leaves a silent decode |
| Guarded TUs | `FloatSemanticsGuard.h` | ✓ WIRED | All 8 isfinite/isnan files include it |

### Data-Flow Trace (Level 4)

Not a UI phase. The chain is `setOutputFormat`, then `activateLayout`, then `vbapTriplets` and `ambiDecodeMatrix`, then `computeGains` and the decode. `[g02-2]` and `[ambi-pin]` drive it through RenderEngine with engine-built data, and the mutation proved the decode reaches the test. Status: ✓ FLOWING.

### Behavioral Spot-Checks

| Behaviour | Command | Result | Status |
|---|---|---|---|
| Debug full suite | `build/tests/SpatialCoreTests` | 200/201, HUTUBS :47 only | ✓ PASS (allowed failure) |
| Release full suite | `build-release/tests/SpatialCoreTests`; `ctest --test-dir build-release/tests` | 201/201; 100% | ✓ PASS |
| 18 Phase 2 tags x 2 builds | per-tag runs | all pass, identical counts | ✓ PASS |
| Shipped triplets unchanged | old-vs-new probe, `cmp` | bit-identical | ✓ PASS |
| Reference headers generated, not typed | three generators vs headers | 3/3 identical | ✓ PASS |
| Pin still discriminates | +0.1% epsilon mutation, both builds, reverted | 11 fail per build; 411 pass after revert | ✓ PASS |
| OSD builds (DR-3) | OSD scratch build, two targets | exit 0, OSD repo clean | ✓ PASS |
| Fast-math refused | clang++ -ffast-math / -ffinite-math-only | `#error` | ✓ PASS |
| No audio-thread assert output | `[robust]` in Debug | 0 "JUCE Assertion" lines | ✓ PASS |

### Probe Execution

No `scripts/*/tests/probe-*.sh` exist and no plan declares one. Step 7c: N/A.

### Requirements Coverage

| Requirement | Source Plans | Description | Status | Evidence |
|---|---|---|---|---|
| EXTR-01 | 02-01, 02-02, 02-03, 02-04, 02-05, 02-07, 02-08, 02-09 | Every public algorithm computes correct gains; the 3D triplet fallback is eliminated or fails loudly | ✓ SATISFIED (consumer path), with the 02-04 decision gap open | All algorithm tests pass in both builds. The off-facade WR-03 snap is the open decision. |
| EXTR-03 | 02-02, 02-06, 02-07, 02-10 | Layouts, format registry and codec return real data | ✓ SATISFIED | SC3, SC4, `[ambi-pin]` |
| VERIFY-01 | 02-02, 02-06, 02-07, 02-10 | Ambisonics convention stated (SpatialCore#11) | ✓ SATISFIED | SC1 |

No orphaned requirements. Tracking note carried from the last report: REQUIREMENTS.md still shows EXTR-01 and EXTR-03 as `[ ]` / "Largely verified by tests" (lines 138, 148, 427, 428). The `mark-complete` tool cannot flip that status text, so set it by hand once the gaps are closed.

### Decision Coverage

`check.decision-coverage-verify`: 20 of 20 trackable CONTEXT decisions "honored". This gate is a substring heuristic and did not detect the reversal of D-06(b) ("replace the snap") and D-06(c) ("jassertfalse in Debug only"), nor D-01's Debug assert, by WR-03 and IN-14. Read those against gaps 02-01 and 02-04, not this count.

### Test Quality Audit

| Test File | Linked Req | Skipped | Circular | Assertion Level | Verdict |
|---|---|---|---|---|---|
| `tests/Core/VBAPTripletSelectionTests.cpp` | EXTR-01 | 0 | No (ear oracle, double pair pan) | Value | OK |
| `tests/Algorithms/PanningLawTests.cpp` | EXTR-01 | 0 | No (PanningReference.h; MDAP labelled cross-check) | Value / behavioural | OK |
| `tests/IO/AmbisonicsCodecTests.cpp` | VERIFY-01, EXTR-03 | 0 | No (scipy literals) | Value | OK |
| `tests/Engine/RenderEngineTests.cpp` (`[ambi-pin]`) | EXTR-03 | 0 | Pin vs verbatim d43cb15 copy (VALID baseline) plus a double-precision anchor | Value; mutation-proven | OK |
| `tests/IO/SpeakerLayoutTests.cpp` | EXTR-03 | 0 | No | Value | OK |

Disabled tests on requirements: 0. Circular patterns: 0. Insufficient assertions: 0.

### Anti-Patterns Found

| File | Line | Pattern | Severity | Impact |
|---|---|---|---|---|
| (all 45 non-planning files changed by the phase) | | TBD / FIXME / XXX / TODO / HACK | none found | Debt-marker gate clear |
| `src/Core/SpatialMath.cpp` | 407-437 | Silent nearest-speaker snap on the fallback path (no diagnostic) | ⚠️ Warning | Already a gap (02-04). It is not counted twice. |
| `tests/Algorithms/PanningLawTests.cpp`, `tests/IO/AmbisonicsCodecTests.cpp` | 67, 877 / 172 | Unqualified issue references | ℹ️ Info | Carried advisory |

### Human Verification Required

None open. All earlier human items are resolved in `02-UAT.md` (status `complete`: 12 tests, 11 passed, 1 skipped and deferred to backlog 999.1, 0 pending). That includes test 11, the 2.5e-5 `[ambi-pin]` bound ("yes"), and test 12, the four 02-10 prohibitions ("accept"), both answered on 2026-10-04. The post-review listening checks for WR-05 and WR-08 are deferred to backlog 999.2 by the user and do not gate this phase. All judgment-tier prohibitions from 02-01 to 02-10 were accepted in UAT tests 5, 9 and 12 and hold as scoped to their plans. One exception: 02-08's "MUST NOT add … a public struct member or function" no longer holds at HEAD because of WR-02. It is covered by gap 02-08.

The five gaps below do need your decision, but they are recorded as gaps (failed must-haves), not as human-verification items.

### Gaps Summary

**One root cause: the code-review fix rounds (25 findings, 4 rounds) changed five things that Phase 2 plan must-haves promised, and none of those changes was signed off as a change to the phase contract.** Nothing is functionally broken. The suites pass, the shipped-layout output is bit-identical, the EAR pins, `[ambi-pin]`, the panning-law suite and the D-02a abort all hold, and OSD builds. These are contract gaps, not code defects. Each closes with a decision, not with a gap-closure plan.

| Group | Must-haves | Change | Weight |
|---|---|---|---|
| A. Fallback and asserts (WR-03, IN-14) | 02-01 (Debug assert, empty triplets), 02-04 (Debug assert on the no-enclosing path), 02-07 (no doc describes a nearest-speaker fallback) | Audio-thread asserts removed. A nearest-speaker snap was reintroduced for hand-built partial lists when the D-06b candidate clamps to zero. Docs updated to say so. | **Substantive:** reverses locked decisions D-06(b)/(c) and D-01's assert. The fixer flagged WR-03 as "needs a human to confirm it"; I found no record of that. Not reachable through RenderEngine. |
| B. Public API additions (WR-02, IN-03) | 02-08 (VBAPTriplet unchanged, header diff comment-only) | `VBAPTriplet::Kind` + `kind()`; `getDecodeMatrix` returns bool | Additive and source-compatible; OSD builds. The fixer flagged IN-03 as "needs a human to confirm it". |
| C. Line endings (IN-13) | 02-09 (CRLF on every line) | Guide and SKILL.md are LF, pinned in .gitattributes | Cosmetic, deliberate |

**To close:** for each group, either accept it (paste the matching override into this file's frontmatter `overrides:`, with your name and the time, then re-run verification, which should then pass) or revert the change. If you accept group A, also amend D-06(b), D-06(c) and the DR-1 note in `02-CONTEXT.md`, so the locked decision matches the code.

**Suggested overrides (not applied; ready to paste):**

```yaml
overrides:
  - must_have: "No VBAP, VBIP or MDAP source file contains a nearest-speaker branch; a hand-built LayoutContext with empty triplets on a height layout trips a Debug-only assert (D-01)"
    reason: "IN-14: JUCE's assert path logs and allocates on the audio thread (DR-1). The invariant is enforced on the message thread by activateLayout's D-02a abort, and a hand-built empty context now pans by 2D VBAP, pinned by the IN-14 [robust] test."
    accepted_by: "{your name}"
    accepted_at: "{ISO timestamp}"
  - must_have: "When no triplet contains a finite direction, computeVBAPGains3D uses the triplet with the largest minimum gain, clamps negatives to 0 and renormalises; an empty triplet list returns silence; a Debug-only assert marks the path (D-06b, D-06c)"
    reason: "WR-03: the audio-thread assert is removed, and a candidate that clamps to zero now gives unity on the nearest speaker instead of silence. Hand-built partial lists only; unreachable through RenderEngine ([ear][coverage]). D-06(b)/(c) amended in CONTEXT."
    accepted_by: "{your name}"
    accepted_at: "{ISO timestamp}"
  - must_have: "The docs describe below-horizon panning on height layouts as the ITU-R BS.2127 (EAR) lower-hemisphere construction, the no-triplet case as the largest-minimum-gain triplet, and non-finite positions as silence (algorithm layer) or last-good (engine); no doc describes a nearest-speaker fallback"
    reason: "Follows from accepting WR-03: the guide and skill describe the new snap accurately, which the docs-match-code prohibition requires."
    accepted_by: "{your name}"
    accepted_at: "{ISO timestamp}"
  - must_have: "VBAPTriplet, LayoutContext, SpatializationAlgorithm and every signature OpenSpatialDelay compiles against are unchanged: the SpeakerLayout.h diff is comment lines only and [consumer-surface] passes (DR-3)"
    reason: "WR-02 and IN-03 are additive and source-compatible (no data member changed; void-to-bool return). [consumer-surface] passes and OSD 30391cd builds against HEAD."
    accepted_by: "{your name}"
    accepted_at: "{ISO timestamp}"
  - must_have: "README.md, docs/integration-guide.md and SKILL.md keep CRLF on every line"
    reason: "IN-13: the guide and SKILL.md were flipped to LF in 8899bcf; restoring CRLF would hide the edits a second time, so LF is pinned in .gitattributes."
    accepted_by: "{your name}"
    accepted_at: "{ISO timestamp}"
```

Three more follow-ups, none of which blocks: qualify the three issue references; set EXTR-01 and EXTR-03 to complete by hand in REQUIREMENTS.md; tick the remaining `02-VALIDATION.md` rows.

---

_Verified: 2026-10-04T00:50:10Z_
_Verifier: Claude (gsd-verifier)_
