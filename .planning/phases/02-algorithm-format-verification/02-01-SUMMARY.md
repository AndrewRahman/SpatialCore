---
phase: 02-algorithm-format-verification
plan: 01
subsystem: spatial-audio-dsp
tags: [vbap, ear, itu-r-bs2127, lower-hemisphere, speaker-layout, render-engine, catch2]
status: complete

requires:
  - phase: 01-documentation-truth-contract-freeze
    provides: frozen count contract (8 algorithms / 23 formats / 15 layouts) and the test baseline
provides:
  - ITU-R BS.2127 (EAR) lower-hemisphere panning for below-horizon sources on height layouts
  - appendLowerHemisphereTriplets builder and four default-initialised VBAPTriplet members
  - regular-first two-pass computeVBAPGains3D, bit-identical to d43cb15 above the horizon
  - one shared height threshold (0.0175 rad) for layoutHasHeight and the triplet builder
  - D-02a Release abort in RenderEngine::activateLayout for layout-table/builder mismatch
  - the four empty-triplet nearest-speaker branches deleted from VBAP/VBIP/MDAP
  - [consumer-surface] compile-time pins for the public surface OpenSpatialDelay uses
affects: [02-04 robustness (D-06b replaces the remaining no-triplet snap), 02-05 VBIP/MDAP, 02-07 docs/deprecation comment]

plan_head_before: bed3f272120b3dfedf827288537f8fb80381e665
plan_head_after: ef95296461c3d1b4bae70f414060b06358252402

actuals:
  tokens: 8857
  tasks: 3
  commits: 4

tech-stack:
  added: []
  patterns:
    - "Lower-hemisphere data is built at layout-build time and read through a const reference on the audio thread"
    - "Verbatim git-blob copy of the pre-change function as a bit-identity oracle inside the test file"

key-files:
  created:
    - tests/Core/VBAPTripletSelectionTests.cpp
  modified:
    - include/SpatialCore/IO/SpeakerLayout.h
    - src/IO/SpeakerLayout.cpp
    - src/Core/SpatialMath.cpp
    - src/Engine/RenderEngine.cpp
    - src/Algorithms/VBAPAlgorithm.cpp
    - src/Algorithms/VBIPAlgorithm.cpp
    - src/Algorithms/MDAPAlgorithm.cpp
    - tests/IO/SpeakerLayoutTests.cpp
    - tests/CMakeLists.txt

key-decisions:
  - "Lower-hemisphere-only hull (ear-level speakers + their -30 degree copies + nadir), not ear's full-sphere hull, per RESEARCH F4 (full hull pans onto height speakers on 5.1.4)"
  - "Regular triplets always tried before lower-hemisphere ones so above-horizon output stays bit-identical to d43cb15"
  - "Extras appended by a separate function after the D-02a guard, never by the regular builder (F11), so the guard and [io][layout] test stay meaningful"

patterns-established:
  - "Tracer slice first: one layout (7.1.4) through engine, builder, selection and algorithm before expanding"
  - "Identity test against a verbatim git-blob copy, so the oracle is the pre-change code regardless of when the test is written"

requirements-completed: [EXTR-01]

