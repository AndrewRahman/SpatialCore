# Phase 1: Documentation Truth & Contract Freeze - Research

**Researched:** 2026-08-11
**Domain:** Documentation accuracy audit + compile-time drift prevention (C++/JUCE static library, no new dependencies)
**Confidence:** MEDIUM — codebase facts are HIGH (directly verified this session); one locked ground-truth number in CONTEXT.md/REQUIREMENTS.md/ROADMAP.md does not match the tree (see Critical Finding below), which caps overall confidence until resolved.

## Summary

This phase has no new stack to research — it is a truth-alignment exercise between five doc
surfaces and the existing, working `include/`/`src/` tree, plus a mechanical `#N` → qualified-issue
rewrite in code comments. The two technical additions are a `constexpr` count-sentinel-adjacent-to-
enum pattern (no existing precedent in this codebase — `grep -rn static_assert include/ src/` [VERIFIED: repo-wide grep, zero matches] returns nothing, so this phase introduces the pattern) and a
small extension to the existing `outputLimiter()` Catch2 coverage.

**Critical finding, discovered during this research session:** independently reading
`include/SpatialCore/IO/OutputFormat.h`, `include/SpatialCore/IO/OutputFormatRegistry.h`, and
`src/IO/OutputFormatRegistry.cpp`/`src/IO/SpeakerLayout.cpp` shows **23 output formats and 15
speaker layouts currently ship**, not the 25 and 14 that CONTEXT.md's D-04 (marked "one-way —
frozen public contract"), REQUIREMENTS.md API-04, and ROADMAP.md's Phase 1 *and* Phase 2 success
criteria all state as "verified from the tree on 2026-08-10." Three independent code locations
agree on 23/15 (enum literal count, the registry's own `NUM_OUTPUT_FORMATS`/`NUM_LAYOUT_DEFS`
constants, and the initializer-list row counts in both `.cpp` files); none of the five doc surfaces,
nor CONTEXT.md, nor REQUIREMENTS.md, contains a 23 or a 15 anywhere. This is the same defect class
(OQ-1/OQ-3, a doc-counting error) the phase exists to eliminate — it just slipped through the
ground-truth verification pass itself. See **Common Pitfall 1** and **Open Question 1** — this
must be resolved with the user before any `static_assert` is written, because D-04 is explicitly
locked as one-way ("Publishing a number and later changing it breaks consumers").

**Primary recommendation:** Treat every doc-surface number fix as a small, mechanical, high-count
diff against the exact line numbers verified in this document — do not re-derive counts from
scratch during planning or execution, re-use the ones below. Resolve the 25/14-vs-23/15 conflict
as a blocking pre-planning question, not a Claude's-discretion item; it changes the literal
constant every `static_assert` in this phase will assert against.

## Architectural Responsibility Map

| Capability | Primary Tier | Secondary Tier | Rationale |
|------------|-------------|----------------|-----------|
| Public contract count correctness (algorithms/formats/layouts/HRTF) | Library headers (`include/SpatialCore/**`) | Docs (README/CLAUDE.md/guide/skills) | The header + adjacent `.cpp` initializer list is the runtime source of truth; docs must mirror it, never the reverse |
| Compile-time drift prevention | Library headers (`constexpr` + `static_assert`) | Test suite (Catch2, `tests/Core`) | `-Wswitch`-safe sentinel constants live next to the enum (DR-3); the test suite is the DR-4-mandated enforcement backstop, not the primary guard |
| Limiter transfer-function contract | Library header docblock (`SpatialMath.h:98-99`) | Test suite (`SpatialMathTests.cpp`) | D-06 locks the header docblock as the sole governing contract; no separate spec doc is created |
| Cross-repo issue reference correctness | Code comments (`include/`, `src/`) | — | Comment-only, mechanical; no runtime or doc-surface component owns this |
| Doc-surface truth (CLAUDE.md, README, integration guide, skills) | Docs (repo root + `.claude/skills/`) | — | These are the two "readers" the phase serves (consumer + auto-loading Claude session); no code change backs them |

## Project Constraints (from CLAUDE.md)

- **No allocation, locks, or logging in any function called from `processBlock`.** Not exercised by
  this phase (comments/docblocks/`constexpr`/`static_assert`/tests only), but any new test helper
  must not be linked into the audio-thread path.
- **NEVER modify the `SpatializationAlgorithm` interface without a major version bump.** This phase
  touches zero virtual methods — count sentinels are free-standing `constexpr int`s per D-11, never
  new enumerators, so DR-2/DR-7 is not implicated.
