---
phase: 02-algorithm-format-verification
reviewed: 2026-10-03T00:00:00Z
depth: standard
files_reviewed: 2
files_reviewed_list:
  - .gitignore
  - tests/Engine/RenderEngineTests.cpp
findings:
  critical: 0
  warning: 1
  info: 3
  total: 4
status: issues_found
---

# Phase 02: Code Review Report (incremental, gap closure 02-10 / G-02-10)

**Reviewed:** 2026-10-03
**Depth:** standard
**Files Reviewed:** 2
**Status:** issues_found

## Summary

Scope was `git diff f68fa3a..HEAD` for `.gitignore` and `tests/Engine/RenderEngineTests.cpp`. The changes replace the `[ambi-pin]` 1e-6 tolerance with `kAmbiPinTolerance = 2.5e-5f` and add a double-precision reference decode (`doublePrecisionAmbiDecode`) with a second tolerance, `kAmbiFloatVsDoubleTolerance = 4e-5`. They also gitignore `build-release/`.

What checks out:

- **Derivation and comment:** the figures in the comment match `.planning/debug/ambi-pin-release-tolerance.md`. That covers cond 501 to 1786, the 5.3e-6 Release spread, the 1.0e-5 worst variant, and the 1.4e-5, 1.7e-5 and 1.8e-5 float-vs-double distances. The ratios are right: 2.5e-5 is 2.5x the 1.0e-5 worst variant, and 4e-5 is 2.2x the 1.8e-5 worst distance.
- **Double decode correctness:** the arithmetic mirrors the library. It uses the same Tikhonov term (the float 0.01 promoted), the same Gauss-Jordan with partial pivoting, and the same `D[s][c] = sum_k E[k][s] * inv[k][c]`.
- **It tests the real library output:** the anchor is compared against `active.ambiDecodeMatrix`, which `RenderEngine.cpp` fills through `AmbisonicsCodec::getDecodeMatrix`. The speaker angles come from the same `layout.speakers[s]` fields, so it is not tautological.
- **Indices and conversions:** ranges are in bounds, since MAX_SPEAKERS == 16 == M and every layout has at most 15 speakers. The float-to-double conversions are correct. The 15-layout iteration is guarded by the `static_assert` and the `layoutsChecked == 15` check.
- **Test run:** I rebuilt `build-release` and ran `[ambi-pin]`. It passes (110 assertions in 2 test cases).
- **`.gitignore`:** `build-release/` is correct. `git check-ignore` confirms the directory is ignored.

One real hole was found: the max-reduction in the new and old checks silently drops NaN, so a NaN decode can pass. Details are in WR-06.

## Warnings

### WR-06: NaN decode entries are silently ignored, so the pin (and the new anchor checks) can pass vacuously

**File:** `tests/Engine/RenderEngineTests.cpp:817-819` (also `:824-827`, `:832`)
**Issue:** The per-layout maxima are accumulated with `std::max (acc, std::abs (x - y))`. `std::max (a, b)` is `(a < b) ? b : a`. With `b == NaN`, `a < b` is false, so `a` is returned and the NaN is dropped. I confirmed this with a compiled snippet: `std::max (0.0f, std::abs (NaN - 1.0f))` yields `0`.

If `active.ambiDecodeMatrix`, `reference`, or `exact` contain NaN, the affected entries do not contribute to `worstHere`, `libVsExact` or `refVsExact`. For example, a library regression that produced NaN from a failed inversion or an uninitialised read would give `worstHere = 0`. All three tolerance CHECKs and the aggregate `worst <= kAmbiPinTolerance` would then pass.

The same-shaped line in the old code already had this hole. The new `libVsExact` and `refVsExact` lines copy it, so the new "independent anchor" shares the blind spot. Nothing else in the file asserts the decode matrix is finite. The only `isfinite` use, at line 61, covers render buffers.

