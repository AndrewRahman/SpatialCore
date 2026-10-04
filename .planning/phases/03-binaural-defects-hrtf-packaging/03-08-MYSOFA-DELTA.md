# Plan 03-08: libmysofa v1.3.2 vs v1.3.5, measured before any change

**0 of 140 pinned values changed, and 0 of 19 recorded loudness numbers changed, in Debug and in Release. One thing did break: v1.3.5 does not compile in SpatialCore as it stands (a missing include folder), and one added line in `CMakeLists.txt` fixes it. v1.3.5 also stopped a damaged file that made v1.3.2 hang.**

Measured 2026-10-04 on the tree at `8e11ac0` (BASE). The main tree is untouched: `CMakeLists.txt` still says `GIT_TAG v1.3.2`, `build/_deps/mysofa-src` is `v1.3.2`, and nothing has been re-baselined.

## How this was measured

- "Before" (v1.3.2) numbers come from the main `build/` (Debug) and `build-release/` (Release), plus a throwaway copy of the same source built against the v1.3.2 libmysofa for the extra probe below.
- "After" (v1.3.5) numbers come from a disposable git worktree (`/tmp/sc-mysofa135-src`) with `GIT_TAG v1.3.5`, built in `/tmp/sc-mysofa135-dbg` (Debug) and `/tmp/sc-mysofa135-rel` (Release). Both trees fetched libmysofa commit `6cc5b15a73e9bd97810d03767082edda7f315881`, which is the commit the `v1.3.5` tag resolves to (checked with `git ls-remote` before use and `rev-parse HEAD` on each fetched tree).
- The worktree carried one extra throwaway test file (a probe that prints every SOFA-derived value at full precision). It was used for both versions and is not part of the main tree.
- The worktree needed one build fix to compile at all (next section). That fix is only in the throwaway copy so far.

## Finding 1: v1.3.5 does not build here without one extra line

v1.3.5's `mysofa.h` now includes a generated file, `mysofa_export.h`. libmysofa generates it in its own build folder and does not publish that folder to code that links it. SpatialCore compiled `HRTFDatabase.cpp` with `fatal error: 'mysofa_export.h' file not found`, in Debug and Release.

The fix used for the measurement (3 lines in the root `CMakeLists.txt`, before `target_link_libraries(SpatialCore ...)`):

```cmake
get_target_property(_sc_mysofa_bindir mysofa-static BINARY_DIR)
target_include_directories(SpatialCore PRIVATE "${_sc_mysofa_bindir}")
```

It asks the `mysofa-static` target where it was built, so it also works when a consumer such as OSD supplies its own copy of libmysofa (the `if(NOT TARGET mysofa-static)` guard). It is harmless for v1.3.2 (the same generated file exists there). If the user chooses either upgrade option, Task 3 adds these lines to the main `CMakeLists.txt`.

## Finding 2: what v1.3.5 changes in the parser (from libmysofa's own history)

Between v1.3.2 and v1.3.5: overflow protection in the array reader and a missing boundary check fixed (the hardening), `bzero` replaced by `memset`, the allowed file size raised to 4 GB, support added for "General FIR-E" files, Windows zlib updated. No resampling or normalisation code changed, which agrees with the numbers below.

## Finding 3 (supplementary, not a pinned value): damaged files

400 deterministic mutations of `hutubs_pp2.sofa` (truncations, a 16-byte corruption of the metadata region, an 8-byte 0xFF run, 64 scattered corrupt bytes), each loaded in its own process through `HRTFDatabase::loadFromMemory`, Release build:

| | v1.3.2 | v1.3.5 |
|---|---|---|
| Accepted (loaded) | 41 | 41 |
| Rejected cleanly | 358 | 359 |
| Hung (killed after 60 s) | 1 (mutation 213) | 0 |

The 41 accepted and 358 rejected cases are the same mutations with the same result in both versions. Mutation 213 (corrupt metadata region) makes v1.3.2 loop until killed and v1.3.5 reject cleanly. No crashes in either. This is a small sample, not a security audit, but it is the first measured evidence that the older parser can be stalled by a damaged file.

## Pinned values: before and after

### Group 1: golden fingerprints at az=90, el=0 (tolerance: length and peak index exact, delays 1e-4 samples, energy and peak 1e-5 relative)

