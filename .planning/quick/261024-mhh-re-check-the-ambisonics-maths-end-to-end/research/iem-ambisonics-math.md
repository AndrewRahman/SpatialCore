# IEM Plug-in Suite: Ambisonics encode / decode math

Source: https://git.iem.at/audioplugins/IEMPluginSuite, shallow clone, commit 6e861ea3c3378ac9c97862991644a99e953f60ee (2026-09-30), read-only, at /tmp/iem-src-0ngi9a/IEMPluginSuite.
All paths below are relative to that repo root. Nothing was built or run.

## 0. One-paragraph summary

- Channel order ACN, normalisation SN3D by default (user switch N3D/SN3D, default SN3D, `useSN3D` default 1.0).
- Internally the SH evaluator produces N3D; SN3D is made by multiplying each channel by `1/sqrt(2n+1)` after evaluation (encoders).
- Coordinates: x front, y LEFT, z up. Azimuth positive = counter-clockwise seen from above = toward the left. Elevation positive = up.
- No Condon-Shortley phase. No per-order weighting, no max-rE, no W gain, no distance/NFC in StereoEncoder or MultiEncoder.
- Max-rE is applied on the DECODE side only: AllRADecoder and SimpleDecoder default to `maxrE` (choice index 1 of basic/maxrE/inphase), and the EnergyVisualizer ALWAYS applies max-rE.
- AllRADecoder builds its matrix assuming N3D input and converts at run time (SN3D input is multiplied by `sn3d2n3d` per channel).

## 1. Encoders

### 1.1 Coordinate convention (`resources/Conversions.h`)

`Conversions<float>::sphericalToCartesian(az, el)` (resources/Conversions.h, final overload):
```
x = cos(el) * cos(az)
y = cos(el) * sin(az)
z = sin(el)
```
Inverse (`cartesianToSpherical`, same file): `az = atan2(y, x)`, `el = atan2(z, sqrt(x^2+y^2))`.
So +x front, +y left, +z up; +az = left (CCW from above); +el = up.
Parameter ranges: azimuth -180..180 deg, elevation -180..180 deg (StereoEncoder/Source/PluginProcessor.cpp:536-553; MultiEncoder/Source/PluginProcessor.cpp:588-604).
The EnergyVisualizer screen mapping confirms it: `HammerAitov::sphericalToXY` (resources/HammerAitov.h:43) has `x = factor * -cosEle * sin(az/2)`, so positive azimuth plots on screen-left (front at centre).

### 1.2 SH evaluation (`resources/efficientSHvanilla.cpp`, `.h`)

- Functions `SHEval0..7(x,y,z,out)` are Peter-Pike Sloan generated code (efficientSHvanilla.cpp:21-24). They evaluate the ORTHONORMAL real SH (Y00 = 1/(2 sqrt(pi)) = 0.28209479), in ACN order, on a unit-vector input (x,y,z). Order 0..3 at efficientSHvanilla.cpp:26-110.
- No Condon-Shortley phase: every sign is +, e.g. `pSH[1]=0.4886*y; pSH[3]=0.4886*x` (efficientSHvanilla.cpp:36-45), `pSH[15] = c*(x^3-3xy^2)`.
- The wrapper `SHEval(N,x,y,z,out,doEncode=true)` (efficientSHvanilla.h, switch at lines ~49-107) multiplies the whole vector by `sqrt4PI = 3.5449077` when `doEncode` is true. Orthonormal x sqrt(4 pi) = **N3D**. So encoder output of `SHEval` is N3D (Y00 = 1, Y1m amplitude sqrt3).
- With `doEncode=false` (decoder design) it multiplies by `decodeCorrection(N) = sqrt4PI/(N+1)^2` (efficientSHvanilla.h:44-47, comment line 41-42 "normalization is done at the decoding stage").
- The Sloan code takes (x,y,z) as the direction vector, input must be unit length; the encoders feed it cos/sin from `sphericalToCartesian`.

### 1.3 N3D -> SN3D

