# Quick task 261024-mhh: re-check the Ambisonics maths end to end

## Objective
Prove through `RenderEngine` (Ambisonics output formats) that the rendered field follows the
AmbiX convention: ACN order, SN3D, no Condon-Shortley phase, azimuth 0 = front, positive
azimuth = left. A rotated or mirrored field must fail a test.

## Task 1: add `tests/Engine/AmbisonicsRenderDirectionTests.cpp` (tag `[ambi]`)
- Register it in `tests/CMakeLists.txt` (`SpatialCoreTests` source list).
- FOA: DC source at 1.5 m (NFC-neutral) at az 0, +90, 180, -90 (el 0): X/Y signs, Z ~ 0, W > 0.
- Elevated case az 0, el +45: Z > 0, X > 0.
- 3OA and 6OA: every channel / W equals evalSH(n) x max-rE(order(n)) / (evalSH(0) x max-rE(0)),
  sign and magnitude, for seven directions.
- No library code (`include/`, `src/`) changes.

## Verification
- Local gate: Debug build of every target, run `SpatialCoreTests` and `SpatialCoreUITests`;
  Release build and `ctest --test-dir build-release/tests`. Run `SpatialCoreTests "[ambi]"` first.
- Mutation check: flip the azimuth sign in `src/Engine/RenderEngine.cpp` (`renderAmbisonicsOutput`,
  the `azRad` line) and confirm the `[ambi]` cases fail; revert.

## Out of scope
- REAPER listening check.
- Part 3 (any follow-up library change).
