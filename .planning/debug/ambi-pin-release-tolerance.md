---
status: diagnosed
trigger: "UAT G-02-10: [ambi-pin] fails in a Release build only. RenderEngineTests.cpp ~line 700, CHECK (worstHere <= 1e-6f): the order-3 decode matrix differs from the test's referenceAmbiDecode by up to 5.3e-6 on 10 of 15 layouts. Debug passes. CI builds Release."
created: 2026-10-03T00:00:00Z
updated: 2026-10-03T00:00:00Z
goal: find_root_cause_only
---

## Current Focus

hypothesis: CONFIRMED. The library decoder (AmbisonicsCodec::getDecodeMatrix) and the test's referenceAmbiDecode are the same float algorithm, but at -O3 on arm64 each is compiled into different vector/scalar loop shapes. With clang's default -ffp-contract=on every `sum += a*b` is an llvm.fmuladd. The loop vectorizer cannot keep a fused multiply-add inside an in-order (non-reassociated) float reduction, so for the vectorized part of the loop it emits a vector fmul plus a strict-order scalar fadd chain (two roundings), and only the scalar remainder stays a fused fmadd (one rounding). The two TUs vectorize the E*E^T dot product differently, so they round differently; each result is a valid float answer, and they differ by ~1e-5 (up to 5.3e-6 here) because the matrix being inverted has condition number ~500-1800. The 1e-6 tolerance is below that float noise floor; it only ever passed because Debug (-O0) makes both copies bit-identical.
test: DONE - see Evidence (fp-contract matrix, vectorizer matrix, per-loop pragma attribution, N-based prediction, double-precision truth, conditioning, x86_64 proxy, real-binary confirmation)
expecting: n/a
next_action: return ROOT CAUSE FOUND (goal find_root_cause_only; do not fix). Fix is plan-phase --gaps territory.

candidate_causes (RCA branching, >=2 categories):
- code (test): the pin asserts near-bit-exactness (1e-6, ~16 ULP at 1.0) between two separately compiled copies of a float Gauss-Jordan inverse. CONFIRMED contributing.
- config/compiler: -ffp-contract=on default + auto-vectorized ordered reduction splits fmuladd; per-TU vector shape differs. CONFIRMED contributing.
- data: layout speaker count N decides how many dot-product terms land in the unfused vector block vs the fused tail; 5 layouts (N = 8, 9, 11) happen to match, 10 do not. CONFIRMED (predicts the 10/5 split exactly).
- environment: arm64 (hardware FMA, aarch64 cost model vectorizes the ordered reduction). x86-64 baseline has no FMA, so not reproduced there. CONFIRMED as gating condition.
- algorithmic difference between library and reference: ELIMINATED (bit-identical when rounding is made identical).
and_gate: YES. Fails only when ALL hold: (1) contraction enabled (fmuladd) on an FMA target, (2) the optimizer vectorizes the dot product (not at -O0), (3) the two TUs get different vectorization shapes (lib: runtime M, [49][16]/[49][49] arrays; ref: constant M=16, [16][16] arrays), (4) N lands where the fused/unfused split differs, (5) the tolerance is below float noise. Removing any one of 1-3 makes the result bit-identical; fixing 5 makes the test robust regardless.

## Symptoms

expected: All tests pass with CMAKE_BUILD_TYPE=Release (the build type .github/workflows/ci.yml uses: ubuntu-latest, -DCMAKE_BUILD_TYPE=Release, ctest).
actual: Release build of HEAD 4df579c, macOS AppleClang 21 arm64: [ambi-pin] "the order-3 speaker decode equals the pre-change private decoder on all 15 layouts (D-09)" fails. Max |active.ambiDecodeMatrix - referenceAmbiDecode| per layout: Quad 3.23e-6, 5.0 2.97e-6, 5.1 2.97e-6, 7.0 5.28e-6, 7.1 5.28e-6, 5.1.2 3.81e-6, 7.1.6 5.02e-6, 9.1.4 3.95e-6, 9.1.6 4.86e-6, SML13.1 3.30e-6 (10 of 15 layouts; the other 5 are exactly 0). Debug passes.
errors: Catch2 CHECK failure, no crash. Whole Release suite: 195 cases in the scratch tree (193 + 2 scratch probes), 194 pass, 1 fails: 11 assertion failures, all in this one case (10 per-layout CHECKs + the aggregate).
reproduction: /tmp/sc-backstop/build-rel/tests/SpatialCoreTests "[ambi-pin]" (scratch worktree of HEAD 4df579c, left in its original state)
started: Test added in 317cc51 (plan 02-06, 2026-10-01). Phase gates ran Debug only; branch gsd-remap is unpushed, so CI never ran it.

