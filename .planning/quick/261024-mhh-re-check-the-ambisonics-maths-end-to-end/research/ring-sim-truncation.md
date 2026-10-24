# Ring sim follow-up: channel-count truncation and OSD's silent surround fallback

Investigation only, nothing edited in any repo. Scripts and raw output in /tmp/ringsim/: trunc.py (A and C, out_trunc.txt), spkgains.cpp + spk.txt (B speaker feeds), fallback.py / fallback2.py (B, out_fallback.txt, out_fallback2.txt). Decoder is the validated IEM AllRADecoder port from ring-sim.md (20-speaker ring, order 6, maxrE, imaginary gain 0); D_flip.npy is the same decoder built with the speaker azimuths negated.
Speaker feeds in B come from the REAL SpatialCore code: I linked libSpatialCore.a, built the 9.1.6 layout with `getLayoutDef(S9_1_6)`, `buildVBAPTripletsForLayout` + `appendLowerHemisphereTriplets`, and called `VBAPAlgorithm` / `ConstantPowerAlgorithm::computeGains` at el 0, az -180..180, then placed each gain at its speaker `channelIndex` (exactly what `RenderEngine` does at RenderEngine.cpp:1786-1798).

## Verdict
- A (pure truncation to K channels): does NOT reproduce it. Every K is exactly left/right mirror symmetric (max |err(az)+err(-az)| 0.0 deg, gain mirror asymmetry 0.1 percent = float noise). Worst direction error is 4.8 deg (IEM plain, K=20/24) or 2.9 deg (SpatialCore dev, K=20/24); K=16/25/36/49 are 0.0. Back-left and back-right are identical to each other. The premise "a partial order breaks mirror symmetry" does not hold here: ACN 16-19 are m=-4..-1, but on a horizontal source only m=-4 and m=-2 are non-zero, and the sin-only m=-4 term gives an error that is odd in az, so the mirrored source gets the mirrored error.
- B (OSD silent fallback to 9.1.6, discrete feeds misread as ACN): DOES reproduce "back-left and back-right wrong in different ways, like a routing problem", and it is OSD-only by construction. It breaks nearly every direction, not only the backs (front centre and hard right are silent with VBAP).
- C (decoder speaker table mirrored): mirrors every plugin identically, cannot be OSD-only. Ruled out as the OSD-specific cause.
- OSD shows NO warning for the fallback (details at the end). One indirect tell: the Algorithm box stops saying "Ambisonics Encode" (greyed) and becomes an active surround algorithm list.

## A. Truncation to K channels (zeros above), grid = 1 deg, az -180..180
Direction = energy-vector azimuth minus true azimuth. Grid max |dir err| in deg, FL/FR = az in (0,90] / [-90,0), BL/BR = az in (90,180) / (-180,-90). "asym" = max over az of |err(az)+err(-az)|; "gain asym" = max mirrored speaker-gain difference as percent of the loudest gain.

| K | enc | FL | FR | BL | BR | asym deg | gain asym | min abs rE |
|---|---|---|---|---|---|---|---|---|
| 16 | IEM plain | 0.0 | 0.0 | 0.0 | 0.0 | 0.0 | 0.1 pct | 0.891 |
| 16 | SC dev | 0.0 | 0.0 | 0.0 | 0.0 | 0.0 | 0.1 pct | 0.911 |
| 20 | IEM plain | 4.8 | 4.8 | 4.8 | 4.8 | 0.0 | 0.1 pct | 0.888 |
| 20 | SC dev | 2.9 | 2.9 | 2.9 | 2.9 | 0.0 | 0.1 pct | 0.910 |
| 24 | IEM plain | 4.8 | 4.8 | 4.8 | 4.8 | 0.0 | 0.1 pct | 0.888 |
| 24 | SC dev | 2.8 | 2.8 | 2.8 | 2.8 | 0.0 | 0.1 pct | 0.910 |
| 25 | IEM / SC dev | 0.0 | 0.0 | 0.0 | 0.0 | 0.0 | 0.1 pct | 0.937 / 0.950 |
| 32 | IEM plain | 2.2 | 2.2 | 2.2 | 2.2 | 0.0 | 0.1 pct | 0.936 |
| 32 | SC dev | 0.8 | 0.8 | 0.8 | 0.8 | 0.0 | 0.1 pct | 0.950 |
| 36 | IEM / SC dev | 0.0 | 0.0 | 0.0 | 0.0 | 0.0 | 0.1 pct | 0.963 |
| 40 | IEM plain | 0.7 | 0.7 | 0.7 | 0.7 | 0.0 | 0.1 pct | 0.963 |
| 40 | SC dev | 0.1 | 0.1 | 0.1 | 0.1 | 0.0 | 0.1 pct | 0.963 |
| 48 | IEM plain | 0.6 | 0.6 | 0.6 | 0.6 | 0.0 | 0.1 pct | 0.963 |
| 48 | SC dev | 0.1 | 0.1 | 0.1 | 0.1 | 0.0 | 0.1 pct | 0.963 |
| 49 | IEM / SC dev | 0.0 | 0.0 | 0.0 | 0.0 | 0.0 | 0.1 pct | 0.974 / 0.965 |

