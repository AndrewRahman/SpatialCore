# Phase 3 Plan 11: Outward items for approval

Drafted 2026-10-04. Nothing in this file has been posted. Items A to C are public text; item D is a
test setting that changes no sound. Every number below was taken from the Plan 03-01, 03-03, 03-04,
03-08 and 03-09 SUMMARY files and re-read from a fresh run of the same tests on the current tree
(Debug); the loudness and cue numbers are identical in Debug and Release.

Numbers that differ from the plan's wording or from the research draft are listed at the end under
"Differences to be aware of".

## A. Comment on Spatial-Media-Lab/OpenSpatialDelay#234 (the issue stays open)

**If approved:** a comment is added to the OpenSpatialDelay issue "Audio artifacts at buffer sizes
below 256 samples", saying SpatialCore's side (the HRTF convolver) is now proven clean at small and
irregular block sizes, and that OpenSpatialDelay's own delay line and pitch shifter are still
unchecked. The issue is not closed.

````text
Update from the SpatialCore side (SpatialCore Phase 3, BUG-02).

One of the three possible causes listed here was SpatialCore's PartitionedConvolver (the HRTF
rendering). It is now covered by automated tests and fixed where it was not clean:

- The convolver matches a double-precision direct convolution to within 1.8e-7 (worst case across
  runs; the test bound is 2e-6) at block sizes 32, 64, 128, 512, 37 and two irregular block
  sequences. Test: [convolver][blocksize].
- The remaining small-block artifact was that the convolver's IR warm-up and crossfade were counted
  in blocks, so at small blocks a new HRIR faded in over only a few milliseconds. Both are now
  counted in samples (warm-up at least the IR length, crossfade at least 2048 samples). A source
  moving 180 degrees through the engine's HRTF path now has the same largest sample-to-sample step
  at 32, 64 and 128-sample blocks as at 512 (ratio 1.00 to 512 at all three; before the change the
  32-sample step was 3.5 times the 512-sample step). Test: [bug02][moving].
- Engine output at 32, 64, 128, 37 and irregular block plans matches the 512-sample render to
  within 1.9e-5 on the HRTF path and exactly on the Simple path. Test: [bug02][steady].
- The engine's profile-switch crossfade is now sample-based too (at least 4096 samples), so
  switching HRTF profiles is click-free and dropout-free at 32, 64, 128 and 512-sample blocks.

Not checked, and the reason this issue stays open: OpenSpatialDelay's own delay line (the 2048-sample
latency compensation) and its pitch shifter / WSOLA stage at small blocks. Those two remain
candidate causes.

These changes reach OpenSpatialDelay when its SpatialCore submodule pointer is bumped to the
Phase 3 result.
````

## B. New issue in AndrewRahman/SpatialCore (label: bug)

**If approved:** a new issue is opened on SpatialCore about a timing limit in the binaural renderer
that only matters at 88.2 kHz and above (and for the SADIE profile at every rate). It records that
Phase 3 deliberately did not change today's behaviour, so the plugin's sound is unchanged. The test
that locks today's behaviour then cites the new issue number.

Title:

````text
BinauralRenderer: 64-sample ITD delay line wraps absolute onset delays (SADIE at all rates, every profile at 88.2 kHz and above)
````

Body:

````text
## What

`BinauralRenderer` delays each ear with an interaural-time-difference (ITD) line that is 64 samples
long: `kITDBufferSize = 64` in `include/SpatialCore/Binaural/BinauralRenderer.h`, read index masked
with `& (kITDBufferSize - 1)` in `src/Binaural/BinauralRenderer.cpp`. `getAlignedHRIR` returns each
ear's absolute onset delay, not the interaural difference. Any absolute delay of 64 samples or more
wraps (is taken modulo 64), so the delay actually applied is not the delay in the data.

## Measured maxima (direction sweep, absolute onset delay in samples)

| Profile | 48 kHz | 96 kHz |
|---|---|---|
| SADIE | 123 | 245 |
| KEMAR | 63 | 126 |
| CIPIC | 58 | not measured |
| HUTUBS | 50 | not measured |
| Bernschuetz | 43 | not measured |

(192 kHz: SADIE 490.) Delays scale with the sample rate, so at 88.2 kHz and above every built-in
profile reaches 64 samples or more. KEMAR at 48 kHz is exactly at the limit.

## Status

This is long-standing behaviour of the shipped sound, not a regression. In SpatialCore Phase 3 it is
pinned, not changed (decision D-16): `[renderer][itd-characterisation]` in
`tests/Binaural/BinauralRendererTests.cpp` records the raw delays and rendered onsets for SADIE and
KEMAR at 44.1 and 48 kHz (20 cases) and checks the wrap mechanism (rendered onset = aligned onset +
floor(delay) mod 64, within 1 sample). Shipped timing at 44.1 and 48 kHz is unchanged by Phase 3.

