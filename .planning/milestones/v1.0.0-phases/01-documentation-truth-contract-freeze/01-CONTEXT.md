# Phase 1: Documentation Truth & Contract Freeze - Context

**Gathered:** 2026-08-11
**Status:** Ready for planning

<domain>
## Phase Boundary

Make every public factual claim about SpatialCore match the tree, and give each ambiguous public
contract exactly one home. **No behavior changes.** The only code edits permitted are comments,
docblocks, count constants, `static_assert`s, and tests — never a change to what the library does.

The phase serves two distinct readers, and both are in scope:
1. **A consumer** reading `README.md` and `docs/integration-guide.md`.
2. **A Claude session in this repo** auto-loading `CLAUDE.md` and `.claude/skills/*`.

Reader (2) is why this phase is first: a wrong `CLAUDE.md` mis-steered the entire 2026-08-09
planning pass. The `.claude/skills/spatialcore-architecture/` skill has the same auto-load
mechanism and is currently worse than `CLAUDE.md` ever was.

</domain>

<decisions>
## Implementation Decisions

### Doc Surface Scope

- **D-01:** The sweep splits into two tiers.
  **Tier A — correct the numbers in place:** `CLAUDE.md`, `README.md`,
  `docs/integration-guide.md`, `.claude/skills/spatialcore-architecture/spatialcore-architecture.md`,
  and the `JUCE 8.0.3` line at `.claude/skills/spatial-audio-dsp/SKILL.md:518`.
  **Tier B — stamp superseded, do NOT renumber:** `docs/development-roadmap.md`,
  `docs/superpowers/plans/2026-03-22-scaffold-spatialcore.md`, `.planning/intel/*.md`.
  Tier B files are dated snapshots. Editing their numbers creates a second source of truth to
  maintain; a header reading "Historical — superseded by `.planning/ROADMAP.md`, 2026-08-10" kills
  the mis-steering with no maintenance debt.
  — **Reversibility:** reversible — comment and header edits only.

- **D-02:** The `.claude/skills/spatialcore-architecture/` skill is treated as a first-class doc
  surface, not a nice-to-have. It auto-loads in this repo and currently states **7 algorithms,
  6 HRTF profiles, 22 formats, 13 layouts** — four wrong numbers via the exact mechanism that
  caused the 2026-08-09 rewrite. Correcting `CLAUDE.md` while leaving this file untouched does not
  achieve the phase goal.

- **D-03:** The general rule that settles Tier B, the integration guide, and the scaffold plan
  uniformly: **docs state current reality; future state is permitted but must be explicitly
  labelled as future.** Aspirational content presented as present-tense fact is the defect class
  this phase exists to eliminate.

### The Numbers (ground truth, verified 2026-08-10 / 2026-08-11)

- **D-04:** Canonical counts: **8** algorithms, **23** output formats, **15** speaker layouts,
  **JUCE 9.0.0**, **5** SOFA HRTF profiles shipped.
  Verified: `grep -l "public SpatializationAlgorithm" include/` → 8; `ls HRTF/*.sofa` → 5;
  `CMakeLists.txt` `GIT_TAG 9.0.0`; `OutputFormat` enum body at
  `include/SpatialCore/IO/OutputFormat.h:9-28` → 23 enumerators, cross-checked against 23
  `OutputFormat::` rows in `src/IO/OutputFormatRegistry.cpp`; `LayoutID` at
  `include/SpatialCore/IO/SpeakerLayout.h:42-45` → 15 named entries before `NUM_LAYOUT_DEFS`,
  which sizes `layoutDefs[NUM_LAYOUT_DEFS]` at `src/IO/SpeakerLayout.cpp:19`.
  Consistency check: 23 − Binaural − Stereo − 6 Ambisonics orders = 15 speaker layouts.
  — **Reversibility:** one-way — these are the frozen public contract under DR-7. Publishing a
  number and later changing it breaks consumers who wrote loops or switches against it.
  — **Corrected 2026-08-11 (was 25 formats / 14 layouts).** The 25/14 pair never came from the
  tree despite the "verified 2026-08-10" label. It originated as a mapper miscount in
  `.planning/codebase/ARCHITECTURE.md:107` and `STRUCTURE.md:119-120`, and commit `f0b14f4`
  copied it into REQUIREMENTS.md and ROADMAP.md. The same remap commit's `TESTING.md:56` says
  "23-format table", so the remap contradicted itself. The 14 was an off-by-one on `LayoutID`
  that misattributed one named entry to the `NUM_LAYOUT_DEFS` sentinel. The enum has been
  unchanged since 2026-07-05. This correction is itself an instance of the defect class the
  phase exists to eliminate, and is the same adjudication already applied to OQ-1 and OQ-3.