At the requested azimuths (IEM plain, K=20): err 0.0 at az 0, +-45, +-90, +-135, 180; +4.0 at +150 (heard 154.0), -4.6 at +170 (heard 165.4), mirrored exactly at -150 and -170. Only |rE| drops (0.97 to 0.89). Truncation is a small, symmetric, direction-preserving blur. Raw rows for every K: out_trunc.txt.
Side notes: REAPER track widths are even, so K=25 and K=49 cannot occur as a track width (the real widths are 26 and 50). A 20-channel track in front of a decoder is plausible, and would sound essentially fine for IEM encoders, which is consistent with "other plugins were fine".

## B. OSD fallback (what the code does)
Reading, dev repo /Users/andrewrahman/conductor/workspaces/openspatialdelay-v1/ljubljana (v1.0.0 at e3eb766 has the identical logic at PluginProcessor.cpp:2772-2810 and :2946):
- Bus width is read in prepareToPlay: `maxBusChannels = getTotalNumOutputChannels();` Source/PluginProcessor.cpp:1820, then `resolveEffectiveFormat (userFormat, maxBusChannels)` :1823, `renderEngine.setOutputFormat (effectiveFormat)` :1830.
- `resolveEffectiveFormat` (:1656-1695): if the requested format needs more channels than the bus (:1676 `if (busChannels >= info.requiredChannels)` else `break`), it falls through to "find the surround format with the most channels that fits the bus", skipping Ambisonics (:1688-1689) and using a strict `>` so the first best wins (:1690).
- Registry (SpatialCore src/IO/OutputFormatRegistry.cpp): the largest non-Ambisonics format is 9.1.6 at 16 channels (7.1.6 / 9.1.4 / SML are 14). So any bus width from 16 to 48 (including 20, 24, 32) resolves to Surround9_1_6; widths 49 and up keep 6OA. Widths 4..15 pick smaller surround formats.
- Layout, from SpatialCore src/IO/SpeakerLayout.cpp:46 (printed by the harness): speaker -> channel: ch0 az +30, ch1 -30, ch2 0, ch3 = LFE, ch4 +90, ch5 -90, ch6 +135, ch7 -135, ch8 +60, ch9 -60, ch10 +45 el 45, ch11 -45 el 45, ch12 +90 el 45, ch13 -90 el 45, ch14 +135 el 45, ch15 -135 el 45. Channels 16..(width-1) are written as silence (buffer.clear, PluginProcessor.cpp:3041). Positive azimuth = left. Ch3 carries a 120 Hz low-passed mono sum at 0.316 (RenderEngine.cpp:1796-1798).
- Algorithm: OSD indexes `algorithms[configAlgorithm]`, the editor's default for surround is Constant Power ("default for surround", PluginEditor.cpp:2375-2376, and :2402 resets an out-of-range index to 1 = Constant Power), VBAP only if the user picked it. I computed both.
- Misread as ACN: ch c lands in ACN c. Ring visibility of those ACNs in a horizontal 6OA decode (unit input, dB re ACN0): ACN2 -62, ACN5 -64, ACN7 -63, ACN10 -64, ACN12 -62, ACN14 -64 (all vertical, invisible on a horizontal ring); ACN11 and ACN13 -20.5; ACN 1,3,4,8,9,15 about +5; ACN6 -4. So centre (ch2), -90 (ch5), -135 (ch7) feeds are nearly dead and +135 (ch6) is audible but is a z-squared term (a broad, non-directional bump).

Results (decoder gain level in dB re a correct 6OA feed at az 0; "loud spk" = azimuth of the loudest ring speaker; full tables in out_fallback2.txt):

VBAP feeds, LFE off (the decisive back-corner comparison):
| src az | feed ch | level dB | loudest ring spk az | abs rE |
|---|---|---|---|---|
| 0 | ch2 1.00 | -72.4 | -54 | 0.44 |
| +45 | ch0+ch8 0.71 each | -6.5 | 180 | 0.00 |
| -45 | ch1+ch9 | -5.4 | 144 | 0.00 |
| +90 | ch4 | -4.6 | 126 | 0.00 |
| -90 | ch5 | -74.1 | 90 | 0.13 |
| +135 (back-left) | ch6 | -14.4 | 162 | 0.00 |
| -135 (back-right) | ch7 | -73.5 | -18 | 0.57 |
| +150 / -150 | ch6 0.97 / ch7 0.97 | -14.7 / -26.1 | -162 / -18 | 0.00 / 0.00 |
| +170 / -170 | ch6+ch7 | -16.1 / -19.2 | -162 / -162 | 0.00 / 0.00 |
| 180 | ch6+ch7 0.71 | -17.4 | -162 | 0.00 |

