# Roadmap: SpatialCore

## Milestones

- ✅ **v1.0.0 SpatialCore v1.0.0 extraction milestone** — Phases 1-6 (shipped 2026-10-24; milestone label only, library version stays 0.5.0, no git tag)

## Phases

<details>
<summary>✅ v1.0.0 SpatialCore v1.0.0 extraction milestone (Phases 1-6) — SHIPPED 2026-10-24</summary>

- [x] Phase 1: Documentation Truth & Contract Freeze (5/5 plans) — completed 2026-08-15
- [x] Phase 2: Algorithm & Format Verification (10/10 plans) — completed 2026-10-04
- [x] Phase 3: Binaural Defects & HRTF Packaging (11/11 plans) — completed 2026-10-04
- [x] Phase 4: Control Surface & UI (8/8 plans) — completed 2026-10-05
- [x] Phase 5: Realtime Safety Hardening (8/8 plans) — completed 2026-10-17
- [x] Phase 6: Consumer Readiness & CI (10/10 plans) — completed 2026-10-24

Full phase detail: `.planning/milestones/v1.0.0-ROADMAP.md`. Requirements (25/25): `.planning/milestones/v1.0.0-REQUIREMENTS.md`. Audit (tech_debt): `.planning/milestones/v1.0.0-MILESTONE-AUDIT.md`.

</details>

## Backlog

### Phase 999.1: Follow-up — Phase 02 deferred UAT follow-up: Test 7 (BACKLOG)

**Goal:** Resolve the UAT checkpoint deferred during Phase 02 verification
**Source phase:** 02
**Deferred at:** 2026-10-03 during /gsd-verify-work 02 session completion
**Follow-ups:**
- [ ] Test 7: Next listening-review round — in OpenSpatialDelay on 7.1.4, lower a sound from ear height to 30 degrees below at about 60 degrees left; confirm it stays put (no drift toward one speaker, no side flip) (deferred 2026-10-03)

### Phase 999.2: Follow-up — Phase 02 deferred UAT follow-up: Test post-review (BACKLOG)

**Goal:** Resolve the UAT checkpoint deferred during Phase 02 verification
**Source phase:** 02
**Deferred at:** 2026-10-04 during /gsd-verify-work 02 session completion
**Follow-ups:**
- [ ] Test post-review: Listening checks for the 2026-10-04 code-review behaviour changes: (a) on a front-only rig (e.g. speakers at 0 and +-30 only), move a sound behind and below the listener and confirm it is never silent and moves smoothly across the gap (WR-05 gap bridge); (b) a DBAP source fed a broken distance (NaN/Inf) still plays, panned as distance 0.5 (WR-08) (deferred 2026-10-04)

### Phase 999.3: Custom HRTF import + loudness standard (BACKLOG)

**Goal:** A user imports their own measured `.sofa` HRTF from the plugin, names it, and keeps using it across sessions at the same perceived level as the built-ins
**Source phase:** 03
**Deferred at:** 2026-10-04 during /gsd-discuss-phase 3 (user has a custom `.sofa`; chose a later milestone over v1)
**Follow-ups:**
- [ ] "Import…" as the last item of the binaural profile dropdown; import copies the file into a personal library and asks for a display name
- [ ] Session opened without the custom file plays KEMAR + warning and keeps the custom choice
- [ ] Define a perceived-loudness standard (pink noise, ear-weighted, over all directions), apply it to custom and built-in profiles; OSD release note for the built-in level change. Input: Phase 3 D-14 measurement
- [ ] Decide shared `SpatialCoreUI` widget vs per-plugin import UI
- Full decision record: `.planning/phases/03-binaural-defects-hrtf-packaging/03-CONTEXT.md` § Deferred Ideas

### Phase 999.4: Listening check — Simple-mode height and rear cues (BACKLOG)

**Goal:** Confirm on headphones that Phase 3's new Simple (Woodworth) cues read as above and behind without sounding unnatural
**Source phase:** 03
**Deferred at:** 2026-10-04 during /gsd-discuss-phase 3 (D-03: test closes SpatialCore#15; listening is not a gate)
**Follow-ups:**
- [ ] After Phase 3 lands: in OSD Simple binaural mode, move a sound from ahead to overhead and from front to back; confirm each is audible and the ear-level front sound is close to before

---
*Roadmap rewritten 2026-08-10 against branch `gsd-remap`, after the original was found to have been planned against a branch missing 42 commits.*
