---
phase: 03-binaural-defects-hrtf-packaging
plan: 05
subsystem: binaural
tags: [hrtf, sofa, binarydata, juce, cmake, shared-folder, resolver, git-lfs]

requires:
  - phase: 03-binaural-defects-hrtf-packaging
    provides: "03-01 [hrtf-embed][signature] test and BinauralMetrics.h helpers; 03-04 Simple-path cue work (no overlap with this plan)"
provides:
  - "SpatialCoreHRTFData BinaryData target (all five SOFA files, or KEMAR only) linked PRIVATE into SpatialCore"
  - "SPATIALCORE_EMBED_ALL_HRTF option (default ON) and PUBLIC SPATIALCORE_EMBEDS_ALL_HRTF=0/1 definition"
  - "Configure-time HDF5 signature guard that refuses to embed a Git LFS pointer"
  - "HRTFProfile.h: D-07 profile table kHRTFProfiles and the status types"
  - "HRTFDatabase::getEmbeddedProfileData and loadFromBinaryData"
  - "HRTFProfileResolver.h: getSharedHRTFFolder, resolveHRTFProfile (shared folder, embedded copy, reported error), describeHRTFProfileStatus"
affects: [03-06 engine-owned profile switch, 03-08 libmysofa pin, SUITE-01 installer, OpenSpatialDelay migration]

plan_head_before: 2f2815923fea5e725bbd9f22136735c1aaf093be
plan_head_after: da9b6591f29ef1e469130b255d3477ac2758231d

actuals:
  tokens: 11000
  tasks: 3
  commits: 5

tech-stack:
  added: []
  patterns:
    - "BinaryData target per data set (SpatialCoreHRTFData, same mechanism as SpatialCoreUIFontData), only the one TU EmbeddedHRTF.cpp includes the generated header"
    - "Build option selects the BinaryData source list; the lookup has no #if, getNamedResource returning nullptr is the 'not embedded' signal"
    - "Resolver is a free function over a caller-owned HRTFDatabase with an injectable folder and size cap, so every rule is testable without touching the real system folder"

key-files:
  created:
    - include/SpatialCore/Binaural/HRTFProfile.h
    - include/SpatialCore/Binaural/HRTFProfileResolver.h
    - src/Binaural/EmbeddedHRTF.cpp
    - src/Binaural/HRTFProfileResolver.cpp
  modified:
    - CMakeLists.txt
    - include/SpatialCore/Binaural/HRTFDatabase.h
    - src/Binaural/HRTFDatabase.cpp
    - tests/Binaural/HRTFEmbeddedTests.cpp

key-decisions:
  - "The shared-file tests use a 'probe' profile: HUTUBS (3) in the all-five build, KEMAR (5) in the KEMAR-only build, so every [hrtf-resolve] rule is exercised in both embedding modes instead of being skipped in OFF"
  - "describeHRTFProfileStatus names the shared file by table file name and says 'no built-in copy' for SharedFileUnreadableNoBuiltIn; it never includes a path"
  - "A path that exists but is a file, not a folder, is treated like a missing folder (silent fallback), alongside the empty-File case the plan listed"

patterns-established:
  - "Profile index meets a file name only in kHRTFProfiles (D-07); never renumber"
  - "Resolver chain order shared -> embedded -> error is re-run on every call (D-11), no cache"

requirements-completed: [DATA-01]

duration: 12 min
completed: 2026-10-04
status: complete
---

# Phase 3 Plan 05: Embedded HRTF profiles and the resolution chain Summary

**SpatialCore now embeds the five SOFA profiles as BinaryData (KEMAR-only via one CMake option), refuses to embed a Git LFS stub, and resolves any profile 0-5 through shared folder, then embedded copy, then a reported error.**

## What you will hear

Nothing changes in the sound yet. This plan puts the data and the lookup rules in place; the engine's profile switch that calls them is Plan 03-06. What now exists: a plugin that links SpatialCore carries the five head-response files inside itself, so there is no install step and no path to set. A file with the same name dropped into `/Library/Application Support/Spatial Media Lab/HRTF` on macOS is picked up ahead of the built-in copy, on the very next load, with no restart. A missing folder falls back to the built-in copy without any message. A broken file (an empty file, a half-copied file, a Git LFS stub, random bytes, or one over 256 MB) falls back to the built-in copy and reports that it did so.

## Performance

- **Duration:** 12 min
- **Started:** 2026-10-04T13:39:39Z
- **Completed:** 2026-10-04T13:52:19Z
- **Tasks:** 3 (Task 1 tracer, Tasks 2 and 3 with RED then GREEN)
- **Files modified:** 8 (4 created, 4 modified)

