# NFC check: SpatialCore Ambisonics encoder (investigation, no source edited)

Script: /tmp/nfccheck/nfc.py (numpy). Raw output: /tmp/nfccheck/out.md. All numbers below come from it.

## 1. What the code does (src/Engine/RenderEngine.cpp, renderAmbisonicsOutput)

    float distMeters = sources.objects[t].distance * 10.0f;  // 0..1 -> 0..10m           (l.1655)
    smoothedNfcDistance[t] += nfcSmoothAlpha * (distMeters - smoothedNfcDistance[t]);    (l.1656, alpha 0.15 per BLOCK, one-pole)
    for (int ord = 0; ord < ambiOrder && ord < kMaxAmbiOrder; ++ord) {                  (l.1660)
        int n = ord + 1;
        float r = std::max (0.05f, smoothedNfcDistance[t]);
        float fPole = n * c / (2*pi * r);               // c = 343
        float fZero = n * c / (2*pi * kNfcReferenceRadius);   // kNfcReferenceRadius = 1.5f (header l.834)
        fPole = min(fPole, sr*0.25); fZero = min(fZero, sr*0.25);
        wPole = tan(pi*fPole/sr); wZero = tan(pi*fZero/sr);
        b0 = 1+wZero; b1 = wZero-1; a0 = 1+wPole; a1 = wPole-1;
        coefficients = { b0/a0, b1/a0, 1.0f, a1/a0 };    // first-order bilinear shelf
    }

Application (l.1715-1726): W (ACN 0) gets NO filter ("Order 0 (W channel): no NFC needed"). For every order n = 1..N ONE first-order
filter nfcFilters[t][n-1] processes the object's mono signal once and the result feeds all (2n+1) SH channels of that order.
So: one first-order shelf per order (not n cascaded sections), per object, coefficients refreshed when smoothed distance moves > 0.01 m.

Analytic response: H(z) = ((1+wZ) + (wZ-1)z^-1) / ((1+wP) + (wP-1)z^-1).
 - z = -1 (Nyquist side): gain exactly 1.  Highs untouched.
 - z = +1 (DC): gain = wZ/wP ~ fZero/fPole = r / 1.5.  The n cancels. DC gain is r/1.5 for EVERY order.
 - n only moves the corner frequencies (both scale with n), so higher orders reach the DC plateau at higher frequency.