## Scope of the fix

A longer delay line, or delaying by the interaural difference only. Needed for 88.2 kHz and above;
a change to the 44.1 / 48 kHz behaviour is a change to shipped timing and needs its own decision, and
the 20 pinned rows would then be updated on purpose.
````

## C. Close AndrewRahman/SpatialCore#15 with a comment

**If approved:** the issue "Simple-binaural path produces no elevation cue" is closed with a comment
saying it is fixed, with the measured numbers and the tests that prove it. The comment also says
OpenSpatialPanner's test that pins the old behaviour must be inverted after it updates SpatialCore.

````text
Fixed in SpatialCore Phase 3 (BUG-01).

Cause as described above: the Simple (Woodworth) binaural path pans on one lateral value, so
elevation and front/back carry no cue. The fix is in the engine, not in
`DirectBinauralAlgorithm` (that algorithm stays left/right-only and a test pins that): the Simple
path now runs a small position-blended cue bank in `RenderEngine` before the Woodworth pan gains - a
rear head-shadow cut, an upward pinna peak and a downward dip. Values and sources are in
`include/SpatialCore/Core/SimpleBinauralCues.h`.

Measured through the engine impulse response at 48 kHz, third-octave band difference against a
source in front (azimuth 0, elevation 0), RMS dB / max dB:

Simple path ([bug01][simple], test bounds 2.0 RMS and 4.0 max):
- behind (azimuth 180): 4.49 / 11.59 (was 0.00 / 0.00)
- overhead (elevation +90): 2.59 / 7.41

The five built-in HRTF profiles ([bug01][hrtf]) separate front from back and from overhead too:

| Profile | front vs back | front vs overhead |
|---|---|---|
| SADIE | 3.55 / 7.99 | 3.29 / 7.32 |
| CIPIC | 4.80 / 15.12 | 4.59 / 13.03 |
| HUTUBS | 3.39 / 7.89 | 3.91 / 9.63 |
| Bernschuetz | 6.14 / 21.42 | 3.82 / 11.77 |
| KEMAR | 6.30 / 19.01 | 3.59 / 8.61 |

Sound that was already correct is unchanged: every ear-level source in the front half
(|azimuth| <= 90) renders bit-identical to the old formula ([bug01][identity]).

Notes:
- This is an audible change to the Simple profile for sources behind, above or below the listener,
  so OpenSpatialDelay needs a release note for it (pending, tracked on the SpatialCore side).
- Per the "Pinned by" section above, OpenSpatialPanner's `[binaural][sc12]` assertions in
  `Tests/DevFormatTests.cpp` pin the old behaviour. After OpenSpatialPanner bumps its SpatialCore
  submodule they must be inverted, not deleted.
- A headphone listening check of the cue shape is on the roadmap backlog; it is not a gate.
````

## D. Loudness tolerance for the [loudness] test (not public; changes no sound)

**If approved:** the loudness test, which today only prints numbers, starts to fail if a built-in
profile gets louder or quieter than measured today, or if the profiles drift further apart from
each other. No profile's level is changed (D-14).

Measured today (K-weighted loudness per ITU-R BS.1770 of pink noise, 12 directions, 48 kHz; the
same in Debug and Release and under libmysofa v1.3.2 and v1.3.5). 1 LU is about 1 dB:

| Profile | Mean LKFS | Lowest direction | Highest direction |
|---|---|---|---|
| 1 SADIE | -20.83 | -23.74 | -19.23 |
| 2 CIPIC | -19.80 | -22.60 | -17.54 |
| 3 HUTUBS | -20.16 | -24.21 | -18.18 |
| 4 Bernschuetz | -19.34 | -21.64 | -18.28 |
| 5 KEMAR | -18.34 | -22.68 | -16.74 |
| 0 Simple (not comparable: includes its own distance gain) | -29.91 | -31.87 | -27.81 |

Spread among profiles 1-5: **2.48 LU** (SADIE quietest, KEMAR loudest).

Proposal (the text that would go into the test):

````text
Each of profiles 1-5 is pinned to its measured mean loudness within 0.5 LU
(-20.83, -19.80, -20.16, -19.34, -18.34 LKFS), and the spread among profiles 1-5 may be at most
2.98 LU (the measured 2.48 plus 0.5). Simple (profile 0) stays excluded. Comment in the test:
"tolerance approved by the user on 2026-10-04 from the Phase 3 measurement (D-14)". The WARN table is kept.
````

Alternative: keep the test record-only, with a comment saying so by the user's decision.

## Differences to be aware of

