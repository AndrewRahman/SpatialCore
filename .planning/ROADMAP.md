# Roadmap: SpatialCore

## Milestones

- ✅ **v0.5.0 SpatialCore extraction milestone** — Phases 1-6 (shipped 2026-10-24; tag `v0.5.0` on main `17224af`; labelled v1.0.0 until 2026-10-25)
- 🚧 **v0.6.0 Pre-release follow-up** — Phases 7-13 (planned 2026-10-25; nothing is released yet, so SpatialCore stays 0.x until OpenSpatialDelay 2.0.0 ships on it)

## Phases

<details>
<summary>✅ v0.5.0 SpatialCore extraction milestone (Phases 1-6) — SHIPPED 2026-10-24</summary>

- [x] Phase 1: Documentation Truth & Contract Freeze (5/5 plans) — completed 2026-08-15
- [x] Phase 2: Algorithm & Format Verification (10/10 plans) — completed 2026-10-04
- [x] Phase 3: Binaural Defects & HRTF Packaging (11/11 plans) — completed 2026-10-04
- [x] Phase 4: Control Surface & UI (8/8 plans) — completed 2026-10-05
- [x] Phase 5: Realtime Safety Hardening (8/8 plans) — completed 2026-10-17
- [x] Phase 6: Consumer Readiness & CI (10/10 plans) — completed 2026-10-24

Full phase detail: `.planning/milestones/v0.5.0-ROADMAP.md`. Requirements (25/25): `.planning/milestones/v0.5.0-REQUIREMENTS.md`. Audit (tech_debt): `.planning/milestones/v0.5.0-MILESTONE-AUDIT.md`.

</details>

## 🚧 v0.6.0 Pre-release follow-up (Phases 7-13)

Built from all 25 GitHub issues open on 2026-10-25. Still open to intake: new issues filed from OpenSpatialDelay or OpenSpatialPanner go into the matching phase below rather than a new one. Phases are grouped by kind of work and sized to roughly 6-11 hours of agent work each (about 62 hours in total); hours are estimates, and owner listening time is not counted. Order: process first, because it sets the gate every later phase runs; release candidate last.

- [ ] Phase 7: Process, docs and consumer close-out (~6 h)
- [ ] Phase 8: Panning correctness — Ambisonics and VBAP (~9 h)
- [ ] Phase 9: Per-object gain parameters — focus, spread, distance curves, metres (~11 h)
- [ ] Phase 10: Per-object DSP stages — air absorption and doppler (~10 h)
- [ ] Phase 11: Shared UI and presets for OpenSpatialPanner (~9 h)
- [ ] Phase 12: Binaural — custom HRTF import and loudness standard (~10 h)
- [ ] Phase 13: First release candidate and Linux/Windows readiness (~7 h)

### Phase 7: Process, docs and consumer close-out

**Goal:** Everyday changes run a light test tier and the full tier runs only before a release; no doc states a stale fact; tickets whose SpatialCore half has shipped are closed or handed to the consumer
**Issues:** #42 (priority: high), #37, #13, #24, #9
**Estimate:** ~6 h
**Work:**
- [ ] #42 Define the light tier (build plus tests for the changed area) and the full tier (Debug and Release suites, `[rt]`, TSan, speaker and DAW checks); review cross-repo pins, landing order and handoffs; update CLAUDE.md "Build System", README "Local test gate" / "Realtime-safety check" and the GSD handoff "Done when" lists
- [ ] #37 `docs/development-roadmap.md` URL (DR-18) and counts (23 formats / 15 layouts); dangling `.planning` paths in `Core/BinauralGains.h:4`, `Core/SourcePosition.h:4`; dated "12 sources <5 ms" skill row; `BinauralRendererTests.cpp:520` title says "keeps the last good sound" but the guard outputs silence
- [ ] #13 Document the `PresetBrowser` API contract (state format, save/load hooks, versioning), or record that it is unsuitable
- [ ] #24 SpatialCore half shipped (cae34a9, `getBlockLayout()`); hand the rest to OSD (openspatialdelay#23: replace `getActiveLayout()` in `processBlock()` before bumping past `ab60c25`)
- [ ] #9 SpatialCore CI green on JUCE 9.0.0 (runs 39058330902, 39074814315); close once OSP's own CI runs green (OpenSpatialPanner#4)

### Phase 8: Panning correctness — Ambisonics and VBAP

**Goal:** Ambisonics output is accurate end to end with a decided max-rE policy, and 3D VBAP picks a stable triangulation on coplanar quads
**Issues:** #45, #32, #22, #39, #40
**Estimate:** ~9 h, plus owner listening
**Work:**
- [ ] #45 Why encoder-side max-rE and the NFC shelf were dropped in #32 / PR #44; where max-rE still applies; decide default vs a max-rE / basic toggle (OSD measured order 3 +4.73 dB, order 1 -1.41 dB after the change)
- [ ] #32 Speaker-rig check at the pre-release review (20-speaker ring, 6th order, OSD as AU and VST3) after openspatialdelay#22 ships; `[ambi]` / `[ambi-pin]` and the AmbiX sign test already pass (quick task 261024-mhh); plain AmbiX done (261024-qlw)
- [ ] #22 Deterministic tie-break for coplanar-quad triangulations in `computeVBAPGains3D` / `buildVBAPTripletsForLayout` (0.45-0.85 gain jumps on 7.1.4 height sweeps)
- [ ] #39 Owner listening: on 7.1.4, lower a sound from ear height to -30° at about 60° left; it stays put (confirms #22)
- [ ] #40 Owner listening: front-only rig gap bridge (WR-05) is never silent and smooth; DBAP with NaN/Inf distance plays as distance 0.5 (WR-08)