- **D-05:** **HRTF profile count is stated as two numbers together, never one.** Canonical phrasing:
  *"5 SOFA HRTF profiles ship; `profileIndex` is 0–5, where 0 = Simple (Woodworth) and 1–5 select
  the SOFA profiles."*
  This was a **user decision**, not a counting fix: Woodworth is user-selectable in the UI, so it
  is a profile. Verified against code — `BinauralRenderer.cpp:26` routes `profileIndex == 0` to the
  no-SOFA Woodworth path; `DirectBinauralAlgorithm.cpp:21` does
  `ctx.profiles[jlimit(0,4, ctx.profileIndex - 1)]`, mapping 1–5 onto the 5 SOFA files.
  **Stating only one of the two numbers is precisely what created OQ-3.** README.md's current
  "6 HRTF Profiles" was not wrong about the count — it was wrong about the kind.
  — **Reversibility:** one-way — `profileIndex` is public in `BinauralContext`
  (`include/SpatialCore/Core/Types.h:87`). Phase 3 inherits this range.

### Limiter Contract Home (API-02 / OQ-2)

- **D-06:** **The header docblock at `include/SpatialCore/Core/SpatialMath.h:98-99` IS the
  governing contract.** It already describes the tanh curve correctly. It sits next to the code so
  it cannot drift, and a consumer reading the header sees it. No new doc is created.

- **D-07:** The March scaffold plan (`docs/superpowers/plans/2026-03-22-scaffold-spatialcore.md`,
  hard ceiling at `:711`) is **stamped historical, not amended.** This is a deliberate deviation
  from OQ-2's literal wording ("amend the SPEC"): that "SPEC" is a one-off dated scaffold plan, and
  Tier B marks it historical. Amending a historical doc to serve as the governing contract
  contradicts D-01.

- **D-08:** The implementation is **unchanged**. `outputLimiter()` keeps
  `1.2589f * std::tanh(x / 1.2589f)` with the non-finite→`0.0f` guard. Rationale is already locked
  (OQ-2, 2026-08-10): OSD shipped publicly at v1.0.0 with this curve, it is `inline` in a public
  header so it is compiled into consumer binaries, and v1's gate is zero regressions.

- **D-09:** **Delete the phantom `DSP/` row from `CLAUDE.md`'s architecture table.**
  `include/SpatialCore/DSP/` **does not exist**. `softClip()` and `outputLimiter()` live in
  `include/SpatialCore/Core/SpatialMath.h` (lines 85 and 100). `PROJECT.md`'s OQ-2 entry also cites
  a nonexistent `include/SpatialCore/DSP/Utilities.h`. This is a fifth `CLAUDE.md` inaccuracy,
  discovered during this discussion and not present in API-05's original list.

### Drift Prevention

- **D-10:** **`static_assert` + Catch2 test. No doc-grep CI script.**
  The doc-grep half is the expensive, brittle half — grepping prose for "8" and "23" yields false
  positives and becomes its own maintenance burden. The compile-time half is cheap and catches
  drift at its source: a dev adding an `OutputFormat` breaks the build immediately.
  — **Reversibility:** reversible.