- **ALWAYS maintain backward compatibility with existing plugins.** D-11's rejection of new
  enumerators exists specifically to protect this rule (`-Wswitch` in OSD's exhaustive switches).
- **ALWAYS run the full test suite before tagging a release.** This is the enforcement mechanism
  D-13 relies on for the new counts test — no new CI machinery is introduced.
- **HRTF profiles are Git-LFS-tracked raw `.sofa` files loaded at runtime, not BinaryData.** CLAUDE.md's
  own Build System line (`:74`) already states this correctly [VERIFIED: `CLAUDE.md:74`, quoted in
  Common Pitfall 2 below] — this phase's CLAUDE.md fix is scoped to the architecture *table*
  (JUCE version, format/layout counts, phantom `DSP/` row), not this already-correct prose line.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions

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

- **D-04:** Canonical counts: **8** algorithms, **25** output formats, **14** speaker layouts,
  **JUCE 9.0.0**, **5** SOFA HRTF profiles shipped.
  Verified: `grep -l "public SpatializationAlgorithm" include/` → 8; `ls HRTF/*.sofa` → 5;
  `CMakeLists.txt` `GIT_TAG 9.0.0`; `NUM_LAYOUT_DEFS` at `include/SpatialCore/IO/SpeakerLayout.h:44`.
  — **Reversibility:** one-way — these are the frozen public contract under DR-7. Publishing a
  number and later changing it breaks consumers who wrote loops or switches against it.
  **⚠️ RESEARCH FLAG (2026-08-11): the 25/14 half of this decision does not match the tree — see
  Critical Finding in Summary, Common Pitfall 1, and Open Question 1 below. The 8/5/9.0.0 half is
  independently re-verified and confirmed correct this session.**

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
  The doc-grep half is the expensive, brittle half — grepping prose for "8" and "25" yields false
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
  **⚠️ RESEARCH FLAG: this session's exhaustive `grep -rn "issue #[0-9]"` found this site list is
  a sample, not exhaustive — see Common Pitfall 3 and the full site table in Code Examples below.**

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
  **Research adds one more item to this list: the 25/14 vs 23/15 count conflict (see Open
  Question 1) — same mis-steering risk, higher stakes because D-04 is otherwise locked one-way.**

### Deferred Ideas (OUT OF SCOPE)

- **Possible `profileIndex` off-by-one in the LF shelf.** `src/Binaural/BinauralRenderer.cpp:106`
  reads `lfShelfActive = (profileIndex == 5);` with the comment *"Only MIT KEMAR needs
  compensation"*. Index 5 is the last SOFA slot, but `mit_kemar_large_pinna.sofa` is 4th of 5
  alphabetically — whether index 5 is MIT KEMAR depends on load order, not directory listing.
  **This is a behavior defect, not a doc-truth item.** Discovered while verifying D-05.
  → **Phase 3** (Binaural Defects & HRTF Packaging). File as an issue if confirmed.
  [VERIFIED: `src/Binaural/BinauralRenderer.cpp:106`, quoted above — confirmed present, unchanged,
  this session.]

- **Selectable hard-clamp limiter mode.** Already recorded as v2 candidate LIMIT-01. Additive, does
  not break DR-2. Not revisited here.

- **A living `docs/api-contract.md`** collecting every frozen public contract in one place.
  Considered and rejected for Phase 1 in favour of D-06 (header-adjacent). Worth reconsidering at
  v3, when a third-party audience exists and the org repo is public.
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| API-01 | Public algorithm count is one number across headers and docs | Confirmed 8 via three independent sources this session: `AllAlgorithms.h` (8 concrete `#include`s beyond the base), `grep -rl "public SpatializationAlgorithm" include/ src/` (8 files), and `CLAUDE.md:6,18`'s existing (already-correct) prose. No doc-surface fix needed for the *number* in CLAUDE.md; README.md `:21` "(7)" and `.claude/skills/spatialcore-architecture/…md:42` "7 Spatialization Algorithms" (missing `ConstantPower` from the name list) still need the fix. |
| API-02 | `outputLimiter()` has one defined transfer function | D-06/D-07/D-08 fully resolve the decision. Code verified unchanged at `include/SpatialCore/Core/SpatialMath.h:100-106`. Existing test coverage at `tests/Core/SpatialMathTests.cpp:66-98` already exercises non-finite, exact-formula-at-multiple-x (including at/below/above ceiling), odd symmetry, and asymptote — see Common Pitfall 4 for the precise gap remaining. |
| API-03 | HRTF profile count is one number across docs, headers, shipped data | Confirmed 5 `.sofa` files in `HRTF/` this session (`ls -la HRTF/*.sofa`, 5 results, all real HDF5, 1.16–36.6 MB). D-05's "two numbers together" phrasing is the acceptance bar, not a bare "5". |
| API-04 | Output-format and speaker-layout counts match the tree | **Conflict found — see Critical Finding / Open Question 1.** Tree shows 23 formats (not 25) and 15 layouts (not 14). This requirement cannot be closed against D-04's stated numbers as written; it needs the corrected numbers once Open Question 1 is resolved. |
| API-05 | CLAUDE.md describes the library that actually exists | `Engine/` row already present (`CLAUDE.md:20`) — API-05's original claim it's missing is stale, confirmed by direct read this session. Remaining real gaps: JUCE version (`:72` "JUCE 8" → 9.0.0), architecture-table format/layout counts (`:21`), phantom `DSP/` row (`:24`, confirmed `include/SpatialCore/DSP/` does not exist via `find`). HRTF-loading prose (`:74`) is already correct — no change needed there. |
| BUG-03 | Cross-repo issue references in code comments are qualified | D-14's 4 cited sites confirmed present at the cited lines, but a repo-wide `grep -rn "issue #[0-9]"` / `#[0-9]"` across `include/` and `src/` found **19 sites across 10 files**, all resolving in `Spatial-Media-Lab/OpenSpatialDelay` (verified live via `gh issue view` for all 10 distinct issue numbers), none resolving in `AndrewRahman/SpatialCore`. Full table in Code Examples. |
</phase_requirements>

## Standard Stack

No new libraries, frameworks, or dependencies are introduced by this phase. It operates entirely
within the existing toolchain already locked by prior phases.

### Core (existing, unchanged)
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| JUCE | 9.0.0 [VERIFIED: `CMakeLists.txt:32`, `GIT_TAG        9.0.0`] | Audio plugin framework | Already the project's framework; this phase only corrects doc mentions of the stale "JUCE 8" |
| Catch2 | v3.7.1 [VERIFIED: `tests/CMakeLists.txt:5`, `GIT_TAG v3.7.1`] | Test framework | Existing test harness; the new counts test and limiter-boundary test extension drop into it |
| libmysofa | v1.3.2 [VERIFIED: `CMakeLists.txt:50`, `GIT_TAG v1.3.2`] | SOFA HRTF parsing | Unaffected by this phase; mentioned only because HRTF count docs reference it indirectly |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| `static_assert` + Catch2 test (D-10) | A doc-grep CI script (`grep -c "8 algorithm" *.md` style) | Rejected by the user: brittle, false-positive-prone, and becomes its own maintenance burden. The compile-time half already catches drift at the source when a dev adds an `OutputFormat` or algorithm. |
| `constexpr` count adjacent to enum (D-11) | New enumerator (e.g. `NumFormats` inside `enum class OutputFormat`) | Rejected: trips `-Wswitch` in any consumer (OSD) holding an exhaustive `switch` over the enum — violates DR-3. `NUM_LAYOUT_DEFS` is a pre-existing, already-shipped exception (it's a real enumerator OSD's switches already handle), not a new precedent. |

**Installation:** None — no new packages. All work is header/comment/prose edits plus one new
test file section, using the existing CMake/Catch2/FetchContent setup already in the tree.

## Package Legitimacy Audit

**Not applicable.** This phase installs no external packages (no `npm install`, `pip install`,
`cargo add`, or new `FetchContent_Declare` targets). All dependencies used by the phase's own
test extension (Catch2) are already declared and pinned in `tests/CMakeLists.txt:2-7`.

## Architecture Patterns

### System Architecture Diagram

```
                    ┌─────────────────────────────────────────┐
                    │   Source of truth: include/ + src/       │
                    │   (enum literals, initializer-list rows,  │
                    │    SpatialMath.h docblock, CMakeLists.txt)│
                    └───────────────┬───────────────────────────┘
                                    │ (this phase: read-only on
                                    │  behavior, comment/const additions)
                    ┌───────────────▼───────────────────────────┐
                    │  New: constexpr count sentinels            │
                    │  (adjacent to OutputFormat, algorithm       │
                    │   headers) + static_assert against them     │
                    └───────────────┬───────────────────────────┘
                                    │ compile-time enforcement
                    ┌───────────────▼───────────────────────────┐
                    │  New/extended: Catch2 counts test +         │
                    │  outputLimiter at/below/above-ceiling test   │
                    │  (tests/Core/, existing harness)             │
                    └───────────────┬───────────────────────────┘
                                    │ DR-4: full suite before tagging
                    ┌───────────────▼───────────────────────────┐
                    │  Doc surfaces (Tier A — corrected in place):│
                    │  CLAUDE.md, README.md, integration-guide.md, │
                    │  spatialcore-architecture skill,             │
                    │  spatial-audio-dsp SKILL.md:518               │
                    └───────────────┬───────────────────────────┘
                                    │ (Tier B — stamped, not renumbered)
                    ┌───────────────▼───────────────────────────┐
                    │  docs/development-roadmap.md,                │
                    │  2026-03-22 scaffold plan, .planning/intel/*  │
                    │  → header: "Historical — superseded by        │
                    │    .planning/ROADMAP.md, 2026-08-10"          │
                    └────────────────────────────────────────────┘

Two readers consume the Tier A surfaces independently:
  Consumer (human)  → README.md, docs/integration-guide.md
  Claude session     → CLAUDE.md (auto-loaded every session),
                        .claude/skills/spatialcore-architecture/*.md (auto-loaded)
```

### Recommended Project Structure

No new directories. Edits land in existing locations:
```
include/SpatialCore/IO/
├── OutputFormat.h           # add NUM_OUTPUT_FORMATS-equivalent constexpr adjacent to enum (D-11)
└── SpeakerLayout.h          # NUM_LAYOUT_DEFS already exists — assert against it, no new constant
include/SpatialCore/Algorithms/
└── (new small header or existing umbrella)  # constexpr algorithm-count sentinel (D-11)
tests/Core/
└── SpatialMathTests.cpp     # extend outputLimiter coverage (small, not new infra)
tests/                        # counts test lands alongside SpatialMathTests.cpp, same harness
```

### Pattern 1: Count sentinel adjacent to enum, never inside it

**What:** A free-standing `constexpr int` declared next to (not inside) the enum it counts,
combined with a `static_assert` at the definition site of the array/registry that must stay in
sync.
**When to use:** Any time a public enum's cardinality must be provable at compile time without
adding a new enumerator (which would break `-Wswitch`-clean consumers).
**Example (pattern to follow — `OutputFormat.h` currently has no sentinel; `OutputFormatRegistry.h`
already has one that plays this exact role for the registry table):**
```cpp
// Source: include/SpatialCore/IO/OutputFormatRegistry.h:9 [VERIFIED, read this session]
static constexpr int NUM_OUTPUT_FORMATS = 23;

// Source: include/SpatialCore/IO/SpeakerLayout.h:41-45 [VERIFIED, read this session]
enum LayoutID
{
    Quad, S5_0, S5_1, S7_0, S7_1, S9_1, S5_1_2, S5_1_4, S7_1_2, S7_1_4, S7_1_6,
    S9_1_4, S9_1_6, Octaphonic, SML13_1, NUM_LAYOUT_DEFS
};
```
`NUM_LAYOUT_DEFS` is D-11's named exception — it is already an enumerator, already shipped, and the
plan should assert against it (`static_assert(NUM_LAYOUT_DEFS == <verified count>, "...")`) rather
than add a duplicate `constexpr`. `OutputFormat.h` has no equivalent inside its own enum — the
registry's `NUM_OUTPUT_FORMATS` is a separate constant in a separate header
(`OutputFormatRegistry.h`) — so D-11 asks for a comparable sentinel to live directly beside
`enum class OutputFormat` in `OutputFormat.h` itself, keeping the two in sync via `static_assert`.

### Pattern 2: Header-adjacent contract docblock (D-06)

**What:** The behavioral contract for a public function lives in the docblock directly above the
function, not in a separate spec document.
**When to use:** For small, stable, `inline` public API surfaces where a separate doc would drift.
**Example — already-present pattern this phase extends, not invents:**
```cpp
// Source: include/SpatialCore/Core/SpatialMath.h:98-106 [VERIFIED, read this session]
// Output Limiter -- tanh-based soft ceiling for speaker protection during self-oscillation
// C-infinity continuous (no derivative discontinuities), asymptotes to +/-threshold
inline float outputLimiter (float x)
{
    if (! std::isfinite (x))
        return 0.0f;
    const float threshold = 1.2589f;  // +2 dB
    return threshold * std::tanh (x / threshold);
}
```
This docblock **already** describes the shipped tanh curve correctly — D-06 designates it the
governing contract as-is; no wording change is required here. The only Tier-B action is stamping
the March scaffold plan's contradictory hard-clamp language (`:711`) historical (D-07).

### Anti-Patterns to Avoid
- **Doc-grep CI scripts (rejected, D-10):** grepping prose for bare numbers ("8", "25") produces
  false positives on unrelated numerals and becomes a second thing to maintain. Use compile-time
  assertions instead.
- **New enumerators purely for counting (rejected, D-11):** breaks `-Wswitch`-clean consumer code
  (OSD) — a public API change requiring a major version bump under DR-2, disproportionate to a
  doc-truth fix.
- **Editing Tier B historical docs' numbers (rejected, D-01/D-03):** creates a second source of
  truth that will drift again; stamp them historical instead.
- **Amending the March scaffold plan to be the new governing spec (rejected, D-07):** contradicts
  D-01's Tier B "stamp, don't renumber" rule and duplicates D-06's header-docblock contract.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Doc/code count drift detection | A prose-grepping CI script | `constexpr` sentinel + `static_assert` + Catch2 test (D-10) | Compile-time enforcement is exact; prose-grepping is heuristic and generates false positives the moment "8" appears in an unrelated sentence |
| Cross-repo issue link correctness | A custom link-checker script that hits the GitHub API at build time | One-time `gh issue view <n> --repo Spatial-Media-Lab/OpenSpatialDelay` verification during planning/execution, then a plain qualified-string rewrite | The requirement (BUG-03) is about the *comment text* being unambiguous, not about live-checking link validity on every build — that would add a network dependency to the build |

**Key insight:** This phase's entire technical surface is "make comments/constants/prose agree with
code that already exists and already works." The temptation to over-engineer (live link-checking,
prose-parsing CI) works against D-03's minimalism and D-10's explicit rejection of a doc-grep
script.