Back-left (+135) comes out at -14 dB, diffuse (rE about 0), mostly on the left/rear speakers; back-right (-135) comes out at -73.5 dB (silent), so the two corners differ by 59 dB. Level asymmetry max over all az: 69.6 dB (VBAP). Only 80 of 361 azimuths have their loudest speaker within 20 deg of the true azimuth. With the LFE feed added (LF only) the picture changes below 120 Hz only: sources collapse toward the rear, +135 vs -135 differ by 3.6 dB.

Constant Power (OSD preset default), LFE off: level -7.4 to -12.6 dB at every az, +135 vs -135 1.3 dB, but directions scrambled: az 0 heard at about +90 (rE 0.27), +45 at -20 (rE 0.10), +-135 diffuse (rE 0.00), +150/+170 heard near -90 (rE 0.23-0.27), -150/-170 heard near +1 (rE 0.005). Only 85 of 361 az have the loudest speaker within 20 deg. Back-left and back-right are both wrong, in different directions (+150 goes to -90, -150 goes to +1), levels within 2.6 dB. (Max level asymmetry 2.6 dB.)

So: either algorithm gives "back-left and back-right wrong in different, non-mirrored ways"; VBAP makes one back corner vanish, Constant Power makes both audible but misplaced. In neither is the image mirrored.
Caveat for the user report: this also wrecks front centre and the sides, so it fits only if the user did not judge those (or judged them by a source that happened to land well). If front, left and right were verified correct, the fallback is ruled out.
Why "other plugins were fine" is still consistent: IEM encoders have no format fallback, they just fill however many channels the track has (scenario A, symmetric and under 5 deg), so on a 16-48 channel track IEM sounds fine while OSD silently becomes a 9.1.6 speaker feed. (I did not verify IEM's own order reduction, only the decoder side.)

## Does OSD warn about the fallback? No, nothing explicit.
- The output dropdown keeps showing the user's selection (6th Order Ambi): `configOutputFormat` is not touched by `resolveEffectiveFormat`; PluginEditor.cpp:918-919 and :2065 set the box from `configOutputFormat`. The only effect on the box is that unavailable items are disabled in the list: PluginEditor.cpp:2289-2297 (`outputFormatBox.setItemEnabled (i + 1, available)`), which does not touch the selected, already-displayed item.
- No label, text, tooltip or colour for it: grep of PluginEditor.cpp for fallback, narrow, bus text returns only the lines above; the word "fallback" appears only in a processor code comment (PluginProcessor.cpp:1675).
- The one visible tell: the Algorithm box. In true Ambisonics output it is greyed with text "Ambisonics Encode" (PluginEditor.cpp:2336-2343), but under the fallback `isAmbiOutput` (computed from the ACTIVE format, :2314-2315) is false, so the box is enabled and lists Constant Power / Ambisonics / DBAP / KNN / MDAP / VBAP / VBIP (:2376-2383). Easy to miss.
- A related bug: the timer re-resolve at PluginEditor.cpp:2305 `if (lastMaxBusChannels != processorRef.getMaxBusChannels())` can never fire, because `lastMaxBusChannels` was assigned from the same value at :2291 just above. So the format is re-resolved only in prepareToPlay (:1823) and on a dropdown change (:927), not on a live bus-width change from the timer. REAPER normally calls prepare again on a pin or track-width change, so this probably does not matter, but a stale effective format after a width change is possible if the host does not.
- Also relevant: the processor's default output bus is `discreteChannels (50)` (PluginProcessor.cpp:877-879) and VST3 accepts any layout (:1575-1580), so the width OSD ends up with is whatever REAPER's pin connector gives it. A plugin pin config narrower than the track would trigger the fallback silently.

## C. Speaker table azimuth sign flipped in the decoder (correct feeds)
Same decoder built with speaker azimuths -18k deg, correct IEM plain 6OA feed. Result: every source is heard mirrored, exactly: src +45 -> -45.0, +90 -> -90.0, +135 -> -135.0, +150 -> -150.0, +170 -> -170.0, 0 -> 0.0, 180 -> 180.0 (|rE| 0.974 throughout). Front and back centre are unchanged; left and right swap for every source and every plugin on that decoder. It cannot make back-left and back-right wrong "in different ways" and cannot be OSD-only, so it is ruled out unless other plugins were also not checked for L/R. (If the table were wrong only in channel order rather than sign, the result would again be common to all plugins.)

## What to ask the user / next check
1. Channel count of the OSD output pin in REAPER (the OSD UI, Output box vs Algorithm box: is the Algorithm box greyed "Ambisonics Encode" or an active list?). Active list = fallback. Fastest single check.
2. If it is the fallback: widen the REAPER track and the OSD pin to 50 channels (or pick a lower order that fits).
3. If the Algorithm box says "Ambisonics Encode" (no fallback): the earlier ring-sim.md verdict stands, the maths is correct, and the cause is outside OSD's encoder (tap labelling, +az = left, decoder speaker table).