Fields: irLength, delayL, delayR, energyL, energyR, peakL, peakR, peakIndexL, peakIndexR. All 45 values (5 profiles x 9 fields) are identical at full printed precision.

| Profile | v1.3.2 Debug | v1.3.5 Debug | v1.3.2 Release | v1.3.5 Release | Difference | Moved |
|---|---|---|---|---|---|---|
| SADIE D2 KU100 | `{ 256, 72, 102, 40.8401279, 0.798788257, 2.41877556, 0.425458729, 100, 102 }` | same | same | same | 0 | no |
| CIPIC 003 | `{ 218, 25, 54, 8.71853987, 0.107548025, 1.64910412, -0.127475545, 26, 42 }` | same | same | same | 0 | no |
| HUTUBS PP2 | `{ 279, 18, 48, 197.710969, 3.40356553, 6.14737415, 0.711916804, 27, 21 }` | same | `{ 279, 18, 48, 197.71094, 3.4035652, 6.14737368, 0.711916804, 27, 21 }` | same as v1.3.2 Release | 0 (Debug vs Release differ by up to 1.5e-7 relative, as recorded in 03-02, in both versions) | no |
| Bernschuetz KU100 | `{ 128, 11, 43, 5.42644277, 0.302691799, 1.26062512, 0.268568307, 11, 16 }` | same | same | same | 0 | no |
| MIT KEMAR Large Pinna | `{ 558, 31, 61, 2.00355748, 0.163567331, 0.515830636, -0.121116042, 39, 52 }` | same | same | same | 0 | no |

Extra, beyond what the tests pin: the same fingerprint at four more directions (az 0, -90, 180 and az 45 el 30) for each profile, loaded from the file at 48 kHz, loaded from the embedded copy at 48 kHz, and loaded from the file at 44.1 kHz (the resampling path): 25 + 5 + 5 further fingerprints per build type, every one identical between v1.3.2 and v1.3.5, in Debug and in Release.

### Group 2: synchronous-load IR length and position count (exact)

| Profile | IR length v1.3.2 / v1.3.5 | Positions v1.3.2 / v1.3.5 | Moved |
|---|---|---|---|
| SADIE D2 KU100 | 256 / 256 | 8802 / 8802 | no |
| CIPIC 003 | 218 / 218 | 1250 / 1250 | no |
| HUTUBS PP2 | 279 / 279 | 440 / 440 | no |
| Bernschuetz KU100 | 128 / 128 | 16020 / 16020 | no |
| MIT KEMAR Large Pinna | 558 / 558 | 710 / 710 | no |

Identical in Debug and Release. (The 44.1 kHz loads, not pinned by any test, also match: 236, 200, 256, 118, 512.)

### Group 3: D-16 ITD characterisation table (`[renderer][itd-characterisation]`, exact)

All 20 rows (SADIE and KEMAR, 44.1 and 48 kHz, five directions each; raw delay L/R and rendered onset L/R) are identical between v1.3.2 and v1.3.5 in Debug and in Release: 80 values, 0 moved. Example rows (raw delay L/R, rendered onset L/R, samples):

| Row | v1.3.2 | v1.3.5 | Moved |
|---|---|---|---|
| SADIE 48 kHz az 90 | 72/102, 80/110 | 72/102, 80/110 | no |
| SADIE 48 kHz az -90 | 66/82, 68/84 | 66/82, 68/84 | no |
| KEMAR 48 kHz az 90 | 31/61, 62/92 | 31/61, 62/92 | no |
| KEMAR 44.1 kHz az 45 el 30 | 31/45, 62/76 | 31/45, 62/76 | no |

### Group 4: embedded IR lengths (`[hrtf-embed]`, exact)

`kExpectedIRLength` = 256, 218, 279, 128, 558 for profiles 1 to 5. Embedded copies load through `loadFromMemory` and `loadFromBinaryData` to the same lengths and positions under v1.3.5 (and the same fingerprints, Group 1 extra). 5 values, 0 moved, Debug and Release.

### Group 5: K-weighted loudness (`[loudness]`, recorded, never asserted, D-14)

Identical to the printed 0.01 LU in both versions and both build types (19 numbers, 0 moved):

