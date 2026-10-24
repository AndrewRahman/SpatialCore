# Phase 1: Documentation Truth & Contract Freeze - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in CONTEXT.md — this log preserves the alternatives considered.

**Date:** 2026-08-11
**Phase:** 1-documentation-truth-contract-freeze
**Areas discussed:** Doc surface scope, Limiter contract home, Drift prevention, BUG-03 issue-reference form

---

## Area Selection

All four offered gray areas were selected for discussion.

| Option | Description | Selected |
|--------|-------------|----------|
| Doc surface scope | How far does "every public number" reach? | ✓ |
| Limiter contract home | Amend the March scaffold plan, or move the contract? | ✓ |
| Drift prevention | One-time correction, or a mechanized guard? | ✓ |
| BUG-03 issue-reference form | Which qualified form for cross-repo issue refs? | ✓ |

---

## Doc Surface Scope

### Q1 — How far does the correction sweep reach?

| Option | Description | Selected |
|--------|-------------|----------|
| Shipped surfaces only | CLAUDE.md, README.md, docs/integration-guide.md | |
| Shipped + skill | Above plus `.claude/skills/spatialcore-architecture` | |
| Full sweep | Above plus `development-roadmap.md` and `.planning/intel/*.md` | |
| You decide | Pick the boundary serving "CLAUDE.md stops mis-steering" | ✓ |

**User's choice:** You decide.
**Claude's call:** Two tiers — Tier A corrects numbers in place (CLAUDE.md, README.md,
integration-guide.md, spatialcore-architecture skill, spatial-audio-dsp SKILL.md:518); Tier B stamps
files historical without renumbering (development-roadmap.md, the March scaffold plan,
`.planning/intel/*.md`).
**Notes:** Rationale for the split — Tier B files are dated snapshots. Renumbering them creates a
second source of truth to maintain. A "superseded by .planning/ROADMAP.md" header removes the
mis-steering at zero maintenance cost.

### Q2 — Is Simple (Woodworth) an HRTF profile?

| Option | Description | Selected |
|--------|-------------|----------|
| 5 profiles | Woodworth is the no-SOFA fallback, not a profile | |
| 6 entries, index 0 = Simple | User-selectable in the UI, therefore a profile | ✓ |
| Read the code first | Whatever `profileIndex` indexes today is the answer | |

**User's choice:** 6 entries, index 0 = Simple (Woodworth).
**Notes:** Verified against code after the answer — `BinauralRenderer.cpp:26` routes
`profileIndex == 0` to the Woodworth path; `DirectBinauralAlgorithm.cpp:21` maps 1–5 onto the 5 SOFA
files. Code agrees with the user. Canonical phrasing states both numbers together, because stating
only one is what created OQ-3 in the first place.

---

## Limiter Contract Home

The user asked "What is the outputLimiter? Please elaborate" before answering. A full explanation was
given: a one-line pure function at `Core/SpatialMath.h:100` returning
`1.2589f * tanh(x / 1.2589f)` with a non-finite guard, serving as speaker protection during
self-oscillation; `inline` in a public header, therefore compiled into consumer binaries.

| Option | Description | Selected |
|--------|-------------|----------|
| Header docblock is the contract | `SpatialMath.h:98-99` already describes tanh correctly; cannot drift from the code | ✓ |
| New `docs/api-contract.md` | One living doc collecting every frozen public contract | |
| Amend the scaffold plan | Lowest deviation from OQ-2's literal wording | |

**User's choice:** Header docblock is the contract.
**Notes:** This deviates from OQ-2's literal instruction to "amend the SPEC." Justified: that SPEC is
a one-off dated scaffold plan already assigned to Tier B, and amending a historical doc to serve as
the governing contract contradicts the tier rule. The scaffold plan gets a historical stamp instead.

**Findings surfaced during this area:**
- `include/SpatialCore/DSP/` does not exist. CLAUDE.md claims a `DSP` module owning `softClip()` and
  `outputLimiter()`; both live in `Core/SpatialMath.h`. PROJECT.md's OQ-2 also cites a nonexistent
  `DSP/Utilities.h`. Fifth CLAUDE.md inaccuracy, not in API-05's original list.
- CLAUDE.md already contains the `Engine/` row at `:20`, so that part of API-05's stated scope is
  stale.
- `tests/Core/SpatialMathTests.cpp:66-95` already covers most of success criterion 2.

---