### Phase 9: Per-object gain parameters — focus, spread, distance curves, metres

**Goal:** One design for the new per-object parameters that change how gains are computed, implemented where well defined
**Issues:** #20, #10, #4, #16
**Estimate:** ~11 h
**Work:**
- [ ] #10 Research and design a spread API across the 8 algorithms (`SourcePosition` has no spread field; changing `computeGains()` needs a major-version bump, so prefer a context field)
- [ ] #20 Per-source focus exponent for VBAP/VBIP (p = 1 VBAP, 0.5 VBIP), designed with #10; never renumber OSD's VBIP menu index
- [ ] #4 Engine-side distance attenuation curves (inverse, inverse-square, linear, exponential), per object, replacing consumer-supplied `distGainPerSample` behind an opt-in flag
- [ ] #16 Real end-to-end distance unit: the map's metres label (`SpatialMapComponent.cpp:265`) and the DSP distance (`DirectBinauralAlgorithm.cpp:38`) are unrelated today

### Phase 10: Per-object DSP stages — air absorption and doppler

**Goal:** The engine can filter and pitch each object by its distance and motion, realtime-safe
**Issues:** #5, #17
**Estimate:** ~10 h
**Work:**
- [ ] #5 Air-absorption HF rolloff filter stage per object, driven by distance (uses the Phase 9 distance unit)
- [ ] #17 Doppler resampling delay line so `DopplerVelocity::getSmoothedSemitones()` can be applied; raise `DopplerVelocity::kMaxObjects` (12) to `MAX_SOURCES` (32)

### Phase 11: Shared UI and presets for OpenSpatialPanner

**Goal:** The shared Spatial Map and Object Panel work at 32 objects, offer rotate-all and per-object energy, and support OSP's position presets
**Issues:** #31, #6, #7, #2
**Estimate:** ~9 h
**Work:**
- [ ] #31 `objectColours` has 12 entries, so objects 13-32 are invisible; check the 32-wide `ObjectPanel` layout (colours are an OSP design decision)
- [ ] #6 Global master transform: rotate all objects around a common centre
- [ ] #7 Per-object RMS/energy display on `SpatialMapComponent`
- [ ] #2 OSP position-preset system: SpatialCore's share (state format and `PresetBrowser` hooks, built on the Phase 7 contract); the plugin UI lives in OSP

### Phase 12: Binaural — custom HRTF import and loudness standard

**Goal:** A user imports a measured `.sofa`, names it and keeps using it across sessions at the same perceived level as the built-ins
**Issues:** #38, #41
**Estimate:** ~10 h, plus owner listening
**Work:**
- [ ] #38 "Import..." as the last item of the profile dropdown, copying into a personal library with a display name; a session missing the file plays KEMAR with a warning and keeps the choice; a perceived-loudness standard (pink noise, ear-weighted, all directions) applied to custom and built-in profiles, with an OSD release note; shared `SpatialCoreUI` widget vs per-plugin UI. Constraints: indices never renumbered (D-07); the engine never writes to disk today. Decision record: `.planning/milestones/v0.5.0-phases/03-binaural-defects-hrtf-packaging/03-CONTEXT.md` "Deferred Ideas"
- [ ] #41 Owner listening on headphones: Simple mode ahead-to-overhead and front-to-back are audible, and ear-level front sounds as before

### Phase 13: First release candidate and Linux/Windows readiness

**Goal:** The first `v*` release-candidate tag run is green on all five CI jobs, with the deferred checks done and the Linux/Windows risks measured or accepted on record
**Issues:** #35, #36, #34
**Estimate:** ~7 h, plus CI time
**Work:**
- [ ] #35 Engine coverage >= 93% on the merged tree; watch `[hrtf-switch][churn]`; realtime-safety check (TSan + `[rt]`); first CI run since 310fb93; full Debug run; `SPATIALCORE_EMBED_ALL_HRTF=OFF` build. Needs the owner's go-ahead: since PR #46 (2026-10-25) CI runs only when started by hand (`gh workflow run ci.yml`), and a tag push starts nothing
- [ ] #36 Read the first Linux/Windows run; widen or restructure only the tight-tolerance tests and golden hashes that fail; note the reason on the rest
- [ ] #34 Measure the JUCE fallback FFT `SpinLock` (shared `SharedFFTCache`) with N instances at 48 kHz / 32 samples on Linux and/or Windows; decide accept vs per-instance FFT; update CLAUDE.md, README and `SharedFFTCache.h`

---
*Roadmap rewritten 2026-08-10 against branch `gsd-remap`, after the original was found to have been planned against a branch missing 42 commits.*