## Runtime State Inventory

BUG-03 is a string-replacement task (bare `#N` → `Spatial-Media-Lab/OpenSpatialDelay#N` in code
comments), which triggers this section's checklist. All five categories were checked explicitly
this session:

| Category | Items Found | Action Required |
|----------|-------------|------------------|
| Stored data | None — the string being replaced (`#N`) exists only inside `//` and `/* */` C++ comments in `include/`/`src/`, never in a database, config value, or persisted key. | None |
| Live service config | None — no external service (n8n, Datadog, etc.) references these comment strings. GitHub issue numbers `#131`, `#50`, `#234`, `#96`, `#47`, `#89`, `#37`, `#100`, `#168`, `#179` are read-only references *to* GitHub's tracker; rewriting the comment text does not touch the tracker itself. | None |
| OS-registered state | None — comments are not embedded into any OS-level registration (Task Scheduler, launchd, pm2). | None |
| Secrets/env vars | None — no env var or secret name contains these issue numbers. | None |
| Build artifacts | None — issue-number comments are stripped by the compiler; no build artifact embeds the comment text, so no rebuild-triggered drift is possible from this change beyond the normal recompile. | None (normal recompile only, since the file content changes) |

**Nothing found in any category** — BUG-03 is a pure comment-text edit with zero runtime,
service, OS, secret, or artifact surface. This confirms D-14's own characterization ("Comment-only,
mechanical").