## Eliminated

- hypothesis: the library decoder and the reference are algorithmically different (different SH, epsilon, pivoting, M)
  evidence: with -ffp-contract=off on both TUs the result is bit-identical on all 15 layouts (max diff 0, 0 unequal entries); same at -O0; same with -fno-vectorize on both. Source is the same arithmetic (only array dimensions [49][16]/[49][49] vs [16][16] and runtime vs constant M differ). evalSH is the one function in SpatialMath.cpp for both, so E is bit-identical.
  timestamp: 2026-10-03
- hypothesis: E (the SH encoding matrix) differs between the two paths
  evidence: both call spatialcore::evalSH from the same object file with the same az/el floats; the stage-level difference only appears after E, in the dot-product reduction
  timestamp: 2026-10-03
- hypothesis: the failure is Gauss-Jordan (elementwise fused update) rounding, or the final E^T*inv product
  evidence: per-loop pragma matrix: disabling vectorization only on the EET s-loop makes each TU produce the canonical scalar hash 9e2fe002f689e894; disabling it on the final k-loop changes nothing in either TU (hash unchanged). The GJ elementwise vector ops (fmla/fmls) are numerically identical to the scalar fused ones.
  timestamp: 2026-10-03
- hypothesis: -ffast-math or reassociation is involved
  evidence: not applied (CMakeLists.txt:164 note); remarks and IR show llvm.vector.reduce.fadd with a start accumulator (ordered reduction). Order of additions is preserved; only fusion differs.
  timestamp: 2026-10-03
- hypothesis: the HRTF/LFS situation or the HUTUBS test interacts with this failure
  evidence: independent; see Evidence 14
  timestamp: 2026-10-03

## Evidence

- timestamp: 2026-10-03
  checked: reproduce in the scratch Release binary
  found: "/tmp/sc-backstop/build-rel/tests/SpatialCoreTests [ambi-pin]" fails with the UAT numbers (7.1: 5.27501e-06, 5.1.2: 3.8147e-06, 7.1.6: 5.02169e-06, 9.1.4: 3.94881e-06, 9.1.6: 4.85778e-06, SML13.1: 3.30433e-06). Full-suite Release run: only this test case fails.
  implication: reproduced; matches UAT exactly

- timestamp: 2026-10-03
  checked: standalone harness /tmp/sc-ambi-exp (build.sh, main.cpp, ref.cpp = verbatim referenceAmbiDecode extracted from the test, lib = src/IO/AmbisonicsCodec.cpp unmodified, SpatialMath.cpp, SpeakerLayout.cpp; same flags as the CMake Release build: -O3 -DNDEBUG -std=gnu++17 -arch arm64)
  found: baseline reproduces the real test numerically (10 layouts differ, 5 bit-equal; worst 5.275e-06 on 7.0/7.1; per-layout maxima identical to the UAT list). Bit-equal layouts are ids 5, 7, 8 (N=9), 9 (N=11), 13 (N=8).
  implication: the harness is a faithful model of the real test; experiments below transfer

- timestamp: 2026-10-03
  checked: fp-contract matrix in the harness (default is clang -ffp-contract=on)
  found: |
    both TUs -ffp-contract=off          -> worst diff 0.000e+00, 15/15 bit-equal
    lib only off                         -> worst 1.005e-05, 15/15 differ
    ref only off                         -> worst 1.005e-05, 15/15 differ
    both =on / both =fast                -> same as default (5.275e-06, 10/5)
    both -O0 (the Debug flags)           -> worst 0.000e+00, 15/15 bit-equal
  implication: fused-multiply-add is in play. When either TU fuses and the other does not, ALL 15 layouts differ by up to 1.0e-5, so the 5.3e-6 failure is the "partially fused in different places" case.

