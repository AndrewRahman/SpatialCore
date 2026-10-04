---
phase: 02-algorithm-format-verification
plan: 06
subsystem: spatial-audio-dsp
tags: [ambisonics, spherical-harmonics, sn3d, acn, ambix, decode-matrix, render-engine, output-format, catch2, scipy]
status: complete

requires:
  - phase: 02-algorithm-format-verification
    provides: "02-02: tests/reference/ShReference.h (49 scipy SN3D literals at az 64, el 10)"
  - phase: 02-algorithm-format-verification
    provides: "02-04: last prior editor of SpatialMath.cpp, RenderEngine.cpp/.h and RenderEngineTests.cpp (sequencing only, no behavioural dependency)"
provides:
  - "evalSH: correct SN3D at every order to 6 (22 constants fixed), the single SH implementation in SpatialCore"
  - "AmbisonicsCodec::evaluateSH as a one-line forwarder to spatialcore::evalSH (both public names kept)"
  - "AmbisonicsCodec::getDecodeMatrix writes nothing for numSpeakers > MAX_SPEAKERS (D-20)"
  - "ACN / SN3D / no-Condon-Shortley convention docblocks on evalSH, the SpatialMath.cpp header and the AmbisonicsCodec class"
  - "RenderEngine with one decoder: activateLayout calls AmbisonicsCodec::getDecodeMatrix (3, ...); the private copy is deleted"
  - "[sn3d], [roundtrip], [decode-guard], [ambi-pin], [format-resolve] tests"
affects: [02-07 docs (README, integration-guide, SKILL.md must state the same convention), OpenSpatialDelay 4OA-6OA sessions (22 channels change level), Phase 6 coverage]

plan_head_before: e8e72976037a00a31f15244017ff98e2efd34daf
plan_head_after: 408adeaf3f31216c2238e1534df4e9d18b4a440b

actuals:
  tokens: 10542
  tasks: 3
  commits: 5

tech-stack:
  added: []
  patterns:
    - "Pin a refactor to the old code by copying it out of a fixed git blob (d43cb15), not the working tree, so the reference cannot drift with the change"
    - "Run the pin against the unchanged code first: a pass proves the test-local reference is faithful before the original is deleted"
    - "A round trip that cannot see per-channel scale is labelled a smoke test in the test itself, with the pinv(S Y) = pinv(Y) S^-1 reason"

key-files:
  created: []
  modified:
    - src/Core/SpatialMath.cpp
    - include/SpatialCore/Core/SpatialMath.h
    - src/IO/AmbisonicsCodec.cpp
    - include/SpatialCore/IO/AmbisonicsCodec.h
    - src/Engine/RenderEngine.cpp
    - include/SpatialCore/Engine/RenderEngine.h
    - tests/IO/AmbisonicsCodecTests.cpp
    - tests/Engine/RenderEngineTests.cpp

key-decisions:
  - "D-08 is constants only: the 22 substitutions from RESEARCH Reference Data B, nothing else in evalSH changed; post-fix max error against the 49 scipy literals 2.4e-7, addition-theorem deviation 2.8e-6 (was 19.54)"
  - "D-09 is a proven pure refactor: the new order-3 decode equals the verbatim d43cb15 decoder with max |diff| = 0 on all 15 layouts; the memset additionally clears rows beyond the speaker count, which the old code left stale"
  - "AndrewRahman/SpatialCore#11 is referenced as closed in commit 2c4e38d's message (no gh action taken); the docs half of the convention statement lands in Plan 02-07"

patterns-established:
  - "Convention text (ACN, SN3D, no Condon-Shortley, radians, az 0 front, +az left, el 0 horizon, AmbiX) is stated identically on evalSH and AmbisonicsCodec; Plan 02-07 copies it into docs"

requirements-completed: [VERIFY-01, EXTR-03]