So the claim is confirmed: DC asymptote +10.5 dB at 5 m, +16.5 dB at 10 m, -9.5 dB at 0.5 m, on every order >= 1, relative to W.
At the grid frequencies the plateau is only reached for the high orders (order 6, 10 m: +15.1 dB at 20 Hz; order 1, 10 m: +6.0 dB at 20 Hz, because
order 1's pole is at 5.5 Hz / zero at 36 Hz).

## 2. Code magnitude (dB), r x order x frequency, 48 kHz

| order | r (m) | 20 Hz | 50 Hz | 100 Hz | 200 Hz | 500 Hz | 1000 Hz |
|---|---|---|---|---|---|---|---|
| 1 | 0.5 | -8.5 | -5.8 | -2.9 | -1.0 | -0.2 | -0.0 |
| 1 | 1.0 | -2.9 | -1.6 | -0.6 | -0.2 | -0.0 | -0.0 |
| 1 | 1.5 | +0.0 | +0.0 | +0.0 | +0.0 | +0.0 | +0.0 |
| 1 | 3 | +3.7 | +1.3 | +0.4 | +0.1 | +0.0 | +0.0 |
| 1 | 5 | +5.2 | +1.6 | +0.5 | +0.1 | +0.0 | +0.0 |
| 1 | 10 | +6.0 | +1.8 | +0.5 | +0.1 | +0.0 | +0.0 |
| 2 | 0.5 | -9.3 | -8.1 | -5.8 | -2.9 | -0.7 | -0.2 |
| 2 | 1.0 | -3.3 | -2.7 | -1.6 | -0.6 | -0.1 | -0.0 |
| 2 | 1.5 | +0.0 | -0.0 | +0.0 | +0.0 | +0.0 | +0.0 |
| 2 | 3 | +5.2 | +3.1 | +1.3 | +0.4 | +0.1 | +0.0 |
| 2 | 5 | +8.1 | +4.2 | +1.6 | +0.5 | +0.1 | +0.0 |
| 2 | 10 | +10.4 | +4.7 | +1.8 | +0.5 | +0.1 | +0.0 |
| 3 | 0.5 | -9.4 | -8.8 | -7.3 | -4.5 | -1.3 | -0.4 |
| 3 | 1.0 | -3.4 | -3.1 | -2.3 | -1.1 | -0.2 | -0.1 |
| 3 | 1.5 | -0.0 | +0.0 | +0.0 | +0.0 | +0.0 | +0.0 |
| 3 | 3 | +5.6 | +4.2 | +2.3 | +0.8 | +0.2 | +0.0 |
| 3 | 5 | +9.2 | +6.1 | +3.0 | +1.0 | +0.2 | +0.0 |
| 3 | 10 | +12.7 | +7.2 | +3.3 | +1.1 | +0.2 | +0.1 |
| 4 | 0.5 | -9.5 | -9.1 | -8.1 | -5.8 | -2.1 | -0.7 |
| 4 | 1.0 | -3.5 | -3.3 | -2.7 | -1.6 | -0.4 | -0.1 |
| 4 | 1.5 | +0.0 | +0.0 | +0.0 | +0.0 | +0.0 | +0.0 |
| 4 | 3 | +5.8 | +4.8 | +3.1 | +1.3 | +0.3 | +0.1 |
| 4 | 5 | +9.7 | +7.3 | +4.2 | +1.6 | +0.3 | +0.1 |
| 4 | 10 | +13.9 | +9.0 | +4.7 | +1.8 | +0.3 | +0.1 |
| 5 | 0.5 | -9.5 | -9.3 | -8.5 | -6.7 | -2.9 | -1.0 |
| 5 | 1.0 | -3.5 | -3.3 | -2.9 | -1.9 | -0.6 | -0.2 |
| 5 | 1.5 | +0.0 | -0.0 | +0.0 | +0.0 | -0.0 | +0.0 |
| 5 | 3 | +5.9 | +5.2 | +3.7 | +1.8 | +0.4 | +0.1 |
| 5 | 5 | +10.0 | +8.1 | +5.2 | +2.3 | +0.5 | +0.1 |
| 5 | 10 | +14.7 | +10.4 | +6.0 | +2.5 | +0.5 | +0.1 |
| 6 | 0.5 | -9.5 | -9.4 | -8.8 | -7.3 | -3.6 | -1.3 |
| 6 | 1.0 | -3.5 | -3.4 | -3.1 | -2.3 | -0.8 | -0.2 |
| 6 | 1.5 | +0.0 | +0.0 | +0.0 | +0.0 | +0.0 | +0.0 |
| 6 | 3 | +5.9 | +5.4 | +4.2 | +2.3 | +0.6 | +0.2 |
| 6 | 5 | +10.1 | +8.7 | +6.1 | +3.0 | +0.7 | +0.2 |
| 6 | 10 | +15.1 | +11.5 | +7.2 | +3.3 | +0.7 | +0.2 |


## 3. Textbook NFC-HOA (Daniel 2003), same grid, R = 1.5 m
Filter used: F_n(x) = sum_{j=0..n} (n+j)!/((n-j)! j!) (-i/(2x))^j, x = k r = 2 pi f r / c; response = |F_n(k r) / F_n(k R)|.
LF asymptote (R/r)^n  ->  n * 20 log10(R/r) dB. (This is the encoder near-field term for a source at r combined with the decoder compensation for speakers at R.)

| order | r (m) | 20 Hz | 50 Hz | 100 Hz | 200 Hz | 500 Hz | 1000 Hz |
|---|---|---|---|---|---|---|---|
| 1 | 0.5 | +8.5 | +5.8 | +2.9 | +1.0 | +0.2 | +0.0 |
| 1 | 1.0 | +2.9 | +1.6 | +0.6 | +0.2 | +0.0 | +0.0 |
| 1 | 1.5 | +0.0 | +0.0 | +0.0 | +0.0 | +0.0 | +0.0 |
| 1 | 3 | -3.7 | -1.3 | -0.4 | -0.1 | -0.0 | -0.0 |
| 1 | 5 | -5.2 | -1.6 | -0.5 | -0.1 | -0.0 | -0.0 |
| 1 | 10 | -6.0 | -1.8 | -0.5 | -0.1 | -0.0 | -0.0 |
| 2 | 0.5 | +18.7 | +16.3 | +10.5 | +3.9 | +0.6 | +0.1 |
| 2 | 1.0 | +6.8 | +5.3 | +2.4 | +0.6 | +0.1 | +0.0 |
| 2 | 1.5 | +0.0 | +0.0 | +0.0 | +0.0 | +0.0 | +0.0 |
| 2 | 3 | -10.6 | -5.2 | -1.5 | -0.3 | -0.1 | -0.0 |
| 2 | 5 | -16.1 | -6.4 | -1.8 | -0.4 | -0.1 | -0.0 |
| 2 | 10 | -19.3 | -6.9 | -1.9 | -0.4 | -0.1 | -0.0 |
| 3 | 0.5 | +28.4 | +27.0 | +21.6 | +10.1 | +1.4 | +0.3 |
| 3 | 1.0 | +10.4 | +9.6 | +6.1 | +1.5 | +0.2 | +0.0 |
| 3 | 1.5 | +0.0 | +0.0 | +0.0 | +0.0 | +0.0 | +0.0 |
| 3 | 3 | -17.2 | -12.1 | -3.9 | -0.8 | -0.1 | -0.0 |
| 3 | 5 | -28.3 | -15.5 | -4.6 | -0.9 | -0.1 | -0.0 |
| 3 | 10 | -36.9 | -16.7 | -4.8 | -1.0 | -0.1 | -0.0 |
| 4 | 0.5 | +38.0 | +37.1 | +33.3 | +19.7 | +2.8 | +0.5 |
| 4 | 1.0 | +14.0 | +13.4 | +10.9 | +3.5 | +0.3 | +0.1 |
| 4 | 1.5 | +0.0 | +0.0 | +0.0 | +0.0 | +0.0 | +0.0 |
| 4 | 3 | -23.5 | -19.9 | -8.8 | -1.5 | -0.2 | -0.0 |
| 4 | 5 | -39.8 | -27.6 | -10.1 | -1.8 | -0.2 | -0.1 |
| 4 | 10 | -56.0 | -30.0 | -10.6 | -1.9 | -0.2 | -0.1 |
| 5 | 0.5 | +47.6 | +46.9 | +44.2 | +31.4 | +5.5 | +0.8 |
| 5 | 1.0 | +17.5 | +17.1 | +15.3 | +7.2 | +0.6 | +0.1 |
| 5 | 1.5 | +0.0 | +0.0 | +0.0 | +0.0 | +0.0 | +0.0 |
| 5 | 3 | -29.7 | -27.1 | -16.1 | -2.9 | -0.3 | -0.1 |
| 5 | 5 | -50.8 | -40.8 | -18.6 | -3.3 | -0.3 | -0.1 |
| 5 | 10 | -75.0 | -45.7 | -19.3 | -3.4 | -0.4 | -0.1 |
| 6 | 0.5 | +57.1 | +56.6 | +54.5 | +43.9 | +10.2 | +1.3 |
| 6 | 1.0 | +21.1 | +20.7 | +19.4 | +12.2 | +0.9 | +0.2 |
| 6 | 1.5 | +0.0 | +0.0 | +0.0 | +0.0 | +0.0 | +0.0 |
| 6 | 3 | -35.8 | -33.8 | -24.5 | -5.3 | -0.4 | -0.1 |
| 6 | 5 | -61.5 | -53.8 | -29.4 | -5.9 | -0.5 | -0.1 |
| 6 | 10 | -93.2 | -63.1 | -30.5 | -6.2 | -0.5 | -0.1 |


## 4. Side by side and verdict

| order | r | code 20/100/1k | theory 20/100/1k | theory LF asymptote |
|---|---|---|---|---|
| 1 | 0.5 | -8.5/-2.9/-0.0 | +8.5/+2.9/+0.0 | +9.5 |
| 1 | 5 | +5.2/+0.5/+0.0 | -5.2/-0.5/-0.0 | -10.5 |
| 1 | 10 | +6.0/+0.5/+0.0 | -6.0/-0.5/-0.0 | -16.5 |
| 3 | 0.5 | -9.4/-7.3/-0.4 | +28.4/+21.6/+0.3 | +28.6 |
| 3 | 5 | +9.2/+3.0/+0.0 | -28.3/-4.6/-0.0 | -31.4 |
| 3 | 10 | +12.7/+3.3/+0.1 | -36.9/-4.8/-0.0 | -49.4 |
| 6 | 0.5 | -9.5/-8.8/-1.3 | +57.1/+54.5/+1.3 | +57.3 |
| 6 | 5 | +10.1/+6.1/+0.2 | -61.5/-29.4/-0.1 | -62.7 |
| 6 | 10 | +15.1/+7.2/+0.2 | -93.2/-30.5/-0.1 | -98.9 |


Verdict: both (b) and (c), plus a third difference.
 (b) Direction is inverted. Theory: r > R attenuates LF of order n, r < R boosts it. Code: r > R boosts, r < R cuts. The code's dB values are the exact negative of the
     theory's at order 1 (e.g. 5 m, 20 Hz: code +5.2, theory -5.2). Theory at order 1 is the reciprocal of the code's filter.
 (c) The n-th power is missing. Theory's LF slope is n*20log10(r/R) (order 6 at 10 m: -99 dB asymptote, -93 dB at 20 Hz). The code is capped at 20log10(r/R) for every order
     (+16.5 dB). It is a bounded first-order shelf; theory is an order-n filter with n poles at the origin (unbounded at DC), which is why real NFC systems either
     high-pass or stabilise it. The code looks like a "stabilised, bounded" shape but with the wrong sign and the wrong depth.
 (d) The corner frequency also differs: theory's transitions go as c*n/(2 pi r) up to a multi-pole roll with higher-order structure (order 6 at 5 m still -29 dB at 100 Hz,
     -6 dB at 200 Hz); the code's shelf has finished its work by ~200 Hz for r=10 m n=6 (+3.3 dB).