coverage:
  - id: D1
    description: "A below-horizon source on 7.1.4 is panned by the EAR construction through RenderEngine and VBAPAlgorithm: nadir gives 1/sqrt(7) on all 7 ear-level speakers and 0 on the 4 height speakers"
    requirement: "EXTR-01"
    verification:
      - kind: unit
        ref: "tests/Core/VBAPTripletSelectionTests.cpp#EAR tracer: nadir on 7.1.4 through RenderEngine and VBAPAlgorithm spreads 1/sqrt(7) over the ear-level ring (D-04)"
        status: pass
    human_judgment: false
  - id: D2
    description: "A source directly under an ear-level speaker gets unity on it, and no below-horizon source on 7.1.4 reaches an elevated speaker (power within 1e-5 of 1)"
    requirement: "EXTR-01"
    verification:
      - kind: unit
        ref: "tests/Core/VBAPTripletSelectionTests.cpp#EAR tracer: a source directly under the 30-degree speaker gets unity on it (D-04 meridian)"
        status: pass
      - kind: unit
        ref: "tests/Core/VBAPTripletSelectionTests.cpp#EAR tracer: a below-horizon source never reaches an elevated speaker on 7.1.4 (D-04, D-05)"
        status: pass
    human_judgment: false
  - id: D3
    description: "computeVBAPGains3D output at every elevation >= 0.5 degrees on all 8 height layouts is bit-identical (==) to the d43cb15 function (262,104 probes)"
    requirement: "EXTR-01"
    verification:
      - kind: unit
        ref: "tests/Core/VBAPTripletSelectionTests.cpp#VBAP 3D: above-horizon output is bit-identical to the pre-change function on every height layout (D-04, D-06b)"
        status: pass
    human_judgment: false
  - id: D4
    description: "layoutHasHeight and buildVBAPTripletsForLayout share one 0.0175 rad threshold; kLayoutExpectations is tied to NUM_LAYOUT_DEFS; lower-hemisphere triplets are flagged, ear-level-only and appended only to height layouts"
    requirement: "EXTR-01"
    verification:
      - kind: unit
        ref: "tests/IO/SpeakerLayoutTests.cpp#SpeakerLayout: layoutHasHeight and the triplet builder agree at 0.5, 0.8 and 1.1 degrees (D-03)"
        status: pass
      - kind: unit
        ref: "tests/IO/SpeakerLayoutTests.cpp#SpeakerLayout: lower-hemisphere triplets are flagged, ear-level-only, and appended only to height layouts (D-04)"
        status: pass
    human_judgment: false
  - id: D5
    description: "The D-02a abort fires in every build type on a layout-table/builder mismatch, only in activateLayout on the message/prepare thread, never reachable from renderBlock"
    requirement: "EXTR-01"
    verification:
      - kind: other
        ref: "awk '/^void RenderEngine::renderBlock/,/^}/' src/Engine/RenderEngine.cpp | grep -c abort  (returns 0); activateLayout has exactly one code-line std::abort"
        status: pass
    human_judgment: true
    rationale: "The abort itself is intentionally untested at runtime (it kills the test process). Its placement and absence from renderBlock are checked by grep; that it is reachable only by a developer editing layoutDefs is a design claim, not something a test asserts."
  - id: D6
    description: "The empty-triplet nearest-speaker branches no longer exist in VBAP/VBIP/MDAP, and the public surface OpenSpatialDelay compiles against is pinned"
    requirement: "EXTR-01"
    verification:
      - kind: unit
        ref: "tests/Core/VBAPTripletSelectionTests.cpp#DR-3: the public surface OpenSpatialDelay compiles against is unchanged (D-02b)"
        status: pass
      - kind: other
        ref: "test -z \"$(grep -rn nearestSpeaker3DFallback src/)\""
        status: pass
    human_judgment: false

duration: 6 min
completed: 2026-10-01
---

# Phase 2 Plan 01: EAR lower hemisphere and VBAP fallback removal Summary

**ITU-R BS.2127 (EAR) lower-hemisphere panning for below-horizon sources on all 8 height layouts, proven bit-identical to the old `computeVBAPGains3D` above the horizon, plus deletion of the four empty-triplet nearest-speaker branches and a Release abort on layout-table mismatch.**

## Performance

- **Duration:** about 6 min (ledger created 00:08:09, close-out at 00:14)
- **Tasks:** 3 of 3, plus 1 corrective commit
- **Files:** 10 changed (1 created, 9 modified), +806 / -51 against `plan_head_before`

## Accomplishments

- **Tracer (Task 1), `69eae4d`.** `appendLowerHemisphereTriplets` builds the lower hull of the ear-level speakers, a virtual -30 degree copy under each, and a virtual nadir, in double precision at layout-build time. `computeVBAPGains3D` now runs two passes: regular triplets first with the loop body unchanged, lower-hemisphere triplets only if pass 1 found nothing. On 7.1.4 a nadir source gives 0.37796447 (1/sqrt(7)) on each of the 7 ear-level speakers and 0 on the 4 height speakers; a source under the 30 degree speaker gets unity on it.
- **Bit-identity proof.** 262,104 probes (360 azimuths x 91 elevations, 0.5 to 90 degrees, on 8 height layouts) compared with exact `==` against a verbatim copy of `computeVBAPGains3D` from the `d43cb15` git blob. Zero mismatches. Every probe was also confirmed to sit inside a regular triplet, so the old fallback is never hit above the horizon.
- **One height threshold (Task 2), `edecf03`.** `layoutHasHeight` and the builder now share `kHeightThresholdRad = 0.0175f`. The `[d03]` test was written first and failed at 0.8 degrees (`false == true`, the documented mismatch) before the fix. Only then was the Release `std::abort()` armed in `activateLayout`, in the order RESEARCH Pitfall 5.1 requires.
- **Fallback branches deleted (Task 3), `266fd40`.** The four `else if (layoutHasHeight ...)` branches in VBAP, VBIP and MDAP (two sites) are gone; each site is now triplets -> 3D, else 2D, with a Debug-only `jassert`. The public `nearestSpeaker3DFallback` stays in `SpatialMath.h`, untouched.
- **Consumer surface pinned.** A `[consumer-surface]` test binds `setOutputFormat`, field-by-field `VBAPTriplet` construction, the 4-member `LayoutContext`, and seven free-function signatures.

### Lower-hemisphere triplet counts (vs RESEARCH F13)

Measured with a throwaway probe (reverted, not committed). Identical to F13 on every layout.

| Layout | Regular | Lower-hemisphere |
|--------|---------|------------------|
| 5.1.2 | 25 | 25 |
| 5.1.4 | 72 | 25 |
| 7.1.2 | 45 | 35 |
| 7.1.4 | 118 | 35 |
| 7.1.6 | 235 | 35 |
| 9.1.4 | 190 | 45 |
| 9.1.6 | 355 | 45 |
| SML13.1 | 188 | 40 |