1. Item A: the research draft and the plan say the 32-sample moving-source step "was 4.6 times" the
   512-sample step. Re-measured on this tree by the Plan 03-03 test it was 3.49 times (1.25 times at
   64, 1.05 times at 128). The draft above uses the measured 3.5. The research figure came from a
   different sweep and level.
2. Item A: the convolver's worst difference is 1.8e-7 (range 1.5e-7 to 1.8e-7 over the seven block
   plans), not the research's 2e-7 / 2.1e-7.
3. Item B: the maxima for CIPIC, HUTUBS and Bernschuetz at 96 kHz were not measured; the text says
   "not measured" and relies on delays scaling with the sample rate. The 03-01 ITD table holds
   only five directions (SADIE 48 kHz max 102); the 123-sample maximum is the research's full sweep.
4. Item C: Phase 3 is 52 commits ahead of origin and not yet on `main`. The comments name tests and
   files, not commit hashes, so nothing dangles; but closing #15 and commenting on OSD#234 now says
   "fixed" before the code is on `main`.
5. Item D: loudness for Simple reads -29.91 here (it was -29.37 in the Plan 03-01 summary, before
   later plans changed the Simple path).

## Decision (2026-10-04, from the user, per item)

| Item | Decision | Effect |
|---|---|---|
| A (comment on Spatial-Media-Lab/OpenSpatialDelay#234) | Approved as written, **posting deferred** | Not posted. Post only after Phase 3 is pushed to `main`. |
| B (new SpatialCore issue, label `bug`, ITD wrap) | Approved as written | Filed now: AndrewRahman/SpatialCore#25. |
| C (close AndrewRahman/SpatialCore#15 with a comment) | Approved as written, **posting deferred** | Not posted, not closed. Post and close only after Phase 3 is pushed to `main`. |
| D (loudness tolerance) | Approved as written | Bounds added to `tests/Binaural/ProfileLoudnessTests.cpp`. |

D-13 and D-03 closure are therefore "approved, posting deferred to push" (this resolves difference 4
above: the comments say "fixed" only once the code is on `main`). D-16's issue half and D-14's
tolerance half are done.

## Pending after push

Run these only after Phase 3 is pushed to `main`. Texts are the fenced blocks of sections A and C
above, extracted to temporary files (no edits). Run from the repository root.

```bash
F=.planning/phases/03-binaural-defects-hrtf-packaging/03-11-OUTWARD.md
python3 - "$F" <<'PY'
import re, sys
t = open(sys.argv[1], encoding='utf-8').read()
a = t.split("## A. Comment")[1].split("## B. New issue")[0]
c = t.split("## C. Close")[1].split("## D. Loudness")[0]
open('/tmp/03-11-A-body.md', 'w').write(re.findall(r"````text\n(.*?)\n````", a, re.S)[0] + "\n")
open('/tmp/03-11-C-body.md', 'w').write(re.findall(r"````text\n(.*?)\n````", c, re.S)[0] + "\n")
PY

# A: comment on OpenSpatialDelay#234 (do NOT close it; D-13)
gh issue comment 234 --repo Spatial-Media-Lab/OpenSpatialDelay --body-file /tmp/03-11-A-body.md
gh issue view 234 --repo Spatial-Media-Lab/OpenSpatialDelay --json state     # expect OPEN

# C: comment and close SpatialCore#15 (D-03)
gh issue close 15 --repo AndrewRahman/SpatialCore --comment "$(cat /tmp/03-11-C-body.md)"
gh issue view 15 --repo AndrewRahman/SpatialCore --json state                # expect CLOSED
```

After running, append the comment URL and the closed state to the "Done" section below.

## Done

- **B filed:** https://github.com/AndrewRahman/SpatialCore/issues/25 (label `bug`, title and body
  exactly as in section B). Cited in the `[renderer][itd-characterisation]` table comment in
  `tests/Binaural/BinauralRendererTests.cpp` as "Tracked by AndrewRahman/SpatialCore#25".
- **D applied:** profiles 1-5 pinned to -20.83, -19.80, -20.16, -19.34, -18.34 LKFS within 0.5 LU;
  spread among profiles 1-5 at most 2.98 LU; Simple excluded; WARN table kept; comment "tolerance
  approved by the user on 2026-10-04 from the Phase 3 measurement (D-14)".
- **A posted (UAT go-ahead 2026-10-04):**
  https://github.com/Spatial-Media-Lab/OpenSpatialDelay/issues/234#issuecomment-5984074066 -
  issue state OPEN (D-13).
- **C posted (UAT go-ahead 2026-10-04):**
  https://github.com/AndrewRahman/SpatialCore/issues/15#issuecomment-5984074231 - issue state
  CLOSED. It had already been auto-closed at 2026-10-04T20:20:37Z by commit bb8b3d1 when Phase 3
  was pushed to main (no comment then), so only the comment was added; no separate close was needed.
