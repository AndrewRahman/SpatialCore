---
gsd_state_version: '1.0'  # placeholder; syncStateFrontmatter overwrites on first state.* call
status: planning
progress:
  total_phases: 6
  completed_phases: 0
  total_plans: 0
  completed_plans: 0
  percent: 0
---

# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-08-09)

**Core value:** A Spatial Media Lab plugin author gets production-grade spatial rendering by linking one library, so the only audio code they write is their own effect.
**Milestone:** v1 — OpenSpatialDelay ships on SpatialCore as a submodule with zero regressions
**Current focus:** Phase 1 — Documentation Truth & Contract Freeze

## Current Position

Phase: 1 of 6 (Documentation Truth & Contract Freeze)
Plan: 0 of TBD in current phase
Status: Ready to plan
Last activity: 2026-08-10 — codebase re-mapped on branch `gsd-remap`; REQUIREMENTS.md and ROADMAP.md rewritten with all 14 open GitHub issues triaged

**Branch note:** planning now tracks `gsd-remap` = `origin/spatialcore-v2-extraction` + `origin/main`.
The 2026-08-09 pass was planned against a branch missing 42 commits of code, so its codebase map
described a tree that no longer matched reality. Do not plan against `origin/main` alone.

Progress: [░░░░░░░░░░] 0%

## Performance Metrics

**Velocity:**
- Total plans completed: 0
- Average duration: —
- Total execution time: —

**By Phase:**

| Phase | Plans | Total | Avg/Plan |
|-------|-------|-------|----------|
| - | - | - | - |

**Recent Trend:**
- Last 5 plans: none yet
- Trend: —

*Updated after each plan completion*

## Accumulated Context

### Decisions

Full log in PROJECT.md Key Decisions. Affecting current work:

- DR-5 **locked, reconciliation decided**: DR-5 says SpatialCore embeds HRTF profiles as BinaryData; the tree loads them from disk. Resolved 2026-08-10: embed as DR-5 says for v1, and add a shared-folder lookup ahead of it so the embedding can later be reduced to a single fallback profile without code changes. Phase 3 owns it.
- DR-1 **locked**: no allocation, locks, or logging in anything reachable from `processBlock`. Owns Phase 5.
- DR-7 **locked**: `SpatializationAlgorithm` is frozen. Contract ambiguity is a defect, not a preference.
- **User ruling 2026-08-10:** OpenSpatialDelay is the v1 consumer, not OpenSpatialPanner. OSD is the tree SpatialCore was extracted from and ships publicly at v1.0.0, so it is the only oracle that can prove zero regressions. Verified: OSD does not yet consume SpatialCore — the migration is unstarted.
- **Discharged:** the 2026-08-09 ruling that `src/` is "partial until audited" has been satisfied by evidence. The suite builds and passes 144 TEST_CASEs / 1585 assertions across 16 files. Phases no longer open with a re-implementation audit.

### Pending Todos

- Consider filing GitHub issues for two confirmed defects that no issue tracks: the `TrajectoryEngine` non-atomic position floats, and the second audio-thread allocation at `BinauralRenderer.cpp:134,158-163`.

### Blockers/Concerns

- **OQ-1** — algorithm count. **CLOSED: 8**, verified from the tree.
- **OQ-2** — `outputLimiter()`. **CLOSED 2026-08-10: keep the shipped tanh soft ceiling**; amend the SPEC to match. OSD ships v1.0.0 with that curve, so it is the known sound. Hard-clamp mode deferred to v2 as LIMIT-01.
- **OQ-3** — HRTF profile count. **CLOSED: 5**, verified from the tree.
- **OQ-4** (Phase 6) — test coverage target. **Blocked, not merely undefined**: no coverage tooling exists in the build, so there is nothing to measure against. Wire up instrumentation, read the real number, then set the target. The old "~5%" figure was never measured.
- **OQ-5** (not a v1 phase) — org repo migration pending; `docs/integration-guide.md` publishes a submodule URL that does not resolve.
- **OQ-6** (Phase 3) — HRTF packaging. **CLOSED 2026-08-10: lookup chain now, embedded default for v1.** Target architecture is the user's — profiles on disk once per machine in a shared folder, all SML plugins reference it; better than embedding chiefly because it enables user-supplied SOFA files. But v1 still embeds all 5, because the shared folder needs a signed installer that does not exist and OSD ships drag-and-drop. Phase 3 builds the resolution chain (shared folder → embedded → loud error) behind `SPATIALCORE_EMBED_ALL_HRTF=ON`; flipping it OFF later is a build flag, not a redesign. Tracked as SUITE-01.

Also open: v1 cannot close without OpenSpatialDelay-side work (REQ-osd-consume-spatialcore-submodule) that has no SpatialCore phase by design.

## Deferred Items

| Category | Item | Status | Deferred At |
|----------|------|--------|-------------|
| *(none)* | | | |

## Session Continuity

Last session: 2026-08-09 23:52
Stopped at: Plan rewritten on branch `gsd-remap`; 14 GitHub issues triaged; OQ-1/2/3/6 all closed. Only OQ-4 (coverage target) remains, and it is blocked on wiring up coverage tooling.
Resume file: None
