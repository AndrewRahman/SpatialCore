# Phase 1: Documentation Truth & Contract Freeze - Pattern Map

**Mapped:** 2026-08-11
**Files analyzed:** 24 (5 Tier A docs, 3 Tier B stamps, 2 header sentinel sites, 2 test-file edits, ~10 BUG-03 comment sites (19 occurrences), 2 optional planning-doc corrections)
**Analogs found:** 24 / 24 — every file in scope is itself an existing file being edited in place (this phase creates zero new source files; it edits comments/constants/prose in existing files). "Analog" below means the *nearest already-correct precedent inside the same file or a sibling file* to copy the pattern from.

**Numeric contract for all excerpts below: 8 algorithms, 23 output formats, 15 speaker layouts, JUCE 9.0.0, 5 SOFA HRTF profiles.** Do not write 25/14/7/6/22/13 anywhere in a plan.

## File Classification

| File to Modify | Role | Data Flow | Closest Analog (pattern source) | Match Quality |
|---|---|---|---|---|
| `include/SpatialCore/IO/OutputFormat.h` | config (enum + sentinel) | transform (compile-time constant) | `include/SpatialCore/IO/OutputFormatRegistry.h:9` (`NUM_OUTPUT_FORMATS`) + `include/SpatialCore/IO/SpeakerLayout.h:41-45` (`NUM_LAYOUT_DEFS` enumerator-sentinel) | exact — two existing sibling precedents for the same pattern |
| `include/SpatialCore/IO/SpeakerLayout.h` | config (assert only, no new constant) | transform | itself — `NUM_LAYOUT_DEFS` already exists at `:44`; D-11 says assert against it, don't duplicate | exact |
| `include/SpatialCore/Algorithms/AllAlgorithms.h` (or new sentinel header) | config (enum-adjacent count) | transform | `include/SpatialCore/IO/OutputFormatRegistry.h:9` (`static constexpr int NUM_OUTPUT_FORMATS = 23;`) | role-match |
| `tests/Core/SpatialMathTests.cpp` | test | request-response (pure function assertions) | itself — existing `TEST_CASE`s at `:66-98` are the exact pattern to extend | exact |
| new `tests/Core/CountsTests.cpp` (or append to `SpatialMathTests.cpp`) | test | CRUD-adjacent (compile-time/runtime pin) | `tests/Core/SpatialMathTests.cpp` full file (header, `using namespace`, `TEST_CASE` style, tag convention `[spatialmath][outputlimiter]`) | role-match |
| `CLAUDE.md` (architecture table + JUCE line) | config/doc | transform (prose) | itself — table rows for `Engine`/`Binaural` are already correct; `I/O`/`DSP` rows are the defect | exact (self-correcting doc) |
| `README.md` | doc | transform | `CLAUDE.md`'s already-correct component table as the tone/format reference | role-match |
| `docs/integration-guide.md` | doc | transform | `README.md`'s corrected sections (same phase, same numbers) | role-match |
| `.claude/skills/spatialcore-architecture/spatialcore-architecture.md` | doc (auto-loaded skill) | transform | `CLAUDE.md`'s corrected architecture table (D-02: same first-class doc-surface treatment) | role-match |
| `.claude/skills/spatial-audio-dsp/SKILL.md` (`:518`, `:522`) | doc (auto-loaded skill) | transform | `CLAUDE.md:72,74` (JUCE version line + already-correct HRTF-loading prose) | exact |
| `docs/development-roadmap.md` | doc (Tier B, stamp only) | transform | none needed — new pattern (historical-header stamp), first instance in repo | new pattern (see Shared Patterns) |
| `docs/superpowers/plans/2026-03-22-scaffold-spatialcore.md` | doc (Tier B, stamp only) | transform | same stamp pattern as `development-roadmap.md` | new pattern (shared) |
| `.planning/intel/*.md` | doc (Tier B, stamp only) | transform | same stamp pattern | new pattern (shared) |
| `include/SpatialCore/Binaural/SharedFFTCache.h:17` | comment (BUG-03) | — | itself — bare `(issue #131)` at `:17` is the exact string to qualify | exact |
| `include/SpatialCore/Binaural/PartitionedConvolver.h` (`:27,43,49,55`) + `.cpp` (`:7,50,90,122,172,200`) | comment (BUG-03) | — | `SharedFFTCache.h:17` sets the qualification format | exact |
| `include/SpatialCore/Binaural/HRTFDatabase.h:58` / `.cpp:119,170` | comment (BUG-03) | — | same | exact |
| `include/SpatialCore/Binaural/BinauralRenderer.h:21` | comment (BUG-03) | — | same | exact |
| `include/SpatialCore/UI/SpatialMapComponent.h:131-132` | comment (BUG-03) | — | same | exact |
| `include/SpatialCore/OSC/OSCPortValidation.h:9` | comment (BUG-03) | — | same | exact |
| `src/UI/PresetBrowser.cpp:25,135` | comment (BUG-03) | — | same | exact |
| `src/Trajectory/TrajectoryEngine.cpp:185` | comment (BUG-03) | — | same | exact |
| `src/Engine/RenderEngine.cpp:47` | comment (BUG-03) | — | same | exact |
| `.planning/PROJECT.md` (OQ-5, OQ-2 DSP path) | doc (planning, discretionary) | transform | `CLAUDE.md`'s D-09 phantom-`DSP/`-row deletion as the reference correction | role-match |
| `.planning/REQUIREMENTS.md` (API-05 stale claims) | doc (planning, discretionary) | transform | same | role-match |

