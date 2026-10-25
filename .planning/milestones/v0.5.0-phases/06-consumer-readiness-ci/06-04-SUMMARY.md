---
phase: 06-consumer-readiness-ci
plan: 04
subsystem: infra
tags: [consumer-harness, git-submodule, cmake, juce, vst3, headless-sound-check, intg-01, hrtf-embedded]

requires:
  - phase: 06-consumer-readiness-ci
    provides: "plan 06-01 umbrellas (SpatialCore.h, SpatialCoreUI.h) and consumed-mode default; plan 06-02 getUnsuppliedGainsBlockCount(); plan 06-03 regression tests"
provides:
  - "examples/harness: a consumer template (CMake project, HarnessProcessor/HarnessEditor, HarnessCheck sound check)"
  - "examples/harness/scripts/build-consumer.sh: throwaway consumer repo with a real GitHub submodule at a given commit, Release build, warning gate, sound check"
  - "The full D-04 headless sound check: 23 formats, 12 algorithms, fades, counters, editor"
  - "An OpenSpatialDelay-like configuration (libmysofa v1.3.2, -ffast-math, CMake policy 3.5) that also builds and passes"
affects: [06-05-ci, 06-07-coverage, 06-09, 06-10, osd-migration]

plan_head_before: c5dd31813db76900854645975077081d6f6b9c6b
plan_head_after: 206420e764ad56af61f36b5bff4296c5978c14e7

actuals:
  tokens: 11400
  tasks: 2
  commits: 2

tech-stack:
  added: []
  patterns:
    - "Throwaway consumer repo: the submodule step is performed against the GitHub remote at a fetched commit, never against the local folder"
    - "Sound check tests finiteness on the float's exponent bits so it stays honest under -ffast-math"
    - "Shared juce::SharedResourcePointer<SMLLookAndFeel> in the editor, setLookAndFeel(nullptr) in its destructor"

key-files:
  created:
    - examples/harness/CMakeLists.txt
    - examples/harness/src/HarnessPlugin.h
    - examples/harness/src/HarnessPlugin.cpp
    - examples/harness/src/SoundCheck.cpp
    - examples/harness/scripts/build-consumer.sh
    - examples/harness/README.md
  modified: []

key-decisions:
  - "The harness is a template copied into a throwaway repository, not a committed submodule (no .gitmodules in this repo, nothing in SpatialCore's own build)"
  - "Fade-in and fade-out are checked on channel 0 only, with exact float equality (juce::exactlyEqual), for every switch except the first format after prepare"
  - "First format (Binaural) is not republished after the warm-up, because a republish of the active format costs a two-block dip"
  - "The warning gate matches /consumer/(src|SpatialCore/include|SpatialCore/src)/ so /tmp versus /private/tmp spellings of the work directory cannot hide a warning"

requirements-completed: [INTG-01]