| Profile | Mean LKFS v1.3.2 / v1.3.5 | Lowest direction | Highest direction | Moved |
|---|---|---|---|---|
| 0 Simple (no SOFA file) | -29.91 / -29.91 | -31.87 / -31.87 | -27.81 / -27.81 | no |
| 1 SADIE | -20.83 / -20.83 | -23.74 / -23.74 | -19.23 / -19.23 | no |
| 2 CIPIC | -19.80 / -19.80 | -22.60 / -22.60 | -17.54 / -17.54 | no |
| 3 HUTUBS | -20.16 / -20.16 | -24.21 / -24.21 | -18.18 / -18.18 | no |
| 4 Bernschuetz | -19.34 / -19.34 | -21.64 / -21.64 | -18.28 / -18.28 | no |
| 5 KEMAR | -18.34 / -18.34 | -22.68 / -22.68 | -16.74 / -16.74 | no |
| Spread among profiles 1-5 | 2.48 LU / 2.48 LU | | | no |

(Profile 0 reads -29.91 here against -29.37 in the Plan 03-01 summary because the Simple profile changed in later plans; it does not use libmysofa and is the same under both versions.)

## Tag results (all pass under v1.3.5 where they pass under v1.3.2)

| Tag | v1.3.2 Debug | v1.3.5 Debug | v1.3.2 Release | v1.3.5 Release |
|---|---|---|---|---|
| `[binaural]` | 108 assertions, 20 cases pass | same | same | same |
| `[renderer][itd-characterisation]` | 222 assertions, 1 case pass | same | same | same |
| `[hrtf-embed]` | 145 assertions, 4 cases pass | same | same | same |
| `[hrtf-resolve]` | 759 assertions, 10 cases pass | same | same | same |
| `[loudness]` | 21 assertions, 2 cases pass | same | same | same |

Full suite under v1.3.5: Debug 253 of 253 plus the 1 throwaway probe case (254 of 254 run, exit 0), Release the same (exit 0). Main tree (v1.3.2) before this plan: Debug 253 of 253, Release 253 of 253 (Plan 03-07 totals).

## What the choice in Task 2 comes to

- Nothing pinned moved, so there is no number to re-baseline. `keep-upgrade` is the applicable upgrade option and `upgrade-and-update-numbers` would update zero numbers (the two upgrade options would produce the same tree).
- The upgrade needs the 3-line `CMakeLists.txt` change above plus the pin and a comment edit.
- SpatialCore's pin only governs builds where SpatialCore fetches libmysofa itself (its own tests and CI, and any consumer that does not supply `mysofa-static`). The CMake comment says OSD fetches its own copy at v1.3.2 under the name `libmysofa`, so for OSD the guard skips SpatialCore's fetch and OSD's own pin decides which parser OSD ships. If the user upgrades here, OSD would need its own pin bumped separately to get the hardened parser (a decision for the OSD repo, not made here).

## Decision

**2026-10-04: the user chose `keep-upgrade`.** SpatialCore now pins libmysofa v1.3.5 (tag resolves to `6cc5b15a73e9bd97810d03767082edda7f315881`; `git -C build/_deps/mysofa-src rev-parse HEAD` prints that commit after the reconfigure).

Applied in the main tree (`CMakeLists.txt` only):

- `GIT_TAG v1.3.2` became `GIT_TAG v1.3.5`; the OSD-coexistence guard comment now names v1.3.5 and keeps the explanation that a consumer supplying its own `mysofa-static` skips this fetch and decides its own version.
- The 3-line build fix from Finding 1 was added after `target_link_libraries(SpatialCore ...)` (queries `mysofa-static` for its build directory, adds it as a private include folder for `mysofa_export.h`).
- No test file was edited and no golden, IR length, ITD entry or tolerance moved: the table above shows 0 of 140 pinned values changed.

Verification on the main tree after the change (v1.3.5):

| Build | Result |
|---|---|
| Debug (`build/`, `./build/tests/SpatialCoreTests`) | All tests passed, 309062 assertions in 253 test cases, exit 0 (the known `juce_LeakedObjectDetector.h` FFT line prints at exit, as in every earlier plan) |
| Release (`build-release/`, `ctest`) | 100% tests passed out of 253 |

Follow-up, not done here: OSD pins its own libmysofa (v1.3.2) under the name `libmysofa`, so OSD keeps the old parser until its own pin is bumped. That is a change in the OSD repo and was deliberately not touched.