### `[vbap3d-identity]` mismatches

None. No investigation was needed. The plan's flagged assumption stands: exact coplanar ties depend on float rounding that could differ between compilers. This run was Apple clang on arm64 only; cross-compiler determinism is not claimed here.

## Test results

- Full suite, Debug: **156 test cases, 155 passed, 1 failed** (284,444 assertions).
- The one failure is `HUTUBS PP2 — golden HRIR checksum at az=90deg (own control)`. It is **pre-existing** (Phase 1 `deferred-items.md` item 01-01) and unrelated to this plan. It is still the only failure.
- With that test excluded by name the suite exits 0: **155 cases, all passed** (284,441 assertions). Baseline was 149 cases; this plan adds 7 (3 `[tracer]`, `[vbap3d-identity]`, `[d03]`, `[io][layout][ear]`, `[consumer-surface]`).
- The "JUCE Assertion failure in RenderEngine.cpp:198" line in the output is the intentional oversized-block test, present before this plan.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Line endings of `tests/CMakeLists.txt` were rewritten**
- **Found during:** close-out review of `git diff --stat` (101 changed lines where 1 was expected)
- **Issue:** The file uses CRLF. My scripted edit in Task 1 read and wrote it in text mode, normalising every line to LF and turning a one-line registration into a whole-file diff. No other touched file had CRLF.
- **Fix:** Restored the original bytes from `bed3f27` and re-applied only the added source line. Net change against the pre-plan file is now `1 insertion`.
- **Files modified:** `tests/CMakeLists.txt`
- **Verification:** `git diff bed3f27 HEAD --stat -- tests/CMakeLists.txt` shows 1 insertion; suite rebuilt and re-run, 155 passed with HUTUBS excluded.
- **Commit:** `ef95296`

### Acceptance-criterion wording (not a code deviation)

**Task 1, criterion "`grep -c -- '-1e-6f'` in `computeVBAPGains3D` returns at least `2`".** The command counts matching *lines*. The tolerance appears three times, all on one line (`g0 >= -1e-6f && g1 >= -1e-6f && g2 >= -1e-6f`), so it returns `1`. The pre-change function also returns `1` by this command. I kept a single loop body shared by both passes rather than duplicating it to reach a line count. The intent (tolerance unchanged, D-07) is met: `grep -o` finds 3 occurrences, and `-1e-5f` appears nowhere (`0`). The plan author likely assumed two separate loops.

**Total deviations:** 1 auto-fixed (Rule 1, self-inflicted), 1 criterion-wording note.
**Impact:** None on behaviour. The CRLF fix is diff hygiene only.

## Decisions Made

- Used the lower-hemisphere-only hull as RESEARCH F4 specifies, after the plan's own F4 analysis showed the full-sphere hull pans onto height speakers on 5.1.4.
- Shared one loop for both selection passes (keyed on `lowerHemisphere != wantLower`) instead of two copies, which keeps pass-1 arithmetic textually identical to the old code.
- Wrote the D-03 test as a loop with `DYNAMIC_SECTION` per elevation rather than `GENERATE`, matching the plan's "one SECTION per elevation".

## Known Stubs

None.

## Threat Flags

None. The only new trust-boundary behaviour is the D-02a abort, which is in the plan's threat model (T-02-02). It lives only in `activateLayout`; `renderBlock` contains no `abort`.

## Authentication Gates

None.

## Issues Encountered

- The `[d03]` red step and the `tests/CMakeLists.txt` line-ending problem are covered above. Nothing else blocked.
- The pre-existing HUTUBS PP2 checksum failure remains and is owned by Phase 3.

## Next Phase Readiness

Ready for the next plan. Plan 02-04 replaces the remaining no-triplet snap inside `computeVBAPGains3D` (D-06b); that block was deliberately left untouched here. Plan 02-07 Task 1 adds the comment-only deprecation note to `nearestSpeaker3DFallback` in `SpatialMath.h`.

## Self-Check: PASSED

- Created/modified files exist: `tests/Core/VBAPTripletSelectionTests.cpp`, `src/IO/SpeakerLayout.cpp`, `include/SpatialCore/IO/SpeakerLayout.h`, `src/Core/SpatialMath.cpp`, `src/Engine/RenderEngine.cpp`.
- Commits found in `git log`: `69eae4d`, `edecf03`, `266fd40`, `ef95296`; `git log --all --grep="02-01"` returns 4.
- All task acceptance criteria re-run and passing (criterion-wording note above).
- Plan-level verification: build exits 0; `[tracer]`, `[vbap3d-identity]`, `[d03]`, `[io][layout]`, `[consumer-surface]` each pass; suite minus the known HUTUBS failure exits 0; `grep -rn nearestSpeaker3DFallback src/` prints nothing.
