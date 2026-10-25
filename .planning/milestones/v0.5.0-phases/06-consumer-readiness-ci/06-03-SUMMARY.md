---
phase: 06-consumer-readiness-ci
plan: 03
subsystem: testing
tags: [catch2, regression, mutation-testing, osd50, osd89, osd96, osd131, osd234, convolver, hrtf, test-01]

requires:
  - phase: 06-consumer-readiness-ci
    provides: "plan 06-02: configured build/ and build-release/ trees, [intg02] detector"
provides:
  - "[osd50] click-free mid-stream IR change test at 64 and 512 samples, red against a hard swap"
  - "[osd89] KEMAR zero-delay onset fallback test, red when the fallback is removed"
  - "[osd89] great-circle pole threshold test with an ear-level control, red against independent az/el checks"
  - "Seven-row D-17 mutation evidence table covering OSD#50, #89 x2, #96, #131 x2, #234"
  - "TEST-01 dedicated-test evidence for PartitionedConvolverTests.cpp and BinauralRendererTests.cpp"
affects: [06-07-coverage, 06-09, verify-work]

plan_head_before: a9f35be3e6e809d93dd64a2d138fc7dd91adb383
plan_head_after: d2d605922ea91694dd6bcc021ca5a1667bdc3a05

actuals:
  tokens: 3430
  tasks: 3
  commits: 2

tech-stack:
  added: []
  patterns:
    - "Regression test is shown red against a scratch restoration of the old behaviour, then the code is restored with git checkout (Phase 5 D-07, Phase 6 D-17)"
    - "Local copy of the library's onset rule in a test, so the test states the expectation instead of calling the code under test"

key-files:
  created: []
  modified:
    - tests/Binaural/PartitionedConvolverTests.cpp
    - tests/Binaural/MitKemarLargePinnaTests.cpp
    - tests/Binaural/BinauralRendererTests.cpp

key-decisions:
  - "The four existing regression tests (#96, #131 x2, #234) were re-run red at this HEAD rather than cited from older summaries, as D-17 literally asks"
  - "Neighbouring KEMAR cases REQUIRE the SOFA load, so the new [osd89] and [osd50] cases do the same instead of skipping; a missing HRTF folder fails loudly"
  - "Task 3 changed no tracked file: its evidence is the table below, so it has no commit of its own"

requirements-completed: [TEST-01]

coverage:
  - id: D1
    description: "OSD#50: an IR change mid-stream is click-free at 64- and 512-sample calls (step ratio 1.00, RMS never below 0.95x steady, output equals direct convolution with the new IR within 2e-6), and a hard swap into the playing slot turns it red"
    requirement: TEST-01
    verification:
      - kind: unit
        ref: "tests/Binaural/PartitionedConvolverTests.cpp#PartitionedConvolver: an IR change mid-stream is click-free (OSD#50 dual slots)"
        status: pass
    human_judgment: false
  - id: D2
    description: "OSD#89 onset fallback: KEMAR's reported delays equal its raw waveform onsets and the aligned ears start within 1 sample at az +90, -90 and +45; removing the zero-delay branch turns it red"
    requirement: TEST-01
    verification:
      - kind: unit
        ref: "tests/Binaural/MitKemarLargePinnaTests.cpp#MIT KEMAR Large Pinna - OSD#89: zero SOFA delays fall back to the waveform onsets, so the aligned ears start together"
        status: pass
    human_judgment: false
  - id: D3
    description: "OSD#89 pole threshold: at elevation 89.5 a 30 degree azimuth change keeps the HRIR (bit-identical output) and a 2 degree ear-level move changes it; independent az/el checks turn it red"
    requirement: TEST-01
    verification:
      - kind: unit
        ref: "tests/Binaural/BinauralRendererTests.cpp#BinauralRenderer: OSD#89 near the pole a large azimuth change under 1 degree of arc keeps the HRIR"
        status: pass
    human_judgment: false
  - id: D4
    description: "OSD#96, #131 (map faces and FFT cache) and #234 regression tests each shown red again at this HEAD against a scratch restoration of the old behaviour, and green after restore"
    requirement: TEST-01
    verification:
      - kind: unit
        ref: "./build-release/tests/SpatialCoreTests \"[multi][isolation]\"; ./build-release/tests/SpatialCoreUITests \"[ui][multi]\"; ./build-release/tests/SpatialCoreTests \"[fftcache]\"; ./build-release/tests/SpatialCoreTests \"[bug02],[convolver][transition]\""
        status: pass
    human_judgment: false
  - id: D5
    description: "TEST-01 dedicated-test clause: PartitionedConvolverTests.cpp (7 cases) and BinauralRendererTests.cpp (11 cases) are tracked and in the SpatialCoreTests source list"
    requirement: TEST-01
    verification:
      - kind: other
        ref: "git ls-files; tests/CMakeLists.txt lines 30-31; grep -c TEST_CASE"
        status: pass
    human_judgment: false

