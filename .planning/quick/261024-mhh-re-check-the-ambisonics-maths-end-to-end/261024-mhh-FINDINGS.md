# Findings: Ambisonics re-check (SpatialCore#32), 2026-10-24

## Verdict

The heard fault (20-speaker ring, 6th order, IEM AllRADecoder, REAPER; back-left and back-right wrong in different ways) is most likely OpenSpatialDelay's AU build. The AU cannot take a 50-channel output (`auval` FAIL on dev.9 and dev.14: "Cannot Set Output Num Channels:50 when unit says it can"). OSD then silently falls back to 9.1.6 while the UI still says 6th order. The show projects load OSD as AU. VST3 is unaffected. Tracked in openspatialdelay#22.

## SpatialCore maths verified

- `evalSH` against an independent scipy reference, all 49 channels: max error 1.1e-6.
- The IEM AllRADecoder port was validated to 0.002 degrees.
- SpatialCore's 6th-order output lands within 0.02 degrees on the ring, with no left/right asymmetry.
- Tests: c73dba2 and c292b47.

## Two SpatialCore defects

They degrade the image but do not move it.

- Inverted NFC shelf: low-frequency gain is r/1.5 (+10.5 dB at 5 m, +16.5 dB at 10 m). Theory says far sources should lose bass, and there is no per-order power. Dates from 24e3185.
- The encoder-side max-rE weighting is applied twice, because IEM decoders apply it again.

Planned fix: plain AmbiX SN3D output (SpatialCore#32).

## Ruled out

Truncation (track widths), a mirrored speaker table, OSD position/azimuth conversion, the plus/minus 180 wrap, channel order.

## Also found

- OSD v1.0.0 has wrong SN3D constants for orders 4-6.
- The OSD wiki has the azimuth sign backwards in 5 places.
- The licence claim was corrected (commits 9a6e8c8 and 041bde1).

## Research files

All in `research/` (see `research/README.md`):
[iem-ambisonics-math](research/iem-ambisonics-math.md), [evalsh-independent-check](research/evalsh-independent-check.md), [osd-ambi-path](research/osd-ambi-path.md), [nfc-check](research/nfc-check.md), [ring-sim](research/ring-sim.md), [ring-sim-truncation](research/ring-sim-truncation.md), [osd-bus-negotiation](research/osd-bus-negotiation.md), [UNTESTED-partial-plain-ambix-fix.diff](research/UNTESTED-partial-plain-ambix-fix.diff) (reference only).
