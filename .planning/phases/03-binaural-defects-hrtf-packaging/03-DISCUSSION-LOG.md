# Phase 3: Binaural Defects & HRTF Packaging - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in CONTEXT.md — this log preserves the alternatives considered.

**Date:** 2026-10-04
**Phase:** 03-binaural-defects-hrtf-packaging
**Areas discussed:** Simple-mode elevation fix, Who switches profiles, Shared-folder rules, Small-block 'done' bar

Pre-answered (not re-asked): resolution chain shared folder → embedded → error; all 5 embedded in
v1 behind `SPATIALCORE_EMBED_ALL_HRTF=ON`; shared folder paths; convert-then-embed rejected
(OQ-6, 2026-08-10).

---

## Simple-mode elevation fix

**Q1: How should Simple mode get height and front/back cues?**

| Option | Description | Selected |
|--------|-------------|----------|
| Add light tone filters | Rear high-cut + pinna-style elevation cue; stays HRTF-free; OSD release note like VBIP | ✓ |
| Front/back only | Rear filter only; criterion 1 stays unmet for Simple | |
| Retire Simple mode into HRTF | Simple renders through embedded KEMAR; more CPU | |
| Leave Simple mode as-is | Simple = left/right only; #15 stays open | |

**Q2: How strong should the new cues be?**

| Option | Description | Selected |
|--------|-------------|----------|
| Subtle | ~3-6 dB, close to today's sound | |
| Pronounced | ~10+ dB, bigger change | |
| You decide | Literature values, judged by listening check | ✓ |

**Q3: Keep a switch back to today's flat Simple sound?**

| Option | Description | Selected |
|--------|-------------|----------|
| No, new sound only | One behaviour, VBIP precedent | ✓ |
| Yes, a 'legacy' flag | Off by default, more code/tests | |

**Q4: What closes SpatialCore#15?**

| Option | Description | Selected |
|--------|-------------|----------|
| Test now, listen later | Automated test on Simple + HRTF; listening check to backlog | ✓ |
| Test + your listening check | Phase gated on headphone approval | |

---

## Who switches profiles

**Q1: Who should own 'switch to profile N'?**

| Option | Description | Selected |
|--------|-------------|----------|
| SpatialCore, one call | Lookup + load + click-free swap in the engine; consumer glue deleted | ✓ |
| SpatialCore loads, OSD swaps | Each plugin repeats swap logic | |
| Keep OSD glue, add helper | Criterion 3 only half true | |

**Q2: Behaviour during a slow load (SADIE 36 MB)?**

| Option | Description | Selected |
|--------|-------------|----------|
| Load in background | Returns instantly; old profile plays, then crossfade | ✓ |
| Load right away, UI waits | Simpler; window may freeze briefly | |

**Q3: Profile can't be found anywhere — what does the listener get?**

| Option | Description | Selected |
|--------|-------------|----------|
| Keep old profile + report | No dropout; status for plugin UI | ✓ |
| Silence + report | Mute until valid profile | |

**Notes:** Claude stated (not asked): keep OSD's existing profile numbering (0 = Simple, 1-5) so saved sessions reopen on the same profile.

---

## Shared-folder rules

**Q1: How does a dropped-in file replace a built-in profile?**

| Option | Description | Selected |
|--------|-------------|----------|
| Same filename wins | Exact-name override; other files ignored | ✓ |
| Also list unknown files | Extra profiles; needs UI + session handling | |

**Q2: Same-name file present but broken?**

| Option | Description | Selected |
|--------|-------------|----------|
| Use embedded + report | Fall back, show status | ✓ |
| Use embedded silently | Hide the problem | |

**Q3: Also check a per-user folder?**

| Option | Description | Selected |
|--------|-------------|----------|
| System folder only | As locked; installer writes there | ✓ |
| User folder, then system folder | No-admin overrides; more cases | |

**Notes:** Claude stated: folder checked on every profile load; Windows path implemented but unverified.

---

## Small-block 'done' bar

**Q1: What proves the binaural path is clean at small buffer sizes?**

| Option | Description | Selected |
|--------|-------------|----------|
| Automated match test | 32/64/128 + irregular sizes vs 512 reference, within rounding | ✓ |
| Test + DAW listening | Plus a DAW session before closing | |

**Q2: What happens to OSD#234 when Phase 3 passes?**

| Option | Description | Selected |
|--------|-------------|----------|
| Comment, leave open | Convolver proven clean; OSD delay/pitch still unchecked | ✓ |
| Close it | Risky if OSD's own code also glitches | |
| Don't touch it | Track only in SpatialCore docs | |

---

## Claude's Discretion

- Simple-mode filter design and strength (user: "you decide").
- Embedded BinaryData naming, API name, status-reporting shape, EMBED_ALL=OFF mechanism.
- HutubsPP2 Debug golden fix; SharedFFTCache leak report (here or Phase 5).
- CLAUDE.md HRTF wording correction (stale since `87cb7a3`, 2026-07-06; also wrong about OSD).

## Deferred Ideas

- User-supplied HRTFs as extra profiles (with SUITE-01).
- Per-user HRTF folder.
- Headphone listening check of new Simple cues → ROADMAP backlog.
- OSD-side: release note, delete OSD HRTF glue + BinaryData, check delay/pitch at small blocks.
- OpenSpatialPanner-side: invert `[binaural][sc12]` assertions.
