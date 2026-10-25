---
phase: 01-documentation-truth-contract-freeze
reviewed: 2026-08-15T10:55:00Z
depth: standard
files_reviewed: 29
files_reviewed_list:
  - .claude/skills/spatial-audio-dsp/SKILL.md
  - .claude/skills/spatialcore-architecture/spatialcore-architecture.md
  - CLAUDE.md
  - README.md
  - docs/development-roadmap.md
  - docs/integration-guide.md
  - docs/superpowers/plans/2026-03-22-scaffold-spatialcore.md
  - include/SpatialCore/Algorithms/AllAlgorithms.h
  - include/SpatialCore/Binaural/BinauralRenderer.h
  - include/SpatialCore/Binaural/HRTFDatabase.h
  - include/SpatialCore/Binaural/PartitionedConvolver.h
  - include/SpatialCore/Binaural/SharedFFTCache.h
  - include/SpatialCore/Engine/RenderEngine.h
  - include/SpatialCore/IO/OutputFormatRegistry.h
  - include/SpatialCore/IO/SpeakerLayout.h
  - include/SpatialCore/OSC/OSCPortValidation.h
  - include/SpatialCore/UI/PresetBrowser.h
  - include/SpatialCore/UI/SpatialMapComponent.h
  - src/Binaural/HRTFDatabase.cpp
  - src/Binaural/PartitionedConvolver.cpp
  - src/Engine/RenderEngine.cpp
  - src/Trajectory/TrajectoryEngine.cpp
  - src/UI/PresetBrowser.cpp
  - src/UI/ReverseSlider.cpp
  - src/UI/SpatialMapComponent.cpp
  - tests/Binaural/SadieD2KU100Tests.cpp
  - tests/CMakeLists.txt
  - tests/Core/CountsTests.cpp
  - tests/Core/SpatialMathTests.cpp
  - tests/OSC/OSCPortValidationTests.cpp
findings:
  critical: 0
  warning: 3
  info: 1
  total: 4
status: issues_found
---

# Phase 01: Code Review Report — documentation-truth-contract-freeze

**Reviewed:** 2026-08-15T10:55:00Z
**Depth:** standard
**Files Reviewed:** 29
**Status:** issues_found

## Summary

This phase is documentation-correctness work: count sentinels (01-01), cross-repo issue-citation
qualification (01-02), factual corrections in CLAUDE.md/README.md/integration-guide/skills
(01-03), `outputLimiter` boundary test coverage + scaffold-plan stamping (01-04), and Tier-B
document stamping (01-05).

Verified against the actual enum/registry bodies (not the literals):
- `NUM_OUTPUT_FORMATS = 23` — correct. `OutputFormat` enum has 23 members (`Binaural` … `Ambisonics6OA`), and `OutputFormatRegistry::table` is a `std::array<OutputFormatInfo, 23>` whose 23-entry aggregate initializer matches exactly (a size mismatch here would be a compile error, so this pairing is self-enforcing).
- `NUM_LAYOUT_DEFS == 15` — correct, and structurally self-verifying: `NUM_LAYOUT_DEFS` is the auto-incremented trailing enumerator of `LayoutID`, so it *is* the count, not a hand-typed number next to the count.
- `NUM_ALGORITHMS == 8` — correct today (8 classes derive `SpatializationAlgorithm` and are `#include`d in `AllAlgorithms.h`), but the constant itself is a hand-typed literal with no compiler-enforced link to that list (see WR-01 below).

All 39 issue-citation rewrites in the reviewed files (`#N` → `Spatial-Media-Lab/OpenSpatialDelay#N`) landed inside comments/docblocks only — no executable code, string literal, or test assertion was touched by the citation-rewrite commits. The `outputLimiter` test split (01-04) preserves all 9 original boundary values across the three new named cases and checks them against the real `1.2589f * tanh(x/1.2589f)` implementation, which was correctly left unmodified. Tier B document stamps (`docs/development-roadmap.md`, `docs/superpowers/plans/2026-03-22-scaffold-spatialcore.md`) are present, clearly worded, and placed immediately above the stale content they disclaim.