## Accomplishments

- `SpatialCoreHRTFData` static library, linked PRIVATE into `SpatialCore`: all five files with `SPATIALCORE_EMBED_ALL_HRTF=ON` (default), `mit_kemar_large_pinna.sofa` only with OFF. Library sizes: ON 61,025,928 bytes, OFF 1,168,888 bytes (about 1 MB, as expected).
- Configure-time guard: each embedded file's first 8 bytes must be `894844460d0a1a0a` or CMake stops with `FATAL_ERROR`.
- `HRTFProfile.h`: the D-07 table (`kHRTFProfiles`, six slots, display names unchanged) plus `HRTFLoadState`, `HRTFProfileSource`, `HRTFProfileProblem`, `HRTFProfileStatus` for Plan 03-06.
- `HRTFDatabase::getEmbeddedProfileData` (one lookup, no `#if`) and `loadFromBinaryData`, reproducing on-disk IR lengths 256, 218, 279, 128, 558 for profiles 1-5. `loadFromFile`, `loadFromMemory` and `loadFromBytes` are untouched.
- `resolveHRTFProfile` with the rules D-06, D-08, D-09, D-10, D-11, D-18 and the 256 MB cap; read-only on the shared folder.
- 12 new test cases (3 `[hrtf-embed]`, 9 `[hrtf-resolve]`; 10 in the OFF tree thanks to the OFF-only `[kemar-only]` case).

## Task Commits

1. **Task 1: Tracer, KEMAR from embedded bytes end to end** - `84e7032` (feat)
2. **Task 2 RED: loadFromBinaryData tests for all five profiles and the OFF expectations** - `7eee0b0` (test; stub returns false, `[hrtf-embed][load]` fails for the right reason)
3. **Task 2 GREEN: HRTFDatabase::loadFromBinaryData** - `c399609` (feat)
4. **Task 3 RED: shared-folder, embedded-fallback and reported-error tests** - `e7b41d4` (test; stub resolver, 9 of 9 `[hrtf-resolve]` cases fail)
5. **Task 3 GREEN: the resolution chain** - `da9b659` (feat)

**Plan metadata:** the docs commit following this SUMMARY.

Tracer gate: interactive run, `end-of-phase` mode, tracer `<verify>` automated-only; the verify (configure, build, `[hrtf-embed]`) was re-run end to end and passed, so execution continued to the expansion tasks.

## LFS-guard proof (Task 1, Step 5)

A disposable worktree at `84e7032` had `HRTF/hutubs_pp2.sofa` overwritten with a three-line LFS pointer. Configuring it exited non-zero (`CONFIGURE_EXIT=1`) with:

```
CMake Error at CMakeLists.txt:94 (message):
  SpatialCore: hutubs_pp2.sofa is not a real SOFA/HDF5 file (a Git LFS
  pointer?).  Run: git lfs pull

-- Configuring incomplete, errors occurred!
```

The worktree was removed afterwards; `git worktree list` no longer shows `/tmp/sc-lfs-guard`.

## Verification results

| Check | Result |
|---|---|
| `[hrtf-embed]` Debug | 4 cases pass (145 assertions) |
| `[hrtf-embed]` OFF tree (`/tmp/sc-kemar-only`) | 4 cases pass (125 assertions) |
| `[hrtf-resolve]` Debug | 9 cases pass (713 assertions) |
| `[hrtf-embed],[hrtf-resolve]` Release | 13 cases pass (858 assertions) |
| `[hrtf-embed],[hrtf-resolve]` OFF tree | 14 cases pass (837 assertions; 10 `[hrtf-resolve]` incl. `[kemar-only]`) |
| Full Debug suite | 241 test cases pass (308,887 assertions); was 229 after 03-04, +12 |
| Release `ctest` | 100% passed, 241/241 |
| Consumer build (`/tmp/osd-dr3-check`, OpenSpatialDelay + OpenSpatialDelayTests against this branch) | EXIT 0; both embed sets (HRTFData and SpatialCoreHRTFData) link, no duplicate-symbol error |
| `grep -c SpatialCoreHRTFData.h src/Binaural/EmbeddedHRTF.cpp` / `grep -rl ... include/` | 1 / no output |
| `grep -cE "createDirectory\|deleteFile\|moveFileTo\|replaceWithData" HRTFProfileResolver.cpp` | 0 |
| `git diff BASE..HEAD -- src/Binaural/HRTFDatabase.cpp` | only the added `loadFromBinaryData`; no change inside `loadFromFile`, `loadFromMemory`, `loadFromBytes` |

