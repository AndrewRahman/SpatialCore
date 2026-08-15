---
phase: 01-documentation-truth-contract-freeze
verified: 2026-08-15T00:00:00Z
status: gaps_found
score: 4/6 must-haves verified
behavior_unverified: 0
overrides_applied: 0
gaps:
  - truth: "CLAUDE.md's GitHub identity line, README.md's submodule instruction, and the spatialcore-architecture skill's submodule instruction are consistent with the phase's own resolved D-16/D-17/DR-18 position on the canonical remote URL (API-05)"
    status: failed
    reason: >
      This phase (plan 01-05, DR-18) explicitly took a position on the org-vs-personal-remote
      question and rewrote docs/integration-guide.md accordingly, stating in a new blockquote
      that "the organisation URL does not resolve yet — do not substitute it." That statement is
      accurate: `git remote -v` shows origin is `https://github.com/AndrewRahman/SpatialCore.git`.
      But three other files inside this same phase's own reviewed/edited scope were not brought
      into agreement, and now directly contradict the just-corrected guidance:
        - CLAUDE.md:8 states `**GitHub:** https://github.com/Spatial-Media-Lab/SpatialCore` as
          the project's canonical identity, with no caveat — this is a factually inaccurate claim
          in the one document that auto-loads into every session in this repo, which is the exact
          failure mode this phase's own ROADMAP rationale names ("A wrong CLAUDE.md is loaded into
          every session in this repo... Fix the map before using it").
        - README.md:74 publishes `git submodule add https://github.com/Spatial-Media-Lab/SpatialCore.git SpatialCore`
          as a runnable instruction, and README.md:92 explicitly forwards the reader to
          docs/integration-guide.md "for the complete integration tutorial" — so a reader following
          README's own pointer lands on a page that says the URL README just gave them "does not
          resolve yet."
        - .claude/skills/spatialcore-architecture/spatialcore-architecture.md:68 publishes the same
          stale `git submodule add` example, and this file auto-loads into every session alongside
          CLAUDE.md.
      Plan 01-03 touched all three of these files in the same commit wave for other corrections in
      the same sections (counts, JUCE version, architecture table), so this is not stale content the
      phase never reached — it is a fresh three-way contradiction the phase's own edits introduced or
      left unresolved on a question the phase itself decided.
    artifacts:
      - path: "CLAUDE.md"
        issue: "Line 8 states Spatial-Media-Lab/SpatialCore as the canonical GitHub URL, contradicting the phase's own DR-18 finding that this URL does not resolve yet"
      - path: "README.md"
        issue: "Line 74 publishes a submodule-add command using the non-resolving org URL"
      - path: ".claude/skills/spatialcore-architecture/spatialcore-architecture.md"
        issue: "Line 68 publishes the same non-resolving submodule-add example; this file auto-loads every session"
    missing:
      - "Apply the same 'Remote topology' framing (or at minimum the AndrewRahman/SpatialCore URL with a footnote) to CLAUDE.md's GitHub line, README.md's submodule example, and the skill file's submodule example — or revert integration-guide.md's wording so all four surfaces agree on one canonical statement."
  - truth: "The canonical counts are pinned in code so a doc surface can no longer disagree with the tree undetected — no count constant can drift without the build failing (D-04, plan 01-01 must-have)"
    status: partial
    reason: >
      True for OutputFormat (NUM_OUTPUT_FORMATS is checked against the enum's actual last member,
      Ambisonics6OA) and true for LayoutID (NUM_LAYOUT_DEFS is a compiler auto-incremented trailing
      enumerator, structurally incapable of drifting from the enum body). False for algorithms:
      `include/SpatialCore/Algorithms/AllAlgorithms.h:19-24` declares
      `static constexpr int NUM_ALGORITHMS = 8;` and then `static_assert (NUM_ALGORITHMS == 8, ...)`
      — a literal compared against itself, which can never fail regardless of how many algorithm
      headers are added or removed from the file. The runtime `[counts]` test
      (`tests/Core/CountsTests.cpp:27-30`) has the identical defect: `CHECK (NUM_ALGORITHMS == 8)`
      checks the constant against itself, not against `AllAlgorithms.h`'s actual include list. If a
      9th algorithm is added and the literal is not bumped by hand, neither gate turns red — the
      exact drift scenario D-04 and this phase were created to catch, confirmed present by direct
      inspection of both files (matches code-review WR-01, which the orchestrator independently
      confirmed).
    artifacts:
      - path: "include/SpatialCore/Algorithms/AllAlgorithms.h"
        issue: "NUM_ALGORITHMS static_assert is a tautology (8 == 8); not derived from the include list"
      - path: "tests/Core/CountsTests.cpp"
        issue: "The NUM_ALGORITHMS [counts] test case checks the constant against itself, not against the actual header body"
    missing:
      - "Derive NUM_ALGORITHMS from something real (an array of algorithm names/factories sized by the include list) so a future addition/removal is compiler- or test-enforced, or narrow the file-banner and test-name claims so they stop asserting a drift-proof guarantee they don't provide for the algorithm count specifically."