coverage:
  - id: D1
    description: "A consumer holding only a git submodule of SpatialCore from GitHub at the pushed commit builds a Release VST3 plugin and a console check, both linking SpatialCore and SpatialCoreUI, and gets binaural sound from the embedded KEMAR profile"
    requirement: INTG-01
    verification:
      - kind: integration
        ref: "HARNESS_WORK_DIR=/tmp/sc-p6-harness examples/harness/scripts/build-consumer.sh <sha> (exit 0, HarnessCheck: PASS, submodule status equals the pushed commit)"
        status: pass
    human_judgment: false
  - id: D2
    description: "The sources include exactly the two umbrella headers and use every opt-in facade feature (setHRTFProfile, engineSelectsHRTF, engineFadesFormatSwitch, setAlgorithmIndex, getBlockLayout, engineComputesGains, engineDerivesDispatch)"
    requirement: INTG-01
    verification:
      - kind: other
        ref: "grep -h '#include <SpatialCore/' examples/harness/src/* ; grep -c for the seven names in HarnessPlugin.cpp (13 hits, each name at least once)"
        status: pass
    human_judgment: false
  - id: D3
    description: "Headless sound check over all 23 formats (finite, not silent, block layout reports the requested format, exact-0 fade out and in on each switch), 12 algorithm indices on 7.1.4 and Stereo, a moving object, profile 5 Ready from Embedded, both counters 0, editor painted with the SML look-and-feel"
    requirement: INTG-01
    verification:
      - kind: integration
        ref: "build-consumer.sh log: 23 'PASS format' lines, 0 'FAIL' lines, 'HarnessCheck: PASS (23 formats, 12 algorithms, profile 5 embedded)'"
        status: pass
      - kind: integration
        ref: "scratch mutations of the consumer copy: engineFadesFormatSwitch=false gave 22 FAIL lines; engineComputesGains=false gave 7 FAIL lines"
        status: pass
    human_judgment: false
  - id: D4
    description: "The Release build has zero compiler warnings in the harness's own sources and in SpatialCore's sources and public headers, with JUCE's recommended warning flags on the harness targets"
    requirement: INTG-01
    verification:
      - kind: integration
        ref: "build-consumer.sh warning gate (exit 4 on any); both runs print 'warnings: 0 in the harness and in SpatialCore'"
        status: pass
    human_judgment: false
  - id: D5
    description: "The OpenSpatialDelay-like configuration (consumer fetches libmysofa v1.3.2, -ffast-math on its own targets, -DCMAKE_POLICY_VERSION_MINIMUM=3.5) also builds and passes the sound check"
    requirement: INTG-01
    verification:
      - kind: integration
        ref: "HARNESS_OSD_LIKE=1 HARNESS_WORK_DIR=/tmp/sc-p6-harness-osd build-consumer.sh <sha> (exit 0, 23 format PASS lines)"
        status: pass
    human_judgment: false
  - id: D6
    description: "The script refuses a malformed commit id (exit 2), a non-GitHub remote (exit 2) and a commit the remote does not have (exit 3), and never falls back to a local copy"
    requirement: INTG-01
    verification:
      - kind: integration
        ref: "build-consumer.sh abc; SPATIALCORE_REMOTE_URL=/tmp/elsewhere build-consumer.sh 000...0; HARNESS_WORK_DIR=/tmp/sc-p6-harness-bad build-consumer.sh 000...0 (exits 2, 2, 3)"
        status: pass
    human_judgment: false
  - id: D7
    description: "The harness README reads clearly as OpenSpatialDelay's migration reference"
    verification: []
    human_judgment: true
    rationale: "Clarity of prose and the usefulness of the HarnessPlugin.cpp comments to a migrating author cannot be asserted by a test; the greps only prove the required names are present"

duration: 19min
completed: 2026-10-17
status: complete
---

# Phase 6 Plan 04: Consumer harness over a real GitHub submodule Summary

**`examples/harness` plus `build-consumer.sh` build a throwaway consumer repo around a real GitHub submodule of SpatialCore at the pushed commit, compile a Release VST3 and a console check with both link targets and two umbrella includes, and the check hears sound from all 23 output formats, all 12 algorithms and the embedded KEMAR profile, warning-clean, in the default and the OpenSpatialDelay-like configuration.**

## Performance

- **Duration:** 19 min wall clock
- **Started:** 2026-10-17T16:11:59Z
- **Completed:** 2026-10-17T16:31:44Z (plus the summary commit)
- **Tasks:** 2 (a tracer, then the expansion)
- **Files modified:** 6 created, 0 modified
- **BASE:** `c5dd31813db76900854645975077081d6f6b9c6b`

## Accomplishments

- INTG-01 on macOS: a repository that holds nothing but a git submodule of SpatialCore (from `https://github.com/AndrewRahman/SpatialCore.git`, at the exact commit) builds and runs. A default build takes about 95 s with JUCE and libmysofa reused.
- The harness is the copy-paste reference for OpenSpatialDelay: `HarnessProcessor::processBlock` sets all four context flags every block, reads the block layout with `getBlockLayout()` by reference after `renderBlock`, and each facade call carries a comment on why it is there.
- The sound check has teeth: with `engineFadesFormatSwitch` turned off in the consumer copy it fails 22 times (every switch), and with `engineComputesGains` off it fails 7 times (silent formats).
- The Release build is clean in everything this repo owns. SpatialCore's own sources and public headers produce no warning under the harness build; the only warning text in the log comes from libmysofa (`dataobject.c:625`, unused variable) and a make jobserver notice.

## Evidence

**Pushed commits.** The work branch `AndrewRahman/gsd-discuss-phase-6` was pushed three times (first push created the branch): `c5dd318` (carries 06-01 to 06-03, the library side), `dffd11b` (Task 1), `206420e` (Task 2). Never `main`, never forced.

**Submodule status (default run, final):** ` 206420e764ad56af61f36b5bff4296c5978c14e7 SpatialCore (206420e)`, equal to `git rev-parse HEAD`. The Task 1 run at `dffd11b` printed ` dffd11b60e692661805d55bac432dd569dabe98f SpatialCore (dffd11b)`, and the tracer's first run at `c5dd318` printed the same id as that push.