## Pattern Assignments

### `include/SpatialCore/IO/OutputFormat.h` (config, add sentinel)

**Analog:** `include/SpatialCore/IO/OutputFormatRegistry.h:9` and `include/SpatialCore/IO/SpeakerLayout.h:41-45`

**Current full file** (45 lines) — enum has no adjacent sentinel:
```cpp
// include/SpatialCore/IO/OutputFormat.h:1-28
#pragma once

namespace spatialcore
{

// Output formats — Binaural, Stereo, Surround, Octaphonic, Atmos, SML, Ambisonics
// 1 Binaural + 1 Stereo + 15 Surround + 6 Ambisonics = 23 total
// Stereo mode (Equal Power, VBAP, XY, MS, Blumlein) selected via algorithm parameter
enum class OutputFormat
{
    Binaural = 0,
    Stereo,
    Quad, Surround5_0, Surround5_1, Surround7_0, Surround7_1,
    Surround9_1,
    Octaphonic,
    Surround5_1_2, Surround5_1_4, Surround7_1_2,
    Surround7_1_4, Surround7_1_6, Surround9_1_4, Surround9_1_6,
    SurroundSML13_1,
    AmbisonicsFOA, AmbisonicsSOA, AmbisonicsHOA,
    Ambisonics4OA, Ambisonics5OA, Ambisonics6OA
};
```
The enum's own comment (`:7`) already says "23 total" — confirms 23 is correct, no enum-body edit needed.