duration: 17min
completed: 2026-10-17
status: complete
---

# Phase 6 Plan 03: OSD regression tests with D-17 mutation evidence Summary

**Every OpenSpatialDelay issue named by ROADMAP Phase 6 criterion 5 (#50, #89, #96, #131, #234) now has a regression test that was shown red against its old behaviour at this HEAD: three new Catch2 cases (`[osd50]`, two `[osd89]`) plus four re-run mutations, with no library change left behind.**

## Performance

- **Duration:** 17 min
- **Started:** 2026-10-17T15:52Z (approx.)
- **Completed:** 2026-10-17T16:09Z
- **Tasks:** 3 (Task 3 is evidence only)
- **Files modified:** 3 test files
- **BASE:** `a9f35be3e6e809d93dd64a2d138fc7dd91adb383`

## Accomplishments

- OSD#50 had no test of its own. `[osd50]` feeds a 440 Hz sine through a convolver, changes the IR mid-stream from "pass through" to "delay by 27 samples" (a quarter period, so an equal-power fade neither dips nor bumps), and checks the largest step, the RMS floor and the settled output against a direct convolution.
- OSD#89 was only a comment. Its two halves now have direct tests: the KEMAR waveform-onset fallback (`getAlignedHRIR`) and the great-circle HRIR reload threshold (`updateSourceHRIR`), the latter with an ear-level control so a renderer that ignores `updateSourceHRIR` cannot pass.
- The four existing regression tests (#96, #131 map faces, #131 FFT cache, #234) were each driven red again at this HEAD and restored, rather than citing the Phase 3 and Phase 5 records.
- No mutation remains: `git diff -- src include` is empty and `git log a9f35be..HEAD -p -- src include` shows nothing.

## Green measurements (targets from the plan truths)

| Case | Measured (Release; Debug green too) | Target |
|------|-------------------------------------|--------|
| `[osd50]` 64-sample calls | steady step 0.0230351; step through the change 0.0230352 (ratio 1.00); min 256-sample RMS 0.2746 (0.972x steady); worst abs diff vs IR B 1.42e-7; max abs diff vs IR A 0.561 | ratio <= 1.5; RMS >= 0.8x; vs B <= 2e-6; vs A > 1e-3 |
| `[osd50]` 512-sample calls | steady step 0.0230352; step through the change 0.0230352 (ratio 1.00); min 256-sample RMS 0.2698 (0.954x steady); worst abs diff vs IR B 1.87e-7; max abs diff vs IR A 0.561 | same |
| `[osd89]` onset, az +90 | raw delays 0/0; raw onsets L 31, R 61; reported delays 31/61; aligned onsets 31/31 | delayR - delayL == rawOnsetR - rawOnsetL; aligned onsets within 1; raw onsets >= 10 apart at +-90 |
| `[osd89]` onset, az -90 | raw delays 0/0; raw onsets 61/31; reported 61/31; aligned 31/31 | same |
| `[osd89]` onset, az +45 | raw delays 0/0; raw onsets 34/51; reported 34/51; aligned 34/34 | same (the 10-sample rule applies to +-90 only) |
| `[osd89]` pole, el 89.5, az 0 to 30 | max abs diff 0 over 8192 samples; memcmp 0 on both ears | bit-identical |
| `[osd89]` control, 2 degrees at ear level | max abs diff 0.2186 | > 1e-6 |

Every green value met its target, so no bound was changed.

## D-17 Mutation Evidence (seven rows)

All mutations were made in the working tree only, built in Release, run under a 600-second alarm, and restored with `git checkout -- <file>` before the next. Each was followed by a rebuild and a green re-run.

| # | Issue | Mutation (scratch, never committed) | Test run | Red result at this HEAD | Earlier record it confirms | Green after restore |
|---|-------|--------------------------------------|----------|--------------------------|----------------------------|---------------------|
| 1 | OSD#50 | `PartitionedConvolver::setIR` Idle branch loads the new IR into the active slot and returns (old single-slot hard swap) | `"[osd50]"` | `CHECK( through <= 1.5 * steady )` failed in both sections: 0.22136 <= 0.03455, step ratio **9.61** at 64 and at 512 samples (2 of 12 assertions failed) | none (no prior test named #50; `[convolver][transition]` pins the fade but not #50) | `"[osd50],[convolver],[bug02]"` 84 assertions in 6 cases; Debug `"[osd50]"` 12 assertions |
| 2 | OSD#89 onset | Deleted the zero-delay `if` block of `HRTFDatabase::getAlignedHRIR` | `"[mitkemar][osd89]"` | at az +90, -90 and +45: `CHECK( delayR - delayL == rawOnsetR - rawOnsetL )` failed (reported delays 0/0 against 30, -30 and 17) and `CHECK( abs (alignedOnsetR - alignedOnsetL) <= 1 )` failed (30, 30 and 17 samples apart); 6 of 16 assertions failed | none (the D-16 ITD table is not labelled #89) | `"[osd89]"` 28 assertions in 2 cases (Release and Debug); `"[binaural]"` 136 assertions in 22 cases |
| 3 | OSD#89 pole | `BinauralRenderer::updateSourceHRIR` great-circle early return replaced by independent azimuth and elevation checks | `"[renderer][osd89]"` | both `CHECK( memcmp (...) == 0 )` failed (left and right, memcmp result 3); max abs diff 0.1322 between the renderer that stayed and the one told it moved; the ear-level control stayed green (0.2186), as designed | none | `"[osd89]"` and `"[binaural]"` green as above |
| 4 | OSD#96 | `BinauralRenderer::hrtfDatabase` became a reference bound to one function-local static `HRTFDatabase` | `"[multi][isolation]"` | crash (exit 133 SIGTRAP, 133, 134 SIGABRT): **3 of 3 runs** | 05-06-SUMMARY "D-07 Mutation Results" row 1 (crash, 3 of 3) | 8 assertions in 1 case, exit 0 |
| 5 | OSD#131 map faces | `SpatialMapComponent::monoFaces_` became a per-instance `std::unique_ptr<MonoFaces>` | `SpatialCoreUITests "[ui][multi]"` | 4 of 17 assertions failed: construction delta **16 == 1**, 15 of 16 face identities differ, `countAfterClear - countBefore` 16 == 1, long-lived repaint delta 221 == 21 | 05-07-SUMMARY "D-07 Mutation Result" (delta 16, 15 of 16 differ) | 17 assertions in 2 cases |
| 6 | OSD#131 FFT cache | `SharedFFTCache::getOrCreate` returned a fresh `std::make_shared` FFT every call | `"[fftcache]"` | 3 of 4 cases failed, 6 of 34 assertions: two requests for one order returned different objects, the cache did not keep an FFT alive, the concurrent case saw 15 distinct FFTs per order (`differing14 == 15`, `differing15 == 15`) | 05-06-SUMMARY "D-07 Mutation Results" rows 2 and 3 | `"[fftcache]"` passes inside the full Debug and Release runs |
| 7 | OSD#234 | `PartitionedConvolver::process`: warm-up ends on `stateBlockCount >= kWarmupBlocks` alone and the fade is `kCrossfadeBlocks * numSamples` with no sample floor | `"[bug02],[convolver][transition]"` | `[bug02][moving]` `step64` 0.02791 vs bound 0.02685 and `step32` 0.07806 vs 0.02685 (2.9x); `[convolver][transition]` 32-sample section: warm-up `worstWarmup` 0.4997 vs 2e-6 and `midFromB` 5.6e-8 vs 1e-3; 558-sample section `worstWarmup` 0.1981 vs 2e-6; 5 of 55 assertions failed | 03-03-SUMMARY "Task 2 measurements" (32-sample and 558-sample sections failed on the pre-change convolver) | `"[bug02],[convolver][transition],[osd50]"` 67 assertions in 4 cases |

All seven were red. After the last restore `git status --porcelain -- src include` prints nothing.

## TEST-01 dedicated-test evidence

| File | Tracked (`git ls-files`) | Line in `tests/CMakeLists.txt` (SpatialCoreTests source list) | `TEST_CASE` count |
|------|--------------------------|----------------------------------------------------------------|-------------------|
| `tests/Binaural/PartitionedConvolverTests.cpp` | yes | 30 | 7 |
| `tests/Binaural/BinauralRendererTests.cpp` | yes | 31 | 11 |

Coverage numbers for both files follow in Plan 06-07.

## Task Commits

1. **Task 1 (tracer): OSD#50 click-free IR change** - `7044bf5` (test)
2. **Task 2: OSD#89 onset fallback and pole threshold** - `d2d6059` (test)
3. **Task 3: re-run red evidence for #96, #131, #234 and TEST-01 evidence** - no commit (changes no tracked file, per the plan)

**Plan metadata:** recorded by the docs commit that adds this SUMMARY.

## Files Created/Modified

- `tests/Binaural/PartitionedConvolverTests.cpp` - `[convolver][osd50]` case, two sections (64 and 512-sample calls)
- `tests/Binaural/MitKemarLargePinnaTests.cpp` - `[binaural][mitkemar][osd89]` case and a local copy of the onset rule
- `tests/Binaural/BinauralRendererTests.cpp` - `[binaural][renderer][osd89]` case, two sections (pole, ear-level control)

## Decisions Made

- Re-ran the three previously recorded mutations (#96, #131, #234) instead of citing the old results: D-17 reads "confirm each of the four", and RESEARCH had suggested the cheaper reading. All three reproduced their recorded red, so the evidence is now current for this tree.
- Followed the neighbouring cases' style of `REQUIRE`-ing the SOFA load. The plan said "skip with a clear reason", but the surrounding KEMAR cases do not skip, and a skip would let a missing HRTF folder pass quietly.

## Deviations from Plan

None - plan executed exactly as written.

The TDD cycle for this plan is test-after-the-fact by nature (the behaviour under test already exists in the library), so the RED evidence is a mutation red run rather than a failing commit that precedes a `feat` commit. There is no `feat(06-03)` commit by design: no file under `src/` or `include/` changed.

## TDD Gate Compliance

The plan is `type: execute` with `tdd="true"` tasks over already-implemented behaviour. RED is the recorded mutation run for each new test (rows 1 to 3 above); the `test(06-03)` commits hold the tests; there is no GREEN implementation commit because the implementation exists and the library was left unchanged (`git log a9f35be..HEAD -p -- src include` is empty).

## Full gate

- Debug `./build/tests/SpatialCoreTests`: 399 test cases, 398 passed, 1 skipped, 329112 assertions all passed, exit 0. The one skip is `UnsuppliedGainsTests.cpp:499` ("the Debug assertion logs by design; the Release run is the proof"), from Plan 06-02, not this plan.
- Debug `./build/tests/SpatialCoreUITests`: 17 test cases, 152 assertions, exit 0.
- Release `ctest --test-dir build-release --no-tests=error`: 416 of 416 passed (399 core plus 17 `ui:`), 128 s; `ctest -N | grep -c 'ui:'` prints 17.
- The Debug runs end with the documented permanent-FFT leak report (D-08), unchanged.

## Issues Encountered

None. One scratch edit of a first draft used a BSD `sed` expression that failed before touching anything; the edit was redone with Python and no file was left half-changed.

## Known Stubs

None.

## Threat Flags

None. This plan adds tests only and changes no network, auth or file-access surface.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- Ready for 06-04. `[osd50]` and `[osd89]` exist as tags; Plan 06-07 can measure coverage for `PartitionedConvolver` and `BinauralRenderer` against the dedicated test files recorded above.
- ROADMAP Phase 6 criterion 5 now has recorded red evidence for all five OSD issues in this phase.

## Self-Check: PASSED

- `tests/Binaural/PartitionedConvolverTests.cpp`, `tests/Binaural/MitKemarLargePinnaTests.cpp`, `tests/Binaural/BinauralRendererTests.cpp` exist and are modified by the two task commits.
- Commits `7044bf5` and `d2d6059` are ancestors of HEAD.
- `git diff --quiet -- src include` passes; `git status --porcelain -- src include` prints nothing.
- Acceptance greps: `[osd50]` count 1 and `OpenSpatialDelay#50` count 1 in `PartitionedConvolverTests.cpp`; `[osd89]` count 1 and `OpenSpatialDelay#89` count 1 in each of the other two files; `"[osd89]"` reports 2 test cases passed, none skipped.

---
*Phase: 06-consumer-readiness-ci*
*Completed: 2026-10-17*
