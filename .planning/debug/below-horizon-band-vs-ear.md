---
status: diagnosed
trigger: "UAT G-02-2: below-horizon 0 to -30 degree band on height layouts does not pan as BS.2127 (EAR) (7.1.4 az 60: M+030/M+090 lean 0.72/0.69 instead of 0.7071/0.7071, side flip between -20 and -25)"
created: 2026-10-03T00:00:00Z
updated: 2026-10-03T00:00:00Z
goal: find_root_cause_only
---

## Current Focus

hypothesis: CONFIRMED (verifier's hypothesis, with a sharper mechanism). SpatialCore builds, for each ear-level/-30 deg trapezoid, all 4 overlapping triangles of the coplanar quad and picks one by minimum gain sum. Both triangulations give exactly equal sums in real arithmetic, so the pick is decided by enumeration order and 1-2 ULP float noise, and the two candidates are mirror images. EAR pans the quad as a QuadRegion whose downmixed result is the elevation-independent pair pan at the source azimuth.
test: DONE - real-engine probe vs ear 2.1.0 on all 8 height layouts; float32 prototype of a pair-pan fix vs ear
expecting: n/a
next_action: return ROOT CAUSE FOUND (goal find_root_cause_only; do not fix)

## Symptoms

expected: On height layouts, a source between 0 and -30 degrees elevation pans as BS.2127 (EAR): 7.1.4 az 60 gives M+030 = M+090 = 0.7071 at every elevation 0..-30 (CONTEXT D-04 promised the horizon pan holds to -30 then blends to equal gain on ear-level speakers)
actual: 7.1.4 az 60 VBAPAlgorithm via RenderEngine LayoutContext: el0 0.7071/0.7071; -5 0.7210/0.6929; -10 0.7343/0.6788; -15 0.7472/0.6646; -20 0.7513/0.6599; -25 0.6768/0.7361 (flip); -30 0.6941/0.7199
errors: None (numeric deviation up to ~0.044 gain / ~0.5 dB at az 60; up to 0.055 gain / 1.23 dB elsewhere in the band; side flip between -20 and -25)
reproduction: 02-UAT.md Test 2
started: Discovered in UAT verify-work phase 02

## Eliminated

- hypothesis: the deviation is a geometry error in the lower hull (wrong vertices / wrong -30 ring)
  evidence: nadir cap (EAR VirtualNgon) matches ear to 0.0000 on all 8 layouts over 18k-21k points each; the trapezoid vertices are identical to EAR's QuadRegion vertices (ear-level pair + -30 copies at the same azimuths)
  timestamp: 2026-10-03
- hypothesis: the lean is a smooth geometric bias of one triangulation
  evidence: at every sampled point exactly two lower triplets enclose the source (99.8% of 108k points per layout), they are mirror images of each other, and their gain sums tie (77-80% bit-equal, rest within 2 ULP). Winner flips 398-886 times per 3600 azimuth steps
  timestamp: 2026-10-03
- hypothesis: blending the two triangulations (issue 22 option 3) would reproduce EAR
  evidence: prototype P2 leaves max |dg| 0.0146-0.0254 (0.55 dB) vs ear on every layout
  timestamp: 2026-10-03
- hypothesis: one fixed split per quad (issue 22 option 2) would reproduce EAR
  evidence: prototype P3 removes the jitter but keeps max |dg| 0.0550-0.0553 (1.23 dB) vs ear
  timestamp: 2026-10-03

## Evidence

- timestamp: 2026-10-03
  checked: probe (/tmp/probe, linked to build/libSpatialCore.a at HEAD ac59252) 7.1.4 az 60 el 0..-30 step 5
  found: reproduces UAT numbers exactly (0.7210/0.6929, 0.7472/0.6646, 0.6768/0.7361)
  implication: harness is faithful

- timestamp: 2026-10-03
  checked: replay of computeVBAPGains3D selection at 7.1.4 az 60, el -5/-15/-20/-25/-30
  found: exactly 2 enclosing lower triplets; at -5/-15 tri 120 (a,b,a') vs 121 (a,b,b') sums 1.173660040 vs 1.173660040 and 1.184705496 vs 1.184705496 (bit-equal; first enumerated wins, leans to spk0); at -20 tri 123 sum 1.176707625 vs tri 134 sum 1.176707506 (1 ULP, tri 134 wins, leans to spk3); at -25 and -30 equal again, 123 wins. Candidates are exact mirrors: g=(0.4242,0.5752,0.1743) vs (0.5752,0.4242,0.1743) before downmix
  implication: lean side = enumeration order on exact ties + ULP noise, not geometry; side flip between -20 and -25 is a float coin flip

- timestamp: 2026-10-03
  checked: ear/core/point_source.py (_configure_full, extra_pos_vertical_nominal, QuadRegion, VirtualNgon)
  found: hull over nominal positions of real + extra(-30 copies of every mid speaker when no lower layer) + virtual nadir (+ virtual zenith unless T+000/UH+180); coplanar triangles merged into facets; facets touching a virtual speaker -> VirtualNgon; others -> Triplet / QuadRegion (bilinear). Extras downmix 1:1 onto their mid speaker, then renormalise
  implication: each ear-level/-30 trapezoid is ONE QuadRegion, never triangulated

- timestamp: 2026-10-03
  checked: EAR(az,el) vs EAR(az,0) for every EAR QuadRegion that is a lower-ring trapezoid, el -1..-30, az -180..180 step 1, all 8 layouts
  found: max difference 7e-16 .. 9e-16
  implication: EAR's band pan is exactly the elevation-independent horizon pan at the source azimuth (chord parameter x depends on azimuth only because a' and b' sit directly under a and b; the downmix collapses (1-x)(1-y)+(1-x)y = 1-x, x)

- timestamp: 2026-10-03
  checked: SpatialCore (real engine) vs ear, el -1..-30 band, 1 deg grid, per layout, max |dg| / max dB on speakers > 0.1
  found: 5.1.2 0.0553/1.235; 5.1.4 lower-ring quads 0.0548/1.235 (but 0.5599 in EAR's rear-gap facets that reach height speakers); 7.1.2, 7.1.4, 7.1.6, 9.1.4, 9.1.6 0.0552/1.229; SML13.1 0.0550/1.229. Nadir cap (EAR VirtualNgon): 0.0000 on all. Same trapezoid defect continues below -30 where wide-span chords dip lower (0.0338 on 7.1.x/9.1.x at az -165 el -31)
  implication: defect is uniform across layouts; only 5.1.4 has an additional structural difference

- timestamp: 2026-10-03
  checked: tie statistics, el -1..-30 step 1 x az step 0.1, per layout
  found: pairs of enclosing triplets (99.8%+): exact sum tie 77-80%, within 2 ULP the rest, never larger; candidate-vs-candidate gain gap up to 0.101 after downmix. Azimuth sweep 7.1.4: winner flips 398 (el -5), 884 (el -10), 886 (el -20), 594 (el -25); max gain step per 0.1 deg 0.041/0.076/0.101/0.096 vs 0.0053 at the horizon and 0.0025 in the nadir cap
  implication: same mechanism as issue 22, measured; the band is jittery for moving sources as well as leaning

- timestamp: 2026-10-03
  checked: 5.1.4 rear gap, EAR vs SpatialCore across the horizon (az 180, -120; el +5..-20)
  found: EAR puts 0.41 on U+-135 even at el 0 and +5 (az 180: M110 0.577, M-110 0.577, U135 0.408, U-135 0.408); SpatialCore stays on M+-110 (0.707/0.707). EAR facets there: Tri (110,0)(110,-30)(135,45), Tri (-135,45)(-110,0)(-110,-30), Quad (-135,45)(110,-30)(-110,-30)(135,45)
  implication: on 5.1.4 EAR itself does not hold the horizon pan in the rear gap; matching it needs EAR-style panning ABOVE the horizon too (conflicts with bit-identity); RESEARCH F4 and the lower-only hull were a deliberate avoidance of this

- timestamp: 2026-10-03
  checked: VBAP/VBIP/MDAP call sites
  found: all three reach the band only via computeVBAPGains3D (VBAPAlgorithm.cpp:19, VBIPAlgorithm.cpp:31, MDAPAlgorithm.cpp:50/97). MDAP at el 0 on 7.1.4/9.1.6 steps up to 0.076/0.077 per 0.1 deg (VBAP el 0: 0.0053) because its 8-direction ring dips into the band
  implication: one fix site covers the family; MDAP at the horizon benefits

- timestamp: 2026-10-03
  checked: float32 prototype replica of computeVBAPGains3D (probe mode proto): regular triplets unchanged (pass 0), nadir-cap triangles (pass 1), one wedge triplet (a, b, nadir) per adjacent ear-level pair with nadirGain 0 (pass 2). Same VBAPTriplet fields, no new members
  found: vs ear over the whole lower hemisphere (1 deg grid) max |dg| 1.8e-7..4.4e-7 on 5.1.2, 7.1.2, 7.1.4, 7.1.6, 9.1.4, 9.1.6, SML13.1 and on 5.1.4's lower-ring quads; 0 coverage holes on the full sphere (all 8 layouts, 361x181 grid); above-horizon output bit-identical to the real function on 259,920 points
  implication: no triangulation reproduces EAR (P2/P3), but a pair-pan region representable as an ordinary VBAPTriplet entry does, with only a selection-order change inside computeVBAPGains3D

- timestamp: 2026-10-03
  checked: prototype residual on 5.1.4 after the pair-pan change
  found: every remaining deviation from ear (4200 band points) lies at |az| >= 111 (the 110/-110 rear gap, where EAR's facets reach U+-135/45); |az| <= 110 matches ear to float rounding
  implication: 5.1.4 rear gap cannot be made EAR-exact without EAR-style panning above the horizon

- timestamp: 2026-10-03
  checked: issue 22 figures on current code (sweep, 0.1 deg az, VBAP)
  found: 7.1.4 el 20 max step 0.607, el 40 0.751; 5.1.4 el 40 0.851; 9.1.6 el 20 0.716 - identical to the issue. The prototype is bit-identical above the horizon (0 mismatches / 259,920 points), so it leaves all of these unchanged
  implication: band defect and #22 share a mechanism but are separable; the band fix does not touch #22, and #22's options 1-3 (tie epsilon / fixed split / blend) do not close the band (P3 0.055, P2 0.015-0.025 vs ear)

- timestamp: 2026-10-03
  checked: prototype continuity and counts
  found: band max step 0.0053 per 0.1 deg on all layouts (SML13.1 0.0035) = the horizon figure, vs 0.101 today; lower list shrinks from 5n to 2n entries (n = ear-level speakers: 25->10 on 5.1.x, 35->14 on 7.1.x, 45->18 on 9.1.x, 40->16 on SML13.1)
  implication: no extra audio-thread cost; the existing 0.12 band bound in PanningLawTests.cpp can tighten to the 0.01 cap bound

- timestamp: 2026-10-03
  checked: git provenance
  found: D-04 promise "horizon pan holds down to -30" in b338fb0 (2026-09-30, CONTEXT); the measured QuadRegion difference (<= 0.0551, 5.1.4 up to 0.56) was recorded the same day in 0747a28 (RESEARCH F4) and again in a97382b (gen_ear_reference.py header) but never reconciled with D-04; lower hull implemented in 69eae4d as overlapping triangles; band test loosened to 0.12 in 3e97ad4; docs written as plain EAR in 3f88997 (02-07)
  implication: the deviation was known and scoped out of the oracle test, but the claim in D-04 and the docs was not narrowed to match

## Resolution

root_cause: In appendLowerHemisphereTriplets (src/IO/SpeakerLayout.cpp:257-339) every ear-level/-30 degree trapezoid, a planar quad, is emitted as all 4 of its overlapping triangles (the facet filter at :298-311 deliberately keeps coplanar quads whole), and computeVBAPGains3D (src/Core/SpatialMath.cpp:279-316, strict `sum < bestGainSum`) picks one of the two enclosing triangles by minimum gain sum. In a planar facet all triangles have the same gain sum (n.p/d), so the pick is enumeration order on exact ties and 1-2 ULP noise otherwise, and the two candidates are mirror-image leans (up to 0.101 apart). EAR (ear/core/point_source.py QuadRegion) pans the same trapezoid as one bilinear region, which with the -30 copies downmixed 1:1 onto their ear-level speakers collapses to the elevation-independent 2D pair pan at the source azimuth. Triangulating (any split, or averaging the splits) cannot reproduce it: the region model is wrong, not the triplet data. Additionally on 5.1.4 EAR's rear-gap facets reach the height speakers, which the deliberate lower-only hull (RESEARCH F4) does not copy.
fix: (not applied - diagnose only)
verification: (not applicable)
files_changed: []
