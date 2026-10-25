# Research files for quick task 261024-mhh (SpatialCore#32)

Copied unchanged from the local, git-ignored `.context/` on 2026-10-24.

- `iem-ambisonics-math.md` - how IEM encoders and the AllRADecoder do their Ambisonics maths, and the validated port.
- `evalsh-independent-check.md` - SpatialCore `evalSH` compared with an independent scipy reference, all 49 channels.
- `osd-ambi-path.md` - how OpenSpatialDelay reaches the Ambisonics output (position, azimuth, channel order).
- `nfc-check.md` - the near-field shelf in `renderAmbisonicsOutput` checked against Daniel 2003 (it is inverted).
- `ring-sim.md` - simulated 20-speaker ring, 6th order: where SpatialCore's output lands.
- `ring-sim-truncation.md` - simulation of channel truncation (track widths) as a cause; ruled out.
- `osd-bus-negotiation.md` - the AU 50-channel output failure and the silent fall back to 9.1.6 (openspatialdelay#22).
- `UNTESTED-partial-plain-ambix-fix.diff` - an interrupted, unbuilt, untested attempt at the plain AmbiX fix. Reference only; never applied. The diff itself is unmodified.