deferred: []
---

# Phase 01: Documentation Truth Contract Freeze Verification Report

**Phase Goal:** Freeze the public count contract, the `outputLimiter()` contract, and CLAUDE.md's
factual accuracy in the tree, so no documentation surface can state a number or claim that
disagrees with the code — API-01 through API-05, BUG-03.
**Verified:** 2026-08-15
**Status:** gaps_found
**Re-verification:** No — initial verification

## Goal Achievement

### Observable Truths

| # | Truth | Status | Evidence |
|---|-------|--------|----------|
| 1 | Headers and docs state 8 algorithms, 5 HRTF profiles, 23 output formats, and 15 speaker layouts, sourced from `OutputFormat.h:9-28` and `SpeakerLayout.h:42-45` | VERIFIED | `OutputFormat` enum has 23 contiguous members (`Binaural`…`Ambisonics6OA`); `LayoutID` ends in `NUM_LAYOUT_DEFS` = 15; `AllAlgorithms.h` includes 8 concrete algorithm headers. CLAUDE.md's I/O row states "23 formats" / "15 ITU-R layouts", Algorithms row states "8 spatialization algorithms". README.md, integration-guide.md, and both skill files carry the same numbers. Residual-stale-literal sweep (`JUCE 8`, `22 output formats`, `13 ITU-R`, `7 Spatialization`, `6 HRTF`, etc.) across all five Tier A surfaces returns nothing. |
| 2 | `outputLimiter()` has exactly one governing contract (the tanh soft ceiling docblock at `Core/SpatialMath.h:98-99`); the scaffold plan's contradicting hard-clamp language is stamped historical, not amended; the implementation is unchanged; a test pins the curve at, below, and above the ceiling, and for non-finite input | VERIFIED | `SpatialMath.h:98-106` is byte-identical across the whole phase (`git log` shows no phase-01 commit touches this file). `tests/Core/SpatialMathTests.cpp` has named cases `outputLimiter: below the ceiling`, `: at the ceiling`, `: above the ceiling`, plus the pre-existing `: non-finite input returns 0.0`. `docs/superpowers/plans/2026-03-22-scaffold-spatialcore.md:1-9` carries the "Historical — superseded by" stamp, with a specific sentence pointing at the real docblock and instructing "Do not implement from the sketch below," inserted as a pure addition (no line deleted). |
| 3 | CLAUDE.md's architecture table includes the `Engine/` module, states JUCE 9.0.0, and describes HRTF data the way it actually loads | VERIFIED | CLAUDE.md's table has an `Engine` row (`Engine/RenderEngine.h`) and a `Core` row (`Core/SpatialMath.h`) with no `DSP` row (`find include -type d -name DSP` is empty). Build System section states `JUCE 9.0.0, C++17, CMake 3.22+`, matching `CMakeLists.txt`'s `GIT_TAG 9.0.0`. HRTF data bullet correctly states runtime loading via `HRTFDatabase::loadFromFile`, no BinaryData step. |
| 4 | Every `#NNN` in a code comment resolves in the tracker it names — OSD references are written `Spatial-Media-Lab/OpenSpatialDelay#NNN` | VERIFIED | `grep -rhoE '[[:alnum:]/-]*#[0-9]+' include/ src/ tests/ \| grep -cv 'OpenSpatialDelay#'` returns `0`. No doubled prefix (`OpenSpatialDelay#Spatial-Media-Lab` count is `0`). 39 qualified citations across 15 distinct numbers per the orchestrator's already-verified `gh issue view` sweep. |
| 5 | (Plan 01-01 must-have) The canonical counts are pinned in code so a doc surface can no longer disagree with the tree undetected — no count can drift without the build failing (D-04) | **FAILED (partial)** | True for `NUM_OUTPUT_FORMATS` and `NUM_LAYOUT_DEFS` (both are structurally tied to the enum body). **False for `NUM_ALGORITHMS`**: `AllAlgorithms.h:19-24`'s `static_assert (NUM_ALGORITHMS == 8, ...)` and `CountsTests.cpp:27-30`'s `CHECK (NUM_ALGORITHMS == 8)` are both tautologies — `8 == 8` — with no compiled link to the actual list of 8 `#include`d algorithm headers. Adding or removing an algorithm without hand-bumping the literal trips no gate. Confirmed by direct inspection; matches code-review WR-01. |
| 6 | (Derived from API-05 + phase rationale) CLAUDE.md's factual claims — including its stated canonical GitHub URL — are reproducible from the tree, and no Tier A surface this phase edited contradicts another on a question the phase itself resolved | **FAILED** | CLAUDE.md:8 states `Spatial-Media-Lab/SpatialCore` as the canonical `GitHub:` identity with no caveat. The actual origin remote is `https://github.com/AndrewRahman/SpatialCore.git` (`git remote -v`). `docs/integration-guide.md:22-31` — rewritten by this phase's plan 01-05 (DR-18) — explicitly states the organisation URL "does not resolve yet — do not substitute it." README.md:74 and the spatialcore-architecture skill:68 both still publish the org URL as a runnable `git submodule add` command. README.md:92 forwards readers to integration-guide.md "for the complete integration tutorial," so a reader following README's own pointer meets a direct contradiction. |