**Fix:** Make non-finite values fail explicitly, and accumulate in a way that propagates NaN:
```cpp
const float  dNew = std::abs (active.ambiDecodeMatrix[s][c] - reference[s][c]);
const double dLib = std::abs (static_cast<double> (active.ambiDecodeMatrix[s][c]) - exact[s][c]);
const double dRef = std::abs (static_cast<double> (reference[s][c]) - exact[s][c]);
REQUIRE (std::isfinite (dNew));
REQUIRE (std::isfinite (dLib));
REQUIRE (std::isfinite (dRef));
worstHere = std::max (worstHere, dNew);
libVsExact = std::max (libVsExact, dLib);
refVsExact = std::max (refVsExact, dRef);
```
Alternatively, add a separate CHECK that every `ambiDecodeMatrix[s][c]` for `s < numSpeakers`, `c < 16` is finite. Do the same for the `exact` matrix.

## Info

### IN-07: The +0.1% epsilon "smallest change the pin must catch" is resolved on only 5 of 15 layouts

**File:** `tests/Engine/RenderEngineTests.cpp:692-696`, `:708`
**Issue:** The comment states the +0.1% Tikhonov change moves the decode by 5.8e-5 to 6.3e-5 on 7.0, 7.1, 7.1.2, 7.1.4 and 7.1.6. The bound is one absolute number, 2.5e-5, applied to every layout. On the other 10 layouts the same change moves the decode by less than that (less than 2.5e-5, by implication), so it is not caught there. The pin as a whole still fails (02-10-SUMMARY records this), so the claim is true. But the comment does not say that the guarantee rests on 5 layouts, or that a regression confined to the other layouts has a coarser floor. This matters if a future change is layout-specific, such as speaker order in Quad or 5.x only.
**Fix:** Add one sentence to the comment saying that the +0.1% resolution holds on the 7.x layouts only and that other layouts rely on the O(0.1 to 1) class of regressions. A scale-aware per-layout bound would be the stronger option, as the debug doc's option (b) describes.

### IN-08: `doublePrecisionAmbiDecode` drops the library's guards and shares float E with the other two decoders

**File:** `tests/Engine/RenderEngineTests.cpp:715-782`
**Issue:**
- (a) The double solve omits the library's `if (std::abs (diagVal) < 1e-10f) continue;` skip and has no `N <= M` guard. The library has the D-20 guard. `E[M][M]` indexed by `s < N` would overflow if `N > 16`. This cannot happen with the current layouts, since 15 is the maximum, but it is an unguarded stack write in a test helper. With Tikhonov the diag skip never triggers, and a zero diag would give NaN, which WR-06 would hide.
- (b) The anchor uses the float `evalSH` values promoted to double. It therefore verifies the linear solve and its conditioning only, not the SH evaluation. That is deliberate and documented ("only precision differs"), and the SH constants are out of this pin's scope per the D-08 note. If `evalSH` regresses, all three decoders move together and the pin stays green. Nothing in the changed code says where SH correctness is pinned.
**Fix:** Add `REQUIRE (N <= M)` at the top of the helper. Optionally add a one-line comment naming the test that pins `evalSH` against known values.

### IN-09: Comment hygiene

**File:** `tests/Engine/RenderEngineTests.cpp:673-707`
**Issue:** There are three small problems in the derivation comment.
- It cites the debug-doc path twice, at lines 673-674 and 705-707, with a duplicated "full derivation" sentence.
- It says the bound is "2.5x below the +0.1% change". The measured ratio is 5.8e-5 / 2.5e-5, about 2.3x, which is what 02-10-SUMMARY says.
- The helper block is indented inside the anonymous namespace while `referenceAmbiDecode` above it is intentionally unindented (to diff against the blob). The unindented function and indented siblings in one namespace are visually inconsistent. A short note at the `namespace` line would prevent someone "fixing" the indentation and breaking the byte-for-byte diffability.
**Fix:** Drop the duplicate paragraph, change "2.5x" to "2.3x", and note the intentional indentation split.

---

_Reviewed: 2026-10-03_
_Reviewer: Claude (gsd-code-reviewer)_
_Depth: standard_