**Sentinel pattern to copy** (from `OutputFormatRegistry.h:9`, D-11's `constexpr`-adjacent style):
```cpp
// include/SpatialCore/IO/OutputFormatRegistry.h:9 [existing precedent, same repo]
static constexpr int NUM_OUTPUT_FORMATS = 23;
```
Per D-11, `OutputFormat.h` currently has no sentinel of its own (the registry's constant lives in a
different header). The plan should decide: either add a second `constexpr` directly beside
`enum class OutputFormat` in this file with a `static_assert` tying it to the registry's constant,
or assert the registry's existing constant against a freshly counted literal — either way, copy the
`static constexpr int NAME = N;` declaration style verbatim from `OutputFormatRegistry.h:9`.

**`static_assert` message pattern (D-12 — must name Tier A files):**
```cpp
static_assert (NUM_OUTPUT_FORMATS == 23,
    "OutputFormat count changed -- update CLAUDE.md, README.md, "
    "docs/integration-guide.md, .claude/skills/spatialcore-architecture/"
    "spatialcore-architecture.md");
```

---

### `include/SpatialCore/IO/SpeakerLayout.h` (assert only, no new constant — D-11 exception)

**Analog:** itself, `NUM_LAYOUT_DEFS` already shipped

**Existing enumerator sentinel** (lines 41-45, do not duplicate):
```cpp
// include/SpatialCore/IO/SpeakerLayout.h:41-45
enum LayoutID
{
    Quad, S5_0, S5_1, S7_0, S7_1, S9_1, S5_1_2, S5_1_4, S7_1_2, S7_1_4, S7_1_6,
    S9_1_4, S9_1_6, Octaphonic, SML13_1, NUM_LAYOUT_DEFS
};
```
15 named entries before the sentinel → `NUM_LAYOUT_DEFS == 15`. Add only:
```cpp
static_assert (NUM_LAYOUT_DEFS == 15,
    "LayoutID count changed -- update CLAUDE.md, README.md, "
    "docs/integration-guide.md, .claude/skills/spatialcore-architecture/"
    "spatialcore-architecture.md");
```
placed near the enum (e.g. immediately after it, or in `SpeakerLayout.cpp` beside the
`layoutDefs[NUM_LAYOUT_DEFS]` array definition — planner's call, both are "adjacent").

---

### `include/SpatialCore/Algorithms/AllAlgorithms.h` (algorithm-count sentinel — new addition)

**Analog:** `OutputFormatRegistry.h:9` constant-declaration style; algorithm list from the umbrella header itself

**Current umbrella header** (confirms 8 concrete algorithms + 1 abstract base, verbatim):
```cpp
// include/SpatialCore/Algorithms/AllAlgorithms.h (full file)
#pragma once

// Spatial Media Library -- all spatialization algorithm headers
#include <SpatialCore/Algorithms/SpatializationAlgorithm.h>
#include <SpatialCore/Algorithms/DirectBinauralAlgorithm.h>
#include <SpatialCore/Algorithms/VBAPAlgorithm.h>
#include <SpatialCore/Algorithms/AmbisonicsAlgorithm.h>
#include <SpatialCore/Algorithms/VBIPAlgorithm.h>
#include <SpatialCore/Algorithms/KNNAlgorithm.h>
#include <SpatialCore/Algorithms/DBAPAlgorithm.h>
#include <SpatialCore/Algorithms/MDAPAlgorithm.h>
#include <SpatialCore/Algorithms/ConstantPowerAlgorithm.h>
```
8 concrete algorithm headers beyond the abstract base (`SpatializationAlgorithm.h`). Add, copying the
`OutputFormatRegistry.h:9` constant style:
```cpp
static constexpr int NUM_ALGORITHMS = 8;
static_assert (NUM_ALGORITHMS == 8,
    "Algorithm count changed -- update CLAUDE.md, README.md, "
    ".claude/skills/spatialcore-architecture/spatialcore-architecture.md");
```
Placement per D-11: a free-standing `constexpr`, never a new enumerator (there is no algorithm enum
in this codebase — algorithms are concrete classes, so this is purely a header-comment-adjacent
constant, not an enum-adjacent one; still satisfies D-11's spirit).

---

### `tests/Core/SpatialMathTests.cpp` (extend `outputLimiter` boundary coverage)

**Analog:** itself — copy the existing `TEST_CASE` style exactly

**File header / imports** (lines 1-8, copy verbatim for any new test file too):
```cpp
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <SpatialCore/Core/SpatialMath.h>
#include <cmath>
#include <limits>

using namespace spatialcore;
using Catch::Matchers::WithinAbs;
```

**Existing `TEST_CASE` pattern to extend or split** (lines 73-81 — already covers at/below/above
ceiling in one parameterized loop; Common Pitfall 4 in RESEARCH.md says this may already satisfy
API-02, so treat the task as "verify or split for auditability," not "write from scratch"):
```cpp
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
Tag convention to reuse for any new named split cases: `"[spatialmath][outputlimiter]"`.

---

### New counts test (e.g. `tests/Core/CountsTests.cpp`)

**Analog:** `tests/Core/SpatialMathTests.cpp` full structure

Copy the same include block + `using namespace spatialcore;`, add a `TEST_CASE` tagged `[counts]`
pinning `NUM_OUTPUT_FORMATS == 23`, `NUM_LAYOUT_DEFS == 15`, `NUM_ALGORITHMS == 8` at runtime as the
DR-4 backstop to the header `static_assert`s (D-13). Register the new `.cpp` in `tests/CMakeLists.txt`
the same way `SpatialMathTests.cpp` is already registered there (read `tests/CMakeLists.txt`'s source
list at plan time to copy the exact add-file line).

---

### `CLAUDE.md` (architecture table + JUCE line — Tier A)

**Analog:** itself — the `Engine` row (`:19`) and HRTF-loading prose (`:74`, quoted below) are
already correct and are the tone/format reference for the corrected rows.

**Rows needing correction** (current text, verbatim):
```
| I/O | `IO/*.h` | OutputFormatRegistry (22 formats), SpeakerLayout (13 ITU-R layouts), AmbisonicsCodec (SH eval, decode matrices) |
| DSP | `DSP/*.h` | softClip(), outputLimiter() |
```
→ I/O row: `22` → `23`, `13` → `15`. DSP row: delete entirely (D-09 — `include/SpatialCore/DSP/`
does not exist; the functions live in `Core/SpatialMath.h:85,100`).

**Already-correct row to use as the pattern for phrasing** (no change needed, cite as reference):
```
| Algorithms | `Algorithms/*.h` | 8 spatialization algorithms: ConstantPower, VBAP, VBIP, KNN, DBAP, MDAP, Ambisonics, DirectBinaural |
```

**JUCE version line** — find and replace `JUCE 8` → `JUCE 9.0.0` (mirrors `CMakeLists.txt:32`'s
`GIT_TAG 9.0.0`, already the ground truth).

---

### `README.md`, `docs/integration-guide.md`, `.claude/skills/spatialcore-architecture/spatialcore-architecture.md`, `.claude/skills/spatial-audio-dsp/SKILL.md` (Tier A)

**Analog:** `CLAUDE.md`'s corrected architecture table (above) for numbers/format; D-05's exact
canonical HRTF phrasing for the profile count.

**D-05 canonical HRTF phrasing (copy verbatim wherever HRTF count appears):**
```
5 SOFA HRTF profiles ship; profileIndex is 0-5, where 0 = Simple (Woodworth) and 1-5 select
the SOFA profiles.
```
Replaces "6 HRTF Profiles: Simple (Woodworth), MIT KEMAR, SADIE II D2, CIPIC Subject003, HUTUBS PP2,
Bernschuetz KU100" wherever it appears (`README.md:34`, skill `:44`).

**Exact line-by-line fix table** (all verified this session, reuse without re-deriving):

| File | Line | Current | Fix |
|---|---|---|---|
| `README.md` | 21 | `### Spatialization Algorithms (7)` | `(8)` |
| `README.md` | 36 | `### Output Format Support (22 formats)` | `(23 formats)` |
| `README.md` | 39 | `- 13 Surround (Quad through 9.1.6 Atmos)` | `15 Surround` |
| `README.md` | 43 | `- 13 ITU-R BS.775/BS.2051 standard layouts...` | `15 ITU-R...` |
| `README.md` | 95 | `- **JUCE 8** (C++17)...` | `JUCE 9.0.0` |
| `docs/integration-guide.md` | 7 | `- JUCE 8 (C++17)` | `JUCE 9.0.0` |
| `docs/integration-guide.md` | 22 | `git submodule add https://github.com/Spatial-Media-Lab/SpatialCore.git SpatialCore` | swap to `AndrewRahman/SpatialCore` per D-17, label org URL as post-proof destination |
| `docs/integration-guide.md` | 166 | `- 22 output formats` | `23 output formats` |
| skill `spatialcore-architecture.md` | 42 | `7 Spatialization Algorithms \| VBAP, VBIP, KNN, DBAP, MDAP, Ambisonics (HOA), DirectBinaural (Woodworth)` | `8 ...` + add `ConstantPower` to the name list |
| skill `spatialcore-architecture.md` | 45 | `22 Output Formats \| 1 Binaural + 1 Stereo (5 modes) + 13 Surround (Quad-9.1.6) + 6 Ambisonics (FOA-6OA)` | `23 ... 15 Surround ...` |
| skill `spatialcore-architecture.md` | 46 | `13 Speaker Layouts \| ITU-R BS.775/BS.2051, SMPTE channel ordering` | `15 Speaker Layouts \| ...` |
| skill `spatial-audio-dsp/SKILL.md` | 518 | `- Framework: JUCE 8.0.3 (git submodule at JUCE/)` | `JUCE 9.0.0` |
| skill `spatial-audio-dsp/SKILL.md` | 522 | `- HRTF data: 5 SOFA files in HRTF/ embedded as binary resources (Git LFS tracked)` | mirror `CLAUDE.md:74`'s already-correct wording: `loaded at runtime via HRTFDatabase::loadFromFile (Git LFS tracked)` |

---

### Tier B stamp pattern (`docs/development-roadmap.md`, `docs/superpowers/plans/2026-03-22-scaffold-spatialcore.md`, `.planning/intel/*.md`)

**Analog:** none exists in-repo yet — this is a new pattern this phase introduces (D-01/D-03). Use
this exact header, inserted at the top of each Tier B file, and do not touch any numbers below it:
```markdown
> **Historical — superseded by `.planning/ROADMAP.md`, 2026-08-10.** Counts and status in this
> document are a dated snapshot and are not maintained. See `CLAUDE.md`/`README.md` for current
> canonical counts.
```
Apply identically to all three Tier B locations (`docs/development-roadmap.md`,
`docs/superpowers/plans/2026-03-22-scaffold-spatialcore.md`, each file under `.planning/intel/`).
Do not renumber anything below the stamp — that is the entire point of Tier B (D-01).

---

### BUG-03 comment sites (19 occurrences, 10 files — full table below)

**Analog:** `include/SpatialCore/Binaural/SharedFFTCache.h:17` — the simplest, single-occurrence
site, sets the exact qualification format for all 19.

**Current form:**
```cpp
// include/SpatialCore/Binaural/SharedFFTCache.h:11-17
// Process-global FFT cache -- prevents vDSP twiddle table use-after-free
// when multiple plugin instances create/destroy FFT setups concurrently.
// vDSP shares internal twiddle factor memory across setups of the same order;
// destroying the last setup frees the shared table even if another thread's
// vDSP_fft_zrip is reading from it.  The cache creates once, never destroys.
// (issue #131)
```
**Target form (D-14's exact qualification):**
```cpp
// (issue Spatial-Media-Lab/OpenSpatialDelay#131)
```
Mechanical find/replace: `#131` → `Spatial-Media-Lab/OpenSpatialDelay#131`, and identically for every
other bare `#N` in the table below. Preserve everything else in the comment verbatim.

**Full site table (use this, not D-14's 4-site sample — RESEARCH.md Pitfall 3):**

| File | Line(s) | Bare form | Qualify to |
|---|---|---|---|
| `include/SpatialCore/Binaural/SharedFFTCache.h` | 17 | `#131` | `Spatial-Media-Lab/OpenSpatialDelay#131` |
| `include/SpatialCore/Binaural/PartitionedConvolver.h` | 27, 43, 49, 55 | `#50, #131, #50, #234` | qualify each in place |
| `include/SpatialCore/Binaural/HRTFDatabase.h` | 58 | `#47` | qualify |
| `include/SpatialCore/Binaural/BinauralRenderer.h` | 21 | `#96` | qualify |
| `include/SpatialCore/UI/SpatialMapComponent.h` | 131, 132 | `#168` (×2) | qualify |
| `include/SpatialCore/OSC/OSCPortValidation.h` | 9 | `#179` | qualify |
| `src/Binaural/PartitionedConvolver.cpp` | 7, 50, 90, 200, 122, 172 | `#131, #50×3, #234×2` | qualify each |
| `src/Binaural/HRTFDatabase.cpp` | 119, 170 | `#89, #47` | qualify |
| `src/UI/PresetBrowser.cpp` | 25, 135 | `#37` (×2) | qualify |
| `src/Trajectory/TrajectoryEngine.cpp` | 185 | `#100` | qualify |
| `src/Engine/RenderEngine.cpp` | 47 | `#131` | qualify |

## Shared Patterns

### Tier A doc-number correction
**Source:** `CLAUDE.md`'s already-correct `Engine`/`Algorithms` rows and `:74`'s HRTF-loading prose.
**Apply to:** `README.md`, `docs/integration-guide.md`, both `.claude/skills/*` files.
Canonical numbers: 8 algorithms, 23 output formats, 15 speaker layouts, JUCE 9.0.0, "5 SOFA HRTF
profiles ship; profileIndex is 0-5..." (D-05 two-number phrasing, never a bare "5" or "6").

### Compile-time count sentinel
**Source:** `include/SpatialCore/IO/OutputFormatRegistry.h:9` (`static constexpr int NUM_OUTPUT_FORMATS = 23;`) and `include/SpatialCore/IO/SpeakerLayout.h:44` (`NUM_LAYOUT_DEFS` enumerator).
**Apply to:** `OutputFormat.h` (new sentinel), `AllAlgorithms.h` (new sentinel), `SpeakerLayout.h`
(assert-only, no new constant per D-11's stated exception).
```cpp
static constexpr int NAME = N;
static_assert (NAME == N, "... names the Tier A files to update ...");
```

### Header-adjacent contract docblock (no new doc file)
**Source:** `include/SpatialCore/Core/SpatialMath.h:98-99` (`outputLimiter` docblock) — already
correct, D-06 designates it the governing contract as-is, zero edit required.
**Apply to:** nothing new — cited as the precedent that D-06 deliberately does not add a
`docs/api-contract.md` (rejected per RESEARCH.md's "Don't Hand-Roll").

### Bare `#N` issue qualification
**Source:** `include/SpatialCore/Binaural/SharedFFTCache.h:17`.
**Apply to:** all 19 occurrences across the 10 files in the BUG-03 table above.
Format: `Spatial-Media-Lab/OpenSpatialDelay#N` (D-14), mechanical string substitution only.

### Tier B "stamp, don't renumber" header
**Source:** new pattern this phase introduces (no prior precedent) — see Tier B section above.
**Apply to:** `docs/development-roadmap.md`, `docs/superpowers/plans/2026-03-22-scaffold-spatialcore.md`, `.planning/intel/*.md`.

## No Analog Found

| File | Role | Data Flow | Reason |
|---|---|---|---|
| Tier B stamp header text | doc | transform | No prior "historical/superseded" banner exists anywhere in this repo — this phase originates the convention (see Shared Patterns, D-01/D-03). Use the exact wording given above for all three Tier B locations to keep it uniform. |
| `constexpr` sentinel adjacent to a non-enum umbrella header (`AllAlgorithms.h`) | config | transform | `OutputFormatRegistry.h:9`'s sentinel sits beside a `class`, and `SpeakerLayout.h:44`'s sits inside an `enum`; there is no existing precedent for a bare `constexpr` beside an include-only umbrella header with no enum/class of its own. Use the `OutputFormatRegistry.h:9` declaration style verbatim; placement (this file vs. a new `AlgorithmCount.h`) is a planner discretion call, not a pattern gap. |

## Metadata

**Analog search scope:** `include/SpatialCore/**`, `src/**`, `tests/Core/**`, `CLAUDE.md`,
`README.md`, `docs/**`, `.claude/skills/**`, `.planning/**`.
**Files scanned:** ~30 (targeted reads/greps per RESEARCH.md's already-exhaustive line citations;
no blind full-tree scan needed since RESEARCH.md already pinpointed every line).
**Pattern extraction date:** 2026-08-11