- **D-11:** Count constants are declared as `constexpr int` **adjacent to** each enum, **never as a
  new enumerator inside it.** A new enumerator would trip `-Wswitch` in any consumer holding an
  exhaustive `switch`, and DR-3 requires OpenSpatialDelay keeps building clean.
  Exception: `NUM_LAYOUT_DEFS` (`include/SpatialCore/IO/SpeakerLayout.h:44`) already exists as an
  enumerator and already ships — leave it alone, assert against it.
  `include/SpatialCore/IO/OutputFormat.h:9` has no sentinel; add one adjacent. Same for algorithms.
  — **Reversibility:** costly — adding an enumerator later and removing it is a public API change.

- **D-12:** Each `static_assert` message **names the Tier A files to update**. That converts a
  silent doc lag into a directed instruction at the exact moment someone changes a count.

- **D-13:** A Catch2 test pins the same numbers, so DR-4 ("run the full suite before tagging")
  already enforces them. No new CI machinery is introduced by this phase.

### Cross-Repo Issue References (BUG-03)

- **D-14:** Write them exactly as BUG-03 specifies: `Spatial-Media-Lab/OpenSpatialDelay#N`.
  **This form resolves today** — verified 2026-08-11 via `gh`: the repo exists, is public, was
  pushed 2026-05-06, and issue #234 resolves to *"Audio artifacts at buffer sizes below 256
  samples"* (OPEN). An earlier reading in this discussion that the org path would 404 was wrong;
  that concern applies only to `Spatial-Media-Lab/SpatialCore`, which does not exist.
  Sites: `SharedFFTCache.h:17`, `PartitionedConvolver.cpp:7` (`#131`), `:46,89` (`#50`),
  `:120-124` (`#234`). Comment-only, mechanical.

### Remote Topology (user correction — supersedes OQ-5's framing)

- **D-15:** **`AndrewRahman/SpatialCore` is the deliberate development remote, not an accident
  awaiting cleanup.** Development stays on the personal remote until the pipeline is proven. The
  reason is risk containment: `Spatial-Media-Lab/OpenSpatialDelay` is public and **in use by real
  people right now**, so migrating it onto an unproven SpatialCore could break a live plugin.
  Org migration is **gated on proof, not on a date** — SpatialCore, OSD-on-SpatialCore, and
  OpenSpatialPanner all land on the org together once the process is proven.

- **D-16:** Therefore `PROJECT.md`'s OQ-5 is **rewritten as a stated architectural decision with a
  proof gate, not left as an open question.** Its current framing ("migration pending; the guide is
  wrong today") is wrong on both halves — nothing is pending, and the personal remote is
  correct-by-design.

- **D-17:** `docs/integration-guide.md` carries the **working dev remote
  (`AndrewRahman/SpatialCore`) as the live `git submodule add` instruction**, with the
  `Spatial-Media-Lab/SpatialCore` URL labelled as the post-proof destination. During v1 there is no
  third-party consumer to serve — the only person following this guide is the user migrating OSD,
  and a guide that fails on line one serves nobody. This is D-03 applied.

### Claude's Discretion

- **D-01 tier split** — the user answered "you decide" on doc surface scope. The A/B boundary,
  and the choice to stamp rather than renumber Tier B, are Claude's calls. A planner may adjust
  which specific file lands in which tier, but not the two-tier principle.
- **D-10 through D-13 drift mechanism** — the user answered "you decide." The `static_assert`
  + Catch2 choice, the rejection of doc-grep CI, and the `constexpr`-adjacent-to-enum rule are
  Claude's calls, grounded in DR-3 (`-Wswitch` in consumers) and DR-4 (suite runs before tagging).
- **Planning-doc corrections** — `PROJECT.md` and `REQUIREMENTS.md` contain their own factual
  errors discovered here: OQ-5's framing (D-16), the nonexistent `DSP/Utilities.h` path (D-09),
  and API-05's now-stale claim that CLAUDE.md "omits the `Engine/` module" (it does not — the row
  is present at `CLAUDE.md:20`). **Recommendation: correct them in this phase.** They mis-steer
  planning by the same mechanism as `CLAUDE.md`, which is the phase's stated target. This was
  raised at wrap-up and the user elected to proceed without a separate discussion, so treat it as
  a recommendation the planner may scope, not a locked requirement.

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Governing plan documents
- `.planning/ROADMAP.md` §"Phase 1: Documentation Truth & Contract Freeze" — the four success
  criteria this phase is measured against.