Two issues found that go against the phase's own stated purpose: one of the three new count-contract tests doesn't actually test anything (WR-01), and the phase's own edit to `docs/integration-guide.md` creates a fresh three-way documentation contradiction about the canonical GitHub URL that none of the other in-scope docs were updated to match (WR-03).

## Warnings

### WR-01: `NUM_ALGORITHMS` sentinel and its "runtime backstop" test are pure tautologies — they verify nothing

**File:** `include/SpatialCore/Algorithms/AllAlgorithms.h:19-24`, `tests/Core/CountsTests.cpp:27-30`

**Issue:** `tests/Core/CountsTests.cpp`'s file-level comment claims all three `[counts]` test cases are a "runtime backstop... that reads the enum/registry directly rather than re-stating the literal, so a mismatch between the sentinel and the actual enum body still turns the suite red." That claim is true for the `OutputFormat` and `LayoutID` cases (the latter's sentinel is a compiler-computed enumerator, not a typed number), but false for algorithms:

```cpp
// AllAlgorithms.h
static constexpr int NUM_ALGORITHMS = 8;
static_assert (NUM_ALGORITHMS == 8, "...");   // 8 == 8, always true, checks nothing

// CountsTests.cpp
TEST_CASE ("counts: NUM_ALGORITHMS pins 8 spatialization algorithms", "[counts]")
{
    CHECK (NUM_ALGORITHMS == 8);              // still just 8 == 8
}
```

`NUM_ALGORITHMS` is not derived from anything — it is not the size of an array of algorithm instances, not an auto-incrementing enumerator, and (confirmed via grep) it is not referenced anywhere else in `include/` or `src/` to size a table. It is a hand-typed integer literal compared against itself, both at compile time and at runtime. If an 8th→9th algorithm is added to `AllAlgorithms.h` and the author forgets to bump the literal, neither the `static_assert` nor this `TEST_CASE` will ever fail — the exact drift scenario D-04/D-13 and this phase (01-01) were created to catch. This is the specific "tautological" failure mode the phase brief called out as a risk to check for, and it is present.

**Fix:** Either derive the count from something real (e.g., a `constexpr std::array<const char*, N>` of algorithm names, or a factory function returning a fixed-size array of instances, sized so the array literal itself enforces the count the way `OutputFormatRegistry::table` does), or — if no such derivation is practical given the current architecture — change the test/assert wording to stop claiming it "reads the actual body," since currently it doesn't. At minimum, drop `NUM_ALGORITHMS` from the "reads the enum/registry directly" claim in the `CountsTests.cpp` file banner so the comment doesn't overstate the guarantee.

### WR-02: `OutputFormat` count check only catches insertion-before-last drift, not append-after-last drift

**File:** `tests/Core/CountsTests.cpp:16-20`

