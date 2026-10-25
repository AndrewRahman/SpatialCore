---
phase: 07-process-docs-consumer-closeout
reviewed: 2026-10-25T00:00:00Z
depth: standard
files_reviewed: 13
files_reviewed_list:
  - .claude/skills/cross-platform-builds/REFERENCE.md
  - .claude/skills/spatial-audio-dsp/SKILL.md
  - .claude/skills/spatialcore-architecture/spatialcore-architecture.md
  - .github/workflows/ci.yml
  - CLAUDE.md
  - README.md
  - docs/development-roadmap.md
  - docs/integration-guide.md
  - examples/harness/README.md
  - include/SpatialCore/Core/BinauralGains.h
  - include/SpatialCore/Core/SourcePosition.h
  - include/SpatialCore/UI/PresetBrowser.h
  - tests/Binaural/BinauralRendererTests.cpp
findings:
  critical: 0
  warning: 1
  info: 3
  total: 4
status: issues_found
---

# Phase 7: Code Review Report

**Reviewed:** 2026-10-25
**Depth:** standard
**Files Reviewed:** 13
**Status:** issues_found

## Summary

Phase 7 is docs, comments and one test title. I diffed every file against `91c0f30` and checked the claims against the code and against the built binaries.

**No code or YAML semantics changed.**
- `ci.yml` differs only in comment lines. The `on:` block is `workflow_dispatch` only, with the `realtime` boolean input. The `realtime-nightly` `if:` is `startsWith(github.ref,'refs/tags/v') || (workflow_dispatch && inputs.realtime)`, so the new comments describing a manual run from a `v*` ref are accurate.
- The two Core shim headers and `PresetBrowser.h` changed in comments only. `cmake --build build-release --target SpatialCoreUI` is up to date, and the repo-wide scanners still pass against the current tree: `[bug03]` (IssueReferenceTests), `UmbrellaTests`, `CountsTests`, `[rule]` and the whole `SpatialCoreUITests`.
- The test change is a title and a comment. The new title matches the behaviour in `BinauralRenderer.h:56-62` (silence, no state advanced). `updateSourceHRIR` keeps the previous HRIR, as the new comment says.

**Claims verified against code**
- 23 formats and 15 layouts match `NUM_OUTPUT_FORMATS` and `NUM_LAYOUT_DEFS`, which are `static_assert`-ed.
- Commit `0b35e55` exists, dated 2026-07-05, and did move `SourcePosition` and `BinauralGains` into `Types.h`.
- PR #46 is a merge in the history ("ci manual-dispatch only").
- The README light-tier commands work on the built binary:
  - `--filenames-as-tags "[#BinauralRendererTests]~[.]"` gives 10 cases.
  - The folder one-liner works for `Binaural` and `OSC`.
  - `[osc]` is on every case in `tests/OSC/`.
  - `~[.]` really excludes the hidden `[.][ambi-maxre-capture]` case in `RealtimeSafetyTests.cpp`, while `[#RealtimeSafetyTests]` alone includes it.
  - `[binaural]` is on 25 cases.
- The PresetBrowser contract matches `src/UI/PresetBrowser.cpp`:
  - It starts a new submenu on every category change, so a split category appears twice.
  - An empty-category entry is dropped by `flushSubMenu`.
  - The ID is `originalIndex + 1`.
  - `result > 0` gates `onSelect`.
  - The "(empty)" User submenu is added only when no entry has exactly `"User"`.
  - The separator goes only before a user entry that follows a factory entry.
- JUCE source (`juce_PopupMenu.cpp`) confirms the other contract claims:
  - `Item` copies its sub-menu (`std::unique_ptr<PopupMenu>`), so the open menu does not depend on `activeMenu`.
  - The target component and look-and-feel are held through `WeakReference`.
  - `withDeletionCheck` is not used.
  - `addItem` asserts on ID 0.

One claim in the new contract text does not hold in code (WR-01). Three smaller inaccuracies follow.

## Warnings

### WR-01: Docs promise that Escape closes the save overlay, but the focused TextEditor consumes Escape