- `.planning/REQUIREMENTS.md` §"Public Contract Resolution" (lines 45–89) — API-01 through API-05
  acceptance criteria; §BUG-03 (line 244).
- `.planning/PROJECT.md` §"Key Decisions" — DR-1 through DR-7 locked rules; §"Open Questions" for
  the OQ-1/2/3/5 history. **Note: contains errors this phase should correct — see D-09, D-16.**

### Tier A — surfaces to correct
- `CLAUDE.md` — architecture table (JUCE 8→9.0.0, 22→23 formats, 13→15 layouts, delete the phantom
  `DSP/` row per D-09). The `Engine/` row is already present at `:20`; API-05's claim that it is
  missing is stale.
- `README.md` — `:34` "6 HRTF Profiles", `:36` "22 formats", `:43` "13 ITU-R", `:95` "JUCE 8".
- `docs/integration-guide.md` — `:7` "JUCE 8", `:166` "22 output formats", and the submodule URL
  per D-17.
- `.claude/skills/spatialcore-architecture/spatialcore-architecture.md` — `:42` 7 algorithms,
  `:44` 6 HRTF profiles, `:45` 22 formats, `:46` 13 layouts. Auto-loads every session.
- `.claude/skills/spatial-audio-dsp/SKILL.md:518` — "JUCE 8.0.3".

### Tier B — stamp historical, do not renumber
- `docs/development-roadmap.md` — its "What Does NOT Exist Yet" section lists all five extraction
  requirements as missing; all five are implemented and tested.
- `docs/superpowers/plans/2026-03-22-scaffold-spatialcore.md` — the "SPEC". Hard ceiling at `:711`;
  DR-15 (superseded by DR-5) originates here.
- `.planning/intel/*.md` — derived from the 2026-08-09 ingest; `decisions.md:69` still says JUCE 8.

### Code — ground truth for the numbers
- `include/SpatialCore/Core/SpatialMath.h:98-106` — **the limiter contract (D-06).** `softClip()`
  at `:85`, `distanceAttenuation()` at `:110`.
- `include/SpatialCore/IO/OutputFormat.h:9` — `OutputFormat` enum; no count sentinel exists yet.
- `include/SpatialCore/IO/SpeakerLayout.h:44` — `NUM_LAYOUT_DEFS` sentinel already present.
- `include/SpatialCore/SpatialCore.h` — umbrella header; the 8 algorithm includes are at `:17-25`.
- `include/SpatialCore/Core/Types.h:87` — public `profileIndex` (D-05).
- `CMakeLists.txt:28-36` — JUCE `GIT_TAG 9.0.0`.
- `tests/Core/SpatialMathTests.cpp:66-95` — existing `outputLimiter` coverage.

### Conflict history
- `.planning/INGEST-CONFLICTS.md` — the 11 warnings from the 2026-08-09 ingest. Several are already
  resolved by the re-map; read before re-litigating any of them.

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- **`NUM_LAYOUT_DEFS`** (`include/SpatialCore/IO/SpeakerLayout.h:44`) — an existing, shipped count
  sentinel used at `src/IO/SpeakerLayout.cpp:19,81-83`. It is the precedent and the assert target
  for layouts; no new constant needed there.