## Common Pitfalls

### Pitfall 1: The phase's own "frozen" ground-truth numbers don't match the tree (CRITICAL)

**What goes wrong:** CONTEXT.md's D-04 (marked one-way/frozen), REQUIREMENTS.md's API-04, and
ROADMAP.md's Phase 1 success criterion 1 *and* Phase 2 success criteria 1/3 all state **25 output
formats and 14 speaker layouts**, "verified from the tree on 2026-08-10." Independently reading the
actual headers this session shows **23 and 15**:
- `include/SpatialCore/IO/OutputFormat.h:6-7` [VERIFIED, quoted]: `// 1 Binaural + 1 Stereo + 15
  Surround + 6 Ambisonics = 23 total` — the enum's own comment states 23, and a full parse of the
  `enum class OutputFormat { ... }` body confirms exactly 23 named values.
- `include/SpatialCore/IO/OutputFormatRegistry.h:9` [VERIFIED, quoted]: `static constexpr int
  NUM_OUTPUT_FORMATS = 23;`
- `src/IO/OutputFormatRegistry.cpp:13-46` [VERIFIED]: the `OutputFormatRegistry::table` initializer
  list has exactly 23 rows (counted programmatically this session).
- `include/SpatialCore/IO/SpeakerLayout.h:41-45` [VERIFIED, quoted]: `enum LayoutID { Quad, S5_0,
  S5_1, S7_0, S7_1, S9_1, S5_1_2, S5_1_4, S7_1_2, S7_1_4, S7_1_6, S9_1_4, S9_1_6, Octaphonic,
  SML13_1, NUM_LAYOUT_DEFS };` — 15 names before the sentinel, so `NUM_LAYOUT_DEFS == 15`.
- `src/IO/SpeakerLayout.cpp:19-66` [VERIFIED]: `static const LayoutDef layoutDefs[NUM_LAYOUT_DEFS]
  = { ... }` has exactly 15 designated rows (Quad through SML 13.1, counted this session).

Git history rules out a recent code change explaining the gap: `git log -- include/SpatialCore/IO/
OutputFormat.h include/SpatialCore/IO/SpeakerLayout.h` shows the last edit to either file was
`149d50d`, dated **2026-07-05** — five weeks before the "verified 2026-08-10" ground-truth pass.
Both files were already at 23/15 when that verification was performed.
**Why it happens:** This looks like the same miscounting-error class as the already-closed OQ-1/
OQ-3 (algorithm and HRTF counts), except this time the error occurred in the "ground truth"
verification pass itself rather than in a doc surface. `.planning/intel/constraints.md:40`
[VERIFIED, quoted] shows the original scaffold design listed 22 named formats (missing
`Surround9_1`, added later per the "ITU-R BS.2051 System H" comment) — so "22" was briefly correct
at design time, drifted to 23 when a format was added, and the 2026-08-10 pass may have taken "22"
from the design doc and added 3 rather than re-parsing the live enum, landing on 25 instead of 23.
This is speculative reconstruction, not a verified root cause — the important fact is simply that
the current header state is 23/15, confirmed by three independent code locations.
**How to avoid:** Do not write a `static_assert(NUM_OUTPUT_FORMATS == 25, ...)` or `static_assert
(NUM_LAYOUT_DEFS == 14, ...)` — both would fail to compile immediately against the current tree.
Surface this conflict to the user before planning locks in a number (see Open Question 1) — D-04
is explicitly one-way ("breaks consumers who wrote loops or switches against it"), so silently
"correcting" it to 23/15 without a decision point skips exactly the kind of user confirmation this
phase's own process (OQ-1 → user, OQ-3 → user) modeled for the first two count conflicts.
**Warning signs:** Any plan step that writes `25` or `14` into a `static_assert`, doc line, or test
assertion should be treated as suspect until Open Question 1 is closed.

### Pitfall 2: Not every "known inaccuracy" in REQUIREMENTS.md/PROJECT.md is still true

**What goes wrong:** REQUIREMENTS.md's API-05 acceptance criteria (written 2026-08-09/10) list four
inaccuracies including "SOFA files 'embedded as BinaryData' (they load from disk)" and "the
component table omits the `Engine/` module entirely." Reading current `CLAUDE.md` this session
shows both of these are **already fixed**: `CLAUDE.md:74` [VERIFIED, quoted] reads *"HRTF data: 5
SOFA files (Git LFS tracked), loaded at runtime via `HRTFDatabase::loadFromFile` — no BinaryData
compilation step is involved,"* and `CLAUDE.md:20` [VERIFIED, quoted] has an `Engine` row present.
`git log -- CLAUDE.md` shows two intervening commits (`87cb7a3 docs: correct stale
BinaryData-embedding claims for HRTF SOFA files (CR-01)`, `2e3b090 docs: document RenderEngine as
the consumer render facade`) already closed these two items before this phase started.
**Why it happens:** REQUIREMENTS.md was rewritten 2026-08-10 but CLAUDE.md kept receiving fixes
after that snapshot (commit `2e3b090`, `3da7d89` are also post-dated relative to some intel docs).
**How to avoid:** Treat REQUIREMENTS.md/PROJECT.md's specific inaccuracy *lists* as historical
motivation, not a literal current-state checklist — always re-verify against the live file before
writing a plan task. CONTEXT.md's canonical_refs already does this correctly ("The `Engine/` row is
already present at `:20`; API-05's claim that it is missing is stale") — this research confirms
that correction and extends it to the BinaryData claim as well.
**Warning signs:** A plan task that says "add the missing Engine/ row to CLAUDE.md" or "fix the
BinaryData claim in CLAUDE.md" would be redundant work against a no-op diff.

### Pitfall 3: BUG-03's four cited sites are a sample, not the full sweep

**What goes wrong:** D-14 cites four sites (`SharedFFTCache.h:17`, `PartitionedConvolver.cpp:7,
46/89, 120-124`) as "the" BUG-03 sites. A repo-wide `grep -rn "issue #[0-9]"` across `include/` and
`src/` this session found **19 occurrences across 10 files**, and the D-14 line numbers for
`PartitionedConvolver.cpp` are themselves slightly off (`:46` and `:89` do not contain issue refs;
the real lines are `:50` and `:90`; `:120-124` is `:122` for the primary reference and `:172` for a
secondary mention of the same issue).
**Why it happens:** D-14 appears to have been written from a partial grep during CONTEXT-gathering,
scoped to the Binaural subsystem only, rather than a full `include/`+`src/` sweep.
**How to avoid:** Use the full verified table in Code Examples below as the BUG-03 site list for
planning, not D-14's four-site sample. Success Criterion 4 ("**every** `#NNN`") requires the
exhaustive list.
**Warning signs:** A plan that only touches the 4 D-14 sites will leave 15 more bare `#N` references
unqualified, failing Success Criterion 4 on re-verification.