- timestamp: 2026-10-03
  checked: vectorizer matrix in the harness (-fno-vectorize -fno-slp-vectorize)
  found: |
    default lib hash cb452769c5e162fa; default ref hash 9b53468e0bc32161 (differ)
    lib novec  -> 9e2fe002f689e894
    ref novec  -> 9e2fe002f689e894
    both novec -> identical, 15/15 bit-equal
    lib alone changes its own output (cb45... -> 9e2f...) when vectorization is turned off
  implication: the library's own bits depend on vectorization. Neither default result is "the" answer; both differ from the plain scalar-fused result in different ways.

- timestamp: 2026-10-03
  checked: disassembly (otool -tvV) of the two functions, and IR (-emit-llvm)
  found: |
    Frontend IR contains llvm.fmuladd.f32 for the reductions (contract=on): 6 sites in the lib TU, 5 in the ref TU.
    -O3 IR: fmul <4 x float> followed by llvm.vector.reduce.fadd.v4f32(float %acc, ...) (an ordered reduction with a running accumulator, not reassociated).
    ref asm, dot-product loop over s: 16-wide block (4x fmul.4s, then 16 sequential scalar `fadd s0,s0,sX` from extracted lanes), then a 4-wide block (fmul.4s + 4 fadd), then a scalar tail of `fmadd s0,s1,s2,s0`.
    lib asm, same loop: if N < 8 only the scalar fused chain; for 8 <= N < 16 an 8-wide block (2x fmul.4s + 8 sequential fadd, NO fma) then a fmadd tail.
  implication: the vectorized part of the loop is two roundings per term (product rounded, then added); the scalar tail is one rounding (fused). The vector shape (lib 8, ref 16/4) differs per TU.

- timestamp: 2026-10-03
  checked: per-loop attribution with #pragma clang loop vectorize(disable) on copies of the sources (2x2x2x2 matrix over {lib,ref} x {EET s-loop, final k-loop}); copies live in /tmp/sc-ambi-exp, the main checkout is untouched
  found: |
    Disabling vectorization on the EET loop only (the `sum += E[i][s]*E[j][s]` loop over s < numSpeakers), in BOTH TUs -> bit-identical (15/15), hash 9e2fe002f689e894.
    Disabling it in just one TU -> still different (15/15 differ).
    Disabling vectorization of the final E^T*inv product loop (in either TU) changes nothing.
  implication: the entire divergence comes from one loop: EET = E*E^T dot product, trip count = number of speakers

- timestamp: 2026-10-03
  checked: falsifiable prediction from the asm block structure: terms in the unfused vector block = lib {8 if 8<=N<16, else 0}; ref {floor(N/4)*4 for N<16 (4-wide block, 16-wide when N>=16)}
  found: |
    N=4:  lib 0 unfused / ref 4  -> differ   (observed: Quad differs)
    N=5:  lib 0 / ref 4          -> differ   (5.0, 5.1 differ)
    N=7:  lib 0 / ref 4          -> differ   (7.0, 7.1, 5.1.2 differ)
    N=8:  lib 8 / ref 8          -> equal    (Octaphonic equal)
    N=9:  lib 8 / ref 8          -> equal    (9.1, 5.1.4, 7.1.2 equal)
    N=11: lib 8 / ref 8          -> equal    (7.1.4 equal)
    N=13: lib 8 / ref 12         -> differ   (7.1.6, 9.1.4, SML13.1 differ)
    N=15: lib 8 / ref 12         -> differ   (9.1.6 differs)
  implication: the prediction matches all 15 layouts. This is the mechanism, not just a correlation.

- timestamp: 2026-10-03
  checked: real test binary rebuilt in /tmp/sc-backstop/build-rel with -ffp-contract=off on exactly the two TUs involved (src/IO/AmbisonicsCodec.cpp and tests/Engine/RenderEngineTests.cpp), then restored
  found: "[ambi-pin]: All tests passed (80 assertions in 2 test cases); [engine] 836 assertions in 17 cases pass. Restored binary is byte-identical to the original (cmp) and fails again as in the UAT."
  implication: confirmed on the real artifact, not only the harness