- **`tests/Core/SpatialMathTests.cpp:66-95`** — already pins `outputLimiter` for non-finite input,
  the exact `1.2589 * tanh(x/1.2589)` formula, odd symmetry, and the ±1000 asymptote. **Success
  criterion 2 is mostly already satisfied.** The remaining gap is explicit at/below/above-ceiling
  cases. This is a small extension, not new test work — plan it as such.
- The Catch2 suite (144 `TEST_CASE`s, 1585 assertions, 16 files) builds and passes. A counts test
  drops into the existing harness with no new infrastructure.

### Established Patterns
- **Header-adjacent contracts.** `SpatialMath.h` already carries provenance comments
  (`:78-81` cites the exact OSD source lines the functions were consolidated from) and behavioral
  docblocks (`:98-99` describes the tanh curve as C-infinity continuous). D-06 extends a pattern
  that exists rather than inventing one.
- **`inline` in public headers.** `outputLimiter`/`softClip`/`distanceAttenuation` are all `inline`
  and compiled into consumer binaries — which is exactly why D-08 forbids touching them.
- **Bare `#N` issue citations.** The existing style cites OSD issues unqualified. BUG-03 changes
  the convention; apply it consistently to any new comment written in this phase.

### Integration Points
- `include/SpatialCore/IO/OutputFormat.h` and `SpeakerLayout.h` — where the new `constexpr` count
  constants and `static_assert`s land.
- `tests/Core/` — where the counts test lands, alongside `SpatialMathTests.cpp`.
- `.claude/skills/` — an unusual integration point, but a real one: skills auto-load and are
  therefore part of the doc contract (D-02).

### Constraints Carried In
- **DR-2 / DR-7:** the `SpatializationAlgorithm` interface is frozen. Nothing here touches it.
- **DR-3:** OpenSpatialDelay must keep building clean — the direct reason for D-11.
- **DR-4:** full suite before tagging — the enforcement mechanism D-13 relies on.

</code_context>

<specifics>
## Specific Ideas

- **"Simple (Woodworth) is a profile, because it's user-selectable in the UI."** The user's framing
  for D-05, and the reason the answer is "both numbers" rather than "5". The UI is the arbiter of
  what counts as public surface here, not the file count on disk.
- **"To prevent breaking a live plugin which is currently in use by people."** The user's stated
  reason for the personal-remote topology (D-15). This is the phrase to preserve — it explains why
  the arrangement is correct rather than transitional, and it is the same instinct behind the whole
  zero-regressions milestone gate.
- **Migration is gated on proof, not on a date.** SpatialCore, OSD-on-SpatialCore, and
  OpenSpatialPanner ship to the org together once the pipeline is proven.

</specifics>

<deferred>
## Deferred Ideas

- **Possible `profileIndex` off-by-one in the LF shelf.** `src/Binaural/BinauralRenderer.cpp:106`
  reads `lfShelfActive = (profileIndex == 5);` with the comment *"Only MIT KEMAR needs
  compensation"*. Index 5 is the last SOFA slot, but `mit_kemar_large_pinna.sofa` is 4th of 5
  alphabetically — whether index 5 is MIT KEMAR depends on load order, not directory listing.
  **This is a behavior defect, not a doc-truth item.** Discovered while verifying D-05.
  → **Phase 3** (Binaural Defects & HRTF Packaging). File as an issue if confirmed.

- **Selectable hard-clamp limiter mode.** Already recorded as v2 candidate LIMIT-01. Additive, does
  not break DR-2. Not revisited here.

- **A living `docs/api-contract.md`** collecting every frozen public contract in one place.
  Considered and rejected for Phase 1 in favour of D-06 (header-adjacent). Worth reconsidering at
  v3, when a third-party audience exists and the org repo is public.

</deferred>

---

*Phase: 1-documentation-truth-contract-freeze*
*Context gathered: 2026-08-11*