### Pitfall 4: The `outputLimiter` boundary-condition test may already be done

**What goes wrong:** CONTEXT.md's code_context section says Success Criterion 2's test-pinning is
"mostly already satisfied. The remaining gap is explicit at/below/above-ceiling cases." Reading
`tests/Core/SpatialMathTests.cpp:73-81` [VERIFIED, quoted below] this session shows the existing
`TEST_CASE ("outputLimiter: matches exact 1.2589 * tanh(x/1.2589) formula", ...)` already loops over
`{ -5.0f, -1.2589f, -0.5f, 0.0f, 0.5f, 1.2589f, 2.0f, 5.0f, 100.0f }` — this set already includes
values exactly at the ceiling (±1.2589), below it (±0.5, 0.0), and above it (±5.0, ±2.0/100.0), plus
a separate `TEST_CASE` for non-finite input at `:66-70`.
```cpp
// Source: tests/Core/SpatialMathTests.cpp:73-81 [VERIFIED, read this session]
TEST_CASE ("outputLimiter: matches exact 1.2589 * tanh(x/1.2589) formula", "[spatialmath][outputlimiter]")
{
    const float threshold = 1.2589f;
    for (float x : { -5.0f, -1.2589f, -0.5f, 0.0f, 0.5f, 1.2589f, 2.0f, 5.0f, 100.0f })
    {
        float expected = threshold * std::tanh (x / threshold);
        CHECK_THAT (outputLimiter (x), WithinAbs (expected, 1e-6f));
    }
}
```
**Why it happens:** The existing test satisfies the success criterion's *substance* but not
necessarily its *legibility* — a single parameterized loop over 9 values doesn't self-document
"this specifically covers at/below/above the ceiling" the way three separately named `TEST_CASE`s
would for someone auditing Success Criterion 2 later.
**How to avoid:** Plan a small, low-risk task — either (a) verify the existing loop already
satisfies the criterion and note it as evidence rather than write new test code, or (b) split it
into three named cases (`"at ceiling"`, `"below ceiling"`, `"above ceiling"`) purely for
auditability, with identical assertions. Either is a small task; don't scope it as "add missing
boundary tests," since the boundary values are already present.
**Warning signs:** A plan that treats this as a from-scratch test-writing task is over-scoping a
near-complete item.

## Code Examples

### BUG-03 — full verified site inventory (supersedes D-14's 4-site sample)

All ten distinct issue numbers below were verified live via `gh issue view <n> --repo
Spatial-Media-Lab/OpenSpatialDelay` this session (all resolve; states noted) and via `gh issue view
<n> --repo AndrewRahman/SpatialCore` (all ten return "Could not resolve to an issue" — confirming
the bare `#N` form is currently either a dead link or, once SpatialCore's own tracker grows past
these numbers, a silently-wrong one).

| File | Line(s) | Bare form | OSD issue state | Qualify to |
|------|---------|-----------|------------------|------------|
| `include/SpatialCore/Binaural/SharedFFTCache.h` | 17 | `(issue #131)` | CLOSED — "CoreGraphics crash with multiple plugin instances loaded simultaneously" | `Spatial-Media-Lab/OpenSpatialDelay#131` |
| `include/SpatialCore/Binaural/PartitionedConvolver.h` | 27 | `(issue #50)` | CLOSED — "Binaural HRTF: perceptual pops on azimuth/elevation movement" | `#50` |
| `include/SpatialCore/Binaural/PartitionedConvolver.h` | 43 | `(issue #131)` | CLOSED (see above) | `#131` |
| `include/SpatialCore/Binaural/PartitionedConvolver.h` | 49 | `(issue #50)` | CLOSED (see above) | `#50` |
| `include/SpatialCore/Binaural/PartitionedConvolver.h` | 55 | `(issue #234)` | OPEN — "Audio artifacts at buffer sizes below 256 samples" | `#234` |
| `include/SpatialCore/Binaural/HRTFDatabase.h` | 58 | `(issue #47)` | CLOSED — "Binaural HRTF: pops/clicks on azimuth/elevation movement (HRIR crossfade needed)" | `#47` |
| `include/SpatialCore/Binaural/BinauralRenderer.h` | 21 | `(issue #96` | CLOSED — "Two plugin instances on same track causes buzzing and Reaper crash" | `#96` |
| `include/SpatialCore/UI/SpatialMapComponent.h` | 131, 132 | `issue #168 round 2/3` | CLOSED — "[Manual] DOC-016: Do UI screenshots match current v1.0 build?" | `#168` |
| `include/SpatialCore/OSC/OSCPortValidation.h` | 9 | `issue #179` | OPEN — "OSC: No validation prevents Send and Receive ports from being set to the same number" | `#179` |
| `src/Binaural/PartitionedConvolver.cpp` | 7 | `(issue #131)` | CLOSED (see above) | `#131` |
| `src/Binaural/PartitionedConvolver.cpp` | 50, 90, 200 | `(issue #50)` | CLOSED (see above) | `#50` (×3) |
| `src/Binaural/PartitionedConvolver.cpp` | 122, 172 | `(issue #234)` | OPEN (see above) | `#234` (×2) |
| `src/Binaural/HRTFDatabase.cpp` | 119 | `(issue #89)` | CLOSED — "HRTF profiles sound filtered/warbled across multiple profiles" | `#89` |
| `src/Binaural/HRTFDatabase.cpp` | 170 | `(issue #47)` | CLOSED (see above) | `#47` |
| `src/UI/PresetBrowser.cpp` | 25, 135 | `issue #37` | CLOSED — "Bug: Reaper crash in macOS Metal GPU driver during plugin use" | `#37` (×2) |
| `src/Trajectory/TrajectoryEngine.cpp` | 185 | `(issue #100)` | CLOSED — "Forward/Reverse direction toggle broken for Bounce, Line, and Random trajectories" | `#100` |
| `src/Engine/RenderEngine.cpp` | 47 | `(issue #131` | CLOSED (see above) | `#131` |

19 total occurrences across 10 files, 10 distinct issue numbers, all rewriting to
`Spatial-Media-Lab/OpenSpatialDelay#N`. [VERIFIED: repo-wide `grep -rn "#[0-9]" include/ src/`,
cross-checked line-by-line this session; `gh issue view` run for all 10 distinct numbers against
both repos.]

### Count-sentinel pattern to add (D-11) — illustrative, exact wording is a planning decision