**Score:** 4/6 truths verified (0 present, behavior-unverified)

### Required Artifacts

| Artifact | Expected | Status | Details |
|----------|----------|--------|---------|
| `tests/Core/CountsTests.cpp` | Runtime `[counts]` backstop pinning 23/15/8 | VERIFIED (structurally) / **STUB-LIKE for algorithms** | File exists, 3 `TEST_CASE`s tagged `[counts]`, registered in `tests/CMakeLists.txt`. The `OutputFormat` and `LayoutID` cases are genuine backstops; the `NUM_ALGORITHMS` case is a tautology (see Truth 5). |
| `include/SpatialCore/Algorithms/AllAlgorithms.h` | `NUM_ALGORITHMS` count sentinel | VERIFIED (exists, wired) / weak guarantee | Constant present, referenced by the test above; the guarantee it provides is materially weaker than the format/layout sentinels it was modeled on. |
| `CLAUDE.md` | Corrected architecture table + build-system section | VERIFIED for table/JUCE/HRTF; **FAILED for GitHub identity line** | See Truths 3 and 6. |
| `.claude/skills/spatialcore-architecture/spatialcore-architecture.md` | Corrected auto-loaded architecture skill | VERIFIED for counts; **FAILED for submodule URL** | Counts (8/23/15, `ConstantPower` present) all correct. `git submodule add` example at line 68 still uses the non-resolving org URL. |
| `docs/superpowers/plans/2026-03-22-scaffold-spatialcore.md` | Historical stamp | VERIFIED | Stamp present, zero-deletion diff confirmed. |
| Tier B stamps (`docs/development-roadmap.md`, `.planning/intel/*.md`) | Supersession stamps | VERIFIED | All 5 intel files carry the stamp; development-roadmap.md's existing note was augmented in place with a zero-deletion diff. |
| `.planning/PROJECT.md`, `.planning/REQUIREMENTS.md` | Corrected planning docs | VERIFIED | `SpatialCore/DSP/Utilities.h` path removed, `gated on proof` language present, REQUIREMENTS.md's rewrite-rationale table states 23/15, BUG-03 states the full 39/17/15 inventory. |

