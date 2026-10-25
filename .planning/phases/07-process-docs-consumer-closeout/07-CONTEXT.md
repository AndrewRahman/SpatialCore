# Phase 7: Process, docs and consumer close-out - Context

**Gathered:** 2026-10-25
**Status:** Ready for planning

<domain>
## Phase Boundary

Documentation and issue housekeeping only; no engine or API change. Delivers:
- A written two-tier test policy (#42): a light everyday tier and a full pre-release tier, stated the same way in CLAUDE.md "Build System", README "Local test gate" / "Realtime-safety check", PROJECT.md decisions and the GSD handoff "Done when" lists, plus a short written cross-repo landing order.
- The stale-fact fixes of #37 and the stale CI-trigger wording (ci.yml is manual-dispatch only since PR #46).
- The `PresetBrowser` contract documented (#13).
- #24 and #9 closed with evidence.

</domain>

<decisions>
## Implementation Decisions

### Test tiers (#42)
- **D-01 (user):** Two tiers, split by moment, not one fixed gate.
  - **Light tier, while iterating:** build the Release tree, then run only the tests for the area that changed (Catch2 tag or file filter, e.g. `SpatialCoreTests "[binaural]"`, `"[osc]"`, `"[trajectory]"`, `"[io]"`; UI changes run `SpatialCoreUITests`). Seconds, not minutes.
  - **Before merging or pushing to `main`:** the full Release `ctest --test-dir build-release/tests` (~2 min, includes the 20 s `[rt]` trap). Reason: `main` has no CI and no branch protection, so this is the only thing that catches a break in another area.
  - **Full tier, only when preparing a release:** Debug build and both executables, Release ctest, `[rt]`, the `build-tsan` realtime-safety check, coverage, the consumer harness, the speaker-rig and DAW listening checks, and (with the user's go-ahead) the manual CI run.
  - Supersedes the everyday-gate wording of D-19 (D-18 and D-19's release-only items stay as they are). Record as a new PROJECT.md decision row.
- **D-02:** A doc-only or planning-only change needs no test run.
- **D-03:** The tier names and the commands appear once in README "Local test gate"; CLAUDE.md and the handoff "Done when" lists point at it instead of repeating the full gate.

### CI wording (part of #42)
- **D-04:** Fix every surface that says a `v*` tag push starts CI. Fact since PR #46 (2026-10-25): `.github/workflows/ci.yml` has only `workflow_dispatch` (with the `realtime` input). Surfaces known so far: CLAUDE.md "Build System" and "Consumer Plugins" (harness line), the CLAUDE.md realtime-safety bullet, README lines ~135, ~141, ~173, ~195, PROJECT.md D-20 and CI-01 rows. Grep for `release tag`, `v\*`, `tag is pushed` to find the rest. A manual run still needs the user's go-ahead.

### Cross-repo order and handoffs (part of #42)
- **D-05:** Landing order stays: SpatialCore commit and push first, then each consumer bumps its submodule pin in its own repo. Handoffs to OSD/OSP are written as an issue in the consumer repo (as openspatialdelay#23 and OpenSpatialPanner#4 were), and the handoff's "Done when" uses the light tier unless the handoff is release prep.
- **D-06:** Phase 7 changes SpatialCore docs only. OSD and OSP adopting the same tiers is a note in the consumer issues, not edits in those repos.

### Stale docs (#37)
- **D-07:** `docs/development-roadmap.md`: replace the submodule URL with `https://github.com/AndrewRahman/SpatialCore`; update counts to 23 formats / 15 layouts (keep the historical banner).
- **D-08:** Remove the dangling `.planning/phases/08-...` paths in `include/SpatialCore/Core/BinauralGains.h:4` and `Core/SourcePosition.h:4` (replace with a plain description or the commit, no path that does not exist).
- **D-09:** `.claude/skills/spatial-audio-dsp/SKILL.md:567`: the 12-source "<5 ms" figure is not re-measured in this phase; mark it as a dated 12-source measurement or remove the row.
- **D-10:** Rename the test at `tests/Binaural/BinauralRendererTests.cpp:520` and fix the comments at :446 and :548 so they say the guard outputs silence. Only the title and comments change, so it runs under the light tier (`"[binaural]"`).

### PresetBrowser (#13)
- **D-11:** `PresetBrowser` is menu chrome only: it shows a category/preset popup from a consumer-supplied `Entry` list and echoes back `originalIndex`, and owns a `PresetSaveOverlay` whose `onSave` hands back a name. It stores no state, has no state format and no versioning. Document that contract in the header (and a short integration-guide section): what the consumer supplies, what it gets back, threading (message thread), lifetime of the async menu, and that state format, save/load and versioning are the consumer's. Not "unsuitable": the state-format share for OSP presets is already Phase 11 (#2), which builds on this contract.

### Issue close-out
- **D-12:** #24: close now. openspatialdelay#23 is closed; OSD main 717d6e2 pins SpatialCore 2f16bae and its `processBlock` reads `getBlockLayout()`. The "1.1.0 bump" item no longer applies (SpatialCore stays 0.x until OSD 2.0.0 ships on it, CLAUDE.md "Versioning").
- **D-13 (user):** #9: close now on SpatialCore's own JUCE 9.0.0 evidence (CI runs 39058330902 and 39074814315, green). OSP's own CI (last main run 39050086785 failed, 2026-10-17) stays tracked in OpenSpatialPanner#4. No GitHub run is started for this.
- **D-14:** #42, #37, #13 close when their changes are merged to main, each with a comment naming the commit.

### Claude's Discretion
- Exact wording and section layout of the README tier section and the PresetBrowser docs.
- Which tags map to which source folders in the light-tier table.

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Test policy and CI
- `README.md` "Local test gate" (~L113-135) and "Realtime-safety check" (~L137-175) — current gate wording to rewrite
- `CLAUDE.md` "Build System" (Tests bullet), "Design Principles" realtime-safety bullet, "Consumer Plugins" harness line — stale CI-trigger wording
- `.github/workflows/ci.yml` L24-30 — the trigger is `workflow_dispatch` only
- `.planning/PROJECT.md` decisions D-18, D-19, D-20 (~L233-235) and CI-01 (~L65)
- `.planning/HANDOFF-ambisonics-fix.md` "Done when" (L23, L49) — example of a handoff that demanded the full gate
- GitHub issue #42 — problem statement

### Stale docs
- GitHub issue #37 — the item list
- `docs/development-roadmap.md` L26, L75, L93
- `include/SpatialCore/Core/BinauralGains.h:4`, `include/SpatialCore/Core/SourcePosition.h:4`
- `.claude/skills/spatial-audio-dsp/SKILL.md:567`
- `tests/Binaural/BinauralRendererTests.cpp:446, :520, :548`; `include/SpatialCore/Binaural/BinauralRenderer.h:59` (already-correct wording)

### PresetBrowser
- `include/SpatialCore/UI/PresetBrowser.h`, `src/UI/PresetBrowser.cpp`
- `docs/integration-guide.md` — where a consumer-facing section goes
- GitHub issue #13; ROADMAP Phase 11 (#2) builds on this contract

### Consumer close-out
- AndrewRahman/openspatialdelay#23 (closed, last comment has the evidence) — for #24
- AndrewRahman/OpenSpatialPanner#4 (open) — for #9

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- Catch2 tags already partition the suite by area: `[trajectory]` 33, `[osc]` 27, `[io]` 20, `[binaural]` 18, `[spatialmath]` 15, `[ambisonics]` 11, `[ui]` 11, `[algorithms]` 8. Test folders mirror source folders (`tests/Algorithms`, `Binaural`, `Core`, `Engine`, `IO`, `OSC`, `Trajectory`, `UI`), so a folder-to-tag table is enough for the light tier.

### Established Patterns
- Docs use plain, dated decision references (D-xx with a date); new decision rows go in PROJECT.md's decision table.
- Commits on planning/doc-only changes carry `[skip ci]` (harmless now that CI is manual).

### Integration Points
- No code integration; only one test title changes.

</code_context>

<specifics>
## Specific Ideas

- The user's goal for #42: "rapid prototyping by default; all thorough testing happens in preparation for a release."
- Never start a GitHub Actions run without asking.

</specifics>

<deferred>
## Deferred Ideas

- Re-measuring the 32-source processBlock time for the skill table — a later perf phase, if wanted.
- OSD and OSP adopting the same test tiers in their own repos — note in their issues; edits happen in those repos.
- OSP's failing CI on main — OpenSpatialPanner#4.

</deferred>

---

*Phase: 07-process-docs-consumer-closeout*
*Context gathered: 2026-10-25*