```cpp
// include/SpatialCore/IO/OutputFormat.h — sentinel to add adjacent to the enum
// (exact placement/naming is a planning decision; NUM_OUTPUT_FORMATS already
// exists as a *separate* constant in OutputFormatRegistry.h:9 — decide whether
// this phase adds a second sentinel in OutputFormat.h itself with a
// static_assert tying the two together, or asserts registry-side only)
enum class OutputFormat { /* ...23 current values, unchanged... */ };
// static_assert message per D-12, e.g.:
// static_assert(NUM_OUTPUT_FORMATS == <verified count>,
//     "OutputFormat count changed -- update CLAUDE.md, README.md, "
//     "docs/integration-guide.md, .claude/skills/spatialcore-architecture/"
//     "spatialcore-architecture.md");
```

```cpp
// include/SpatialCore/IO/SpeakerLayout.h — NUM_LAYOUT_DEFS already exists (D-11 exception)
// Source: include/SpatialCore/IO/SpeakerLayout.h:41-45 [VERIFIED]
enum LayoutID
{
    Quad, S5_0, S5_1, S7_0, S7_1, S9_1, S5_1_2, S5_1_4, S7_1_2, S7_1_4, S7_1_6,
    S9_1_4, S9_1_6, Octaphonic, SML13_1, NUM_LAYOUT_DEFS
};
// Add only the static_assert, no new constant:
// static_assert(NUM_LAYOUT_DEFS == <verified count>, "... names Tier A files ...");
```

### Doc-surface fix inventory (Tier A) — exact current lines, verified this session

