# OSD Ambisonics path audit (read-only investigation, 2026-10-24)

## Checkouts
- OSD v1.0.0 tag = e3eb766 (Apr 17 2026), repo https://github.com/AndrewRahman/openspatialdelay.git.
  Tag is reachable from every worktree; no SpatialCore submodule at v1.0.0. It has its OWN inlined
  Ambisonics (Source/PluginProcessor.cpp, 6434 lines). Only submodule is JUCE.
- /Users/andrewrahman/conductor/workspaces/openspatialdelay/beijing-v5 and .../resume-session
  (branch AndrewRahman/resume-session, 0da193a, May 6): main lineage. Source/PluginProcessor.cpp is
  byte-identical to v1.0.0 (git diff v1.0.0 HEAD -- Source touches only PluginEditor.* and PluginProcessor.h, +203 lines).
  Same DSP as v1.0.0, no SpatialCore.
- DEV: /Users/andrewrahman/conductor/workspaces/openspatialdelay-v1/ljubljana
  (branch AndrewRahman/hrir-crossfade-blocks, e4ee3cf, Oct 24 2026; 512 commits past v1.0.0).
  Uses SpatialCore submodule pinned at 4efd69e (branch osd-234-sadie-onset; this commit is NOT in the
  trenton repo history, but src/Engine/RenderEngine.cpp and src/Core/SpatialMath.cpp in the pin are byte-identical to trenton HEAD 73bdc72 for the Ambi path).
  Dev PluginProcessor.cpp is 4272 lines (render code moved to RenderEngine).
  Also: openspatialdelay-v1/hrir-crossfade-blocks is a second checkout of the same branch (same e4ee3cf).
- Bumped tag "v1.0.6" (ee1867b) is an older Mar 30 commit, not a release newer than v1.0.0.

## 1. Position -> az/el (identical convention in v1.0.0 and dev; no bug found)
v1.0.0 (tmp extract /tmp/osd100/Source = git archive v1.0.0):
- Param: PluginProcessor.cpp:466-470 azimuth NormalisableRange(-180,180,0.1), no skew, default 0.
  Elevation -90..90 (473), distance 0..1 (479).
- Map UI: PluginEditor.cpp:846-857 spatialToPixel x = cx - sin(az)*d*r, y = cy - cos(az)*d*r  => +az = LEFT, 0 = up/front.
  pixelToSpatial PluginEditor.cpp:869 az = atan2(-dx,-dy). Round trip consistent, back half (|az|>90) handled correctly, range (-180,180].
  Drag writes param through objectPositionChanged PluginEditor.cpp:3241-3245 (convertTo0to1).
- Knob: PluginEditor.cpp:1708 range -180..180; setReversed(true) 2387 (display only).
- Global tap offset: PluginEditor.cpp:3255-3295 adds delta then wrapAzimuth (TrajectoryEngine.h:8) -> [-180,180]; delta via unwrapAzimuthDelta (h:16).
- Trajectory: TrajectoryEngine.cpp:73-78 / 451 wrap to [-180,180]; Cartesian shapes use map x=sin(az), y=cos(az), az=atan2(x,y) (e.g. 197-205): self-consistent with +az=left on map? NOTE x=sin(az) here is only an internal frame, round trips to the same az.
- ADM-OSC: PluginProcessor.cpp:5481-5486 azDeg = atan2(-x, y) (ITU BS.2127: +x right => negative az). Consistent with +az left.
- Audio thread read: PluginProcessor.cpp:4497-4509 (trajectory.getFinalAz or APVTS raw), degrees -> radians at 5284-5285, straight into evalSH.
- Dev: same lines at ljubljana Source/PluginProcessor.cpp:2896-2906, passed as degrees in RenderSources 3199; engine converts RenderEngine.cpp:1686-1690. SpatialCore map UI SpatialMapComponent.cpp:105,121 and ADMOSCReceiver.cpp:31 are the same formulas.

## 2. Smoothing
No smoothing of azimuth anywhere. Block-rate position, SH COEFFICIENTS are linearly interpolated per sample between
previous and current block (v1.0.0 PluginProcessor.cpp:5300-5352; engine RenderEngine.cpp:1708-1745). Interpolating coefficients, not degrees,
so no long-way-round across +-180. Trajectory is ticked per block on the audio thread (4458-4483), no UI-rate stair-step.

## 3. Ambisonics output
- Bus: PluginProcessor.cpp:1790-1797 declares output discreteChannels(50) (stereo in Ableton). isBusesLayoutSupported 2448-2457: VST3 accepts ANY layout
  (REAPER gives track channel count); AU rejects discrete (comment: use VST3 for Ambisonics).
  Format registry 74-79: FOA 4, SOA 9, HOA 16, 4OA 25, 5OA 36, 6OA 49. Dev: same, ljubljana Source/PluginProcessor.cpp:876.
- Fallback hazard: maxBusChannels = getTotalNumOutputChannels() at prepare (v1.0.0 :2943). resolveEffectiveFormat (dev :1655-1695, v1.0.0 equivalent)
  returns a NON-Ambisonics surround format if bus channels < format.requiredChannels ("Never fall back to Ambisonics").
  So an Ambi format selected on a track narrower than (order+1)^2 silently becomes discrete surround with fixed speaker channel assignments (routing-like). Check track channel count/order.