## Drift Prevention

| Option | Description | Selected |
|--------|-------------|----------|
| Mechanized guard | Catch2 count test + CI script grepping Tier A docs | |
| Code-side only | `static_assert` the counts; docs stay manual | |
| Manual | Correct now, no guard | |
| You decide | Pick based on how much machinery this phase carries | ✓ |

**User's choice:** You decide.
**Claude's call:** `static_assert` + Catch2 test, no doc-grep CI script.
**Notes:** The doc-grep half is brittle — grepping prose for "8" and "25" yields false positives and
becomes its own maintenance burden. The compile-time half is cheap and catches drift at its source.
Two supporting rules: count constants are `constexpr int` adjacent to each enum rather than new
enumerators (a new enumerator trips `-Wswitch` in consumers, and DR-3 requires OSD keeps building
clean); `static_assert` messages name the Tier A files to update, converting silent doc lag into a
directed instruction. `NUM_LAYOUT_DEFS` already exists and ships — assert against it, don't replace it.

---

## BUG-03 Issue-Reference Form

**Question was withdrawn and corrected mid-area.**

The question as first posed assumed `Spatial-Media-Lab/OpenSpatialDelay` did not exist. The user
corrected this. Verified via `gh`: the repo exists, is public, was pushed 2026-05-06, and issue #234
resolves to "Audio artifacts at buffer sizes below 256 samples" (OPEN). The 404 concern applies only
to `Spatial-Media-Lab/SpatialCore`, which genuinely does not exist.

**Resolution:** no decision needed. Write the references exactly as BUG-03 specifies —
`Spatial-Media-Lab/OpenSpatialDelay#N`. Mechanical edit at 4 sites.

### Follow-up — the integration guide's submodule URL

A reframed question was posed about `docs/integration-guide.md` publishing a SpatialCore submodule
URL that does not resolve. The user answered with a topology correction rather than selecting an
option:

> "We are currently developing the SpatialCore on my local repo until we have proven that it works
> properly for the plugins. We then ship OpenSpatialDelay and the new OpenSpatialPanner plus the
> newly created SpatialCore on the Spatial-Media-Lab github repo... during development and testing,
> and to prevent breaking a live plugin which is currently in use by people, we need to keep it on
> my personal repos."

**Notes:** This supersedes PROJECT.md's OQ-5 framing. The personal remote is correct-by-design, not
a pending migration. Org migration is gated on proof, not a date, and all three repos move together.
The integration guide therefore carries the working dev remote as the live instruction with the org
URL labelled as the post-proof destination.

---

## Wrap-Up

| Option | Description | Selected |
|--------|-------------|----------|
| Ready for context | Write CONTEXT.md | ✓ |
| Discuss "done" criteria | Whether a verification pass re-derives every number | |
| Discuss `.planning/` doc corrections | Whether PROJECT.md / REQUIREMENTS.md errors are in scope | |

**User's choice:** Ready for context.
**Notes:** The `.planning/` doc corrections were raised but not separately discussed. Recorded in
CONTEXT.md under Claude's Discretion as a recommendation the planner may scope, not a locked
requirement.

---

## Claude's Discretion

- **Doc surface tier split (D-01)** — user answered "you decide." The A/B boundary and the
  stamp-don't-renumber rule are Claude's calls.
- **Drift mechanism (D-10 – D-13)** — user answered "you decide." `static_assert` + Catch2, the
  rejection of doc-grep CI, and the `constexpr`-adjacent-to-enum rule are Claude's calls, grounded
  in DR-3 and DR-4.
- **Planning-doc corrections** — PROJECT.md OQ-5 framing, the nonexistent `DSP/Utilities.h` path,
  and API-05's stale "omits Engine" claim. Recommended for inclusion; left to the planner to scope.

## Deferred Ideas

- **Possible `profileIndex` off-by-one in the LF shelf** — `BinauralRenderer.cpp:106` sets
  `lfShelfActive = (profileIndex == 5)` commented "Only MIT KEMAR needs compensation", but
  `mit_kemar_large_pinna.sofa` is 4th of 5 alphabetically. Behavior defect, not doc truth.
  → Phase 3.
- **Selectable hard-clamp limiter mode** — already recorded as v2 candidate LIMIT-01.
- **A living `docs/api-contract.md`** — considered and rejected for Phase 1 in favour of the
  header-adjacent contract. Worth reconsidering at v3.
