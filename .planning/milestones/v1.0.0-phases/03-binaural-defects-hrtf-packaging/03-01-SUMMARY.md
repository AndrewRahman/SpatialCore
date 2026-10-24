---
phase: 03-binaural-defects-hrtf-packaging
plan: 01
subsystem: testing
tags: [catch2, binaural, hrtf, partitioned-convolver, itu-r-bs1770, k-weighting, itd, characterisation]

requires:
  - phase: 02-correctness-and-hardening
    provides: "Debug 208/209 and Release 209/209 baseline, build-release tree, RenderEngine facade with engineComputesGains"
provides:
  - "tests/Binaural/BinauralMetrics.h: the shared Phase 3 measurement header (engine driver, impulse response, third-octave metric, K-weighting, switch-continuity metrics, generators, direct-convolution oracle, block plans, synchronous profile loader)"
  - "Dedicated PartitionedConvolverTests.cpp and BinauralRendererTests.cpp (EXTR-02)"
  - "BUG-01 HRTF-half distinguishability test on all five built-in profiles (D-03)"
  - "D-12 convolver oracle over seven block plans plus pending-IR deferral"
  - "D-16 pin of the 64-sample ITD line (20-row table, Debug and Release identical)"
  - "D-14 K-weighted loudness record of every profile (recorded, not asserted)"
  - "HDF5 signature check of the five SOFA files and the legacy-swap click baseline at 128 and 512 samples"
  - "tests/CMakeLists.txt complete for the whole phase (six new sources); no later Phase 3 plan edits it"
affects: [03-02, 03-03, 03-04, 03-05, 03-06, 03-07, 03-08, 03-09, 03-10, 03-11]

actuals:
  tokens: 13500
  tasks: 3
  commits: 4

plan_head_before: 0bde73c74929029747f8dce56bdc0187bf372385
plan_head_after: 8601043860e45eba9a9cafa7b228eba6d7a7f018
commits: 4

tech-stack:
  added: []
  patterns:
    - "Shared header-only measurement helpers in namespace spatialcore::test, written once and never edited by later plans"
    - "Engine impulse response (settle 12 blocks, then one unit impulse) as the oracle for HRTF cue tests"
    - "Characterisation table with capture date, commit and library version in the comment (pin, do not fix)"
    - "Record-only measurement via WARN, with only sanity assertions (D-14)"

key-files:
  created:
    - tests/Binaural/BinauralMetrics.h
    - tests/Binaural/BinauralCueTests.cpp
    - tests/Binaural/PartitionedConvolverTests.cpp
    - tests/Binaural/BinauralRendererTests.cpp
    - tests/Binaural/ProfileLoudnessTests.cpp
    - tests/Binaural/HRTFEmbeddedTests.cpp
    - tests/Engine/ProfileSwitchTests.cpp
  modified:
    - tests/CMakeLists.txt

key-decisions:
  - "Convolver oracle signals use IR amplitude 0.05 (not the plan's 0.5) so the output scale matches the research measurement the 2e-6 bound came from; the bound itself is unchanged"
  - "ITD characterisation pins the raw delays and rendered onsets exactly as measured; the wrap mechanism (rendered onset = aligned onset + floor(delay) mod 64, within 1 sample) holds on all 20 cases including KEMAR with its shelf"
  - "No loudness tolerance asserted and no profile level changed (D-14); the numbers go to the user once, at Plan 03-11 item D"

patterns-established:
  - "Test-only plan measures the tree as it ships: git diff against the plan base is empty for src, include and the top-level CMakeLists.txt"
  - "tests/CMakeLists.txt is CRLF; edit it byte-preserving (a text-mode rewrite turns the whole file into a diff)"

requirements-completed: [EXTR-02, BUG-01, BUG-02]