The Debug and Release runs print `Leaked objects detected: 2 instance(s) of class FFT` at exit. This is the process-global SharedFFTCache and appears on `[renderer]` alone too, so it predates this plan; not touched here.

## Files Created/Modified

- `CMakeLists.txt` - `SPATIALCORE_EMBED_ALL_HRTF`, HDF5 guard, `SpatialCoreHRTFData` target, PRIVATE link, PUBLIC `SPATIALCORE_EMBEDS_ALL_HRTF`, two new sources.
- `include/SpatialCore/Binaural/HRTFProfile.h` - profile table, `isValidHRTFProfileIndex`, status types, `describeHRTFProfileStatus` declaration.
- `include/SpatialCore/Binaural/HRTFDatabase.h` / `src/Binaural/HRTFDatabase.cpp` - `getEmbeddedProfileData`, `loadFromBinaryData`.
- `src/Binaural/EmbeddedHRTF.cpp` - the only TU that includes the generated header.
- `include/SpatialCore/Binaural/HRTFProfileResolver.h` / `src/Binaural/HRTFProfileResolver.cpp` - folder, chain and status text.
- `tests/Binaural/HRTFEmbeddedTests.cpp` - all `[hrtf-embed]` and `[hrtf-resolve]` tests.

## Decisions Made

- Tests run the whole resolver matrix in both embedding modes through a "probe profile" (3 in ON, 5 in OFF) rather than skipping in OFF; plus an OFF-only `[kemar-only]` case for profiles 1-4.
- A shared "folder" that is actually a file is skipped like a missing folder.
- Status text for `SharedFileUnreadableNoBuiltIn` reads "profile N failed: shared file X unreadable, no built-in copy" (the plan fixed only the other three strings).

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Test fixtures: zero-byte file and relative File constructor**
- **Found during:** Task 3 GREEN (first run of `[hrtf-resolve]`)
- **Issue:** `juce::File::replaceWithData` deletes the file for a zero-length payload, so the "zero bytes" case never created the file and reported no problem; and `juce::File (probeFileName())` is a relative-path constructor that trips a JUCE assertion in the `[case]` test.
- **Fix:** zero-length fixture created with `File::create()` and its size asserted; upper-case name built with string operations.
- **Files modified:** `tests/Binaural/HRTFEmbeddedTests.cpp`
- **Verification:** all 9 `[hrtf-resolve]` cases pass in Debug, Release and the OFF tree.
- **Committed in:** `da9b659`

**2. [Process] RED commits carry a stub**
- **Found during:** Tasks 2 and 3
- **Issue:** a RED commit that only adds tests would fail to compile, which is not a valid RED.
- **Fix:** each RED commit also adds the declaration plus a stub returning false/empty, so the tests fail at run time for the right reason; the GREEN commit replaces the stub.
- **Committed in:** `7eee0b0`, `e7b41d4`

---

**Total deviations:** 1 auto-fixed (Rule 1, test fixture only), 1 process note.
**Impact on plan:** none on the shipped API; no scope creep.

## Issues Encountered

- None blocking. libmysofa v1.3.2 handled the truncated (100 KB) and random (200 KB) shared files without crashing, so no hardening was needed here; Plan 03-08 (libmysofa pin, D-17) still applies to wider malformed input.

## Known Stubs

None. The RED-commit stubs were replaced in the GREEN commits.

## Threat Flags

None beyond the plan's register. The new shared-folder file read is T-03-08 to T-03-10 as planned: file names only from `kHRTFProfiles`, size cap before reading, empty or non-directory folder skipped, no write, create, rename or delete in the resolver. T-03-11 is proven by the LFS-guard run above. T-03-13 (Windows folder ACLs) remains transferred to SUITE-01; the Windows path is implemented and unverified, and the Linux no-folder path is covered by reading the code only (macOS-only verification).

## Auth Gates

None.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- Plan 03-06 can call `resolveHRTFProfile` from the engine's loader thread and report `HRTFProfileStatus` through `describeHRTFProfileStatus`; everything it needs is in the public headers.
- Embedded copy doubles the binary weight (about 2 x 58 MB) in OpenSpatialDelay until its own HRTFData copy is deleted in the migration.

## Self-Check: PASSED

Created files present on disk (HRTFProfile.h, HRTFProfileResolver.h, EmbeddedHRTF.cpp, HRTFProfileResolver.cpp). Commits `84e7032`, `7eee0b0`, `c399609`, `e7b41d4`, `da9b659` exist in `git log`. `git rev-list --count 2f28159..HEAD` = 5 matches `commits: 5`. Plan-level verification (1-3) re-run and green; all acceptance criteria met.

---
*Phase: 03-binaural-defects-hrtf-packaging*
*Completed: 2026-10-04*