**Issue:**
```cpp
CHECK (static_cast<int> (OutputFormat::Ambisonics6OA) == NUM_OUTPUT_FORMATS - 1);
```
This checks that the *named* last member (`Ambisonics6OA`) sits at index `NUM_OUTPUT_FORMATS - 1`. It correctly catches an insertion in the middle of the enum (which would shift `Ambisonics6OA`'s implicit value and break the check). It does **not** catch a new member appended *after* `Ambisonics6OA` without bumping `NUM_OUTPUT_FORMATS`: `Ambisonics6OA`'s own enum value is unchanged by such an append, so the check silently continues to pass at the old (now-wrong) count. Since `OutputFormat` (unlike `LayoutID`) has no trailing self-counting enumerator, this is the best available proxy, but the file's banner comment claims a general-purpose backstop against "a mismatch between the sentinel and the actual enum body" — this specific check only covers half of that class of mismatch.

**Fix:** Either accept and document this as a known partial check ("catches reordering/insertion, not append"), or replace it with something that inspects `OutputFormatRegistry::table.size()` (which is genuinely `NUM_OUTPUT_FORMATS`-sized) plus a check that every enum value up to and including the last one round-trips through `getInfo()` without hitting an out-of-bounds/default-constructed entry.

### WR-03: This phase's own doc fix (docs/integration-guide.md) creates a fresh 3-way contradiction on the canonical GitHub URL

**File:** `docs/integration-guide.md:22-31` vs. `CLAUDE.md:8`, `README.md:74`, `.claude/skills/spatialcore-architecture/spatialcore-architecture.md:68`

**Issue:** Commit `cd6e0af` (01-05) rewrote `docs/integration-guide.md`'s submodule-add example and added an explanatory note:

```
git submodule add https://github.com/AndrewRahman/SpatialCore.git SpatialCore
...
> **Remote topology.** `AndrewRahman/SpatialCore` is the deliberate development remote and the
> URL to use today. `Spatial-Media-Lab/SpatialCore` is the post-proof destination... The
> organisation URL does not resolve yet — do not substitute it.
```

This matches the actual `git remote -v` for this repo (`origin  https://github.com/AndrewRahman/SpatialCore.git`), so the integration-guide.md correction itself is accurate. But three other files that are in this phase's own reviewed scope, and were touched by 01-03 for other corrections in the same sections, were **not** updated to match, and now directly contradict the just-corrected guidance:

- `CLAUDE.md:8` — `**GitHub:** https://github.com/Spatial-Media-Lab/SpatialCore` (stated as the project's canonical GitHub identity, no caveat)
- `README.md:74` — `git submodule add https://github.com/Spatial-Media-Lab/SpatialCore.git SpatialCore` (the exact same submodule-add example, with the URL integration-guide.md says "does not resolve yet")
- `.claude/skills/spatialcore-architecture/spatialcore-architecture.md:68` — same `Spatial-Media-Lab/SpatialCore` submodule-add example

A reader following README.md's own "Integration" section (which explicitly forwards to `docs/integration-guide.md` "for the complete integration tutorial") will hit a URL that integration-guide.md itself says doesn't resolve yet. This is precisely the class of cross-document factual contradiction this phase exists to eliminate, and it was introduced (or at least left unresolved) by this phase's own edit rather than being pre-existing stable state.

**Fix:** Apply the same "Remote topology" framing (or at minimum the same `AndrewRahman/SpatialCore` URL with a footnote) to `CLAUDE.md`, `README.md`, and the `spatialcore-architecture.md` skill, or revert the integration-guide.md wording to match whatever the other three files intend to say. Pick one canonical statement and propagate it everywhere the submodule URL is quoted.

## Info

### IN-01: `CountsTests.cpp` test name overstates what the OutputFormat test checks

**File:** `tests/Core/CountsTests.cpp:16`

**Issue:** `TEST_CASE ("counts: OutputFormat enum and registry agree on 23 formats", "[counts]")` — the body never touches `OutputFormatRegistry::table` or `getInfo()`; it only checks `NUM_OUTPUT_FORMATS` against itself and `OutputFormat::Ambisonics6OA`'s ordinal. The registry/enum size relationship is actually enforced elsewhere (the `std::array<OutputFormatInfo, NUM_OUTPUT_FORMATS>` aggregate-initializer in `OutputFormatRegistry.cpp`, and `tests/IO/SpeakerLayoutTests.cpp:89`'s `kGoldenFormatTable` size check), not by this test.

**Fix:** Rename to something like `"counts: OutputFormat enum's last member matches the NUM_OUTPUT_FORMATS sentinel"`, or fold in an actual registry-table-size assertion so the name matches the body.

---

_Reviewed: 2026-08-15T10:55:00Z_
_Reviewer: Claude (gsd-code-reviewer)_
_Depth: standard_
