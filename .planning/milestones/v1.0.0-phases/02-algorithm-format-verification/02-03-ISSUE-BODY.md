## Summary
`computeVBAPGains3D` (`src/Core/SpatialMath.cpp`) picks, among all triplets whose gains are all
≥ −1e-6, the one with the **minimum** gain sum (strict `sum < bestGainSum`).
`buildVBAPTripletsForLayout` (`src/IO/SpeakerLayout.cpp`) keeps every non-degenerate speaker triple,
so for any coplanar quad of speakers — for example the rear ear-level pair ±135° together with the
rear height pair ±135°/45° on 7.1.4 — both triangulations of the quad are present and produce
exactly equal gain sums. Which triangulation wins is then decided by float rounding, while the two
candidates' gain vectors differ by up to ~0.6. A source moving smoothly off the horizon on a height
layout therefore jumps between them.

## Evidence (Phase 2 research, real C++ build on branch `gsd-remap`)
- Exact tie on 7.1.4: both triangulations of the rear quad give gain sum `1.325751162` vs
  `1.325751162` (printed to 9 decimals).
- Largest per-step gain change on a 0.1° azimuth sweep (VBAP): 7.1.4 el = 20° → **0.607**;
  7.1.4 el = 40° → **0.751**; 5.1.4 el = 40° → **0.851**; 9.1.6 el = 20° → **0.716**. Ear-level
  (el = 0°) sweeps step at most 0.0053 on every layout.
- A numpy replica of the selection flips triplet on 129 of 3600 azimuth steps at 7.1.4 el = 20°.
- An earlier random-direction estimate (4–13 per 1M directions, jumps up to 0.70) understated it.
- The Phase 2 lower-hemisphere construction (ITU-R BS.2127 / EAR, decision D-04) inherits the same
  rule: in the −30°…0° band its ear-level/−30° trapezoids are coplanar quads, and a prototype
  measured ~0.10 steps per 0.1° of azimuth there (about 20× the smooth slope).
- Cross-platform risk: a value pinned inside a tie region can differ between compilers.

## Not fixed in Phase 2 (decision D-18)
Phase 2 asserts continuity only where the algorithms are continuous: every 2D layout at el = 0 for
VBAP/VBIP/MDAP; height layouts at el = 0 for VBAP/VBIP; ConstantPower, DBAP and Ambisonics at every
tested elevation; the nadir cap below the −30° ring. The height-layout sweeps off the horizon and
the −30°…0° band are excluded and cite this issue. Widening the triplet tolerance to −1e-5 was
rejected (D-07): it flips the triplet choice in 38–111 per 1M directions with gain jumps up to 0.96.

## Options (undecided)
1. A deterministic tie-break epsilon in the min-sum comparison — reproducible, but it changes
   above-horizon output bits, an audible change for OpenSpatialDelay sessions.
2. A proper convex-hull triangulation with one fixed split per coplanar quad.
3. Blending the two triangulations of a coplanar quad (continuous by construction).

Related: #20 (focus/spread). The scoped continuity tests are the `[panning-law][continuity]`
cases in `tests/Algorithms/PanningLawTests.cpp`, which cite this issue for the excluded ranges;
the horizon seam itself, which is continuous, is checked by the `[ear][continuity]` case in
`tests/Core/VBAPTripletSelectionTests.cpp`.
