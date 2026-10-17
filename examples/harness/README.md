# SpatialCore consumer harness

A minimal plugin that consumes SpatialCore **exactly the way OpenSpatialDelay does**: as a git
submodule from GitHub, built in Release, linking both `SpatialCore` and `SpatialCoreUI`, with the
embedded HRTF data and every opt-in facade feature in use. It is also the copy-paste reference for
OpenSpatialDelay's migration: read `src/HarnessPlugin.cpp` for the `RenderEngine` calls, with a short
comment on why each one is there.

This directory is a **template for a consumer repository**. It is never added to SpatialCore's own
build, and it is not a committed submodule of this repository: a `.gitmodules` entry that points at
SpatialCore itself would make `git clone --recurse-submodules` recurse, and the pin could not contain
the commit being tested. Instead `scripts/build-consumer.sh` builds a throwaway consumer repository
around a real submodule.

## What it proves

- **The submodule path (D-01).** A fresh git repository gets `SpatialCore` through `git submodule add`
  from `https://github.com/AndrewRahman/SpatialCore.git`, checked out at exactly the commit under test.
  SpatialCore is never read from the local source folder.
- **Both link targets.** `HarnessPlugin` (a VST3) and `HarnessCheck` (a console app) both link
  `SpatialCore` and `SpatialCoreUI` and share one processor source.
- **Two umbrella includes.** The sources include exactly `<SpatialCore/SpatialCore.h>` and
  `<SpatialCore/SpatialCoreUI.h>`, and no other SpatialCore header.
- **Embedded HRTF data.** Profile 5 (MIT KEMAR) loads from the data embedded in the library, with no
  install step and no path.
- **Every opt-in facade feature.** `setHRTFProfile`, `engineSelectsHRTF`, `engineFadesFormatSwitch`,
  `setAlgorithmIndex` and `getBlockLayout()`, alongside `engineComputesGains` and
  `engineDerivesDispatch`.
- **A warning-clean Release build.** Zero compiler warnings in the harness's own sources and in
  SpatialCore's sources and public headers, with JUCE's recommended warning flags on the harness
  targets as OpenSpatialDelay uses them. The script fails (exit 4) otherwise. Warnings inside JUCE and
  libmysofa are counted for the record only.
- **The headless sound check (D-04).** `HarnessCheck` renders through `processBlock` with no window, no
  audio device and no DAW, and fails unless:
  - every one of the 23 output formats is finite and not silent (channel counts from 2 up to 49);
  - the block layout reports the requested format once the switch has settled;
  - each format switch fades out to an exact 0 and back in from an exact 0;
  - all twelve algorithm indices are not silent on 7.1.4 and on Stereo;
  - an object moving on Binaural stays finite and audible;
  - the HRTF status reports profile 5 `Ready` from the `Embedded` source, before and after the switches;
  - `getUnsuppliedGainsBlockCount()` and `getOversizedBlockCount()` are 0;
  - the editor paints with the shared SML look-and-feel.

## How to run it

The commit must be on GitHub first (push it), because SpatialCore comes from the remote:

    git push origin HEAD
    examples/harness/scripts/build-consumer.sh "$(git rev-parse HEAD)"

`build-consumer.sh <40-hex-commit> [-- extra cmake configure args]`

| Setting | Meaning |
|---|---|
| `HARNESS_WORK_DIR` | Working directory (default: a new temporary directory). The consumer repository is `$HARNESS_WORK_DIR/consumer`; a previous one is removed. |
| `HARNESS_JOBS` | Parallel build jobs (default 4). |
| `HARNESS_OSD_LIKE=1` | The OpenSpatialDelay-like configuration, described below. |
| `SPATIALCORE_REMOTE_URL` | A different remote. It must start with `https://github.com/`. Never `Spatial-Media-Lab/SpatialCore`, which does not resolve yet. |
| `-- args` | Passed to the cmake configure step. To avoid downloading JUCE and libmysofa again, point at an existing tree: `-- -DFETCHCONTENT_SOURCE_DIR_JUCE=/path/to/juce-src -DFETCHCONTENT_SOURCE_DIR_MYSOFA=/path/to/mysofa-src` |

Exit codes: 2 for a bad commit id or remote (usage message), 3 when the commit cannot be fetched from
the remote or the submodule is not at the requested commit (it never falls back to a local copy), 4 for
compiler warnings in the harness or SpatialCore, otherwise the sound check's own status (0 on PASS).

The script seeds the submodule's Git LFS store from the checkout it lives in, so `git lfs pull` only
downloads what is missing (the HRTF files are about 60 MB and GitHub meters LFS bandwidth).

The macOS CI gate runs this script on every push.

## The OpenSpatialDelay-like configuration

`HARNESS_OSD_LIKE=1` mirrors how OpenSpatialDelay builds: the consumer fetches libmysofa **v1.3.2**
itself (SpatialCore then skips its own v1.3.5 fetch, because the `mysofa-static` target already exists)
and compiles its own targets with `-ffast-math`. The sound check reads the exponent bits to test for
non-finite samples, because `std::isfinite` folds to true under fast-math.

- **CMake 4 policy note.** libmysofa v1.3.2 declares an old `cmake_minimum_required`, which CMake 4
  rejects unless the configure is run with `-DCMAKE_POLICY_VERSION_MINIMUM=3.5`. SpatialCore sets that
  policy only after the consumer's own FetchContent has already run, so a consumer that fetches
  libmysofa v1.3.2 itself must pass the flag on the command line. The script does this in
  `HARNESS_OSD_LIKE` mode.
- **Never apply fast-math globally before `add_subdirectory(SpatialCore)`.** `FloatSemanticsGuard.h`
  turns that into a compile error on purpose (the non-finite guards are `std::isfinite` tests that
  fast-math folds away). Per-target fast-math on the consumer's own targets is fine, and is what the
  harness and OpenSpatialDelay do.

## Files

- `CMakeLists.txt` - the consumer project: JUCE 9.0.0, `add_subdirectory(SpatialCore)`, the two targets.
- `src/HarnessPlugin.h`, `src/HarnessPlugin.cpp` - `HarnessProcessor` and `HarnessEditor`.
- `src/SoundCheck.cpp` - the `HarnessCheck` entry point; the exit code is the verdict.
- `scripts/build-consumer.sh` - builds the throwaway consumer repository, the plugin and the check.