coverage:
  - id: D1
    description: "PartitionedConvolver and BinauralRenderer each have a dedicated test file compiled into SpatialCoreTests and run by every full-suite run"
    requirement: "EXTR-02"
    verification:
      - kind: unit
        ref: "build/tests/SpatialCoreTests [convolver] (2 test cases) and [renderer] (5 test cases)"
        status: pass
      - kind: integration
        ref: "ctest --test-dir build-release/tests: 221/221"
        status: pass
    human_judgment: false
  - id: D2
    description: "Standalone convolver matches a double-precision direct convolution within 2e-6 on all seven block plans; an IR set during Warmup or Crossfading is deferred and applied"
    requirement: "BUG-02"
    verification:
      - kind: unit
        ref: "tests/Binaural/PartitionedConvolverTests.cpp#[convolver][blocksize] and [convolver][pending-ir]"
        status: pass
    human_judgment: false
  - id: D3
    description: "HRTF path separates front from back and from overhead on all five built-in profiles (RMS band difference >= 2.0 dB, max >= 4.0 dB)"
    requirement: "BUG-01"
    verification:
      - kind: unit
        ref: "tests/Binaural/BinauralCueTests.cpp#[bug01][hrtf]"
        status: pass
    human_judgment: false
  - id: D4
    description: "BinauralRenderer contract pinned: profile 0 silence, setProfile normalisation to 1/sqrt(irLen), KEMAR shelf only at index 5, reset and invalidateSources clear ITD state"
    requirement: "EXTR-02"
    verification:
      - kind: unit
        ref: "tests/Binaural/BinauralRendererTests.cpp#[renderer][simple|normalisation|kemar-shelf|reset]"
        status: pass
    human_judgment: false
  - id: D5
    description: "64-sample ITD line characterised at 44.1 and 48 kHz for SADIE and KEMAR (raw delays and rendered onsets pinned, wrap mechanism asserted)"
    verification:
      - kind: unit
        ref: "tests/Binaural/BinauralRendererTests.cpp#[renderer][itd-characterisation] (Debug and Release)"
        status: pass
    human_judgment: false
  - id: D6
    description: "K-weighted loudness of each profile recorded; no tolerance asserted beyond sanity and the BS.1770 known answer"
    verification:
      - kind: unit
        ref: "tests/Binaural/ProfileLoudnessTests.cpp#[loudness] and [loudness][k-weighting]"
        status: pass
    human_judgment: true
    rationale: "D-14: whether a 2.48 LU spread among the built-in profiles is acceptable is a listening decision the user makes at Plan 03-11 item D; the test deliberately records and does not judge"
  - id: D7
    description: "HDF5 signature check of the five SOFA files and the legacy-swap click baseline at 128 and 512 samples"
    verification:
      - kind: unit
        ref: "tests/Binaural/HRTFEmbeddedTests.cpp#[hrtf-embed][signature]; tests/Engine/ProfileSwitchTests.cpp#[hrtf-switch][click][legacy]"
        status: pass
    human_judgment: false

duration: 12min
completed: 2026-10-04
status: complete
---

# Phase 3 Plan 01: Binaural Test Foundation Summary

**Shared binaural measurement header plus six new test files (convolver oracle, renderer contract, HRTF cue separation on all five profiles, 64-sample ITD pin, K-weighted loudness record, SOFA signature and legacy-swap baseline), all green on the unmodified tree in Debug and Release**

## Performance

- **Duration:** 12 min
- **Started:** 2026-10-04T12:56:10Z
- **Completed:** 2026-10-04T13:08:00Z
- **Tasks:** 3
- **Files modified:** 8 (7 created, 1 modified)

## Accomplishments

- One header (`BinauralMetrics.h`) carries every helper the phase needs, so Plans 03-02 to 03-11 include it and do not edit it. `tests/CMakeLists.txt` now lists every Phase 3 test source; no later plan touches it.
- EXTR-02 met: `PartitionedConvolver` and `BinauralRenderer` each have a dedicated test file (2 and 5 test cases), and the engine-level files are in the build.
- The oracles the code plans will be judged against are on file: the double-precision convolution oracle (D-12), the HRTF third-octave cue metric (D-03), the ITD pin (D-16), the loudness record (D-14) and the legacy-swap baseline.
- No file under `src/`, `include/` or the top-level `CMakeLists.txt` changed: `git diff --stat 0bde73c..HEAD -- src include CMakeLists.txt` is empty.