- timestamp: 2026-10-03
  checked: is 1e-6 justified? double-precision truth (same float E, Gauss-Jordan with partial pivoting in double) and conditioning (numpy, double)
  found: |
    EE^T is exactly singular on every layout (rank <= N <= 15 < 16), so the 0.01 Tikhonov term is what makes it invertible.
    cond(EE^T + 0.01 I) = 501 (Quad) .. 1786 (9.1.6); entries of the inverse reach 100 (= 1/0.01 along the null space); max |D| 0.29 .. 1.32.
    float eps (2^-24) * cond * max|D| = 8.6e-6 (Quad) .. 1.2e-4 (7.1.6).
    Error of a float implementation against the double truth: 3.3e-6 .. 1.4e-5 (default -O3 lib/ref), up to 1.79e-5 (fully unfused) and 1.68e-5 (scalar fused / -O0). Each float variant is ~1e-5 from the truth.
    Two float variants that differ only in where a rounding happens differ by up to 1.0e-5 (contract-off on one side).
  implication: ~5e-6 between two float solves is inside the honest float noise floor (it is smaller than each one's own distance from truth). It is not an algorithmic difference. 1e-6 is 5-50x below the noise floor and could only pass when both copies execute the identical rounding sequence.

- timestamp: 2026-10-03
  checked: how large is a genuine defect in the same units (numpy, double)? (sens.py)
  found: |
    epsilon 0.01 -> 0.010001 (+0.01%): max |dD| = 6.3e-6    (as large as the float noise)
    epsilon +0.1%: 6.3e-5     epsilon +1%: 6.3e-4
    one SH constant off by 1e-4 relative: 3.1e-5     by 1e-3: 3.1e-4
  implication: a tolerance in the 3e-5 .. 1e-4 band still catches anything >= ~0.1%; the regressions D-09 is meant to catch (wrong stride, M, speaker order, dropped regularization) are O(0.1..1). No tolerance can resolve a 0.01% change under float rounding.

- timestamp: 2026-10-03
  checked: provenance of the 1e-6 number
  found: "02-RESEARCH.md F10 (scratch harness s_ambi.cpp) measured `worst 0`, build type not recorded, and recommended 'tolerance 0 (recommend 1e-6f to survive compiler flag changes)'. The 02-06 plan copied 1e-6 (T-02-16, <fails_when>). It was a slack on a bit-identical measurement, never derived from conditioning, and never exercised: -O0 gives exactly 0. The claim 'survives compiler flag changes' is false: it does not survive -O3 on arm64. The research measurement matches this finding at -O0."
  implication: the number came from a Debug-grade bit-identity observation plus an unmeasured safety factor

- timestamp: 2026-10-03
  checked: precedent in this repo
  found: "CMakeLists.txt:164-180 (Phase 8 plan 08-02) already records that per-TU floating-point code generation differs between a separate TU and the original single TU (-ffast-math reassociation) and breaks a bit-identical gate. Same hazard class: two TUs compiling the same float source do not round identically at -O3."
  implication: not new to this codebase; the tolerance test needs to assume it

- timestamp: 2026-10-03
  checked: would CI (ubuntu-latest = x86-64, GCC, -O3 -DNDEBUG, -std=gnu++17) fail? GCC and Docker are not available here; gh is unauthenticated (HTTP 401), so CI history could not be read. Used clang -arch x86_64 under Rosetta as a proxy.
  found: |
    x86_64 clang, default target (no FMA):       lib==ref hash 0e9e8772afbf625b, 0 diff, 15/15 bit-equal
    x86_64 clang, -mavx2 -mfma:                   lib==ref hash 9e2fe002f689e894, 0 diff, 15/15 bit-equal
    x86_64 clang, -mavx2 -mfma -ffp-contract=off: 0 diff
    Reasoning for GCC: baseline x86-64 has no FMA instruction, so GCC's -ffp-contract=fast (default in gnu++17) cannot fuse anything; GCC does not reassociate float sums without -ffast-math/-fassociative-math, so an in-order vectorized sum is rounding-identical to the scalar loop. Both TUs then execute the same IEEE mul/add sequence.
  implication: very likely PASSES on ubuntu-latest x86-64 (not executed with GCC). It would fail or may fail on any FMA-capable target where the vectorizer unfuses ordered reductions: Apple Silicon Release builds (reproduced), possibly aarch64 Linux runners (unverified), possibly x86-64 builds with -march=native/-mfma and GCC (unverified; the clang proxy was clean). So "CI never ran it" is not the reason it is invisible: on today's CI it probably passes; it fails for the developer machine (Apple Silicon, Release) and for the arm64 slice of any shipped build.

- timestamp: 2026-10-03
  checked: question 5, why the HUTUBS PP2 failure (HutubsPP2Tests.cpp:47) did not appear in the Release run
  found: |
    Not skipped, and not an LFS problem: /tmp/sc-backstop/HRTF/hutubs_pp2.sofa is 1,658,802 bytes, the same as the main checkout, a real SOFA file.
    "[hutubs]" in the scratch Release binary: "All tests passed (13 assertions in 3 test cases)".
    "[hutubs]" in the main-checkout Debug binary (./build, CMAKE_BUILD_TYPE=Debug): FAILS, checksum 0x7a35c1f848c2a410 vs golden 0x9e3c2875eeade4b7.
    The test is a bit-exact FNV-1a 64-bit hash of the float HRIR (hashHRIRPair). The golden was captured in 2d6dc08 (2026-07-05, plan 08-04) "proven bit-identical against pre-move OSD output via ctest -R RegressionHarness", i.e. against an optimized build. The optimized arm64 Release build reproduces it; -O0 (no contraction/vectorization) does not.
  implication: the known "pre-existing PP2 failure" is a Debug-only artifact of the same class (FP rounding depends on build type). The Release baseline for the planner is 0 failures other than [ambi-pin]; the Debug baseline is 1 (PP2). The five binaural goldens are exact-bit hashes, stricter than any tolerance test; whether they hold on Linux GCC is unverified (CI history not readable here).

## Resolution

root_cause: |
  Two conditions that must both hold (AND-gate), plus a test design flaw that makes them visible.
  (1) Compiler: at -O3 on arm64 (AppleClang, -ffp-contract=on default, hardware FMA) the E*E^T dot product `sum += E[i][s]*E[j][s]` (src/IO/AmbisonicsCodec.cpp:69-70 in the lib, the same loop in referenceAmbiDecode) is an llvm.fmuladd reduction. The loop vectorizer turns the vectorized portion into fmul.4s + a strict-order scalar fadd chain (unfused, 2 roundings per term) and leaves the scalar remainder as fmadd (fused, 1 rounding). The two TUs vectorize it differently (lib: runtime M, [49][16] arrays, 8-wide block; ref: constant M=16, [16][16] arrays, 16-wide plus 4-wide blocks), so for a speaker count N the number of fused vs unfused terms differs: they agree only for N = 8, 9, 11 and differ for N = 4, 5, 7, 13, 15 (10 of 15 layouts). In Debug (-O0) nothing is vectorized and both copies are the same scalar fused sequence, hence bit-identical.
  (2) Numerics: EE^T is singular (rank <= N <= 15 of 16) and the 0.01 Tikhonov term gives cond ~500-1800 with inverse entries up to 100. A one-rounding change in an EET entry moves the final decode by 1e-6..1e-5. The honest float noise floor of this solve is 5e-6..2e-5 (distance of any float variant from a double solve), above the 1e-6 tolerance. The tolerance was inherited from a Debug-grade `worst 0` measurement (02-RESEARCH F10) with an unmeasured safety factor, and it is effectively a bit-exactness assertion.
  The library decoder is NOT wrong: it is as accurate as the reference (lib 1.4e-5, ref 1.3e-5 from double truth) and algorithmically identical (bit-equal under -ffp-contract=off). The defect is the test's assertion strength, exposed by build-type/TU-dependent FP code generation. Shipped behavior is unaffected at the ~1e-5 (-100 dB) level.
fix: ""
verification: ""
files_changed: []

artifacts (for plan-phase --gaps):
- path: tests/Engine/RenderEngineTests.cpp
  issue: "lines ~700 and ~706: two `CHECK (worstHere <= 1e-6f)` / `CHECK (worst <= 1e-6f)`. Tolerance is below the float-solve noise floor of this matrix (cond 500-1800); only passes when both decoders round identically (Debug). referenceAmbiDecode (lines 574-658) is a byte-identical transplant and should stay verbatim."
- path: src/IO/AmbisonicsCodec.cpp
  issue: "getDecodeMatrix lines 65-72 (EET dot product, s-loop at 69-70) is where the TU-dependent rounding originates; no defect, no change required by this gap. Rewriting it (e.g. double accumulation) would move it away from the pre-change decoder the pin exists to compare against."
- path: tests/Binaural/HutubsPP2Tests.cpp
  issue: "kGoldenChecksum (0x9e3c2875eeade4b7) is a bit-exact float hash: passes Release arm64, fails Debug. Same hazard class; Phase 3 territory, but the planner should know Release is green for PP2."
- path: .github/workflows/ci.yml
  issue: "Linux x86-64 Release only. No arm64/FMA leg, so the CI that exists would probably not catch this class. Phase gates also ran Debug only."
- path: CMakeLists.txt
  issue: "lines 164-180 already document the per-TU FP codegen hazard (-ffast-math not applied). No -ffp-contract policy is set for SpatialCore or the tests."

missing (non-binding fix directions, for the planner):
- Make the tolerance honest. Options: (a) fixed absolute bound in the 3e-5..1e-4 band (worst observed noise 1.8e-5 from truth, 1.0e-5 lib-vs-ref; still flags a +0.1% epsilon change at 6.3e-5 if the bound is ~5e-5; anything O(0.1+) of a real regression is far above it); (b) scale-aware bound = k * 2^-24 * cond(EE^T+0.01I) * max|D| per layout (8.6e-6..1.2e-4 for k=1); (c) additionally or instead assert each decoder within ~3e-5 of a double-precision decode (a double dense decoder already exists as test-local denseDecode in tests/IO/AmbisonicsCodecTests.cpp, anonymous namespace). Update both lines (~700 and ~706) and the comment at 565-570; record why in the test so the value has provenance.
- Alternative or additional: pin -ffp-contract=off for SpatialCore and SpatialCoreTests (proven bit-identical here, 15/15, on the real binary). Costs: perf on the whole DSP path, and it must be re-verified against the OSD regression harness (see CMakeLists.txt 08-02 note). Heavier than needed for a test-tolerance issue.
- Not recommended: changing the library accumulators to double (moves the library away from the pre-change decoder, so a float reference would then differ by ~1e-5 anyway) or editing the reference body (breaks its "byte-identical to d43cb15" provenance).
- Recurrence guard: gate phases on a Release ctest run (build-rel), and consider an arm64/FMA CI leg (macos-latest arm64) so this class is seen before shipping. Release baseline for the suite today: only [ambi-pin] fails (195 scratch cases incl. 2 probes; 193 real), HUTUBS PP2 passes.
- Other tests to watch (candidates only; none failed in Release arm64): tests/Core/VBAPTripletSelectionTests.cpp:787 (1e-6, test-local largestMinGainReference vs computeVBAPGains3D: the closest analog); :86, :152, :861, :1054 (`>= -1e-6f` containment predicates re-implemented in the test; a flip selects a different triplet, a discrete jump; related to issue #22 ties); :1268 (|power-1| <= 2e-6 over ~600k directions); tests/Binaural/*Tests.cpp golden FNV hashes (exact bits). Low risk: SpatialMathTests.cpp softClip/outputLimiter/distanceAttenuation 1e-6 (single expression, ~8-16x headroom), PanningLawTests.cpp:718-721 and :765-766 (same function mirrored, same expression). Safe: AmbisonicsCodecTests.cpp:152 (1e-9, same function twice in one TU), round-trip test (double decoder, 1e-5).
