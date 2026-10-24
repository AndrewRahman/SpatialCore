---
quick_id: 261024-mhh
description: Re-check the Ambisonics maths end to end (Phase 999.5 parts 1-2)
date: 2026-10-24
status: verified
---

# Quick Task 261024-mhh: Ambisonics maths re-check - Summary

## Result

No convention fault. `RenderEngine`'s HOA output follows AmbiX (ACN, SN3D, no CS phase, azimuth 0 front, +azimuth left) at orders 1, 3 and 6. SpatialCore#32 stays closed.

## What was added

`tests/Engine/AmbisonicsRenderDirectionTests.cpp` (3 test cases, tags `[ambi][engine][ambix]`), registered in `tests/CMakeLists.txt`:

- First-order cardinal directions: X and Y signs, Z close to 0, W > 0.
- Elevation of +45 and -45 degrees: Z sign, and |Z| close to |X|.
- 3OA and 6OA: every channel, with W = evalSH x max-rE, within 0.02 at 7 directions.

The source is a DC signal at distance 0.15 (1.5 m, which is `kNfcReferenceRadius`, so the NFC shelf is neutral).

## Gate

- Debug: `[ambi]` 2910 assertions / 7 cases; `[ambi-pin]` 411 / 3; full `SpatialCoreTests` 330733 assertions / 392 cases; `SpatialCoreUITests` 150 / 16.
- Release: `[ambi]` 2910 / 7; `[ambi-pin]` 411 / 3; `ctest --test-dir build-release/tests` 100% of 408 passed, 16 `ui:` tests listed.
- The Debug full run prints the JUCE leak detector line for 7 FFT instances at exit (exit code 0). This is the deliberate process-lifetime SharedFFTCache (D-08).

## Mutation check

On `src/Engine/RenderEngine.cpp:1687` (`azRad` in `renderAmbisonicsOutput`):

- Negating the azimuth: 2 of 3 cases fail, 164 assertions (for example az +90 gave Y = -0.3516).
- Adding 90 degrees: 3 of 3 fail, 325 assertions.
- Reverted. `git diff src/` is empty and `[ambi]` is back to 2910 / 7 passing.

## Known limit

The higher-order case takes its expectations from the library's own `evalSH`, so a sign error inside `evalSH` at ACN 4 or above would not be caught there. ACN 1-3 are pinned independently by the first-order case, and `[ambi-pin]` pins `evalSH` values.

## Remaining (user)

- REAPER listening check.
- Phase 999.5 part 3.

## After the quick task

Later investigation found the likely cause of the heard fault and two SpatialCore defects. See `261024-mhh-FINDINGS.md` (research in `research/`). Issues: SpatialCore#32 (open) and openspatialdelay#22. Fixes are deferred to new sessions; see `.planning/HANDOFF-ambisonics-fix.md`.