- `useSN3D` parameter, default `1.0f` (SN3D) in StereoEncoder (StereoEncoder/Source/PluginProcessor.cpp:~519-528), MultiEncoder (MultiEncoder/Source/PluginProcessor.cpp:536-547), AllRADecoder (AllRADecoder/Source/PluginProcessor.cpp:1459-1470), EnergyVisualizer (:273-283).
- Conversion: after `SHEval` the encoder multiplies channel-wise by `n3d2sn3d[ch]`:
  - MultiEncoder: `if (*useSN3D >= 0.5f) multiply(SH[i], SH[i], n3d2sn3d, nChOut)` (MultiEncoder/Source/PluginProcessor.cpp:255-258).
  - StereoEncoder: lines 236-243 (non high-quality) and 316-326 (sample-wise mode).
- Table `n3d2sn3d[64]` in resources/ambisonicTools.h:118-146: per ACN channel `1/sqrt(2n+1)` = 1, 0.57735 (ACN1-3), 0.44721 (ACN4-8), 0.37796 (ACN9-15), 0.33333 (16-24), 0.30151, 0.27735, 0.25820. Inverse table `sn3d2n3d[64]` at ambisonicTools.h:93; short per-order versions `n3d2sn3d_short[8]`, `resources/Weights.h:134-137`.

### 1.4 MultiEncoder (MultiEncoder/Source/PluginProcessor.cpp)

Per input i, per block (lines 225-271):
```
gain   = dB->linear (gain[i]), 0 if muted / not soloed          (240-246)
az, el = degreesToRadians(azimuth[i], elevation[i])              (248-249)
pos    = sphericalToCartesian(az, el)  (unit vector)             (251-253)
SH[i]  = SHEval(order, pos)  // N3D                              (255)
SH[i] *= n3d2sn3d (if useSN3D)                                    (257-258)
out[ch] += in[i] * SH[i][ch] * gain, linear ramp from previous block's value to this one  (260-269)
```
No order weighting, no max-rE, no extra W gain, no distance, no NFC. Gain is plain dB (-60..+10, default 0 dB; line 606-612). Output is `ambisonicOrder` truncation: `nChOut = min(buffer channels, (order+1)^2)`. Interpolation is a per-block linear ramp of the coefficients (addFromWithRamp), not of the angles.

### 1.5 StereoEncoder (StereoEncoder/Source/PluginProcessor.cpp)