**Warning counts (both runs):** harness and SpatialCore: 0; lines mentioning `warning:` elsewhere: 2 (libmysofa `dataobject.c:625:41 -Wunused-but-set-variable`, and `make[3]: warning: jobserver unavailable`). The tracer's first run caught one real warning in the harness (`-Wshadow-field`, constructor parameter `processor`), fixed by renaming it before the Task 1 commit; Task 2's first run caught `createEditorIfNeeded` deprecated in JUCE 9, switched to `createEditorAndMakeActive()`.

**Refusal exit codes:** `build-consumer.sh abc` exits 2; `SPATIALCORE_REMOTE_URL=/tmp/elsewhere build-consumer.sh 000...0` exits 2; `build-consumer.sh 000...0` (a well-formed id the remote does not have, `fatal: remote error: upload-pack: not our ref`) exits 3.

**Per-format RMS** (default run; sum of the format's channels over the last 8 of 24 blocks, two objects at +30 and -30 degrees, 0.25 tone, distance 1):

| # | Format | Channels | RMS |
|---|--------|----------|-----|
| 0 | Binaural | 2 | 0.06762 |
| 1 | Stereo | 2 | 0.07725 |
| 2 | Quadraphonic | 4 | 0.07203 |
| 3 | 5.0 Surround | 5 | 0.05853 |
| 4 | 5.1 Surround | 6 | 0.05956 |
| 5 | 7.0 Surround | 7 | 0.05889 |
| 6 | 7.1 Surround | 8 | 0.05898 |
| 7 | 9.1 Surround | 10 | 0.05950 |
| 8 | Octaphonic | 8 | 0.06490 |
| 9 | 5.1.2 Atmos | 8 | 0.05898 |
| 10 | 5.1.4 Atmos | 10 | 0.05943 |
| 11 | 7.1.2 Atmos | 10 | 0.05951 |
| 12 | 7.1.4 Atmos | 12 | 0.05899 |
| 13 | 7.1.6 Atmos | 14 | 0.05936 |
| 14 | 9.1.4 Atmos | 14 | 0.05956 |
| 15 | 9.1.6 Atmos | 16 | 0.05902 |
| 16 | SpatialMediaLab 13.1 | 14 | 0.06515 |
| 17 | 1st Order Ambi | 4 | 0.09842 |
| 18 | 2nd Order Ambi | 9 | 0.10806 |
| 19 | 3rd Order Ambi | 16 | 0.11587 |
| 20 | 4th Order Ambi | 25 | 0.12354 |
| 21 | 5th Order Ambi | 36 | 0.12986 |
| 22 | 6th Order Ambi | 49 | 0.13783 |

Other lines: binaural warm-up rms 0.06741; algorithms on 7.1.4: 12 indices, 0 silent, lowest rms 0.05841; algorithms on Stereo: 12 indices, 0 silent, lowest rms 0.05870; moving object on Binaural rms 0.07250; HRTF `profile 5 ready (built-in)` before and after all switches; unsupplied-gains count 0; oversized-block count 0; editor 420 x 420 painted, uses the shared `SMLLookAndFeel`. The OSD-like run reports identical numbers.

**OSD-like result:** `HARNESS_OSD_LIKE=1 HARNESS_WORK_DIR=/tmp/sc-p6-harness-osd` exit 0, `HarnessCheck: PASS (23 formats, 12 algorithms, profile 5 embedded)`, 23 format PASS lines, 0 FAIL lines. The consumer's libmysofa was v1.3.2 (`git describe` in its `_deps`), SpatialCore skipped its own v1.3.5 fetch, the harness targets carried `-ffast-math -Wno-nan-infinity-disabled`, and the configure line carried `-DCMAKE_POLICY_VERSION_MINIMUM=3.5`. Run once locally, as the plan asks. No library defect was exposed by either configuration, so no library change or new SpatialCore test was needed.

## Task Commits

1. **Task 1 (tracer): consumer harness, script, binaural sound from the embedded profile** - `dffd11b` (feat)
2. **Task 2: full D-04 sound check and README** - `206420e` (feat)

**Plan metadata:** recorded by the docs commit that adds this SUMMARY.

## Files Created/Modified

- `examples/harness/CMakeLists.txt` - consumer project: JUCE 9.0.0, `add_subdirectory(SpatialCore)`, `HarnessPlugin` (VST3) and `HarnessCheck` (console), `HARNESS_OSD_LIKE` option
- `examples/harness/src/HarnessPlugin.h`, `HarnessPlugin.cpp` - `HarnessProcessor` and `HarnessEditor`; the two umbrella includes
- `examples/harness/src/SoundCheck.cpp` - the D-04 check; exit code is the verdict
- `examples/harness/scripts/build-consumer.sh` - committed mode 100755; exit codes 2, 3, 4, else the check's
- `examples/harness/README.md` - what it proves, how to run it, the OSD-like mode, the CMake 4 note, why it is not a committed submodule

## Decisions Made

- Kept the harness out of SpatialCore's build and out of `.gitmodules` (RESEARCH Pitfall 1). `git ls-files .gitmodules` prints nothing.
- Checked finiteness on exponent bits rather than `std::isfinite`, so the OSD-like fast-math run cannot silently pass a NaN.
- Skipped re-publishing Binaural for format 0, since the engine is already on it; every other switch calls `setOutputFormat` once and is fade-checked.
- Wrote the warning-gate pattern against the tail of the path (`/consumer/src/`, `/consumer/SpatialCore/include/`, `/consumer/SpatialCore/src/`), because CMake may log `/private/tmp/...` for a `/tmp/...` work directory.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] `-Wshadow-field` in `HarnessEditor` constructor**
- **Found during:** Task 1 (first tracer run, warning gate exit 4)
- **Issue:** constructor parameter `processor` shadowed the inherited `AudioProcessorEditor::processor`
- **Fix:** renamed the parameter to `owner`
- **Files modified:** `examples/harness/src/HarnessPlugin.h`, `HarnessPlugin.cpp`
- **Verification:** gate printed `warnings: 0`
- **Committed in:** `dffd11b`