coverage:
  - id: D1
    description: "evalSH correct SN3D at orders 0-6: 49 scipy literals within 1e-5, addition theorem within 2e-5 at orders 1-6, negative-elevation parity within 1e-5"
    requirement: "EXTR-03"
    verification:
      - kind: unit
        ref: "tests/IO/AmbisonicsCodecTests.cpp#SH: evalSH and AmbisonicsCodec::evaluateSH match 49 scipy reference values at az=64 el=10 (D-11c)"
        status: pass
      - kind: unit
        ref: "tests/IO/AmbisonicsCodecTests.cpp#SH: SN3D addition theorem, sum over m of Y_lm^2 == 1 at orders 1-6 (D-11b)"
        status: pass
      - kind: unit
        ref: "tests/IO/AmbisonicsCodecTests.cpp#SH: negative elevation keeps its true sign, Y(az,-el) = (-1)^(l+|m|) Y(az,el) (D-05)"
        status: pass
    human_judgment: false
  - id: D2
    description: "One SH implementation: AmbisonicsCodec::evaluateSH forwards to evalSH bit-for-bit, duplicate body deleted, both public names kept"
    requirement: "VERIFY-01"
    verification:
      - kind: unit
        ref: "tests/IO/AmbisonicsCodecTests.cpp#SH: AmbisonicsCodec::evaluateSH forwards to evalSH bit-for-bit (D-08)"
        status: pass
      - kind: other
        ref: "grep -c 'return spatialcore::evalSH' src/IO/AmbisonicsCodec.cpp == 1; grep -c 'case 48' src/IO/AmbisonicsCodec.cpp == 0"
        status: pass
    human_judgment: false
  - id: D3
    description: "getDecodeMatrix writes nothing for 17 or 0 speakers and a finite 16 x 16 decode at exactly 16 (D-20)"
    requirement: "EXTR-03"
    verification:
      - kind: unit
        ref: "tests/IO/AmbisonicsCodecTests.cpp#AmbisonicsCodec: getDecodeMatrix writes nothing for more than 16 or for 0 speakers (D-20)"
        status: pass
    human_judgment: false
  - id: D4
    description: "Encode, dense decode, re-encode round trip within 1e-5 at orders 1-6, labelled a decoder-conditioning smoke test (D-11a, D-12)"
    requirement: "EXTR-03"
    verification:
      - kind: unit
        ref: "tests/IO/AmbisonicsCodecTests.cpp#SH: encode, dense decode, re-encode round trip at orders 1-6 (D-11a, smoke test only)"
        status: pass
    human_judgment: false
  - id: D5
    description: "Convention (ACN, SN3D, no Condon-Shortley phase, radians, az/el orientation, AmbiX) stated on evalSH, in SpatialMath.cpp and on AmbisonicsCodec"
    requirement: "VERIFY-01"
    verification:
      - kind: other
        ref: "grep -c 'Condon-Shortley' / 'SN3D' / 'ACN' >= 1 in SpatialMath.h, AmbisonicsCodec.h, SpatialMath.cpp; grep -c 'SpatialCore#11' SpatialMath.h >= 1"
        status: pass
    human_judgment: true
    rationale: "Greps prove presence; whether the wording reads clearly to a consumer is a reviewer's call"
  - id: D6
    description: "RenderEngine uses AmbisonicsCodec::getDecodeMatrix; decode equals the pre-change private decoder on all 15 layouts (max diff 0), stale rows cleared"
    requirement: "EXTR-03"
    verification:
      - kind: unit
        ref: "tests/Engine/RenderEngineTests.cpp#RenderEngine: the order-3 speaker decode equals the pre-change private decoder on all 15 layouts (D-09)"
        status: pass
      - kind: unit
        ref: "tests/Engine/RenderEngineTests.cpp#RenderEngine: decode rows beyond the speaker count are cleared on a layout switch (D-09)"
        status: pass
      - kind: other
        ref: "grep -rn computeAmbiDecodeForLayout src/ include/ prints nothing"
        status: pass
    human_judgment: false
  - id: D7
    description: "All 23 OutputFormats resolve through setOutputFormat to layouts that agree with OutputFormatRegistry (criterion 3)"
    requirement: "EXTR-03"
    verification:
      - kind: integration
        ref: "tests/Engine/RenderEngineTests.cpp#RenderEngine: all 23 output formats resolve to layouts that agree with the registry (criterion 3)"
        status: pass
    human_judgment: false

duration: 7min
completed: 2026-10-01
---

# Phase 2 Plan 06: SN3D Fix, One SH Evaluator, One Decoder Summary

**I corrected 22 wrong SN3D constants at orders 4-6, so 4OA-6OA output is now correct AmbiX. evalSH is now the only SH implementation, and the ACN/SN3D/no-Condon-Shortley convention is written on the code. The engine's private decoder was replaced by `AmbisonicsCodec::getDecodeMatrix`, and the new decode is identical to the old one on all 15 layouts.**

## Performance

- **Duration:** 7 min
- **Started:** 2026-10-01T07:14:13Z
- **Completed:** 2026-10-01T07:21:21Z
- **Tasks:** 3
- **Files modified:** 8

## Accomplishments

- **D-08:** the 22 SN3D constants are fixed. Measured after the fix:
  - **Literals:** max error against the 49 scipy values is **2.4e-7** (ACN 48), well inside the 1e-5 tolerance.
  - **Addition theorem:** max deviation of the sum over m of Y_lm^2 from 1 is **2.8e-6**. That covers orders 1-6, 2000 seeded directions, the poles and the horizon. Before the fix it was **19.54**.
  - **Pre-fix red run:** 44 literal mismatches (22 channels x 2 functions), and orders 4, 5 and 6 failed the addition theorem.
- **One SH evaluator:** `AmbisonicsCodec::evaluateSH` is now a one-line forwarder, and the 100-line duplicate body is deleted. Both public names survive.
- **D-20:** `getDecodeMatrix` no longer writes out of bounds when given more than 16 speakers (threat T-02-14 mitigated).
- **D-11a round trip:** errors are 1.5e-8 (order 1) to 5.4e-8 (order 6). The test is labelled a smoke test only and carries the D-12 note on why it cannot see a scale error.
- **D-10:** the convention is written in all three landing sites.
- **D-09:** the engine has one decoder.
  - **Pin:** **max |new - old| = 0** on all 15 layouts.
  - **Stale rows:** rows past the speaker count are now cleared. The old code left them stale; a case under the `[ambi-pin]` tag proves this.