### Key Link Verification

| From | To | Via | Status | Details |
|------|-----|-----|--------|---------|
| `include/SpatialCore/IO/OutputFormatRegistry.h` | CLAUDE.md/README.md/integration-guide/skill | static_assert failure message names Tier A docs | VERIFIED | `grep -c 'spatialcore-architecture'` in the header returns 1. |
| `tests/Core/CountsTests.cpp` | `tests/CMakeLists.txt` | registered source | VERIFIED | `Core/CountsTests.cpp` present in the executable's source list. |
| `CLAUDE.md` | `.claude/skills/spatialcore-architecture/spatialcore-architecture.md` | both auto-load, must state same counts | VERIFIED for counts; **NOT WIRED for GitHub/submodule URL** | Counts agree; the submodule-URL question does not. |
| `docs/integration-guide.md` | `github.com/AndrewRahman/SpatialCore` | live git submodule add instruction | VERIFIED (in isolation) | integration-guide.md itself is internally correct — the break is that CLAUDE.md/README.md/skill were not brought into agreement with it. |

### Requirements Coverage

| Requirement | Source Plan | Description | Status | Evidence |
|-------------|-------------|--------------|--------|----------|
| API-01 | 01-01, 01-03 | Algorithm count is one number across headers/docs | SATISFIED (current state) / drift-proofing weak | 8 stated everywhere; sentinel mechanism is tautological (Truth 5) |
| API-02 | 01-04 | `outputLimiter()` has one defined transfer function | SATISFIED | Verified Truth 2 |
| API-03 | 01-03 | HRTF profile count is one number across docs/headers/data | SATISFIED | `ls HRTF/*.sofa` returns 5; D-05 sentence present in README and skill |
| API-04 | 01-01, 01-03, 01-05 | Output-format/speaker-layout counts match the tree | SATISFIED | 23/15 confirmed in tree and every Tier A doc; REQUIREMENTS.md rewrite-rationale table corrected |
| API-05 | 01-03, 01-05 | CLAUDE.md describes the library that actually exists | **PARTIALLY SATISFIED** | Architecture table, JUCE version, HRTF description all correct (narrow REQUIREMENTS.md acceptance text scopes this to "the Architecture table"). Broader claim ("describes the library that actually exists") is undermined by the GitHub identity line (Truth 6). |
| BUG-03 | 01-02 | Cross-repo issue references are qualified | SATISFIED | Verified Truth 4 |

No orphaned requirements — all six phase requirement IDs (API-01 through API-05, BUG-03) appear in at least one plan's `requirements` frontmatter and are addressed above.

### Anti-Patterns Found

| File | Line | Pattern | Severity | Impact |
|------|------|---------|----------|--------|
| `include/SpatialCore/Algorithms/AllAlgorithms.h` | 19-24 | Tautological `static_assert` (`NUM_ALGORITHMS == 8` compared to itself) | Warning | Undermines the phase's own "pinned in code, cannot drift" claim for the algorithm count specifically (matches code-review WR-01) |
| `tests/Core/CountsTests.cpp` | 27-30 | Tautological `CHECK` mirroring the above | Warning | Same as above — the "runtime backstop" file banner overstates what this specific test proves |
| `CLAUDE.md` | 8 | Factually inaccurate GitHub URL vs. actual origin remote and vs. this phase's own DR-18 decision | Blocker | Auto-loads into every session; directly contradicts the phase's stated purpose (matches code-review WR-03) |
| `README.md` | 74 | Non-resolving org URL published as a runnable `git submodule add` command | Blocker (same root cause as above) | A reader following README's own forwarding pointer at line 92 hits a contradiction in integration-guide.md |
| `.claude/skills/spatialcore-architecture/spatialcore-architecture.md` | 68 | Same non-resolving submodule example | Blocker (same root cause) | Auto-loads every session alongside CLAUDE.md |
| `tests/Core/CountsTests.cpp` | 16 (file banner) | Test name/banner overstates coverage (registry/enum agreement not actually asserted by that test body) | Info | Matches code-review IN-01; cosmetic, does not affect correctness |