## Verification Results

| Gate | Result |
|---|---|
| Debug full suite (`./build/tests/SpatialCoreTests`) | 221 test cases, 220 passed, 1 failed: `HutubsPP2Tests.cpp:47` only (known, Plan 03-02 fixes it); 307,930 of 307,931 assertions |
| Release (`ctest --test-dir build-release/tests`) | 100% passed, 221/221 |
| Baseline before this plan | Debug 208/209, Release 209/209; this plan adds exactly 12 test cases |
| `[bug01][hrtf]` | 1 test case, 40 assertions (5 profiles x 2 pairs x 2 bounds x 2 checks) |
| `[convolver]` | 2 test cases; `[renderer]` 5 test cases; both pass in Debug and Release |
| `[renderer][itd-characterisation]` | Same 20-row table passes unchanged in Debug and Release |
| Source untouched | `git diff --stat 0bde73c..HEAD -- src include CMakeLists.txt` empty |

Expected Debug noise seen and not counted as failure: libmysofa parse traces on stderr, `JUCE Assertion failure in BinauralRenderer.cpp:162` whenever KEMAR (IR 558) runs at a maximum block of 557 or less, and the `juce_LeakedObjectDetector.h` assertion at exit.

## Measurements recorded for later plans

**HRTF cue separation (engine impulse response, 48 kHz; RMS dB / max dB; bounds 2.0 / 4.0):**

| Profile | front vs back | front vs up90 |
|---|---|---|
| 1 SADIE | 3.55 / 7.99 | 3.29 / 7.32 |
| 2 CIPIC | 4.80 / 15.12 | 4.59 / 13.03 |
| 3 HUTUBS | 3.39 / 7.89 | 3.91 / 9.63 |
| 4 Bernschuetz | 6.14 / 21.42 | 3.82 / 11.77 |
| 5 KEMAR | 6.30 / 19.01 | 3.59 / 8.61 |