**2. [Rule 1 - Bug] Deprecated `createEditorIfNeeded()` in the sound check**
- **Found during:** Task 2 (warning gate exit 4)
- **Issue:** JUCE 9 deprecates it in favour of `createEditorAndMakeActive()`
- **Fix:** switched the call
- **Files modified:** `examples/harness/src/SoundCheck.cpp`
- **Verification:** gate printed `warnings: 0`, check passed
- **Committed in:** `206420e`

### Plan wording that did not hold literally (not a code fix)

- Task 1 acceptance says `grep -rn "harness" CMakeLists.txt examples/CMakeLists.txt` prints nothing. It prints three lines, all pre-existing comments in the root `CMakeLists.txt` (lines 162, 241, 245: "OSD's own plugin/test/harness targets", "regression harness gate"), unrelated to this directory. The intent (the harness is not added to SpatialCore's build) holds: `grep -rn "examples/harness\|add_subdirectory.*harness"` over both files prints nothing.
- Task 1's `<verify>` runs with `$(git rev-parse HEAD)`, which must be on the remote. After each task commit I pushed the branch before running it; the first tracer runs used `c5dd318`, already pushed.

---

**Total deviations:** 2 auto-fixed (2 Rule 1, both in my own new code). **Impact on plan:** none; no library change.

## Issues Encountered

- `PIPESTATUS` is not set in the zsh tool shell, so one refusal exit code printed empty; it was re-run without a pipe and read 3.

## Known Stubs

None.

## Threat Flags

None. The new surface is a script that reaches git and cmake with a validated commit id and an https://github.com/-only remote (T-06-08, T-06-09: refusals proven, `rev-parse HEAD` equality enforced) and the push of the work branch (T-06-10, accepted). The LFS store is seeded from this checkout and the submodule clone skips smudging (T-06-11). All are in the plan's threat register.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- Plan 06-05 can run `examples/harness/scripts/build-consumer.sh "$GITHUB_SHA"` in CI: it needs git-lfs, cmake, a pushed commit, and the macOS toolchain. Pass `-- -DFETCHCONTENT_SOURCE_DIR_*` from a cache to avoid re-downloading JUCE. The script's LFS seeding only helps when the checkout has `lfs/objects` (it skips cleanly when not).
- The consumer build is about 95 s with dependencies reused; a cold JUCE and libmysofa fetch adds download time.
- The branch `AndrewRahman/gsd-discuss-phase-6` now exists on origin (a pushed branch can be removed with `git push origin --delete AndrewRahman/gsd-discuss-phase-6`).

## Self-Check: PASSED

- All six created files exist; `examples/harness/scripts/build-consumer.sh` is mode 100755 (`git ls-files -s`).
- Commits `dffd11b` and `206420e` are ancestors of HEAD; `git ls-files .gitmodules` is empty; `git diff c5dd318 --stat -- src include tests CMakeLists.txt` is empty.
- Acceptance criteria re-run: two umbrella includes only; zero `Spatial-Media-Lab` in the script and the CMake file; all seven facade names present in `HarnessPlugin.cpp` (13 hits); README names `build-consumer.sh`, `HARNESS_OSD_LIKE` and `CMAKE_POLICY_VERSION_MINIMUM`; 23 `PASS format` lines and 0 `FAIL` lines; both logs end `exit=0`; new files are LF.

---
*Phase: 06-consumer-readiness-ci*
*Completed: 2026-10-17*
