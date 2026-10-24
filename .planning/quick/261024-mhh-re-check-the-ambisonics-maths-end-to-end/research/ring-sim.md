# Ring simulation: OSD 6OA -> IEM AllRADecoder -> 20-speaker horizontal ring

Investigation only, nothing edited in any repo. Scripts and raw output: /tmp/ringsim/ (sim.py, run.py, analyze.py, out.txt, out_extra.txt, dump.cpp).

## Verdict (short)
No encoder variant misplaces back-left or back-right sources on this rig, and none is left/right asymmetric.
Energy-vector direction error is under 0.02 deg for every az (1 deg grid, -180..180), for the IEM encoder, SpatialCore/OSD dev, dev with NFC, and OSD v1.0.0. Mirror asymmetry max |err(az)+err(-az)| is 0.003 to 0.011 deg, which is float noise. Error table at az 0, +-45 ... 180 is 0.00 for all.
What DOES differ between encoders is level and pattern shape, not direction: LF-band pattern loses focus with NFC (r=5/10 m), and v1.0.0 at 6OA has a distorted pattern. See below. Nothing here can produce "back-left and back-right are in the wrong place".

## Method (what was replicated)
- Decoder: IEM AllRADecoder `calculateDecoder()` ported line by line: 20 real speakers az 0,18,...,342 (speaker 1 at az 0, +az = left, el 0) plus imaginary speakers at (0,0,1) and (0,0,-1) with gain 0 (the IEM default of 0.0f, used linearly, so imaginary energy is dropped, kappa redistribution term is multiplied by 0); scipy ConvexHull; IEM's own 5200-point t-design parsed from AllRADecoder/Source/tDesign5200.h; first triangle with all gains >= -FLT_EPSILON; per-point L2 normalisation; sum gain x N3D SH x decodeCorrection(6); global normalisation by max loudspeaker-direction L2; order 6. Run time: input x sqrt(2n+1) (SN3D->N3D), x maxre6[49] (parsed from IEM MaxRE.h) x maxRECorrectionEnergy[6]=1.1069 (decoder order = input order so the order-mismatch factor is 1). Decoder weights = maxrE (IEM default).
- SH reference: /tmp/shref/ref.py (scipy, AmbiX SN3D). The SpatialCore dev evalSH and the v1.0.0 inlined evalSH were extracted verbatim from source (src/Core/SpatialMath.cpp, and `git show e3eb766:Source/PluginProcessor.cpp`), compiled unchanged with clang in float, and dumped for az -180..180 step 1 deg, el 0, ACN 0..48. Check: dev vs scipy max abs diff 1.5e-6; v1.0.0 vs scipy max abs diff 3.13 (the known wrong order 4-6 constants, e.g. acn 36/48 x5.66).
- Encoders (all order 6, el 0):
  (a) IEM plain SN3D.
  (b) dev evalSH x cos(n*pi/14) (engine max-rE), NFC neutral.
  (c) (b) x NFC LF gain for orders >= 1. Formula from RenderEngine.cpp: first-order shelf per order, fPole = n*343/(2 pi r), fZero = n*343/(2 pi 1.5), DC gain = wZero/wPole = r/1.5. At 48 kHz: r=5 m is x3.333 (+10.46 dB), r=10 m is x6.667 (+16.48 dB), same for every order 1..6, applied to all orders >= 1 and not to W. LF band only; above a few hundred Hz the filter is unity, i.e. result (b).
  (d) v1.0.0 evalSH x same cos weights (v1.0.0 does apply max-rE and the same NFC, identical code); neutral and with NFC.
- Metrics: speaker gains g = D * (weighted input); energy vector rE = sum g^2 u / sum g^2; velocity vector rV = sum g u / sum |g|.

## Decoder validation
IEM-style plain SN3D encodings, az every 5 deg: max rE direction error 0.002 deg, |rE| constant 0.974 (max-rE on a 20-speaker ring, 20 >= 2*6+1 so no aliasing). Mirror asymmetry 0.003 deg. Imaginary gain 1 (instead of 0) gives the same direction error and |rE| 0.953. Loudest speaker tracks the source; opposite speaker is -43 to -50 dB.