- **Criterion 3:** all 23 output formats agree with the registry (695 assertions).

## Task Commits

1. **Task 1: Correct the 22 SN3D constants and make evaluateSH a forwarder** (TDD)
   - `24670ce` (test, RED: 47 failing assertions)
   - `db9731a` (fix, GREEN)
2. **Task 2: Decode guard, dense round trip, convention docblocks**
   - `2c4e38d` (feat)
3. **Task 3: One decoder, the 15-layout pin and the 23-format cross-table**
   - `317cc51` (test: the pin passes on the old code, the cleared-rows case is RED)
   - `408adea` (refactor, GREEN)

## Files Created/Modified

- `src/Core/SpatialMath.cpp` - 22 corrected constants; the header comment now states the convention
- `include/SpatialCore/Core/SpatialMath.h` - convention docblock on `evalSH`, naming SpatialCore#11
- `src/IO/AmbisonicsCodec.cpp` - `evaluateSH` forwards to `evalSH`; `getDecodeMatrix` has the D-20 guard
- `include/SpatialCore/IO/AmbisonicsCodec.h` (CRLF kept) - class convention docblock and notes on `getDecodeMatrix` and `evaluateSH`
- `src/Engine/RenderEngine.cpp` - `activateLayout` calls `getDecodeMatrix (3, ...)` with a `static_assert` and a `memset`; the private decoder is deleted
- `include/SpatialCore/Engine/RenderEngine.h` - private declaration removed
- `tests/IO/AmbisonicsCodecTests.cpp` - `[sn3d]` x3, forwarder, `[decode-guard]`, `[roundtrip]`
- `tests/Engine/RenderEngineTests.cpp` - `referenceAmbiDecode` (byte-identical to d43cb15 lines 653-734), `[ambi-pin]` x2, `[format-resolve]`

## Decisions Made

- I generated the test-local reference decoder programmatically from the git blob and diffed it against the blob, so its body is byte-identical. I left it unindented so it still diffs cleanly.
- The `getDecodeMatrix` docblock also records:
  - the row-major `[s * M + c]` layout, with rows in speaker order and columns in ACN order;
  - that it writes nothing at 0 or more than 16 speakers;
  - that it uses about 32 KB of stack, so it belongs on the message thread only.

  This covers truth 10 (ordering) where the consumer reads it.
- SpatialCore#11 is referenced as closed in the `2c4e38d` commit message only. No `gh` action was taken.

## Deviations from Plan

### Additions beyond the plan text

**1. [Rule 2 - Missing check] Test for the stale-row clearing**
- **Found during:** Task 3
- **Issue:** The plan's `memset` changes behaviour, since rows past the speaker count are no longer stale, but no planned test could see that change.
- **Fix:** I added a case under the `[ambi-pin]` tag that switches 9.1.6, then Binaural, then Quad, which reuses 9.1.6's buffer. It fails on the old code and passes after the swap.
- **Files modified:** tests/Engine/RenderEngineTests.cpp
- **Committed in:** `317cc51` (RED), `408adea` (GREEN)

**2. [Rule 2 - Missing check] Two extra checks in `[format-resolve]`**
- Speaker channel indices are distinct, and flat layouts carry no triplets.
- Both hold today. They make the D-02a invariant visible from the engine side.
- **Committed in:** `317cc51`

**3. [Rule 1 - Criterion mismatch] Reworded a test comment**
- A comment of mine quoted the `ShReference.h` path, so the `grep -c 'reference/ShReference.h'` criterion counted 2 instead of 1. I reworded it.
- **Committed in:** `db9731a`

---

**Total deviations:** 3 (2 added checks, 1 comment fix)
**Impact on plan:** None of these change behaviour beyond the plan. They only make planned behaviour observable.

## Issues Encountered

None. Line endings were kept as they were: `AmbisonicsCodec.h` is still CRLF, and `tests/CMakeLists.txt` was not touched.

## Test Suite

- **Full suite:** 190 test cases (181 at baseline plus 9 new). 189 pass.
- **Only failure:** the pre-existing `HUTUBS PP2 — golden HRIR checksum at az=90deg (own control)`.
- **Run without HUTUBS:** 306,084 assertions in 189 cases, exit 0.
- **Known false positive:** JUCE prints "Leaked objects: FFT" at exit, as before.

## User Setup Required

None. No external service configuration is required.

## Next Phase Readiness

- Plan 02-07 can now write the same convention text into `README.md`, `docs/integration-guide.md` and `.claude/skills/spatial-audio-dsp/SKILL.md` §7.1/§7.4.
- The existing comment in `tests/IO/AmbisonicsCodecTests.cpp` still says the decode matches OSD's former `computeAmbiDecodeForLayout`. That remains true, and `[ambi-pin]` now proves it.
- OpenSpatialDelay 4OA-6OA sessions will hear 22 channels change level. This is the intended correction (D-08, rated costly).

---
*Phase: 02-algorithm-format-verification*
*Completed: 2026-10-01*

## Self-Check: PASSED