- Two sources (L and R input channels), each encoded as a point source at a direction derived from a quaternion.
- `updateQuaternions()` (lines 152-170): `ypr = [az, -el, roll]` (radians); note the NEGATED pitch, because the quaternion Tait-Bryan pitch is about +y (positive pitch = down), so `-el` makes positive elevation go up. `quaternionDirection.fromYPR(ypr)` (resources/Quaternion.h:137-154, z-y'-x'' intrinsic).
- Width (lines 211-221; 129-137 in prepareToPlay): `q = width_deg*pi/180/4`; `quatLRot = (cos q, 0, 0, sin q)` = rotation about z by `width/2`; `quatL = qDir * quatLRot`, `quatR = qDir * conj(quatLRot)`. Directions: `getCartesian()` (Quaternion.h:101-110) gives the rotated x unit vector: `x = 1-2y^2-2z^2, y = 2xy+2wz, z = 2xz-2wy`.
  - So Left input is rotated `+width/2` about the body z axis, Right `-width/2` (about the already azimuth/elevation/roll-rotated frame, so after elevation the pair spreads along the rotated horizontal "ear" axis, and roll tilts it). width range -360..360 deg, default 0 (both at the same point).
- Then `SHEval(order, left.x, left.y, left.z, SHL)` and same for R (lines 236-237), N3D -> SN3D multiply (241-242), `buffer = L_in*SHL + R_in*SHR` with linear coefficient ramp (249-250). In "highQuality / sample-wise" mode the angles are smoothed per sample (`SmoothedValue`) and SH recomputed per sample (lines 290-313), then the whole buffer is multiplied by `n3d2sn3d` after the fact (316-324).
- No gain on W, no max-rE, no NFC, no distance.
- Output order default Auto; bus defaults ambisonic(7) (1 for VST3 wrapper), lines 35-40.

### 1.6 Does the encoder weight per order? No.
Confirmed by reading all of StereoEncoder/MultiEncoder PluginProcessor.cpp: grep for `maxre`, `MaxRE`, `Weights`, `inPhase` returns nothing in either encoder. An encoded plane wave is a pure `Y_nm(direction)` in SN3D (or N3D).

## 2. Decoders

### 2.1 AllRADecoder design (AllRADecoder/Source/PluginProcessor.cpp, `calculateDecoder()` lines 810-1100)

Inputs: the user's loudspeaker table (azimuth, elevation, radius, gain, channel, "Imaginary" flag) -> `points[]`.

1. Layout check (`checkLayout`, line ~700-790): needs >= 4 loudspeakers (ERROR 2, line 719); duplicates rejected; convex hull via NewtonApple_hull3D (line 722); the origin must lie inside the hull (ERROR 4, line 767-771); no triangle with >1 imaginary speaker (ERROR 5, 773-780); every speaker must be on the hull (ERROR 6); not all imaginary (ERROR 7).
2. Per triangle `L = [v_a v_b v_c]` (3x3 of speaker unit vectors; imaginary speakers' vectors are normalised, 818-838), `inverse(L)` is stored (lines 829-842).
3. Sampling grid: a 5200-point t-design (`tDesign5200`, AllRADecoder/Source/tDesign5200.h; loop at line 853). For each grid direction `s`:
   - `sh = SHEval(N, s, doEncode=false)` i.e. N3D x `sqrt(4pi)/(N+1)^2` (line 856).  N here = decoder order 1..7 (param `decoderOrder`, choice index 0 = "1st", so `N = index+1`, line 814; param at 1475-1497).
   - find triangle where `g = inverse(L) * s` has all components >= -FLT_EPSILON (line 868-872), i.e. VBAP gains, then **normalise to unit L2 (energy) norm**: `g = g / sqrt(g1^2+g2^2+g3^2)` (lines 874-879). That is VBAP with constant-power normalisation (p = 2).
   - Real triangle: `D[row_of_speaker_k] += g_k * sh` for the three speakers (lines 920-935 area, the `else` branch).
   - Triangle with an imaginary speaker: the imaginary speaker's gain is redistributed to the real neighbours that share triangles with it, with a power-preserving factor kappa (`getKappa`, lines ~1100-1108; imaginary-speaker gain property is multiplied in). This is the AllRAD "imaginary loudspeaker" scheme: energy panned to the imaginary speaker is spread to its connected real speakers.
   - The accumulated D is the (speakers x (N+1)^2) matrix = sum over t-design points of (VBAP gain vector) x (N3D SH of the point) (sampling decoder). Hence the decoder matrix expects **N3D** input.
4. Normalisation of the decoder matrix (lines 955-977): for each REAL speaker position `p`: `sh_p = SHEval(N, p)` (encode, with `sqrt4PI` factor, N3D), `E_p = sqrt( sum_m ( sum_n sh_p[n] D(m,n) )^2 )` (L2 norm of loudspeaker gains for a source at that speaker; the sum is of the BASIC, unweighted decode). `maxGain = max_p E_p`; **`D = D / maxGain`** (line 977). So the loudspeaker-gain L2 norm for a source at the worst speaker direction is exactly 1.0 (no max-rE in this normalisation).
5. Stored decoder settings (lines 1064-1067):
   ```
   expectedNormalization = n3d
   weights               = ambisonicWeights     // chosen param
   weightsAlreadyApplied = false
   ```
   so the matrix itself contains NO weights; weights are applied at run time on the input channels.

Weights choice: `weightsStrings = {"basic","maxrE","inphase"}` (line 28-29), parameter default index **1 = maxrE** (line 1529-1532: `AudioParameterChoice("weights", ..., weightsStrings, 1)`). `ReferenceCountedDecoder::Weights { none, maxrE, inPhase }` (resources/ReferenceCountedDecoder.h:40-45), the cast `Weights(round(param))` maps basic=none, maxrE, inphase directly.
The preview images in the plugin (energy / rE maps, lines 981-1055) multiply the SH by `multiplyMaxRE` / `multiplyInPhase` when selected (1006-1009) so the displayed maps use the chosen weights.

Layout with no height (2D ring): AllRAD needs the origin inside a 3D hull, so a horizontal ring alone fails (ERROR 4: "Point of origin is not within the convex hull. Try adding imaginary loudspeakers.", line 770; and a too-flat hull gives ERROR 3, line 764). The user adds imaginary speakers: the editor button calls `addImaginaryLoudspeakerBelow()` which appends an imaginary speaker at cartesian (0,0,-1), gain 0 (PluginProcessor.cpp:580-589; PluginEditor.cpp:306). The default layout in the plugin (lines 80-120+) is an 8 speaker ring plus upper speakers and an imaginary at azimuth 45 deg. A 2D layout is therefore handled by adding imaginary speakers at the zenith and nadir (the plugin does not do it automatically except via that button for "below"). With imaginary speakers having gain 0, their energy is dropped / redistributed per `getKappa`, and the real speakers are normalised so the ring is not quieter than it should be.

### 2.2 Run-time decode weighting (`resources/AmbisonicDecoder.h`, `processInternal` 129-175)

Shared by AllRADecoder and SimpleDecoder. For input order `o` (from channel count) and decoder order `O` (from matrix columns):
```
weights[ch] = ( (O+1)/(o+1) )^(1/4)          // line 143-146: sqrt(sqrt(.)) order mismatch correction
if maxrE:   weights *= maxre_o[ch]  (per-order table, MaxRE.h) ; weights *= maxRECorrectionEnergy[o]   (lines 148-153)
if inPhase: weights *= inPhase_o[ch]; weights *= inPhaseCorrectionEnergy[o]                            (155-160)
if decoder.expectedNormalization != input normalization:
     SN3D input, N3D decoder -> weights *= sn3d2n3d[ch]   (line 164-170)
input[ch] *= weights[ch]; out = D(trimmed to input channels) * input   (matMult.processNonReplacing)
```
- Max-rE per-order weights (resources/MaxRE.h:25-240, one value per degree n, repeated for all m). Table is selected by the ORDER OF THE INPUT SIGNAL N. For N=3 (`maxre3[16]`): n=0 1.0, n=1 0.86155, n=2 0.61340, n=3 0.30643. For N=1: 1.0, 0.57754. N=2: 1.0, 0.77521, 0.40142. N=4: 1.0, 0.90644, 0.73245, 0.50225, 0.24736.
- These are the P_n(cos(137.9deg/(N+1.51))) weights (N-dependent: tables are for input order N, so max-rE changes with the order of the signal fed in).
- `maxRECorrectionEnergy[0..7]` = {1, 1.061115, 1.083513, 1.095090, 1.102149, 1.106900, 1.110314, 1.112886} (MaxRE.h:252-254) = `sqrt(sqrt((N+1)^2... ))` comment says `sqrt(sqrt((N+1)/sum(maxrE(N))))`, restoring energy/loudness lost to max-rE.
- inPhase tables: resources/inPhase.h (order1 0.3333, order2 0.5/0.1, order3 0.6/0.2/0.02857 ...), correction `inPhaseCorrectionEnergy` (inPhase.h:246-247).
- The normalisation switch is applied at the INPUT: a SN3D feed to an N3D-designed matrix gets `sqrt(2n+1)` per channel (sn3d2n3d, ambisonicTools.h:93).

### 2.3 SimpleDecoder (SimpleDecoder/Source/PluginProcessor.cpp)

- Loads decoder matrices from JSON (`resources/ConfigurationHelper.h` `parseVarForDecoder`, lines ~255-305) with fields `ExpectedInputNormalization` (n3d|sn3d), `Weights` (maxrE|inPhase|none), `WeightsAlreadyApplied`. Presets: SimpleDecoder/Source/Presets/*.json (e.g. Quadraphonic.json: `n3d`, `Weights: none`, `WeightsAlreadyApplied: false`).
- Weights parameter ("basic","maxrE","inphase") default index 1 = maxrE (PluginProcessor.cpp:28-29, 874-877). On loading a config the parameter is overwritten with the file's weights setting (line 672-673), and every block `settings.weights = Weights(param)` (412-415), so the user's choice always wins at run time.
- Matrix applied through the same `AmbisonicDecoder` (lines 239-243, 422), so the section 2.2 weighting applies. If a JSON has `WeightsAlreadyApplied: true`, `AmbisonicDecoder::checkIfNewDecoderAvailable` calls `removeAppliedWeights()` (ReferenceCountedDecoder.h:101-117), dividing them out, then reapplies per the setting.
- Input normalisation param `useSN3D` (default SN3D) sets `decoder.setInputNormalization`.
- Sub-woofer paths add `omniGain * sqrt(order+1)` gain (line 376), not relevant to the main decode.

### 2.4 Net result for a typical chain
Encode (SN3D, no weights) -> AllRADecoder (default maxrE, N3D matrix, SN3D->N3D at input, order correction, energy correction) -> speakers.
The max-rE ends up as a decoder-side weighting only.

## 3. EnergyVisualizer (EnergyVisualizer/Source/PluginProcessor.cpp)

- It is a **decoder-to-a-sampling-grid + RMS meter**, not a speaker decode. At construction (lines 62-77) it builds `decoderMatrix` (nSamplePoints x 64), one row per point of a fixed Hammer-Aitov grid (`hammerAitovSampleX/Y/Z`): `SHEval(7, point, doEncode=false)` x `sn3d2n3d[64]`, then `*= 1/decodeCorrection(7)` to undo the 7th-order correction (so rows are the orthonormal SH x sqrt(2n+1), i.e. the SN3D input is converted to N3D at the matrix; the weights handle order scaling).
- Per block (lines 118-165): `workingOrder = min(channels-based order, input order)`.
  ```
  weights = copyMaxRE(workingOrder)                          // ALWAYS max-rE (line 149)
  weights *= maxRECorrection[workingOrder] * decodeCorrection(workingOrder)   (150-152)
  if N3D input: weights *= n3d2sn3d                           (155-156)
  sampled(point) = sum_ch decoderMatrix(point,ch)*weights[ch]*input[ch]
  rms[point] = exp-smoothed RMS of sampled over each block    (169-180)
  ```
  `maxRECorrection` = RUMS-toolbox `n / sum(maxrE(N,true))` (MaxRE.h:241-251, differs from the decoder's `maxRECorrectionEnergy`).
- There is NO option to turn the weights off: the picture always shows a max-rE decode of the signal, over a continuous sphere grid, with ideal (continuous) panning.
- Screen mapping: `HammerAitov::sphericalToXY` (resources/HammerAitov.h:34-45): +az goes to screen LEFT (`x = -cosEle*sin(az/2)*factor`), +el up.

### Why it can look right while a speaker decode sounds wrong
1. It never uses a loudspeaker layout, VBAP, imaginary speakers, nor the per-layout matrix normalisation (`/maxGain`); it samples a smooth continuous grid. A wrong speaker matrix, wrong speaker-channel mapping, wrong layout azimuth sign, missing imaginary speakers or order mismatch can all be invisible there.
2. It always applies max-rE and a max-rE energy correction. If another decoder omits weights (or double-applies them), the visualiser still shows a tight, correct blob.
3. It is insensitive to loudspeaker output routing (channel order) and layout azimuth sign.
4. Its only convention dependency is its own `useSN3D` switch plus the fixed sign convention of SHEval; any encoder/decoder pair that is wrong only in a way that cancels between them, or that is wrong only in speaker-side gain/matrix handling, cannot show in it. It also displays level (RMS), not phase, so polarity errors between channels in a speaker feed are not visible.

## 4. Real SH, ACN 0..15, SN3D, IEM convention

Definitions: phi = azimuth (positive left, CCW from above, 0 = front), theta = elevation (positive up).
`x = cos(theta) cos(phi)`, `y = cos(theta) sin(phi)`, `z = sin(theta)`.
ACN = n^2 + n + m. No Condon-Shortley phase. SN3D (Schmidt semi-normalised): `Y_SN3D = Y_N3D / sqrt(2n+1)`.
Derived directly from efficientSHvanilla.cpp:26-110 coefficients x sqrt(4 pi) / sqrt(2n+1) (checked: e.g. ACN6 0.9461747 x 3.5449077 = 3.3541 = (3/2) sqrt5, /sqrt5 = 1.5 -> `(1/2)(3z^2-1)` after factoring).

| ACN | n,m | SN3D (cartesian) | SN3D (phi, theta) | IEM code |
|---|---|---|---|---|
| 0 | 0,0 | 1 | 1 | pSH[0] |
| 1 | 1,-1 | y | cos(theta) sin(phi) | pSH[1] |
| 2 | 1,0 | z | sin(theta) | pSH[2] |
| 3 | 1,1 | x | cos(theta) cos(phi) | pSH[3] |
| 4 | 2,-2 | sqrt(3) x y | (sqrt3/2) cos^2(theta) sin(2phi) | pSH[4] = 0.54627 * (2xy) |
| 5 | 2,-1 | sqrt(3) y z | (sqrt3/2) sin(2theta) sin(phi) | pSH[5] = 1.09255 * z*y |
| 6 | 2,0 | (1/2)(3z^2 - 1) | (1/2)(3 sin^2(theta) - 1) | pSH[6] |
| 7 | 2,1 | sqrt(3) x z | (sqrt3/2) sin(2theta) cos(phi) | pSH[7] |
| 8 | 2,2 | (sqrt3/2)(x^2 - y^2) | (sqrt3/2) cos^2(theta) cos(2phi) | pSH[8] = 0.54627*(x^2-y^2) |
| 9 | 3,-3 | sqrt(5/8) (3x^2 y - y^3) | sqrt(5/8) cos^3(theta) sin(3phi) | pSH[9] (S2 = 3x^2y - y^3) |
| 10 | 3,-2 | sqrt(15) x y z | (sqrt15/2) sin(theta) cos^2(theta) sin(2phi) | pSH[10] = 1.4453 z (2xy) |
| 11 | 3,-1 | sqrt(3/8) y (5z^2 - 1) | sqrt(3/8) cos(theta) sin(phi) (5 sin^2(theta) - 1) | pSH[11] |
| 12 | 3,0 | (1/2) z (5z^2 - 3) | (1/2) sin(theta)(5 sin^2(theta) - 3) | pSH[12] |
| 13 | 3,1 | sqrt(3/8) x (5z^2 - 1) | sqrt(3/8) cos(theta) cos(phi) (5 sin^2(theta) - 1) | pSH[13] |
| 14 | 3,2 | (sqrt15/2) z (x^2 - y^2) | (sqrt15/2) sin(theta) cos^2(theta) cos(2phi) | pSH[14] |
| 15 | 3,3 | sqrt(5/8) (x^3 - 3 x y^2) | sqrt(5/8) cos^3(theta) cos(3phi) | pSH[15] (C0 = x^3-3xy^2) |

N3D values: multiply the row by sqrt(2n+1): orders 0,1,2,3 -> x1, x sqrt3, x sqrt5, x sqrt7.

Quick numeric check at phi = 0, theta = 0 (front, x=1,y=0,z=0), SN3D ACN0..15:
[1, 0, 0, 1,  0, 0, -0.5, 0, 0.8660,  0, 0, 0, 0, -0.6124, 0, 0.7906]
(ACN6 = -1/2, ACN8 = sqrt3/2, ACN13 = -sqrt(3/8), ACN15 = sqrt(5/8)).

Quick numeric check at phi = 90 deg (left), theta = 0: x=0, y=1, z=0 -> ACN1 = 1, ACN4 = 0, ACN6 = -0.5, ACN8 = -0.866, ACN9 = -0.7906 (sin 270deg = -1 times sqrt(5/8)), ACN11 = -0.6124.

## 5. Things worth comparing against SpatialCore (observations only)

1. Axis convention: IEM is x-front, y-left, z-up, azimuth positive to the LEFT. If SpatialCore uses azimuth positive to the right (clockwise), ACN1 (Y) and all odd-in-sin(phi) terms (ACN 1,4,5,9,10,11) flip sign. AmbiX is exactly this (x front, y left, z up) so "pin to AmbiX" matches IEM.
2. SN3D vs N3D: IEM encodes SN3D by default; `sqrt(2n+1)` per order is the only difference. A decoder designed for N3D fed SN3D (or vice versa) gives per-order level errors that max-rE can mask or exaggerate.
3. IEM's encoder has NO per-order weighting. If SpatialCore applies max-rE in the encoder, (a) a plane wave is no longer a pure Y_nm, (b) a decoder that also applies max-rE (IEM default) would apply it twice.
4. IEM AllRADecoder defaults to max-rE (+ energy correction) on the DECODE side only, and its matrix is unit-normalised so the worst-case speaker-direction L2 gain is 1.
5. IEM decoder matrices expect N3D; SN3D input is converted by `sqrt(2n+1)` at the input.