## Key table (decoder default maxrE, imaginary gain 0). rEerr in degrees, "opp" = level of the speaker opposite the source vs the loudest, dB
| az | (a) IEM rEerr / opp | (b) dev rEerr / opp | (c) NFC 5 m rEerr / opp | (c) NFC 10 m rEerr / opp | (d) v1.0.0 rEerr / opp | (d) v1.0.0 + NFC 5 m rEerr / opp |
|---|---|---|---|---|---|---|
| 0 | 0.00 / -43.2 | 0.00 / -59.1 | 0.00 / -21.0 | 0.00 / -19.2 | 0.00 / -20.5 | 0.00 / -28.3 |
| +45 | 0.00 / -50.3 | 0.00 / -58.2 | 0.00 / -20.3 | 0.00 / -18.4 | 0.00 / -20.8 | 0.00 / -31.7 |
| -45 | 0.00 / -50.3 | 0.00 / -58.2 | 0.00 / -20.3 | 0.00 / -18.4 | 0.00 / -20.8 | 0.00 / -31.7 |
| +90 | 0.00 / -43.2 | 0.00 / -59.1 | 0.00 / -21.0 | 0.00 / -19.2 | 0.00 / -20.5 | 0.00 / -28.3 |
| -90 | 0.00 / -43.2 | 0.00 / -59.1 | 0.00 / -21.0 | 0.00 / -19.2 | 0.00 / -20.5 | 0.00 / -28.3 |
| +120 | 0.00 / -45.7 | 0.00 / -58.7 | 0.00 / -20.7 | 0.00 / -18.9 | 0.00 / -20.6 | 0.00 / -29.5 |
| -120 | 0.00 / -45.7 | 0.00 / -58.7 | 0.00 / -20.7 | 0.00 / -18.9 | 0.00 / -20.6 | 0.00 / -29.5 |
| +135 | 0.00 / -50.2 | 0.00 / -58.2 | 0.00 / -20.3 | 0.00 / -18.5 | 0.00 / -20.8 | 0.00 / -31.6 |
| -135 | 0.00 / -50.2 | 0.00 / -58.2 | 0.00 / -20.3 | 0.00 / -18.5 | 0.00 / -20.8 | 0.00 / -31.6 |
| +150 | 0.00 / -45.7 | 0.00 / -58.7 | 0.00 / -20.7 | 0.00 / -18.9 | 0.00 / -20.6 | 0.00 / -29.5 |
| -150 | 0.00 / -45.7 | 0.00 / -58.6 | 0.00 / -20.7 | 0.00 / -18.9 | 0.00 / -20.6 | 0.00 / -29.5 |
| +170 | 0.00 / -48.2 | 0.00 / -58.3 | 0.00 / -20.4 | 0.00 / -18.6 | 0.00 / -20.8 | 0.00 / -30.8 |
| -170 | 0.00 / -48.2 | 0.00 / -58.4 | 0.00 / -20.4 | 0.00 / -18.6 | 0.00 / -20.8 | 0.00 / -30.7 |
| 180 | 0.00 / -43.2 | 0.00 / -59.1 | 0.00 / -21.0 | 0.00 / -19.2 | 0.00 / -20.5 | 0.00 / -28.3 |

Max |rE direction error| over az -180..180 (1 deg grid), left half (+az) / right half (-az), |rE| range, max mirror asymmetry:
| encoder | left | right | |rE| | asym |
|---|---|---|---|---|
| (a) IEM plain | 0.00 | 0.00 | 0.974 | 0.003 |
| (b) dev | 0.00 | 0.00 | 0.965 | 0.005 |
| (c) dev + NFC 5 m | 0.00 | 0.00 | 0.900 | 0.003 |
| (c) dev + NFC 10 m | 0.00 | 0.00 | 0.866 | 0.003 |
| (d) v1.0.0 NFC neutral | 0.00 | 0.00 | 0.900 | 0.004 |
| (d) v1.0.0 + NFC 5 m | 0.00 | 0.00 | 0.853 | 0.004 |
| (d) v1.0.0 + NFC 10 m | 0.00 | 0.00 | 0.832 | 0.004 |
(All "0.00" are < 0.02 deg; control with decoder weights = basic gives the same, max 0.01 deg.) Velocity-vector direction error is also <= 0.01 deg everywhere; |rV| differs: (a) 0.87, (b) 0.92, (c) 0.73 / 0.70, (d) 0.56, (d)+NFC 0.55.

The loudest speaker is the nearest one to the source in every encoder; +-135 sits exactly between the speakers at 126 and 144 deg (9 deg each side), so the loudest one flips between them from float noise only, not an error.

## What actually differs (pattern, not position)
Side lobes / out-of-phase speakers, loudest-speaker reference, sources at +-135 (identical at +-135, +-45, so symmetric):
- (a) IEM plain: sidelobe -31 dB, 10 negative-gain speakers.
- (b) dev: sidelobe -31.5 dB, 12 negative (encoder max-rE is applied on top of the decoder's own max-rE: double weighting, tighter-but-lower-|rE| 0.965 vs 0.974, and the opposite speaker is -58 dB vs -50 dB; it is quieter at the back, not louder).
- (c) NFC LF band: sidelobe -18.1 dB (5 m), -16.6 dB (10 m), 14 to 16 negative speakers. The NFC boost of orders >= 1 by 10 to 16 dB with W untouched collapses the max-rE taper, so at low frequencies the image gets broader/less focused with out-of-phase energy 16 to 18 dB down at the opposite side. Direction stays right. Taps at different distances get different LF patterns.
- (d) v1.0.0 at 6OA: sidelobe -13.9 dB, with the wrong orders 4-6 constants (and -14 to -11 dB with NFC). Pattern is distorted (rV 0.56, vs 0.92 for correct). Direction is still right because the wrong constants scale +m and -m (sin and cos) of one (n) equally, so the pattern remains a function of angular distance from the source (verified numerically: rE error 0.00 deg, asym 0.004 deg).
Control: if the IEM decoder is set to N3D input while an SN3D feed arrives: direction error still <= 0.01 deg, |rE| drops to 0.915 (a), 0.897 (b), 0.883 (d).

## Conclusion for the hypothesis
On this ring and these encoders the Ambisonics math cannot put back-left or back-right sources in the wrong place or make them differ from each other. Any audible "back-left/back-right different or swapped" must come from outside this math: the tap-label trap documented in osd-ambi-path.md (OSD +az = LEFT, so az -135 is rear-RIGHT; taps 3 and 4 also have different delays), IEM speaker table azimuth sign or channel routing, or a track narrower than 49 channels triggering the surround fallback. The only effects of the OSD side that can be heard on this ring are (i) the doubled max-rE (b), (ii) the NFC LF boost reducing focus (c), and (iii) on v1.0.0 at 4OA to 6OA the wrong orders 4-6 constants (d), none of which is directional or left/right asymmetric.