No `TBD`/`FIXME`/`XXX` debt markers found in any file this phase modified.

### Behavioral Spot-Checks

| Behavior | Command | Result | Status |
|----------|---------|--------|--------|
| `[counts]` suite is green | (already established by orchestrator) `./build/SpatialCoreTests "[counts]"` | 3 test cases, 4 assertions, pass | PASS |
| `outputLimiter` boundary suite is green | Test names present: below/at/above/non-finite | Confirmed via grep of `tests/Core/SpatialMathTests.cpp` | PASS |
| Full suite green modulo pre-existing unrelated failure | (already established) 149 cases, 148 pass, 1 pre-existing unrelated failure (`HutubsPP2Tests.cpp`) | Not attributable to Phase 1 | PASS (not counted against this phase) |
| Zero unqualified `#N` citations | `grep -rhoE '[[:alnum:]/-]*#[0-9]+' include/ src/ tests/ \| grep -cv 'OpenSpatialDelay#'` | `0` | PASS |

Step 7c (Probe Execution): SKIPPED — this phase declares no `scripts/*/tests/probe-*.sh` probes; it is a documentation phase verified via grep/build/test gates, not a probe-based migration phase.

### Human Verification Required

None. All findings in this report are grep/file/build-verifiable; no visual, real-time, or external-service behavior is in scope for a documentation phase.

### Gaps Summary

Four of the six observable truths in this report are cleanly verified: the count contract states
correct numbers everywhere (Truth 1), `outputLimiter()` has exactly one governing contract with
the scaffold plan properly stamped historical and the implementation provably untouched (Truth 2),
CLAUDE.md's architecture table is accurate (Truth 3), and every cross-repo issue citation resolves
correctly (Truth 4). BUG-03 in particular is complete and rigorously proven (idempotent rewrite,
all 15 numbers verified against the live tracker, byte-for-byte prefix-only diff).

Two gaps remain, both confirmed by direct file inspection and both already flagged by the code
review gate that ran before this verification:

1. **WR-03 (the more serious gap).** This phase resolved a real open question — which GitHub
   remote is canonical today — and applied that resolution to `docs/integration-guide.md` only.
   `CLAUDE.md`, `README.md`, and the auto-loading `spatialcore-architecture` skill were all edited
   by this same phase (plan 01-03) in the same sections, for other corrections, and were left
   stating the URL the phase itself just proved doesn't resolve. This is precisely the class of
   defect the phase exists to eliminate, and it is not pre-existing — it is fresh, or at minimum
   newly-exposed inconsistency the phase's own work surfaced without resolving everywhere it
   touched. It directly weakens the phase's own stated rationale ("A wrong CLAUDE.md is loaded
   into every session in this repo... Fix the map before using it").

2. **WR-01 (the narrower gap).** The algorithm-count sentinel does not deliver the drift-proofing
   the phase's own must-haves promised. This does not make any Tier A doc currently wrong — 8 is
   the correct count today — but it means a future algorithm addition/removal will not be caught
   by either the compile-time assertion or the runtime test, unlike the format and layout counts,
   which are genuinely self-enforcing.

**This looks intentional-adjacent but is not accepted as-is.** Both gaps have concrete, low-cost
fixes (propagate the "Remote topology" framing to three more files; derive `NUM_ALGORITHMS` from
the actual include list or an array of instances). Neither requires new design work. To accept
either as a deliberate deviation instead of closing it, add an `overrides:` entry to this file's
frontmatter with a reason, acceptor, and timestamp.

---

_Verified: 2026-08-15_
_Verifier: Claude (gsd-verifier)_