KEMAR reproduces the research numbers exactly (6.30 / 19.01 and 3.59 / 8.61). The other four differ from 03-RESEARCH.md by fractions of a dB to a few dB (SADIE reads higher than the research's 2.90 / 6.24 and 3.05 / 7.09, which sat closest to the bound); every pair clears both bounds, and none was loosened. The weakest measured pair is now HUTUBS front vs back (3.39 / 7.89) and SADIE front vs up90 (3.29 / 7.32).

**Convolver vs double-precision direct convolution (IR 558, 20000 samples):** worst |diff| 1.52e-7 to 1.80e-7 across the seven plans (bound 2e-6; research 1.6e-7 to 2.1e-7). Pending-IR test: 1.16e-7 against the last IR, 0.769 against the superseded IR.

**ITD characterisation (raw delay L/R, rendered onset L/R, samples; captured at commit 7453d93, libmysofa v1.3.2):**

| Profile / rate | az 0 el 0 | az 90 | az -90 | az 180 | az 45 el 30 |
|---|---|---|---|---|---|
| SADIE 44.1 kHz | 75/79, 86/90 | 71/93, 78/100 | 91/74, 101/84 | 75/76, 86/87 | 78/83, 92/97 |
| SADIE 48 kHz | 82/86, 100/104 | 72/102, 80/110 | 66/82, 68/84 | 86/87, 108/109 | 83/91, 102/110 |
| KEMAR 44.1 kHz | 38/38, 76/76 | 29/56, 58/85 | 56/29, 85/58 | 40/40, 80/80 | 31/45, 62/76 |
| KEMAR 48 kHz | 41/41, 82/82 | 31/61, 62/92 | 61/31, 92/62 | 43/43, 86/86 | 34/49, 68/83 |

Largest SADIE 48 kHz raw delay measured here: 102 samples (asserted >= 64, so the line wraps for SADIE at 48 kHz). All raw delays are whole numbers at 3 decimals. The wrap mechanism (rendered onset = aligned onset + floor(delay) mod 64, within 1 sample) held on all 20 cases, KEMAR included.

**Loudness (K-weighted, 12 directions x 0.5 s of pink noise at amplitude 0.25, 48 kHz, Debug):**

| Profile | Mean LKFS | Lowest direction | Highest direction |
|---|---|---|---|
| 0 Simple (not comparable, includes its own distance gain) | -29.37 | -30.55 | -27.92 |
| 1 SADIE | -20.83 | -23.74 | -19.23 |
| 2 CIPIC | -19.80 | -22.60 | -17.54 |
| 3 HUTUBS | -20.16 | -24.21 | -18.18 |
| 4 Bernschuetz | -19.34 | -21.64 | -18.28 |
| 5 KEMAR | -18.34 | -22.68 | -16.74 |

Spread among profiles 1-5: **2.48 LU** (SADIE quietest, KEMAR loudest; research preview 2.35 LU at a different drive level). BS.1770 known answer reads -3.01 within 0.05.

**Legacy swap KEMAR to SADIE, worst step over steady step (sine 440 Hz):** 1.000 at 128 and 1.000 at 512 samples (bound 1.5; research measured 1.15 and 1.00). Pink-noise runs (recorded only): step ratio 1.142 at both sizes; minimum windowed RMS ratio 1.018 at 128 and 0.902 at 512 (the sine run reads 1.841 at 128 and 0.980 at 512, because KEMAR to SADIE is louder at 440 Hz).

**Renderer contract:** KEMAR shelf DC ratio index 5 over index 4 on the same data is 3.98 (= 10^(12/20)); index 3 equals index 4 exactly.

## Task Commits

1. **Task 1: Tracer, engine impulse-response measurement end to end (`[bug01][hrtf]`)** - `7453d93` (test)
2. **Task 2: Dedicated PartitionedConvolver and BinauralRenderer test files with D-16 ITD characterisation** - `a2be01e` (test)
3. **Task 3: D-14 loudness record, HDF5 signature check, legacy-swap click baseline** - `39a28e9` (test)
4. **CRLF repair (see deviations)** - `8601043` (style)

**Plan metadata:** committed after this summary (docs: complete plan)

## Files Created/Modified

- `tests/Binaural/BinauralMetrics.h` - shared Phase 3 measurement helpers, all `inline` in `spatialcore::test`
- `tests/Binaural/BinauralCueTests.cpp` - `[bug01][hrtf]` front/back/overhead separation, five profiles
- `tests/Binaural/PartitionedConvolverTests.cpp` - `[convolver][blocksize]` and `[convolver][pending-ir]`
- `tests/Binaural/BinauralRendererTests.cpp` - `[renderer]` Simple, normalisation, KEMAR shelf, reset, and the D-16 ITD table
- `tests/Binaural/ProfileLoudnessTests.cpp` - `[loudness]` and `[loudness][k-weighting]`
- `tests/Binaural/HRTFEmbeddedTests.cpp` - `[hrtf-embed][signature]` (Plan 03-05 adds embedding and resolution tests here)
- `tests/Engine/ProfileSwitchTests.cpp` - `[hrtf-switch][click][legacy]` (Plans 03-06, 03-07, 03-09 add engine-owned switching tests here)
- `tests/CMakeLists.txt` - six new sources in `SpatialCoreTests`

## Decisions Made

- Kept the research-derived bounds (2.0 / 4.0 dB cue, 2e-6 convolver, 1.5 step ratio) unchanged; none was loosened.
- Pinned ITD behaviour as it ships rather than fixing it (D-16); a later change to those 20 rows is a change to shipped timing and needs a decision.
- Recorded loudness without a tolerance (D-14).

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Convolver oracle signal scale did not match the scale the 2e-6 bound was measured at**
- **Found during:** Task 2
- **Issue:** With the plan's amplitudes (IR 0.5, input 0.5) the output standard deviation is about 2.0, ten times the research probe's (IR sigma 0.03, input sigma 0.3, output about 0.2). Float FFT error scales with output size, so the worst difference read 1.55e-6 to 1.75e-6: inside the 2e-6 bound in Debug but with about 12% margin instead of the 10x the research intended, which would make the test flaky across compilers and build types.
- **Fix:** IR amplitude 0.05 (input amplitude unchanged at 0.5, seeds unchanged) in both convolver tests, with a comment explaining the scale. The bound stays 2e-6. Results now read 1.52e-7 to 1.80e-7, matching the research's 1.6e-7 to 2.1e-7.
- **Files modified:** tests/Binaural/PartitionedConvolverTests.cpp
- **Verification:** `[convolver]` passes in Debug and Release.
- **Committed in:** a2be01e (Task 2 commit)

**2. [Rule 3 - Blocking] tests/CMakeLists.txt line endings**
- **Found during:** SUMMARY preparation (`git diff --stat` showed 59 insertions / 53 deletions on a six-line change)
- **Issue:** The file is CRLF in the tree; my text-mode Python edits rewrote it as LF, turning the three task commits into whole-file diffs.
- **Fix:** Restored CRLF byte-for-byte; the net diff against the plan base is now the six added lines only.
- **Files modified:** tests/CMakeLists.txt
- **Verification:** `git diff --stat 0bde73c..HEAD -- tests/CMakeLists.txt` shows 6 insertions; `cmake -S . -B build` still configures.
- **Committed in:** 8601043

---

**Total deviations:** 2 auto-fixed (1 bug in test scale, 1 blocking line-ending repair)
**Impact on plan:** Neither changes a stated bound or the plan's scope. The first keeps the bound meaningful; the second removes diff noise.

## Issues Encountered

- SADIE, CIPIC, HUTUBS and Bernschuetz cue numbers and the legacy-swap 128 step ratio differ somewhat from 03-RESEARCH.md (see tables); every value is inside its bound, so nothing was stopped or loosened. The research probe used a different harness, so exact agreement was not expected except where the setup matched (KEMAR).
- Tracer gate: autonomous run, interactive mode, end-of-phase human-verify default, automated-only verify. Task 1 was written with the five-profile loop directly rather than KEMAR-first; the verify was re-run and passed (KEMAR numbers match research exactly), so expansion proceeded.

## Known Stubs

None. No stub, skipped test or unrun verify was left behind; nothing was appended to `.planning/WINDOWS.md`.

## Threat Flags

None. Test-only files; no new network, auth, file-access or trust-boundary surface beyond the SOFA read the plan's threat model already covers (T-03-01, T-03-02 mitigated by the bounds and the signature test).

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- Plan 03-02 (HUTUBS golden replacement) runs next and clears the one known Debug failure.
- Plans 03-03 to 03-09 can be judged against measured behaviour: the ITD table must not move unless a decision says so, the loudness record must not move (D-14), and the 32 and 64 sample legacy-swap cases (8.19 and 3.23 per research) are for Plan 03-09 to add.
- Phase 3 tests build in the shared `build/` and `build-release/` trees one plan at a time, as the plan requires.

## Self-Check: PASSED

All seven created files exist on disk; commits 7453d93, a2be01e, 39a28e9 and 8601043 exist; `src/`, `include/` and the top-level `CMakeLists.txt` are unchanged against the plan base; all task acceptance criteria re-checked (CMake lists the six new sources, `[convolver]` lists 2 test cases, `[renderer]` lists 5, the capture comment is present, ctest reports 100%).

---
*Phase: 03-binaural-defects-hrtf-packaging*
*Completed: 2026-10-04*