- Channel write: ACN order, outChannels[c][s] = ambiAccum[c] for c < (order+1)^2 (v1.0.0 :5359-5362; engine RenderEngine.cpp:1748-1752). No remap table, no N3D/SN3D option.
- SH: evalSH ACN/SN3D, az 0 front, +az left (ACN1 = sinAz*cosEl, ACN3 = cosAz*cosEl). Correct through order 3.
- v1.0.0 BUG ORDERS 4-6: inlined evalSH (PluginProcessor.cpp:3361-3455) has wrong SN3D constants for 22 of 33 ACNs 16-48
  (SpatialCore fixed in db9731a "correct 22 SN3D constants at orders 4-6"). Measured old/new ratio: acn16,24 x3; 25,35 x4; 26,34 x3; 27,33 x2.83; 28,32 x1.41; 36,48 x5.66; 37,38,46,47 x4; 39,45 x4.9; 40,44 x1.41; 41,43 x2; others OK.
  Dev (SpatialCore pin) has corrected constants. Only matters if the user ran 4OA/5OA/6OA on v1.0.0.
- max-rE: ALWAYS ON in the encoder, w_n = cos(n*pi/(2N+2)) (v1.0.0 :5231-5237, :5287; engine :1612-1620, 1690). No option. Order 3: 1, .924, .707, .383.
  If the IEM AllRADecoder also has max-rE/order weighting enabled, it is applied twice.
- NFC-HOA: ALWAYS ON for orders 1..N (v1.0.0 :5240-5278, :5342-5351; engine :1622-1665, 1722-1739). Per-source first-order shelf per order, same filter for every m,
  distance = param*10 m, reference radius 1.5 m, smoothing alpha 0.15 per block. DC gain = fZero/fPole = r/1.5 (r=5 m default distance 0.5 -> +10.5 dB, r=10 m -> +16.5 dB on every channel of order>=1 relative to W, below ~100-200 Hz; unity above).
  Level direction looks inverted from physical NFC (far source gets LF boost). Different taps at different distances get different LF level. Non-standard for a consumer feeding an IEM decoder.
- Dry: mono dry sum goes to W (ACN0) only (v1.0.0 :4794-4801; dev :3343-3349). Wet is limited with outputLimiter (tanh) on all channels.
- Dev does NOT set engineComputesGains / engineDerivesDispatch / engineSelectsHRTF (blockCtx at dev :3205-3211 sets only activeFormat/isAmbiOutput/ambiOrder etc.). Ambi path needs only ambiOrder, so these flags are not a problem here.

## 4. Delay taps
No per-tap spread/wobble/rotation on azimuth. Wobble (applyWobble :4175) modulates DELAY time only. Each tap has its own azimuth, plus:
- tap N delay = (N+1) * baseDelay (readObjectSample :4206-4224), per-tap Doppler delay accum, per-tap pitch shift (PV), per-tap filter bank, air absorption by distance (FilterBank), per-tap input channel L/R/Mono (:4231-4237).
- Default positions: PluginProcessor.cpp:1859-1866 initAz = {-45, 45, -135, 135, 0, 90, -90, 180, -30, 30, -60, 60}. Factory preset "Quad Ping-Pong" (PresetData.cpp:76-90, default preset) uses the same four.
- LABEL TRAP: PresetData.cpp comments call az -135 "rear-left", +135 "rear-right", -45 "front-left" etc. Real behaviour is +az = LEFT (map PluginEditor.cpp:853, SH ACN1 sign, ADM-OSC). So tap 3 (-135, 3x delay, e.g. 1.5 s at 500 ms) is actually REAR-RIGHT and tap 4 (+135, 4x delay, 2.0 s) is REAR-LEFT. The two back sources are different echoes at different times, with feedback accumulating on later ones. "Different in each way" and "swapped" impressions fit this without any encoder bug.

## 5. Other
- No azimuth LUT, no L/R mirror in the Ambi path. Stereo-variant math uses -sin(az) (dev :3226-3257) but that is not the Ambi path.
- Block with channels beyond (order+1)^2 are not written by the engine; the post-stage multiplies them by wet gain and limiter (v1.0.0 :4833-4838) so stale buffer content in extra channels would pass through if the host leaves any (50-ch declared bus, JUCE VST3 normally zeroes outputs beyond inputs; unverified in REAPER).
- Elevation default 0 so sinEl terms vanish; no el wrap issue.

## Ranking
1. Tap geometry/label inversion + per-tap delay multiples/feedback (PresetData.cpp:76-90, PluginProcessor.cpp:1859-1866, :4206). Not a code bug but explains asymmetric back-left/back-right behaviour.
2. v1.0.0 only: wrong SN3D orders 4-6 (PluginProcessor.cpp:3424-3455) if run at >=4OA.
3. Format fallback when REAPER track narrower than ambi channel count (dev :1655-1695, v1.0.0 :2943-2946).
4. Always-on NFC (distance dependent LF level on orders>=1) and always-on max-rE, no UI to disable.
5. Dry only into W.
Cleared: x/y<->az formulas, wrapping, normalisation, smoothing, bus channel declaration, channel order, mirror/flip.