| File | Line | Current text [VERIFIED, this session] | Correction needed |
|------|------|------------------------------------------|--------------------|
| `CLAUDE.md` | 21 | `\| I/O \| `IO/*.h` \| OutputFormatRegistry (22 formats), SpeakerLayout (13 ITU-R layouts), AmbisonicsCodec (SH eval, decode matrices) \|` | formats/layouts numbers per Open Question 1 resolution |
| `CLAUDE.md` | 24 | `\| DSP \| `DSP/*.h` \| softClip(), outputLimiter() \|` | Delete row (D-09) — `include/SpatialCore/DSP/` confirmed nonexistent via `find include -type d` this session |
| `CLAUDE.md` | 72 | `**Framework:** JUCE 8, C++17, CMake 3.22+` | `JUCE 9.0.0` |
| `README.md` | 21 | `### Spatialization Algorithms (7)` | `(8)` |
| `README.md` | 34 | `**6 HRTF Profiles:** Simple (Woodworth), MIT KEMAR, SADIE II D2, CIPIC Subject003, HUTUBS PP2, Bernschuetz KU100` | D-05 two-number phrasing |
| `README.md` | 36 | `### Output Format Support (22 formats)` | number per Open Question 1 |
| `README.md` | 39 | `- 13 Surround (Quad through 9.1.6 Atmos)` | `15 Surround` (matches `OutputFormat.h`'s own "15 Surround" comment regardless of Open Question 1's Binaural+Stereo+Ambi framing) |
| `README.md` | 43 | `- 13 ITU-R BS.775/BS.2051 standard layouts with SMPTE channel ordering` | number per Open Question 1 |
| `README.md` | 95 | `- **JUCE 8** (C++17) — audio plugin framework` | `JUCE 9.0.0` |
| `docs/integration-guide.md` | 7 | `- JUCE 8 (C++17)` | `JUCE 9.0.0` |
| `docs/integration-guide.md` | 22 | `git submodule add https://github.com/Spatial-Media-Lab/SpatialCore.git SpatialCore` | D-17: live instruction uses `AndrewRahman/SpatialCore`, org URL labelled post-proof destination |
| `docs/integration-guide.md` | 166 | `\│  \│  - 22 output formats                   \│  \│` | number per Open Question 1 |
| `.claude/skills/spatialcore-architecture/spatialcore-architecture.md` | 42 | `\| 7 Spatialization Algorithms \| VBAP, VBIP, KNN, DBAP, MDAP, Ambisonics (HOA), DirectBinaural (Woodworth) \|` | `8` + add `ConstantPower` to the name list (currently missing entirely) |
| `.claude/skills/spatialcore-architecture/spatialcore-architecture.md` | 44 | `\| 6 HRTF Profiles \| Simple (Woodworth), MIT KEMAR, SADIE II D2, CIPIC Subject003, HUTUBS PP2, Bernschuetz KU100 \|` | D-05 two-number phrasing |
| `.claude/skills/spatialcore-architecture/spatialcore-architecture.md` | 45 | `\| 22 Output Formats \| 1 Binaural + 1 Stereo (5 modes) + 13 Surround (Quad–9.1.6) + 6 Ambisonics (FOA–6OA) \|` | numbers per Open Question 1 |
| `.claude/skills/spatialcore-architecture/spatialcore-architecture.md` | 46 | `\| 13 Speaker Layouts \| ITU-R BS.775/BS.2051, SMPTE channel ordering \|` | number per Open Question 1 |
| `.claude/skills/spatial-audio-dsp/SKILL.md` | 518 | `- Framework: JUCE 8.0.3 (git submodule at `JUCE/`)` | `JUCE 9.0.0` |
| `.claude/skills/spatial-audio-dsp/SKILL.md` | 522 | `- HRTF data: 5 SOFA files in `HRTF/` embedded as binary resources (Git LFS tracked)` | **Additional finding, not in D-01's canonical_refs list**: this repeats the old BinaryData myth CLAUDE.md already corrected (`CR-01`). Should read "loaded at runtime via `HRTFDatabase::loadFromFile`," matching `CLAUDE.md:74`'s already-correct wording. Recommend the planner add this line to the Tier A fix list — D-01 only names the JUCE-version line at `:518` for this file, but `:522` sits four lines below it in the same block and is the same defect class API-05 already fixed once in CLAUDE.md. |

**Additional non-blocking finding:** `docs/integration-guide.md:78` (`spatialcore::SpatializationAlgorithm* algorithms[6];`) is inside a code sample already labelled "conceptual" by the note at `:93-100` ("you do NOT hand-roll `spatialize()`..."), so it is lower priority than the Tier A citations above — flagging it for the planner to decide whether to fix the stale `[6]` or leave the already-caveated example as-is.

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|---------------|--------|
| JUCE 8 (docs) | JUCE 9.0.0 (code) | Commit `538972e fix(SC-6): move standalone JUCE fallback pin to 9.0.0` and `CMakeLists.txt:32` | Five doc surfaces still say "JUCE 8"; this phase fixes all of them |
| HRTF "embedded as BinaryData" (DR-5's original text, PRD) | Loads from disk via `HRTFDatabase::loadFromFile`, Git-LFS-tracked `.sofa` files | Commit `87cb7a3` (`CR-01`) already corrected CLAUDE.md; `.claude/skills/spatial-audio-dsp/SKILL.md:522` still has the stale claim | One more Tier A site to fix beyond D-01's list |
| Hard clamp at 1.2589f (March scaffold plan) | `1.2589f * tanh(x/1.2589f)` soft ceiling | Shipped in OSD v1.0.0; D-06/D-08 lock this as permanent | SPEC prose is stamped historical (D-07), not amended, to avoid a second source of truth |

**Deprecated/outdated:**
- The March 2026 scaffold plan's hard-clamp limiter description (`:711`) — stamped historical
  under Tier B, not corrected in place.
- `docs/development-roadmap.md`'s "What Does NOT Exist Yet" section, which lists all five
  extraction requirements as missing when they are implemented and tested — stamped historical.

## Assumptions Log

| # | Claim | Section | Risk if Wrong |
|---|-------|---------|---------------|
| A1 | The root cause reconstruction for how "25/14" was derived (design doc's "22" + arithmetic drift) | Common Pitfall 1 | Low — explicitly labelled speculative; does not affect the actionable finding (current tree = 23/15), only the narrative explanation of how the error occurred |
| A2 | `docs/integration-guide.md:78`'s `algorithms[6]` is lower priority because the surrounding text already caveats the example as conceptual | Code Examples, additional finding | Low — worst case the planner fixes it anyway (harmless extra edit) or leaves a stale array size in an already-flagged-non-authoritative example |

**All other claims in this research are `[VERIFIED]`** — directly read from the tree, git history,
or confirmed via `gh` this session, with file paths, line numbers, and verbatim quotes given
throughout. No package-legitimacy or external-library claims exist in this phase (no new
dependencies), so the usual `[ASSUMED]`-from-training-data risk class does not apply here — the
open risk is entirely the internal count conflict in Open Question 1.

## Open Questions

> **✅ Open Question 1 was CLOSED by the user on 2026-08-11: the canonical counts are 23 output
> formats and 15 speaker layouts.** D-04, REQUIREMENTS.md API-04 (+ the EXTR-03 acceptance line and
> the traceability table), ROADMAP.md Phase 1 criterion 1 and Phase 2 criterion 3, and the stale
> `.planning/codebase/ARCHITECTURE.md` / `STRUCTURE.md` lines were all amended to 23/15 before the
> planner ran. The provenance question is also answered: the 25/14 pair originated as a mapper
> miscount in `.planning/codebase/ARCHITECTURE.md:107` + `STRUCTURE.md:119-120` during the
> 2026-08-10 remap, which commit `f0b14f4` then copied into REQUIREMENTS.md and ROADMAP.md under a
> "verified from the tree" label it never earned. The same remap commit's `TESTING.md:56` already
> said "23-format table" — the remap contradicted itself. The 14 was an off-by-one that
> misattributed one named `LayoutID` entry to the `NUM_LAYOUT_DEFS` sentinel.
>
> **Planning directive: write 23 and 15.** Every `static_assert`, doc line, and test assertion in
> this phase uses those values. Treat any occurrence of 25 or 14 in a count context as a defect to
> fix. The body of the question is retained below as the audit trail.

1. **[CLOSED — 23/15]** ~~Which output-format and speaker-layout counts are the real frozen contract: D-04's "25/14" or the tree's verified "23/15"?~~
   - What we know: The tree (three independent sources: enum body, registry `constexpr`, and
     `.cpp` initializer-list row counts) says 23 output formats and 15 speaker layouts, unchanged
     since commit `149d50d` (2026-07-05), five weeks before the "verified 2026-08-10" claim. No
     doc surface, CONTEXT.md, REQUIREMENTS.md, or ROADMAP.md contains a "23" or a "15" anywhere.
   - What's unclear: How the 2026-08-10 verification pass arrived at 25/14. Possibly a manual
     miscount, possibly counting a different (non-existent) branch state, possibly transcription
     error from the design-stage "22" intel doc. The research session found no tree state, past or
     present, that produces 25 or 14.
   - Recommendation: **Do not proceed to planning with either number silently assumed.** Surface
     this to the user as a blocking pre-planning confirmation (the same treatment OQ-1 and OQ-3
     received) — D-04 is explicitly locked one-way, so overriding it without a decision point
     breaks the same "ask before freezing a public contract" principle the phase itself
     establishes. If the user confirms 23/15 (matching the tree), all downstream `static_assert`,
     doc-line, and test-assertion numbers in the plan should use 23/15, and ROADMAP.md's Phase 1
     *and* Phase 2 success criteria (Phase 2 also cites "25"/"14" at `ROADMAP.md:64`) should be
     corrected in the same pass this phase already recommends for other planning-doc errors
     (D-09/D-16 precedent).

## Environment Availability

| Dependency | Required By | Available | Version | Fallback |
|------------|------------|-----------|---------|----------|
| CMake | Compiling the new `static_assert`/count-sentinel changes and extended test | ✓ | 4.2.3 [VERIFIED: `cmake --version`] | — |
| `gh` CLI (authenticated) | Verifying BUG-03 issue numbers resolve in `Spatial-Media-Lab/OpenSpatialDelay` before/while rewriting comments | ✓ | 2.88.1, logged in as `AndrewRahman` [VERIFIED: `gh auth status`] | — |
| Catch2 (FetchContent) | New/extended tests | ✓ (already fetched by existing build; requires network on a clean checkout) | v3.7.1 pinned [VERIFIED: `tests/CMakeLists.txt:5`] | none needed — already a build dependency, not new to this phase |
| Git LFS | HRTF `.sofa` files (referenced by API-03's count, unaffected by this phase's edits) | ✓ (files present, real HDF5 content, 1.16–36.6 MB each — not LFS pointer stubs) [VERIFIED: `ls -la HRTF/*.sofa`] | — | — |

**Missing dependencies with no fallback:** None.
**Missing dependencies with fallback:** None — this phase adds no new external dependency; the
existing toolchain (already used by prior phases) is sufficient.

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | Catch2 v3.7.1 (FetchContent) [VERIFIED: `tests/CMakeLists.txt:2-7`] |
| Config file | `tests/CMakeLists.txt` |
| Quick run command | `cmake --build build --target SpatialCoreTests && ./build/SpatialCoreTests "[spatialmath]"` (tag-filtered to just the touched module) |
| Full suite command | `cmake --build build --target SpatialCoreTests && ./build/SpatialCoreTests` (144 `TEST_CASE`s confirmed present this session via `grep -rc TEST_CASE tests/*/*.cpp`, matching STATE.md's prior count) |

### Phase Requirements → Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| API-01 | Algorithm count sentinel matches 8 | unit (compile-time + Catch2) | `./build/SpatialCoreTests "[spatialmath]"` or new `[counts]` tag | ❌ Wave 0 — new counts test |
| API-02 | `outputLimiter` at/below/above ceiling + non-finite | unit | `./build/SpatialCoreTests "[outputlimiter]"` | ✅ Mostly exists — `tests/Core/SpatialMathTests.cpp:66-98`; see Common Pitfall 4 for exact gap |
| API-03 | HRTF profile count (5) documented with the two-number D-05 phrasing | doc-only — no automated test; the shipped `.sofa` count is implicitly covered by existing per-profile Binaural tests (`tests/Binaural/*Tests.cpp`, one file per profile, 5 files present) | `./build/SpatialCoreTests "[binaural]"` | ✅ Existing per-profile test files confirm 5 profiles are wired end-to-end |
| API-04 | Output-format/layout counts match a `static_assert`ed sentinel | unit (compile-time + Catch2) | new counts test, same as API-01 | ❌ Wave 0 — blocked on Open Question 1's resolution before the exact numeric literal can be written |
| API-05 | CLAUDE.md factual claims reproducible from tree | doc-only — no automated test (prose can't be Catch2-asserted); manual verification against this RESEARCH.md's line-by-line table is the closest to "automated" this gets | — | N/A — doc review only |
| BUG-03 | Every `#NNN` resolves in the tracker it names | doc-only — verified via one-time `gh issue view` sweep (already run this session for all 10 distinct numbers); no compile-time or runtime check applies to comment text | `grep -rn "issue #[0-9]" include/ src/` (should return the qualified form after the fix, and zero bare `#N` matches) | N/A — grep-verification, not Catch2 |

### Sampling Rate
- **Per task commit:** `./build/SpatialCoreTests "[spatialmath]"` after limiter/counts test edits; a
  post-edit `grep -rn "issue #[0-9]" include/ src/` after each BUG-03 comment rewrite batch.
- **Per wave merge:** Full suite (`./build/SpatialCoreTests`, no tag filter).
- **Phase gate:** Full suite green before `/gsd-verify-work`, per DR-4 (already-established project
  rule, not new to this phase).

### Wave 0 Gaps
- [ ] A counts `TEST_CASE` (new — asserts `NUM_LAYOUT_DEFS`/algorithm-count/output-format-count
      sentinels at runtime as a DR-4 backstop to the `static_assert`s) — covers API-01/API-04.
- [ ] Decision on whether `outputLimiter`'s existing parameterized test needs splitting into
      explicitly-named at/below/above-ceiling cases, or whether the existing loop already
      satisfies Success Criterion 2 as evidence (Common Pitfall 4) — covers API-02.
- [ ] No new framework install needed — Catch2 is already wired into `tests/CMakeLists.txt`.

*(No gaps for API-03/API-05/BUG-03 — these are doc-review/grep-verification items with no
Catch2-testable behavior, consistent with D-10's decision to keep drift prevention compile-time-only
for the numeric contracts and manual for prose.)*

## Security Domain

This phase changes no runtime behavior, no input handling, no authentication/session/access-control
surface, and introduces no new cryptography or external I/O. It is a documentation-and-comment
truth-alignment plus a compile-time assertion addition.

### Applicable ASVS Categories

| ASVS Category | Applies | Standard Control |
|---------------|---------|-------------------|
| V2 Authentication | No | N/A — no auth surface touched |
| V3 Session Management | No | N/A |
| V4 Access Control | No | N/A |
| V5 Input Validation | No | N/A — no new input parsing; the only "input" is source code/prose text edited by a human/agent during planning, not runtime user input |
| V6 Cryptography | No | N/A |

### Known Threat Patterns for {stack}

None applicable — this phase has no attack surface (no network I/O, no user-supplied data parsing,
no new dependency). The closest analog, "a wrong `static_assert` literal breaks the build," is a
correctness risk (covered in Common Pitfall 1), not a security risk.

## Sources

### Primary (HIGH confidence — direct code/tool verification this session)
- `include/SpatialCore/IO/OutputFormat.h`, `OutputFormatRegistry.h` — enum body and count constant
- `src/IO/OutputFormatRegistry.cpp`, `src/IO/SpeakerLayout.cpp` — initializer-list row counts
- `include/SpatialCore/IO/SpeakerLayout.h` — `LayoutID` enum and `NUM_LAYOUT_DEFS`
- `include/SpatialCore/Core/SpatialMath.h`, `Types.h` — limiter docblock, `profileIndex`
- `include/SpatialCore/SpatialCore.h`, `Algorithms/AllAlgorithms.h` — 8-algorithm confirmation
- `CMakeLists.txt`, `tests/CMakeLists.txt` — JUCE 9.0.0, libmysofa v1.3.2, Catch2 v3.7.1 pins
- `CLAUDE.md`, `README.md`, `docs/integration-guide.md`,
  `.claude/skills/spatialcore-architecture/spatialcore-architecture.md`,
  `.claude/skills/spatial-audio-dsp/SKILL.md` — current doc-surface content, all lines quoted above
- `tests/Core/SpatialMathTests.cpp` — existing `outputLimiter`/`softClip` test coverage
- `git log`, `git show` on `OutputFormat.h`, `SpeakerLayout.h`, `CLAUDE.md` — commit-date evidence
- `gh issue view` against `Spatial-Media-Lab/OpenSpatialDelay` and `AndrewRahman/SpatialCore` —
  live verification of all 10 BUG-03 issue numbers
- `find include -type d` — confirmed `include/SpatialCore/DSP/` does not exist (D-09)
- `.planning/REQUIREMENTS.md`, `.planning/PROJECT.md`, `.planning/STATE.md`, `.planning/ROADMAP.md`,
  `.planning/phases/01-documentation-truth-contract-freeze/01-CONTEXT.md`

### Secondary (MEDIUM confidence)
- None — no web/docs-provider research was needed or available for this phase (all research
  providers reported unavailable per `gsd-tools query init.phase-op 1`: `exa_search`,
  `brave_search`, `firecrawl` all `false`); the phase's domain is entirely internal-codebase truth
  verification, which does not require external documentation lookup.

### Tertiary (LOW confidence)
- `.planning/intel/constraints.md:40`'s "22 formats" design-stage listing — used only as a
  speculative explanation for how D-04's "25" might have originated (Assumption A1), not as a
  factual claim about current state.

## Metadata

**Confidence breakdown:**
- Standard stack: N/A — no new stack; existing JUCE 9.0.0/Catch2 3.7.1/libmysofa 1.3.2 pins directly re-verified
- Architecture (count-sentinel, header-adjacent-contract patterns): HIGH — directly grounded in existing, already-shipped repo precedent (`NUM_LAYOUT_DEFS`, `SpatialMath.h` docblock)
- Doc-surface fix inventory: HIGH — every cited line read directly this session, verbatim quotes included
- BUG-03 site completeness: HIGH — exhaustive repo-wide grep, cross-checked against both repos via `gh`
- **Numeric ground truth (API-04):** MEDIUM — the tree-state numbers (23/15) are HIGH confidence (three independent sources), but the *decision* of which number is canonical is explicitly unresolved pending Open Question 1

**Research date:** 2026-08-11
**Valid until:** Effectively indefinite for the codebase facts (this is a snapshot of an unchanging
tree unless someone adds/removes an `OutputFormat`/`LayoutID` value before planning starts) — but
**Open Question 1 must be closed before planning locks any `static_assert` literal.** Re-verify
counts if any commit touches `include/SpatialCore/IO/OutputFormat.h` or `SpeakerLayout.h` between
this research and plan execution.