**File:** `include/SpatialCore/UI/PresetBrowser.h:21-23` (also `docs/integration-guide.md`, table row "`onSave (name)` ... nothing on Cancel, Escape or a click outside"); code in `src/UI/PresetBrowser.cpp:170-177`
**Issue:**
- The new header comment states "Cancel, Escape or a click outside close it without calling onSave". The guide repeats the Escape claim.
- `PresetSaveOverlay::show()` gives keyboard focus to `nameEditor` (`PresetBrowser.cpp:104`).
- JUCE's `TextEditor::keyPressed` handles Escape itself. It calls `escapePressed()` and returns `consumeEscAndReturnKeys`, which defaults to `true` (`juce_TextEditor.cpp:1914-1919`, `juce_TextEditor.h:825`).
- The key is therefore consumed and never reaches `PresetSaveOverlay::keyPressed`, the only place that dismisses on Escape.
- `nameEditor.onEscapeKey` and `setEscapeAndReturnKeysConsumed(false)` are never set.
- While the name field has focus, which is its normal state, Escape most likely does not close the overlay. That is a pre-existing behaviour bug, and the phase now documents it as a guaranteed contract.
- Phase 7 is docs-only, so the fix may be a wording change or a follow-up issue. I did not run a live UI to confirm.

**Fix:** Either make the code match the contract, or soften the docs. The code fix, in the `PresetSaveOverlay` constructor:
```cpp
nameEditor.onEscapeKey = [this] { dismiss(); };
```
Alternatively drop "Escape" from the header comment and the guide table until that is done. If the claim stays, add a UI test that sends Escape with the name field focused.

## Info

### IN-01: README "70 cases in tests/Binaural/" does not match the built suite (69)

**File:** `README.md:152`
**Issue:**
- The text says "`[binaural]` is on 25 of the 70 cases in `tests/Binaural/` (counted 2026-10-25)".
- In the default Release build (`SPATIALCORE_EMBED_ALL_HRTF` ON), `--list-tests --filenames-as-tags` over all `tests/Binaural/*.cpp` files lists 69 cases.
- The 70 comes from counting `^TEST_CASE` in the sources. One of them, `tests/Binaural/HRTFEmbeddedTests.cpp:570`, is compiled only under `#if ! SPATIALCORE_EMBEDS_ALL_HRTF`.
- The 25 is correct. The README does not say how it counted, and the figure is 2026-10-25-dated.

**Fix:** Write "25 of the 69 cases (a default build; counted with `--list-tests` on 2026-10-25)", or drop the denominator.

### IN-02: Light-tier "comment edit takes the light tier" skips the repo-wide source scanners that live in other areas

**File:** `README.md:121` and the area table at `README.md:136-148`
**Issue:**
- The README says a comment-only edit in a `.h` or `.cpp` takes the light tier, and the table maps each source folder to its own tests folder.
- Several tests scan all of `include/`, `src/` and `tests/` at runtime regardless of which folder you edited:
  - `tests/Core/IssueReferenceTests.cpp` rejects a bare issue number, or the short OSD form, in any comment.
  - `tests/Core/UmbrellaTests.cpp` scans the headers.
  - `FontProvenanceTests` strips and scans UI comments.
- A phase like this one adds issue references in comments in `UI/` and `Core/`. A bare `#N` typed into a `Binaural/` comment passes `area=Binaural` and is caught only by the pre-merge full `ctest`. That is acceptable under the tiers, but the doc implies the light tier covers a comment edit.

**Fix:** Add one sentence, for example: "A comment edit that adds an issue reference or a header should also run `[bug03]`." The command `./build-release/tests/SpatialCoreTests "[bug03]"` takes about a second.

### IN-03: Guide says "one separator goes before the first user entry"; the code and header are narrower

**File:** `docs/integration-guide.md` (PresetBrowser contract, rules list); code at `src/UI/PresetBrowser.cpp:226`
**Issue:**
- The code draws the separator only before the first user entry that follows a factory entry in the same category.
- A category of only user entries gets none. A user entry before any factory entry also gets none.
- The header comment states it precisely ("before the first user entry that follows a factory entry"). The guide summary reads as unconditional.

**Fix:** Align the guide wording with the header: "One separator goes before the first user entry that follows a factory entry in that category."

---

_Reviewed: 2026-10-25_
_Reviewer: Claude (gsd-code-reviewer)_
_Depth: standard_