So it is not "correct"; "inverted" (the previous agent's phrase) is right for the sign, but the magnitude is also wrong by the factor n.
Where the code's result happens to be physically sensible: nowhere except r = 1.5 m (identity). Note kNfcReferenceRadius = 1.5 m also means a source at 0.15 normalised distance is neutral, which the 261024-mhh quick task relies on.

## 5. Audible consequence with a no-NFC decoder (IEM AllRAD) and OSD taps at different distances
A decoder without NFC assumes the encoded signal is a pure plane-wave/ r = R encoding: all orders at equal weight, W and orders >= 1 in phase at all frequencies.
The code breaks that for r != 1.5 m: far taps (the common case, distance 0.3-1.0 normalised = 3-10 m) have orders >= 1 boosted by up to +10..16 dB below roughly 100-300 Hz
(plateau frequency rises with order), while W and everything above ~300 Hz is unchanged. So the low end of far taps is encoded with much too much directional/high-order content and too little omni.
Also, since the shelf is on the distance-smoothed value, a tap whose distance moves sweeps the boost.
Decoded LF pattern (my sim: 20-speaker ring, 2D circular harmonics order 6 = 13 channels, mode-matching pinv decoder, source at 135 deg, speakers 18 deg apart, so the source
sits between speakers 126 and 144; there is no speaker exactly at 315, nearest are 306/324, values below are 306/324 = -0.03/+0.03 for g=3.3 no-maxrE; "opposite" = those):

without max-rE weights (SpatialCore applies max-rE in the encoder):

| g (all orders>=1) | peak speaker az | peak gain | gain at opposite (315 deg) | opposite rel. to peak (dB) | power frac. within +-45 deg of source | omni (W only) share of power |
|---|---|---|---|---|---|---|
| 1.0 | 126 | 0.543 | +0.026 | -26.3 | 0.95 | 0.077 |
| 3.3 | 126 | 1.678 | -0.029 | -35.4 | 0.94 | 0.008 |
| 0.5 | 126 | 0.297 | +0.038 | -17.8 | 0.90 | 0.250 |

full speaker gains for g=1 vs g=3.3 (az: gains):
g=1.0: 0:+0.02, 18:+0.04, 36:-0.06, 54:+0.02, 72:+0.07, 90:-0.12, 108:+0.02, 126:+0.54, 144:+0.54, 162:+0.02, 180:-0.12, 198:+0.07, 216:+0.02, 234:-0.06, 252:+0.04, 270:+0.02, 288:-0.05, 306:+0.03, 324:+0.03, 342:-0.05
g=3.3: 0:-0.05, 18:+0.01, 36:-0.33, 54:-0.06, 72:+0.13, 90:-0.51, 108:-0.06, 126:+1.68, 144:+1.68, 162:-0.06, 180:-0.51, 198:+0.13, 216:-0.06, 234:-0.33, 252:+0.01, 270:-0.05, 288:-0.28, 306:-0.03, 324:-0.03, 342:-0.28

with max-rE weights (SpatialCore applies max-rE in the encoder):

| g (all orders>=1) | peak speaker az | peak gain | gain at opposite (315 deg) | opposite rel. to peak (dB) | power frac. within +-45 deg of source | omni (W only) share of power |
|---|---|---|---|---|---|---|
| 1.0 | 126 | 0.396 | +0.003 | -43.7 | 1.00 | 0.143 |
| 3.3 | 126 | 1.191 | -0.107 | -21.0 | 0.94 | 0.015 |
| 0.5 | 144 | 0.223 | +0.026 | -18.6 | 0.93 | 0.400 |

full speaker gains for g=1 vs g=3.3 (az: gains):
g=1.0: 0:+0.00, 18:+0.00, 36:-0.01, 54:+0.01, 72:-0.00, 90:-0.03, 108:+0.13, 126:+0.40, 144:+0.40, 162:+0.13, 180:-0.03, 198:-0.00, 216:+0.01, 234:-0.01, 252:+0.00, 270:+0.00, 288:-0.01, 306:+0.00, 324:+0.00, 342:-0.01
g=3.3: 0:-0.10, 18:-0.11, 36:-0.14, 54:-0.08, 72:-0.13, 90:-0.21, 108:+0.32, 126:+1.19, 144:+1.19, 162:+0.32, 180:-0.21, 198:-0.13, 216:-0.08, 234:-0.14, 252:-0.11, 270:-0.10, 288:-0.13, 306:-0.11, 324:-0.11, 342:-0.13

Reading the sim (g = 3.3 is the +10.5 dB, 5 m case; applied to all orders >= 1 equally, which is what the code does at DC):
 - Peak direction does not move (still 126/144, the pair straddling 135). Localisation direction is preserved at LF, it is not "pulled" elsewhere.
 - Peak speaker level at LF rises ~ +10 dB relative to the g = 1 case with the same W (0.54 -> 1.68 without max-rE; 0.40 -> 1.19 with it). So far taps get a ~10 dB
   bass-level jump at the speakers nearest the source and the omni (W) part is negligible, i.e. a spatially narrow, loud bass.
 - The pattern is a sharpened beam with strong out-of-phase lobes. Without max-rE the side speakers at 90 and 180 deg go from -0.12 to -0.51 (anti-phase, -10 dB re peak);
   speakers at 306/324 on the far side stay small (|0.03|). The max-rE weighting (applied in the encoder) is what suppresses side lobes: at g = 1 the far side is -43.7 dB re peak,
   but at g = 3.3 every speaker on the ring is anti-phase at -0.08..-0.21 (-21 dB re peak at the worst far-side speaker), i.e. the max-rE protection is largely undone
   because W no longer dominates.
 - Net effect heard: LF of distant sources is louder (about +10..16 dB below ~150 Hz on high orders), arrives from a tight set of speakers with out-of-phase leakage into
   the rest of the ring, so the bass sounds boomy/unstable and image-width/phase inconsistent between taps at different distances, and the level changes with each tap's
   distance rather than staying matched to the distance-gain curve. With physically correct NFC (far = LF reduced on high orders) the LF would instead spread wider and quieter.
   Caveat: this is a 2D mode-matching surrogate, not AllRAD; AllRAD (and IEM's) uses 3D SH with imaginary-speaker regularisation, so absolute numbers differ, trend should hold.
   Caveat 2: I did not model the actual time-varying filter, only its LF plateau.

## 6. Provenance
- NFC code arrives in SpatialCore's initial commit 24e3185 (2026-03-22, "extracted from OpenSpatialDelay v1.0") and was then transplanted into RenderEngine by 150d91f (2026-07-06, 08-06).
  `git log -S"kNfcReferenceRadius"` first hit is 150d91f; the constant and the pole/zero code are older, from OpenSpatialDelay (not in this repo's history).
- No design doc states the intended sign or the target response. The only description is the comment `kNfcReferenceRadius = 1.5f; // meters (typical studio monitoring distance)`
  (RenderEngine.h:834), the label "NFC-HOA per-order shelf filters" and CLAUDE.md "NFC-HOA IIR filters". Search of .planning/ and docs/ found only bit-identity / allocation
  work on it (Phase 5 05-01, [nfc][d10] pins the coefficients bit-for-bit against the OLD formula, so tests lock in the current direction) and quick task 261024-mhh, which uses
  1.5 m as the "NFC-neutral" distance. Nothing derives r/1.5 from Daniel, nothing says boost-for-far is intended.
- Consequence for a fix: [nfc][d10] and the byte-identical Phase 5 captures would need deliberate rebaselining (D-10 said "if not bit-identical: stop and ask").
- IEM encoders apply no NFC (ref only; /tmp/iem-src-0ngi9a not re-read here). Net: IEM/AllRAD with no-NFC corresponds to theory's F_n(kr)/F_n(kr)=1, which is the right thing to feed it.
