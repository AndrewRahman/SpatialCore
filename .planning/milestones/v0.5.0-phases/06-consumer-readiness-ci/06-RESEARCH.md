# Phase 6: Consumer Readiness & CI - Research

**Researched:** 2026-10-17
**Domain:** CMake consumer integration (JUCE 9.0.0 static-lib submodule), GitHub Actions CI (macOS/Linux/Windows), Clang source-based coverage, realtime-safe misuse detection, regression-test evidence
**Confidence:** HIGH for everything run or read this session (coverage numbers, consumer build, CI defect, timings); MEDIUM for runner timings and Linux/Windows first-run outcomes (never run); items tagged `[ASSUMED]` are listed in the Assumptions Log.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions

**Consumer harness (INTG-01)**
- **D-01:** The harness lives **in this repo** (e.g. `examples/harness/`) and pulls SpatialCore in **as a git submodule from the GitHub remote**, exactly as OSD does, then `add_subdirectory(SpatialCore)` + `target_link_libraries(... PRIVATE SpatialCore SpatialCoreUI)`. It must not point at the local source folder — the submodule step is what is being proved. Remote is `https://github.com/AndrewRahman/SpatialCore` (CLAUDE.md; `Spatial-Media-Lab/` does not resolve yet, DR-18). It is built from a clean clone in Release on macOS, in CI.
- **D-02:** **Add a UI umbrella header `<SpatialCore/SpatialCoreUI.h>`** that includes all public UI headers. The harness includes exactly `<SpatialCore/SpatialCore.h>` and `<SpatialCore/SpatialCoreUI.h>`. The DSP umbrella stays UI-free on purpose. Amend ROADMAP Phase 6 SC-1 and REQUIREMENTS INTG-01 ("includes only `<SpatialCore/SpatialCore.h>`") to the two-header wording. Also fold in audit W2 (`OSC/OSCPortValidation.h` missing from the umbrella) and W3 (stale umbrella comment) while touching the umbrellas.
- **D-03:** The harness **uses all five opt-in facade features** with no worked example today (audit W1): `setHRTFProfile`, `engineSelectsHRTF`, `engineFadesFormatSwitch`, `setAlgorithmIndex`, `getBlockLayout()` — alongside `engineComputesGains` and `engineDerivesDispatch`. It is the copy-paste reference for OSD's migration.
- **D-04:** Beyond building, the harness has a **short headless automatic sound check** run in CI: render blocks for each output format and assert output is finite and not silent. No DAW.

**Gains safety (INTG-02)**
- **D-05:** **Keep `engineComputesGains` default `false`** (OSD's sound stays byte-for-byte; the CLAUDE.md compatibility promise holds). Add **detection**: when the flag is off and a block has active objects whose supplied gains are all zero, the engine counts it in a lock-free counter readable from any thread (same pattern as `getOversizedBlockCount()` / `getWriterViewOnRenderThreadCount()`) and asserts in Debug builds. — **Reversibility:** reversible
- **D-06:** In Release: **count only, keep playing.** No audio-thread logging, no silent fallback that computes gains on the consumer's behalf (that would hide the mistake #14 was filed about).
- **D-07:** Scope is **gains only**. The other opt-in flags get no detector. The integration guide gets one clear "set these flags" block naming the recommended set and which mode a consumer must choose (INTG-02 acceptance). No one-call helper API.

**CI (CI-01)**
- **D-08:** Runners (GitHub-hosted, repo is public so macOS minutes are free; the user only has a Mac locally):
  - **macOS (Apple Silicon) — the gate:** full tests.
  - **Linux:** keep the existing job and fix it so it actually runs the tests.
  - **Windows:** build-only (catches breakage before OSD ships a Windows build).
  This honours Phase 5 D-13's "Windows and Linux are checked in Phase 6" in a limited form; amend REQUIREMENTS CI-01's scope note ("cross-platform matrices remain out of scope") accordingly. — **Reversibility:** reversible
- **D-09:** Every push/PR on macOS runs the **same as the local gate**: Debug build + both test executables, Release build + `ctest`, `SpatialCoreUITests`, and the harness build + sound check. CI then replaces "the local gate is the only gate" in CLAUDE.md / README / memory.
- **D-10:** ThreadSanitizer build + `[rt]` trap pass run **nightly (schedule) and on `workflow_dispatch`**, not on every push. They are still run by hand at every phase end and before a release tag.
- **D-11:** **Require green CI to merge to `main`** via GitHub branch protection, turned on once the macOS job is green. This is an outward-facing repo-settings change: confirm with the user at the moment it is applied.
- **D-12:** Fix the existing workflow's stale comments (Phase 8/9 plan references, "suite is empty of new tests") and the `ctest` working-directory / `enable_testing()` defect.

**Coverage (TEST-01, OQ-4)**
- **D-13:** **Measure first, then the user picks the target mid-phase.** Wire instrumentation into CMake, produce the real number per area (algorithms, binaural, engine, IO, OSC, trajectory, UI), then a `checkpoint:decision` presents it in plain terms and the user sets the target. OQ-4 closes there.
- **D-14:** Once set, **CI fails when coverage drops below the target.**
- **D-15:** Reports stay **inside GitHub**: a job summary on each run plus a downloadable artifact. No external service (no Codecov, no account, no data sent to a third party).
- **D-16:** UI code is **measured as a separate number** (from `SpatialCoreUITests`), so the UI cannot move the engine's figure. Amend the REQUIREMENTS TEST-01 note that excluded `src/UI/*.cpp` (written before `SpatialCoreUITests` existed).

**Regression tests (TEST-01)**
- **D-17:** Add the missing `OpenSpatialDelay#50` regression test, and confirm each of the four existing ones (#89, #96, #131, #234) fails against the old behaviour, under Phase 5's D-07 rule (temporarily restore the old behaviour, see red, restore). Record the red evidence.

### Claude's Discretion
- Coverage tool choice (Clang source-based `-fprofile-instr-generate` + `llvm-cov` vs gcov/lcov), and which build type it runs in.
- Exact harness folder name and whether it is a `juce_add_plugin` or a console app (it must link both targets and exercise the five features; a plugin target is preferred if it builds headless in CI).
- Exact detection rule for D-05 (e.g. per-object all-zero gain row, which objects count as active) and the counter's name.
- macOS runner image and JUCE/LFS caching in CI.
- How the Windows build-only job handles LFS HRTF data and the `%ProgramData%` path (build only; no claim that it runs).

### Deferred Ideas (OUT OF SCOPE)
- Running the full test suite on Windows (D-08 keeps Windows build-only).
- Detectors for the other opt-in flags, or a one-call "recommended setup" helper API (D-07).
- External coverage service / README badge (D-15).
- Real-DAW multi-instance check and OSD's own migration: OSD repo.
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| INTG-01 | A consumer plugin can submodule, link, and build against SpatialCore (two targets, Release, macOS, from a clean clone) | Prototype consumer built clean in Release against the local tree (VST3 plugin + console check, both targets linked, non-silent render). Self-referential submodule trap and the "materialise a throwaway consumer repo" answer (Pitfall 1, Pattern 1). Consumed-mode surprises found (Pitfalls 2-4). Umbrella gaps measured (3 headers). |
| INTG-02 | `engineComputesGains` does not silently no-op for a migrating consumer | Exact read-sites of the gain arrays (which render paths consume them), detector placement after `ctx` is finalised, counter pattern at `RenderEngine.h:436`/`RenderEngine.cpp:756,1912`, false-positive map (HRTF/Ambisonics read no gain arrays), existing flag-off tests that supply constants (Pattern 3). |
| CI-01 | SpatialCore is CI-verified against JUCE 9.0.0 (macOS green run, Catch2 suite) | Root cause of "No tests were found" verified; `macos-26` image facts; LFS bandwidth quota risk and cache pattern; action versions; branch-protection facts; nightly constraints (Patterns 4-5, Pitfalls 5-9). |
| TEST-01 | Coverage instrumentation reports a real number; PartitionedConvolver/BinauralRenderer dedicated tests; regression tests for OSD#50/#89/#96/#131/#234 | **Measured** per-area numbers (Debug, whole suite), tool commands that work on this Mac, UI-linkage measurement gotcha, status of each of the five regression tests (3 already have recorded red evidence, #50 missing, #89 only indirectly covered) (Coverage section, Regression-test status table). |
</phase_requirements>

## Summary

Phase 6 is mostly plumbing, and the plumbing has four real traps that were found by running things rather than reading them. (1) CI "runs zero tests" because `include(CTest)` lives only in `tests/CMakeLists.txt`, so the build root has no `CTestTestfile.cmake`; `ctest -N` at the root reports `Total Tests: 0` while `ctest --test-dir tests -N` reports 403. Local CMake 4.4.4 exits 1 on that, the Linux runner's ctest exited 0, so always pass `--no-tests=error`. (2) A harness that registers SpatialCore as a committed submodule of SpatialCore itself is self-referential; the harness must live in-repo as a *template* and a script must materialise a throwaway consumer repo (`git init`, `git submodule add <remote>`, fetch the SHA under test, `git lfs pull`) at CI time. (3) In consumed mode `SPATIALCORE_BUILD_TESTS` still defaults ON, so every consumer's `all` target builds Catch2, `SpatialCoreTests`, `SpatialCoreUITests` and the RTGuard library, and its `ctest` inherits 403 tests. That is exactly the kind of "surprise" INTG-01 is about. (4) UI coverage measured through `SpatialCoreUITests` silently omits every UI source the test binary does not link: 7 of 10 `src/UI/*.cpp` files are invisible, which turns a true 25.9% line figure into a misleading 41.4%.

The measured baseline (Debug, AppleClang 21, whole `SpatialCoreTests` in one process, 387 cases, all pass) is **93.9% line coverage** for the engine side (src + public headers), 85.6% branches, with `src/Binaural` the weakest area at 86.7% and `HRTFDatabase.cpp` the weakest file at 68%. `PartitionedConvolver.cpp` is 96.1% and `BinauralRenderer.cpp` 100% lines, so TEST-01's "dedicated tests" clause is met and verified. UI is 25.9% of 1,618 lines once all ten UI sources are force-linked. Of the five OSD regression tests, #96, #131 and #234 already have recorded red-against-old-behaviour evidence from Phases 3 and 5; **#50 has no test**, and **#89 is only indirectly covered** (the cited line in `SadieD2KU100Tests.cpp:67` is a comment, not a test of the onset-detection fallback or the great-circle pole threshold).

**Primary recommendation:** Build the phase in this order: (a) CMake hygiene (`enable_testing()` at the top level only, `SPATIALCORE_BUILD_TESTS` defaulting to `PROJECT_IS_TOP_LEVEL`, an opt-in `SPATIALCORE_ENABLE_COVERAGE` option), (b) umbrella headers plus completeness tests, (c) the INTG-02 detector, (d) the harness template plus a `build-consumer.sh` that proves the submodule path against the SHA under test, (e) the macOS gate workflow (push it and watch a green run before touching anything else), then Linux/Windows/nightly, LFS caching and the branch-protection checkpoint, (f) coverage script and the `checkpoint:decision` for the target, (g) the #50 and #89 regression tests with mutation evidence, (h) the doc and memory amendments that retire "CI runs zero tests".

## Project Constraints (from CLAUDE.md)

- NEVER allocate memory in any function called from `processBlock`; the D-05 detector runs on the audio thread: one relaxed atomic `fetch_add`, no logging, no allocation. [VERIFIED: CLAUDE.md Critical Rules]
- NEVER modify the `SpatializationAlgorithm` interface without a major bump; the detector must not touch it. [VERIFIED: CLAUDE.md]
- ALWAYS keep backward compatibility with existing plugins: every new flag/counter is additive; `engineComputesGains` stays default `false` (D-05). Flipping `SPATIALCORE_BUILD_TESTS`'s consumed-mode default is a build-system change that needs a conscious call (Assumption A7).
- ALWAYS run the full test suite before tagging a release; the realtime-safety check (README "Realtime-safety check") runs at every phase end and before a release tag. [VERIFIED: CLAUDE.md]
- Remote topology: clone/submodule URL is `https://github.com/AndrewRahman/SpatialCore`; `Spatial-Media-Lab/SpatialCore` does not resolve yet (DR-18). The harness must use the former. [VERIFIED: CLAUDE.md Identity]
- Pre-1.0: stays `0.x`; the doc/memory edits must not imply a 1.x bump. [VERIFIED: CLAUDE.md Versioning; user memory "SpatialCore stays pre-1.0"]
- HRTF profile indices are never renumbered; display names never change (D-07). The harness reads them via `HRTFProfile.h` only.
- Adding an algorithm means editing `AllAlgorithmTypes`; not relevant to this phase except that the harness must not hard-code counts (use `NUM_OUTPUT_FORMATS`).
- Doc surfaces that say "CI runs zero tests" (must be updated when CI runs the suite): `README.md:130`, `README.md:166` ("CI runs none of this until Phase 6"), `docs/integration-guide.md:509`, `CLAUDE.md:141` (Tests bullet), and the user memory file `spatialcore-ship-and-verify-flow.md`. [VERIFIED: grep this session]
- Line endings: `tests/CMakeLists.txt` and `README.md` are CRLF; `docs/integration-guide.md`, `.claude/skills/spatial-audio-dsp/SKILL.md` and `include/SpatialCore/IO/AmbisonicsCodec.h` are pinned LF in `.gitattributes`. Preserve each file's endings when editing. [VERIFIED: `file` output and `.gitattributes`]
- User preferences from memory: the user is not a programmer, runs nothing by hand in UAT, and has only a Mac; the coverage `checkpoint:decision` must be in plain terms; trace provenance before correcting a number in docs.

## Architectural Responsibility Map

| Capability | Primary Tier | Secondary Tier | Rationale |
|------------|-------------|----------------|-----------|
| Zero-gain misuse detection (INTG-02) | Library engine (`RenderEngine::renderBlock`, audio thread) | Consumer tests (read the counter) | Only the engine sees the final per-block context after flag processing; the consumer is the party that made the mistake and must read the count in its own tests. |
| Documenting "which flag mode to choose" | Docs (`docs/integration-guide.md`) | CLAUDE.md summary | INTG-02 acceptance is a documentation statement as well as a detector. |
| Umbrella headers | Library public headers (`include/SpatialCore/`) | Consumer (harness) | Harness proves the umbrellas are self-sufficient from a consumer TU. |
| Consumer proof (INTG-01) | Harness project (own CMake project in `examples/harness/`) | CI script that builds a throwaway consumer repo | The thing being proved is the consumer's build, so it must not be a target of SpatialCore's own build tree. |
| Test discovery (`ctest` at build root) | Top-level `CMakeLists.txt` (`enable_testing()`) | `tests/CMakeLists.txt` (`catch_discover_tests`) | CTest looks for a test file at the top-level build directory. |
| Coverage instrumentation | CMake option (top-level builds only) | CI job + aggregator script | Instrumentation is a compile/link property; reporting is a post-run tool step. |
| Coverage gate and report | CI (macOS job) | `$GITHUB_STEP_SUMMARY` + artifact | D-14/D-15: fail below target, report inside GitHub. |
| Per-OS build/test gating | CI runners (GitHub-hosted) | Branch protection (repo settings) | D-08, D-11. |
| HRTF data in CI | Git LFS (checkout) + `actions/cache` | CMake configure-time HDF5 signature guard | Quota-limited resource; the configure guard already converts a missed `git lfs pull` into a loud failure. |
| Nightly sanitizer pass | Scheduled workflow on `main` | Manual `workflow_dispatch` | D-10; scheduled workflows run only from the default branch. |

## Standard Stack

### Core
| Tool | Version | Purpose | Why Standard |
|------|---------|---------|--------------|
| GitHub-hosted `macos-26` runner (arm64) | image Xcode 26.6, CMake 4.4.3, Ninja 1.13.2, Git 2.55.0, Git LFS 3.8.0, Python 3.14.7, jq 1.8.2 | The CI gate and the coverage/harness jobs | `macos-latest` already maps to this image; local Mac is macOS 26 / AppleClang 21 / CMake 4.4.4, so CI mirrors local. Standard runners are free and unlimited on public repos. [VERIFIED: runner-images macOS 26 arm64 readme; CITED: docs.github.com github-hosted-runners] |
| `actions/checkout` | v7.0.1 (2026-07-20) | Checkout (with `lfs: false` plus cache, see Pattern 4) | Current release. [VERIFIED: `gh api repos/actions/checkout/releases/latest`] |
| `actions/cache` | v6.1.0 (2026-06-26) | Cache `.git/lfs` (and optionally ccache) | Current release. [VERIFIED: gh api] |
| `actions/upload-artifact` | v7.0.2 (2026-10-07) | Coverage report artifact | Current release. [VERIFIED: gh api] |
| Apple clang + `llvm-profdata` / `llvm-cov` via `xcrun` | AppleClang 21.0.0 (local, `xcrun --find` resolves both) | Source-based coverage | Same compiler as the gate; no extra install; per-line and per-branch data. [VERIFIED: ran it, see Coverage section] |
| CMake | 4.4.x on macOS runner/local | Build | Already the project's build system; `ctest --no-tests=error` exists in this version. [VERIFIED: `ctest --help`] |
| Catch2 | v3.7.1 (already pinned) | Tests | Existing. [VERIFIED: tests/CMakeLists.txt:1-7] |
| JUCE | 9.0.0 (already pinned) | Framework; harness must fetch the same tag | CI-01 is "verified against JUCE 9.0.0". [VERIFIED: CMakeLists.txt:29-34] |

### Supporting
| Tool | Version | Purpose | When to Use |
|------|---------|---------|-------------|
| `python3 -I` (stdlib `json` only) | 3.14.7 on runner, 3.9 CLT locally | Aggregate `llvm-cov export` JSON into per-area percentages | Coverage report step. No pip installs. |
| `gh api` | 2.102.0 local | Apply branch protection after user confirmation (D-11) | One-time, user-confirmed. |
| ccache | not preinstalled on macOS image | Optional compile cache | Only if first measured CI run is too slow; do not add speculatively. [ASSUMED] |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| Clang source-based coverage | gcov/lcov | AppleClang does not drive gcov well, lcov would be an extra install; source-based gives branch and region data and was proven here. |
| Throwaway consumer repo built by script | Committed `.gitmodules` entry in this repo pointing at itself | Self-referential: `git clone --recursive` of SpatialCore would clone SpatialCore into itself, each clone pulling 58 MB of LFS, with a chain only bounded by the pin ages. Reject. |
| Console-app sound check beside a plugin | Plugin only, run through a host | No headless host in CI; a console app that instantiates the same `AudioProcessor` was proven to link and render. |
| Per-format `windows-latest` full test run | Build only | D-08 defers it. |

**Installation:** none (no language-package installs in this phase). Tooling comes from the runner image and the Xcode toolchain.

**Version verification:** action versions were read from `gh api repos/actions/<name>/releases/latest` on 2026-10-17 (above). Runner image contents from the official runner-images readme (above). The `macos-14` label is deprecated and "fully unsupported by November 2nd" per the runner-images release notes; do not use it. `macos-latest` currently resolves to the macOS 26 arm64 image; pin `macos-26` explicitly so a future label move cannot change the gate silently. [CITED: github.com/actions/runner-images README; github.blog changelog 2026-05-14]

## Package Legitimacy Audit

No external language packages (npm, PyPI, crates) are installed by this phase. The only third-party actions are first-party `actions/*` (checkout, cache, upload-artifact), whose latest releases were confirmed through `gh api` on the official repositories. FetchContent dependencies (JUCE 9.0.0, Catch2 v3.7.1, libmysofa v1.3.5 for SpatialCore / v1.3.2 for a consumer that supplies its own) are existing pins, not new.

| Package | Registry | Age | Downloads | Source Repo | Verdict | Disposition |
|---------|----------|-----|-----------|-------------|---------|-------------|
| (none) | — | — | — | — | n/a | n/a |

**Packages removed due to [SLOP] verdict:** none
**Packages flagged as suspicious [SUS]:** none

## Architecture Patterns

### System Architecture Diagram

```
 push / pull_request                          schedule (nightly) / workflow_dispatch
        |                                                    |
        v                                                    v
 .github/workflows/ci.yml                        .github/workflows/nightly.yml  (must live on main)
        |                                                    |
   +----+-----------------+-----------------+                |
   |                      |                 |                v
   v                      v                 v        macos-26: Release [rt] trap run
 macos-26 (GATE)       ubuntu-24.04      windows-2022      + build-tsan tree (RelWithDebInfo)
   |                   (tests run)       (build only)      + [tsan] subset + UI suite + full suite
   |  checkout (lfs:false)                                  (README "Realtime-safety check" 1-5)
   |  restore .git/lfs cache --> git lfs pull (no download on hit)
   |  configure/build Debug  --> ./SpatialCoreTests ; ./SpatialCoreUITests
   |  configure/build Release --> ctest --no-tests=error ; ctest -N | grep 'ui:'
   |  scripts/build-consumer.sh <SHA> ----------------------+
   |        |                                               |
   |        v                                               v
   |   mktemp dir: git init; copy examples/harness template; git submodule add <GitHub remote>;
   |   git -C SpatialCore fetch origin <SHA>; checkout; git lfs pull;
   |   cmake -B build -DCMAKE_BUILD_TYPE=Release (harness fetches JUCE 9.0.0 itself, as OSD does)
   |   build HarnessPlugin_VST3 + HarnessCheck ; run HarnessCheck (finite + non-silent, 23 formats)
   |
   +-- separate parallel job "coverage" (macos-26): -DSPATIALCORE_ENABLE_COVERAGE=ON, Debug
         LLVM_PROFILE_FILE=prof/core-%p.profraw ./SpatialCoreTests ; prof/ui-%p.profraw ./SpatialCoreUITests
         llvm-profdata merge -> llvm-cov export -summary-only -> python3 -I aggregate per area
         -> $GITHUB_STEP_SUMMARY + upload-artifact ; exit 1 if engine% < target (after OQ-4 decision)

 Engine side of INTG-02 (inside every job above that renders):
   renderBlock(sources, ctx) -> finalise ctx (derive dispatch, compute gains if flagged)
        -> if !engineComputesGains and the chosen path reads gain arrays and an active object
           has all-zero gains  ->  unsuppliedGainsBlockCount_.fetch_add(1, relaxed) [+ jassertfalse in Debug]
        -> dispatch to the five render paths (unchanged)
```

### Recommended Project Structure

```
include/SpatialCore/
├── SpatialCore.h            # DSP umbrella: add OSC/OSCPortValidation.h, OSC/ADMPositionQuery.h; fix stale header comment (W2, W3)
└── SpatialCoreUI.h          # NEW (D-02): includes the 10 public UI headers
examples/
├── CMakeLists.txt           # unchanged (demo only); the harness is NOT add_subdirectory'd from here
└── harness/                 # NEW template, its own CMake project; expects ./SpatialCore next to it
    ├── CMakeLists.txt       # FetchContent JUCE 9.0.0 (+ optional libmysofa v1.3.2 like OSD), add_subdirectory(SpatialCore)
    ├── src/HarnessProcessor.{h,cpp}   # exercises all seven features (D-03)
    ├── src/SoundCheck.cpp             # console main: 23 formats, finite + non-silent, counter == 0
    └── scripts/build-consumer.sh      # materialises the throwaway consumer repo (Pattern 1)
scripts/
└── coverage-report.py       # NEW: llvm-cov export JSON -> per-area table (stdlib only)
.github/workflows/
├── ci.yml                   # macOS gate + Linux tests + Windows build + coverage job
└── nightly.yml              # TSan + [rt]
tests/
├── Engine/UnsuppliedGainsTests.cpp   # NEW [intg02]
├── Core/UmbrellaTests.cpp            # NEW (source-scan: every public header is in an umbrella or on the allow-list)
└── Binaural/…                        # NEW [osd50], [osd89] cases (in existing files)
```

### Pattern 1: Throwaway consumer repo (answers D-01 without a self-referential submodule)

**What:** `examples/harness/` holds only the consumer's own files. `scripts/build-consumer.sh <sha>` creates a temp directory, `git init`s it, copies the template in, runs `git submodule add https://github.com/AndrewRahman/SpatialCore.git SpatialCore`, fetches and checks out the commit under test inside the submodule, pulls LFS there, then configures and builds in Release and runs the sound check. This *is* the submodule step OSD performs, against the remote, never the local folder.
**Why it is needed:** a `.gitmodules` entry inside SpatialCore that points at SpatialCore makes `git clone --recurse-submodules` recurse; the pin must also be a commit that already exists on the remote and that cannot contain the commit being tested.
**Verified building blocks:**
- `git fetch --depth 1 origin <sha>` against the remote works for a full 40-character SHA (ran it for `7f17bb7dffd1bed81fa6104b458702cf76bfaffd`, printed `FETCH_HEAD`). `actions/checkout` itself fetches by SHA, so `GITHUB_SHA` (including a PR merge commit) is fetchable. [VERIFIED: ran it; PR-merge-SHA case ASSUMED, A4]
- A consumer that `add_subdirectory(SpatialCore)`s it, links `SpatialCore SpatialCoreUI juce::juce_audio_utils`, and includes the umbrella plus individual UI headers builds clean in Release as a `juce_add_plugin(... FORMATS VST3)` and as a `juce_add_console_app` sharing the same processor source, and renders non-silent output (RMS 0.111 on a constant-0.25 source). [VERIFIED: built and ran in `/tmp/sc-harness-exp`, 58 s wall on a 14-core Mac]
- The OSD-like variant also builds: consumer fetches `libmysofa` v1.3.2 itself (SpatialCore then skips its own v1.3.5 fetch because `TARGET mysofa-static` exists, `CMakeLists.txt:48`), applies `-ffast-math -Wno-nan-infinity-disabled` to its own plugin target, sets `SPATIALCORE_BUILD_TESTS=OFF`. [VERIFIED: built and ran, 60 s]. **It needs `-DCMAKE_POLICY_VERSION_MINIMUM=3.5` on the command line**: libmysofa v1.3.2 fails to configure under CMake 4.x otherwise, because SpatialCore only sets that policy variable after the consumer's own FetchContent has already run (`CMakeLists.txt:3`). [VERIFIED: the failure was reproduced, then fixed by the flag]
**Example (shape verified, not the final script):**
```bash
# scripts/build-consumer.sh <sha> [remote-url]
set -euo pipefail
SHA="$1"; URL="${2:-https://github.com/AndrewRahman/SpatialCore.git}"
WORK="$(mktemp -d)"; cp -R "$(dirname "$0")/.." "$WORK/consumer"; cd "$WORK/consumer"
rm -rf scripts; git init -q; git lfs install --local
git submodule add "$URL" SpatialCore
git -C SpatialCore fetch --depth 1 origin "$SHA"
git -C SpatialCore checkout --detach FETCH_HEAD
git -C SpatialCore lfs pull                       # configure refuses to proceed on LFS pointers (CMakeLists.txt:92-99)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_POLICY_VERSION_MINIMUM=3.5
cmake --build build --config Release --target HarnessPlugin_VST3 HarnessCheck -j4
./build/HarnessCheck_artefacts/Release/HarnessCheck
```
(`GIT_LFS_SKIP_SMUDGE=1` on the submodule clone and a restored LFS cache, Pattern 4, keep the quota cost at zero on a cache hit.)

### Pattern 2: Opt-in coverage instrumentation (top-level only)

**What:** `option(SPATIALCORE_ENABLE_COVERAGE ... OFF)`, honoured only when `PROJECT_IS_TOP_LEVEL` and the compiler is Clang/AppleClang; adds `-fprofile-instr-generate -fcoverage-mapping` compile flags and `-fprofile-instr-generate` link flags. A third tree `build-cov/` (add to `.gitignore`) like `build-tsan/`.
**Verified recipe** (this exact flag set was used on this Mac; it was applied through `CMAKE_C_FLAGS`/`CMAKE_CXX_FLAGS`/linker flags rather than the new option, so the option itself is untested):
```bash
cmake -S . -B build-cov -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_FLAGS="-fprofile-instr-generate -fcoverage-mapping" \
  -DCMAKE_CXX_FLAGS="-fprofile-instr-generate -fcoverage-mapping" \
  -DCMAKE_EXE_LINKER_FLAGS="-fprofile-instr-generate" -DCMAKE_SHARED_LINKER_FLAGS="-fprofile-instr-generate"
cmake --build build-cov -j10
LLVM_PROFILE_FILE=prof/core-%p.profraw ./build-cov/tests/SpatialCoreTests
LLVM_PROFILE_FILE=prof/ui-%p.profraw   ./build-cov/tests/SpatialCoreUITests
xcrun llvm-profdata merge -sparse prof/core-*.profraw -o core.profdata
xcrun llvm-cov export build-cov/tests/SpatialCoreTests -instr-profile=core.profdata -summary-only \
  -ignore-filename-regex='(_deps|/tests/|/usr/|Applications|Library/Developer|juce_binarydata)' > core-summary.json
```
Run the executables **directly**, not through `ctest`: `catch_discover_tests` starts one process per test case (403 processes, 403 profraw files; harmless but needless). The `%p` pattern keeps parallel/forked runs from overwriting each other.
**Interaction checked:** `tests/CMakeLists.txt:73` arms the RTGuard trap unless `CMAKE_CXX_FLAGS` matches `-fsanitize`; coverage flags do not match, so the trap library still builds in the coverage tree. Whether the `[rt]` cases still pass with instrumentation on was not measured (the full Debug run passed 387/387 with them armed, so they did). [VERIFIED: `All tests passed (329024 assertions in 387 test cases)`, exit 0]

### Pattern 3: INTG-02 detector (counter + Debug assert), placed after `ctx` is final

**Where the gain arrays are actually read** [VERIFIED: read `src/Engine/RenderEngine.cpp`]:
- `renderSimpleBinauralWoodworth` reads `blockCtx.objGains[t]` (`.cpp:1466,1510-1511,1523`).
- `renderStereoVariant` reads `blockCtx.objGainL/objGainR` (`.cpp:1546,1562,1575`).
- `renderDiscreteSurround` reads `blockCtx.objChannelGains[t][sp]` for `sp < layout.numSpeakers` (`.cpp:1739,1757,1780`).
- The direct-binaural HRTF path and the Ambisonics path read none of these arrays. A detector that fires on every flag-off block would therefore false-positive on any HRTF or Ambisonics consumer. **The check must be path-aware.**
- An object is audible only if `sources.objectLive[t]`, `sources.monoBuffers[t] != nullptr`, `sources.tapFadeGainPerSample[t] != nullptr` and the per-sample tap fade is `> 0` (`renderDiscreteSurround` skips `tapFade <= 0.0f`, `.cpp:1751-1753`). Checking the first or last tap-fade sample of the block is a sufficient, cheap proxy.
**Placement:** in `renderBlock()` right after `const RenderBlockContext& ctx = *dispatchCtx;` (`RenderEngine.cpp:837`), guarded by `! blockCtx.engineComputesGains`. Using the finalised `ctx` means `engineSelectsHRTF`-on-binaural (which fills `objGains` itself, `.cpp:820-826`) is correctly seen as supplied.
**Counter pattern to copy:** `std::atomic<int> oversizedBlockCount_ { 0 };` (`RenderEngine.h:865`), `fetch_add (1, std::memory_order_relaxed)` (`.cpp:756`), accessor `int getOversizedBlockCount() const noexcept` (`RenderEngine.h:436`, `.cpp:1912-1914`), `static_assert (std::atomic<int>::is_always_lock_free, ...)` already present. Debug assert is `jassertfalse;` after the count, as in `noteLegacyRouteCall()` (`.cpp:1866`).
**False-positive map for the plan's tests:**
- `tests/Support/RouteRenderRig.h` and the `[rt]` matrix (`RealtimeSafetyTests.cpp:909-945`, `contextFor`) already supply non-zero constants when the flag is off, so they will not trip the detector. `tests/Engine/ProfileSwitchTests.cpp:1546` deliberately passes zero `objGains` with `engineComputesGains = false` and `engineSelectsHRTF`, which the engine fills itself (WR-06); the detector must read the post-fill `ctx` so that case counts 0.
- `jassertfalse` only logs unless a debugger is attached (JUCE `jassertfalse` breaks only under a debugger); the repo already relies on this ("The deliberate misuse calls trip jassertfalse in Debug: JUCE logs one line each and ...", `ProfileSwitchTests.cpp:1664`; `RenderEngineTests.cpp:1691`). A new misuse test will therefore log, not abort.
- Name suggestion: `getUnsuppliedGainsBlockCount()` (counts blocks, not objects). Counting per block keeps the cost at one loop over at most `MAX_SOURCES` = 12 rows of at most `MAX_SPEAKERS` = 16 floats. [VERIFIED: `Types.h:8-9`]
**Tension to state in the plan:** SC-2 says "fails loudly"; D-06 says Release is count-only. Loud means Debug assert plus a counter the consumer's own tests can read; the integration guide must tell consumers to assert the counter is zero in their tests, and the harness sound check should do so.

### Pattern 4: LFS in CI without burning the bandwidth quota

**Facts:** GitHub Free gives 10 GiB LFS storage and 10 GiB bandwidth per month, Actions downloads count against the repo owner's bandwidth, and once the budget is exhausted LFS use is blocked until the 1st of the next month. [CITED: docs.github.com billing git-lfs; community thread] The five HRTF files total about 61 MB on disk (sadie 36.6 MB, bernschuetz 19.7 MB, the other three 1.2-1.8 MB each). [VERIFIED: `ls -la HRTF`] Planned jobs per push would be macOS gate, coverage, Linux, Windows and harness: five full LFS pulls, about 300 MB per push, so roughly 35 pushes exhaust the month (the plan tier of the user's account was not discoverable, Assumption A8).
**Recipe:** `actions/checkout` with `lfs: false`; `actions/cache` on `.git/lfs` keyed on `hashFiles('HRTF/*.sofa')` (with `lfs: false` those files are the pointer text, which contains the object hash, so the key is stable and changes only when an HRTF file changes); then `git lfs pull`, which downloads nothing on a hit. Keep the key identical across jobs. The harness's submodule has its own LFS store: cache `<consumer>/.git/modules/SpatialCore/lfs`. Caches written on `main` are readable by PR runs; a nightly run keeps it from being evicted. [Pattern ASSUMED, A5; the key behaviour of pointer files is standard git-lfs]
**Keep the existing guard:** the current `ci.yml` step "Verify LFS objects resolved to real bytes" and the configure-time HDF5 signature check (`CMakeLists.txt:92-99`, signature `894844460d0a1a0a`) already turn a missed `git lfs pull` into a loud failure. Port the first to bash that also runs on macOS (`stat -f%z` on macOS, `stat -c%s` on Linux; the current script is GNU-only).

### Pattern 5: Workflow layout

- `ci.yml`: triggers `push` to `main` and `pull_request`, plus `workflow_dispatch`; add `concurrency: group: ${{ github.workflow }}-${{ github.ref }}, cancel-in-progress: true`; `permissions: contents: read`. Jobs with **unique names across all workflows** (required checks are identified by job name): `macos-gate`, `linux-tests`, `windows-build`, `coverage`. [CITED: docs.github.com about-protected-branches: "job names are unique across all workflows"; "skipped" counts as passing]
- `nightly.yml`: `schedule` + `workflow_dispatch`; runs README "Realtime-safety check" steps 1-5. Scheduled workflows run from the latest commit on the default branch only, so the file must be on `main` before it ever fires, and in a public repo they are disabled after 60 days without repository activity. [CITED: docs.github.com events-that-trigger-workflows]
- Current `ci.yml` problems to fix (D-12): `branches: [main, spatialcore-v2-extraction]` (the work branch `gsd-remap` is not listed; `spatialcore-v2-extraction` exists on the remote at `5140c9a`); `actions/checkout@v4`; stale comments at lines 12, 62-64, 68-69; `ctest` at the build root (line 71). [VERIFIED: `ci.yml` read in full]

### Anti-Patterns to Avoid
- **Committing a `.gitmodules` that points at this repo** (Pattern 1).
- **`ctest` at the build root without `enable_testing()`**; and `ctest` without `--no-tests=error`. Local CMake 4.4.4 exits 1 on zero tests, but the Linux run on 2026-10-17 logged "No tests were found!!!" and the step was green. [VERIFIED: local exit codes 1 and 8 with the flag; CI log per CONTEXT.md]
- **Reading UI coverage from `SpatialCoreUITests` alone** (Pitfall 4).
- **A detector that does not know which path reads the arrays** (Pattern 3).
- **Applying `-ffast-math` globally in a consumer before `add_subdirectory(SpatialCore)`**: `src/Core/FloatSemanticsGuard.h` turns that into a compile `#error` by design; per-target fast-math on the consumer's own targets is fine and is what OSD does. [VERIFIED: `FloatSemanticsGuard.h:1-40` and OSD `CMakeLists.txt:150`]

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Coverage collection and math | A custom counter/instrumentation scheme | `-fprofile-instr-generate -fcoverage-mapping` + `llvm-profdata` + `llvm-cov export -summary-only` | Proven on this Mac, gives line/branch/region per file. |
| Per-test-process isolation for coverage | A merge script over 403 profraws | Run the two executables directly with `%p` in `LLVM_PROFILE_FILE` | Two profraw files, same result. |
| Cache of the HRTF data | A custom download or artifact store | `actions/cache` on `.git/lfs` | Keeps bandwidth at zero on a hit without leaving git semantics. |
| Misuse counters | A new reporting channel (logging, callbacks) | The existing relaxed `std::atomic<int>` counter pattern + Debug `jassertfalse` | Realtime-safe, matches Phase 5 D-04/D-09. |
| Source-scan rules ("every public header is in an umbrella") | A shell script in CI | A Catch2 case reading `SPATIALCORE_SOURCE_DIR` (both test executables define it) | Same pattern as the Phase 5 `[rt][rule]` and UI `FontProvenanceTests` rules; runs in the local gate too. [VERIFIED: `tests/CMakeLists.txt:65,101`] |
| Branch protection | Hand-clicking in the web UI | One reviewed `gh api -X PUT repos/AndrewRahman/SpatialCore/branches/main/protection` call, run after the user confirms | Auditable and reversible. |
| Test-count guard | A grep on `ctest` output | `ctest --no-tests=error` plus the README listing check `ctest -N \| grep 'ui:'` | Prevents the "green with zero tests" regression. |

**Key insight:** every piece of "new infrastructure" in this phase already has a working precedent in the repo (counter pattern, source-scan rule, demo `--selftest`, README gate commands). The risk is in glue and in quotas, not in missing tools.

## Common Pitfalls

### Pitfall 1: Self-referential submodule
**What goes wrong:** harness committed with a submodule of its own repo; `git clone --recursive` recurses, and the pin cannot contain the commit under test.
**How to avoid:** Pattern 1.
**Warning signs:** a `.gitmodules` file in this repo; the harness job passing while testing an older SHA than the PR.

### Pitfall 2: Consumed-mode builds compile and register all of SpatialCore's tests
**What goes wrong:** `option(SPATIALCORE_BUILD_TESTS ... ON)` (`CMakeLists.txt:361`) applies in consumed mode. Configuring the prototype consumer produced the targets `Catch2`, `Catch2WithMain`, `SpatialCoreRTGuard`, `SpatialCoreTests`, `SpatialCoreUITests` in the consumer's build, and `tests/CMakeLists.txt:105` runs `include(CTest)`, which puts 403 tests into the consumer's `ctest`. A consumer building `all` pays for them. [VERIFIED: `cmake --build . --target help` in the prototype build]
**How to avoid:** default the option to `${PROJECT_IS_TOP_LEVEL}`; CMake's own documentation gives this exact pattern (`Example_ENABLE_TESTING` defaulting to `PROJECT_IS_TOP_LEVEL`). [CITED: cmake.org enable_testing] The harness must also pass `-DSPATIALCORE_BUILD_TESTS=OFF` defensively. Flipping the default is a behaviour change for existing consumers who relied on it (Assumption A7): put it in front of the user as a one-line decision.
**Related, LOW:** `CMakeLists.txt:3` force-sets `CMAKE_POLICY_VERSION_MINIMUM` in the consumer's cache, and `CMakeLists.txt:9-12` sets `CMAKE_OSX_ARCHITECTURES "arm64"` in SpatialCore's own scope, so a universal-binary consumer would hit a link mismatch; OSD sets arm64 itself (`openspatialdelay/CMakeLists.txt:9`), so it is not affected. [VERIFIED: both files read]

### Pitfall 3: Root has no `CTestTestfile.cmake`
**What goes wrong:** the observed CI defect. Verified: `ls CTestTestfile.cmake` in the build root fails, `ctest -N` prints `Total Tests: 0`, `ctest --test-dir tests -N` prints `Total Tests: 403` of which 16 start with `ui:`.
**How to avoid:** top-level `if(PROJECT_IS_TOP_LEVEL AND SPATIALCORE_BUILD_TESTS) enable_testing() endif()` before `add_subdirectory(tests)`; CI uses `ctest --test-dir build-release --no-tests=error` and the `ui:` listing check. Keep README's `ctest --test-dir build-release/tests` working (both then work).

### Pitfall 4: Unlinked UI sources are invisible to llvm-cov
**What goes wrong:** `SpatialCoreUITests` links `SpatialCoreUI` as a static archive, so a source file with no referenced symbol is never pulled in and has no coverage mapping. The report then lists only `PresetBrowser.cpp`, `SMLLookAndFeel.cpp`, `SpatialMapComponent.cpp` and the UI total reads 41.4%. [VERIFIED: measured]
**How to avoid:** in the coverage tree, also link the ten UI objects directly. `-Wl,-force_load` on the archive fails with 11,987 duplicate symbols because the top-level archive also contains compiled JUCE module objects (`CMakeLists.txt:327-337`). Extracting just the ten `src/UI` objects (`ar -x`) and appending them to the link worked: all ten files appear, UI total **25.9% (419/1618 lines)**. A CMake-native way: a coverage-only `OBJECT` library of `src/UI/*.cpp` added to `SpatialCoreUITests` via `$<TARGET_OBJECTS:...>`. [VERIFIED: manual relink with extracted objects; CMake form is design]
**Warning signs:** a UI report with fewer than ten `src/UI` entries.

### Pitfall 5: Quota exhaustion makes CI red for the wrong reason
See Pattern 4. With five checkouts per push, an uncached pipeline hits the 10 GiB free budget in a few dozen pushes; the failure presents as an LFS smudge error. [CITED: docs.github.com billing]

### Pitfall 6: Required checks vs the user's push flow
Branch `main` is currently unprotected (`gh api .../branches/main/protection` returns 404 "Branch not protected"). The user's recorded flow pushes straight to `main` (`git push origin HEAD:gsd-remap HEAD:main`) and a second project also pushes there. Required status checks must pass **before a direct push** as well, and by default admins are not subject to them; only "Do not allow bypassing" applies them to the owner. [CITED: docs.github.com about-protected-branches; VERIFIED: gh api 404; user memory] The D-11 confirmation must state which of the two the user wants: owner can still push directly (protection only gates other contributors and PR merges) or the owner is gated too (every change goes through a PR). The CI `push` trigger lists `main` and `spatialcore-v2-extraction` but not `gsd-remap`; pushes to `gsd-remap` run nothing today.

### Pitfall 7: Linux and Windows have never run the tests or the full build
The Linux job has built but never executed a single test; the Windows build has never run. Golden fingerprints and timing-sensitive cases were all established on arm64 macOS. First runs may fail for platform reasons (float contraction, libm, `FreeUdpPort.h`, UDP loopback). The code is Windows-aware (`FreeUdpPort.h:16`, `HRTFEmbeddedTests.cpp:16,561-563,789` and `HRTFProfileResolver.cpp:16` use `JUCE_WINDOWS`/`_WIN32`), but that is not a build result. [ASSUMED outcome, A1/A6]. Plan for it: bring the Linux job up with `continue-on-error: true`, triage the failures into fix-or-document, and only then make it a required check; the Windows job likewise.

### Pitfall 8: Nightly scheduled workflow silently never runs
Only fires from `main`; the file must be merged first; auto-disabled after 60 days of no activity; delays at the top of the hour (choose an off-minute cron). [CITED: docs.github.com]

### Pitfall 9: Time budget on the 3-core runner
Measured on this 14-core Mac: Debug instrumented build 75 s wall; **Debug `SpatialCoreTests` 516 s** (single process, instrumented) and `SpatialCoreUITests` 15 s; Release build 70 s wall (348 s CPU); **Release `ctest` serial 126 s for 403 tests**; harness plugin plus check Release build 58 s. The GitHub macOS arm64 standard runner has 3 cores and 7 GB RAM. [VERIFIED: local timings; CITED: github-hosted-runners table] Expect several times longer wall time. A user memory note records a Debug suite exceeding 20 minutes when the machine is loaded, so set explicit `timeout-minutes` per step and measure the first CI run before choosing whether Debug and coverage should be one job. The README's TSan timings (subset 992 s, UI 45 s, whole suite 1938 s on an Apple Silicon Mac) imply the nightly job needs an explicit large `timeout-minutes` (it is capped at 6 h by GitHub for hosted jobs [ASSUMED]).

### Pitfall 10: Debug leak noise
A Debug run prints `JUCE Assertion failure in juce_MessageListener.cpp:50` lines and the known `SharedFFTCache` leaked-object assertion at exit while still exiting 0 (observed again this session; documented in the Phase 5 05-06 summary). CI must key on the exit code, not on the absence of "Assertion" text.

### Pitfall 11: Test-file edit hazards
`tests/CMakeLists.txt` and `README.md` are CRLF. The harness template and workflows are new files (LF). An edit tool that normalises line endings will produce whole-file diffs.

## Code Examples

### Consumer harness CMake (shape verified in `/tmp/sc-harness-exp`)
```cmake
cmake_minimum_required(VERSION 3.22)
project(SpatialCoreHarness VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
include(FetchContent)
FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 9.0.0 GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)
# optional, mirrors OSD: FetchContent_Declare(libmysofa ... GIT_TAG v1.3.2 ...) before add_subdirectory
set(SPATIALCORE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(SpatialCore)      # a git submodule here; never a path into the source tree

juce_add_plugin(HarnessPlugin COMPANY_NAME "SML" PLUGIN_MANUFACTURER_CODE Smlb PLUGIN_CODE Hrns
    FORMATS VST3 PRODUCT_NAME "Harness" COPY_PLUGIN_AFTER_BUILD FALSE)
target_sources(HarnessPlugin PRIVATE src/HarnessProcessor.cpp)
target_compile_definitions(HarnessPlugin PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(HarnessPlugin PRIVATE SpatialCore SpatialCoreUI juce::juce_audio_utils
                                              juce::juce_recommended_config_flags)

juce_add_console_app(HarnessCheck PRODUCT_NAME HarnessCheck)
target_sources(HarnessCheck PRIVATE src/SoundCheck.cpp src/HarnessProcessor.cpp)
target_compile_definitions(HarnessCheck PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_STANDALONE_APPLICATION=1)
target_link_libraries(HarnessCheck PRIVATE SpatialCore SpatialCoreUI juce::juce_audio_utils)
```
Notes: the two targets share one processor source, so JUCE module sources compile once per concrete target, which is the situation the comments at `CMakeLists.txt:151-174` describe and which built clean.

### Harness processor calls that the engine accepts (every name read from the headers this session)
```cpp
#include <SpatialCore/SpatialCore.h>
#include <SpatialCore/SpatialCoreUI.h>            // new umbrella, D-02

engine.prepare (sampleRate, maxBlock);
engine.setOutputFormat (spatialcore::OutputFormat::Binaural);
engine.setHRTFProfile (5);                          // 5 = mit_kemar_large_pinna, "always embedded" (HRTFProfile.h)
engine.setAlgorithmIndex (spatialcore::kAlgorithmIndexVBAP);

spatialcore::RenderBlockContext c;
c.sampleRate = sampleRate;
c.engineComputesGains = c.engineDerivesDispatch = c.engineSelectsHRTF = c.engineFadesFormatSwitch = true;
engine.renderBlock (sources, c, outPtrs, numOutCh);
const auto& layout = engine.getBlockLayout();       // after renderBlock, audio thread
// sound check, message thread: engine.waitForHRTFProfileIdle (10000);
// auto s = engine.getHRTFProfileStatus();  // s.state == HRTFLoadState::Ready, s.source == HRTFProfileSource::Embedded
```
Verbatim values this relies on: `inline constexpr int kAlwaysEmbeddedHRTFProfile = 5;`, `enum class HRTFLoadState : uint8_t { Idle, Loading, Ready, Failed };`, `enum class HRTFProfileSource : uint8_t { None, Simple, SharedFolder, Embedded };` [VERIFIED: `HRTFProfile.h:30,57,59`]; `static constexpr int NUM_OUTPUT_FORMATS = 23;` [VERIFIED: `OutputFormatRegistry.h:9`]; `inline constexpr int kAlgorithmIndexVBAP = 5;` [VERIFIED: `RenderEngine.h:253`]; `bool engineComputesGains = false;` (`:189`), `bool engineDerivesDispatch = false;` (`:200`), `bool engineFadesFormatSwitch = false;` (`:218`), `bool engineSelectsHRTF = false;` (`:238`) [VERIFIED: `RenderEngine.h`]. Determinism: call `setSharedHRTFFolderForTesting` with an empty temp directory so a machine-wide override in `/Library/Application Support/Spatial Media Lab/HRTF/` cannot change the reported source (the method exists "for tests", `RenderEngine.h` HRTF section). Output buffer width per format is `OutputFormatRegistry::getInfo (fmt).requiredChannels` (it exceeds `MAX_SPEAKERS` = 16 for the high-order Ambisonics formats); size from that, as the `[rt]` matrix does with `channelsFor`.

### Per-area aggregation (stdlib only; the version run here keyed on a hard-coded checkout name, so use the repo root in CI)
```python
# python3 -I scripts/coverage-report.py core-summary.json REPO_ROOT
import json, sys, collections
data = json.load(open(sys.argv[1]))['data'][0]['files']
root = sys.argv[2].rstrip('/') + '/'
agg = collections.defaultdict(lambda: [0, 0])
for f in data:
    rel = f['filename'][len(root):] if f['filename'].startswith(root) else f['filename']
    p = rel.split('/')
    if p[0] == 'src':           key = 'src/' + p[1]
    elif p[0] == 'include':     key = 'include/' + p[2]
    else:                       continue
    l = f['summary']['lines']; agg[key][0] += l['covered']; agg[key][1] += l['count']
```
Map `src/<Area>` and `include/<Area>` to the CONTEXT D-13 areas (algorithms, binaural, engine, IO, OSC, trajectory, plus core) and report UI from the UI profile restricted to `src/UI` + `include/SpatialCore/UI`.

### CI step: counting guard and the listing check
```bash
ctest --test-dir build-release --no-tests=error --output-on-failure
ctest --test-dir build-release -N | grep -q 'ui:'     # README "Local test gate" listing check
```

## Measured Coverage Baseline (TEST-01, OQ-4 input)

Run: Debug, AppleClang 21.0.0, HEAD `206f766`, `-fprofile-instr-generate -fcoverage-mapping`, `SpatialCoreTests` as one process (387 cases, 329,024 assertions, **all passed**, exit 0, 516 s) and `SpatialCoreUITests` (all passed, 15 s). JUCE, Catch2, libmysofa, tests and the generated HRTF BinaryData excluded. [VERIFIED: local run, 2026-10-17]

| Area (src + public headers) | Lines covered / total | Line % |
|---|---|---|
| src/Algorithms | 306 / 306 | 100.0 |
| src/Binaural | 765 / 882 | 86.7 |
| src/Core | 271 / 289 | 93.8 |
| src/Engine | 1,216 / 1,259 | 96.6 |
| src/IO | 346 / 351 | 98.6 |
| src/OSC | 278 / 281 | 98.9 |
| src/Trajectory | 447 / 492 | 90.9 |
| include/ headers (all areas) | 163 / 177 | 92.1 |
| **Engine-side total (src + include)** | **3,792 / 4,037** | **93.9** (branches about 85.6) |
| **UI (`SpatialCoreUITests`, all ten sources linked)** | **419 / 1,618** | **25.9** (41.4 if unlinked files are ignored) |

Weakest files: `HRTFDatabase.cpp` 68.0% lines (error and malformed-input branches), `DopplerVelocity.cpp` 81.9%, `TrajectoryEngine.cpp` 93.0%, `PartitionedConvolver.cpp` 96.1%, `BinauralRenderer.cpp` 100%. Zero-covered UI files: `GlobalTapDrawer`, `IOSectionComponent`, `IndicatorToggle`, `OSCSectionComponent`, `ObjectPanel`, `ReverseSlider`, `StyledButton`; `SMLLookAndFeel.cpp` 5.5%.
**TEST-01 "dedicated tests" clause:** `tests/Binaural/PartitionedConvolverTests.cpp` (5 cases, 418 lines) and `tests/Binaural/BinauralRendererTests.cpp` (9 cases, 612 lines) exist and are in `tests/CMakeLists.txt:29-30`. Measured 96.1% and 100% line coverage. The clause is met; the plan only needs to record the evidence. [VERIFIED: files read, test run]
**Plain-terms framing for the checkpoint (user is not a programmer):** "X% of the lines of code that make the sound are executed at least once by the automated tests." Suggest the user choose between (a) a floor just under today's engine number so nothing can quietly get worse, and (b) a higher aspirational number with named gap areas. UI should be reported but gated separately (or not gated), since its starting point is 25.9%.

## Regression-test status for OSD#50 / #89 / #96 / #131 / #234 (D-17)

| Issue | Behaviour fixed | Test today | Recorded red evidence | Needed |
|---|---|---|---|---|
| #50 dual-convolver HRTF transition | `setIR` mid-stream crossfades through a second slot instead of hard-swapping (`PartitionedConvolver.cpp:50,90,209`) | **None cites it.** `[convolver][transition]` pins the cos/sin fade and would go red under a hard swap, but does not name #50 | Phase 3: that test was run against the pre-change convolver (03-03-SUMMARY line 142), but that was about BUG-02 timing, not #50 | New `[osd50]` case: steady sine through the convolver, `setIR` to a different IR mid-stream, assert the largest sample step at the switch stays near the steady-state step; mutation = make `setIR` load into the active slot (old single-slot behaviour) and show red. Helpers `sineWave` and `maxAbsStep` exist (`BinauralMetrics.h:377,391`). |
| #89 onset-detection ITD alignment + great-circle pole threshold | `HRTFDatabase::getAlignedHRIR` derives delays from the waveform when SOFA delay is 0 (`HRTFDatabase.cpp:318-335`); `updateSourceHRIR` uses angular distance, not independent az/el (`BinauralRenderer.cpp:185-196`) | **Not directly.** The cited `SadieD2KU100Tests.cpp:67` is a comment about SADIE bypassing the fallback. `MitKemarLargePinnaTests.cpp:96-111` asserts raw delays are 0 (the precondition, not the fix). The D-16 ITD characterisation (`BinauralRendererTests.cpp:259-390`) pins KEMAR rendered onsets, so it likely goes red if the fallback is removed, but it is not labelled #89 and nothing covers the pole threshold | None for #89 specifically | New `[osd89]` cases: (a) KEMAR `getAlignedHRIR` returns non-zero delays and the two aligned ears' onsets agree after the min-onset shift; (b) a position near the pole that moves 1 degree in azimuth does not reload the HRIR. (b) needs an observable (a reload counter or a convolver IR-set observable) the planner must choose; mutation = delete the zero-delay branch, and revert the threshold to independent az/el. |
| #96 per-renderer `HRTFDatabase` | Use-after-free race on a shared database | `Engine/MultiInstanceTests.cpp:209` "bit-identically ... (OSD#96)" | **Yes**, 05-06-SUMMARY "D-07 Mutation Results" #1: red by crash in 3 of 3 runs | Cite the evidence; re-run only if the user wants fresh proof. |
| #131 shared typeface + FFT cache | 8 instances x 7 typefaces CoreGraphics crash; `SharedResourcePointer` + `SharedFFTCache` | `UI/MultiMapTests.cpp:62`; `Binaural/SharedFFTCacheTests.cpp` | **Yes**, 05-07-SUMMARY (delta 16, 15 of 16 identities differ) and 05-06-SUMMARY mutations #2, #3 | Cite. |
| #234 small-block artifacts | Warm-up and crossfade timed in samples | `PartitionedConvolverTests.cpp:138` `[bug02][moving]`, `[bug02][steady]`, `[convolver][transition]` | **Yes**, 03-03-SUMMARY line 142: 32-sample and 558-sample sections failed on the pre-change convolver | Cite. |

D-17 literally says "confirm each of the four existing ones". The efficient reading is to cite the existing recorded evidence for #96, #131, #234 and run fresh mutations only for the new/strengthened #50 and #89 tests; surface that reading to the user at plan approval rather than re-running three recorded mutations. `OpenSpatialDelay#234` is still open there by design; the SpatialCore test covers only the convolver/engine part. [VERIFIED: issue text read via `gh`]

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| `macos-14` runner | `macos-26` (arm64); `macos-latest` already points at it | label moved from 2026-06-15; macOS 14 unsupported from 2026-11-02 | Pin `macos-26`; do not copy any `macos-14` examples. [CITED: github.blog changelog 2026-05-14, runner-images notes] |
| `actions/checkout@v4`, `upload-artifact@v4` | v7.0.1 / v7.0.2 | 2026 | Update pins in `ci.yml`. [VERIFIED: gh api] |
| Root `ctest` silently passing with no tests (older CMake) | `ctest --no-tests=error` | CMake 3.16+ | Always pass the flag. [VERIFIED: `ctest --help`] |
| `cmake_minimum_required < 3.5` third-party projects (libmysofa v1.3.2) | CMake 4.x refuses them without `CMAKE_POLICY_VERSION_MINIMUM` | CMake 4.0 | A consumer supplying its own libmysofa v1.3.2 must pass `-DCMAKE_POLICY_VERSION_MINIMUM=3.5`. [VERIFIED: reproduced] |

**Deprecated/outdated:**
- Branch filter `spatialcore-v2-extraction` in `ci.yml` and the Phase 8/9 plan-number comments: stale (D-12).
- "CI runs zero tests" statements (list in Project Constraints).

## Assumptions Log

| # | Claim | Section | Risk if Wrong |
|---|-------|---------|---------------|
| A1 | The full `SpatialCoreTests` suite passes on ubuntu x86_64 without test changes | Pitfall 7 | Linux job red on first run; plan needs a triage/fix task and a `continue-on-error` bring-up period |
| A2 | CI wall times: macOS gate 25-40 min per push; coverage job similar | Pitfall 9 | Merge-gating latency; may need to split or cache |
| A3 | ThreadSanitizer and the RTGuard trap work on the GitHub `macos-26` arm64 runner, whole nightly under ~2 h | Pattern 5, Pitfall 9 | Nightly job needs a smaller scope (drop the full-suite step to `workflow_dispatch`) |
| A4 | A fork PR's merge commit can be fetched by SHA from the base repository inside the throwaway consumer | Pattern 1 | Harness job fails for fork PRs only; fall back to `refs/pull/N/merge` |
| A5 | The `.git/lfs` cache keyed on `hashFiles('HRTF/*.sofa')` (pointer text) makes `git lfs pull` download nothing on a hit, including in the harness's submodule git dir | Pattern 4 | Quota exhaustion risk stays; verify by reading the first two runs' LFS download size |
| A6 | The whole tree (including tests) compiles under MSVC | Pitfall 7 | Windows job red; restrict to `SpatialCore SpatialCoreUI` targets and file issues |
| A7 | Changing `SPATIALCORE_BUILD_TESTS`'s consumed-mode default to off does not break OSD or OpenSpatialPanner | Pitfall 2 | A consumer relied on building SpatialCore's tests; check both consumers' CMake first |
| A8 | The user's GitHub plan has the 10 GiB Free-tier LFS quota (plan not discoverable via API) | Pattern 4 | Quota smaller or larger than assumed |
| A9 | `xcrun llvm-cov` / `llvm-profdata` ship in the Xcode 26.6 toolchain on the runner (they do in the local Command Line Tools) | Standard Stack | Coverage job needs `brew install llvm` or a path change |
| A10 | Hosted jobs can run up to 6 h | Pitfall 9 | Nightly scope must shrink |
| A11 | ccache would help and is not needed on day one | Standard Stack | Slow CI; add after the first measured run |

## Open Questions

1. **Coverage target (OQ-4).**
   - What we know: engine-side 93.9% lines / about 85.6% branches; UI 25.9%; area table above.
   - What's unclear: the user's choice (floor vs aspiration; gate UI or not; line vs branch).
   - Recommendation: a `checkpoint:decision` after the first green coverage run, in plain terms, as D-13 specifies; the CI threshold flag stays unset until then.
2. **Branch-protection mode (D-11).** Owner-can-still-push vs owner-gated (Pitfall 6). Needs the user at apply time; do not apply it from a plan task without that confirmation.
3. **Default for `SPATIALCORE_BUILD_TESTS` in consumed mode (A7).** Recommend flipping to `PROJECT_IS_TOP_LEVEL`; check OSD and OSP CMake first.
4. **Observable for the #89 pole-threshold test.** `cachedSourceAz/El` are private. Choose between a test-only counter on `BinauralRenderer`, or an observable on the convolver (IR-set count), or testing through render output with a deliberately large IR difference. Needs a short design step in the plan.
5. **Shared single macOS job vs separate coverage job.** Separate parallel job recommended (clean required-check names, no instrumented Debug as the gate). Revisit after the first timing.
6. **Which headers belong in the DSP umbrella.** Measured missing from `SpatialCore.h`: `Engine/TripleBufferIndex.h`, `OSC/ADMPositionQuery.h`, `OSC/OSCPortValidation.h`. Add the latter two (consumers implement `admPositionQueried` and call `oscPortsConflict`); `TripleBufferIndex.h` is pulled in by `RenderEngine.h` and can go on the allow-list. [VERIFIED: loop over `include/` this session]

## Environment Availability

| Dependency | Required By | Available | Version | Fallback |
|------------|------------|-----------|---------|----------|
| cmake | all builds | yes | 4.4.4 | — |
| AppleClang | build, coverage | yes | 21.0.0 | — |
| llvm-profdata, llvm-cov | coverage | yes (`xcrun --find` resolves both under `/Library/Developer/CommandLineTools/usr/bin`) | AppleClang 21 toolchain | — |
| git-lfs | HRTF data | yes | 3.8.0 | — |
| gh | issues, branch protection | yes | 2.102.0 | — |
| python3 | coverage aggregation | yes (CLT 3.9 locally; 3.14.7 on runner) | 3.9 | — |
| ninja | optional | no (`command -v` found cmake only) | — | Makefiles (the measured timings used them) |
| docker | Linux local repro | not checked | — | Linux is exercised on the GitHub runner only; the user never operates Linux/Windows (CONTEXT specifics) |
| Network to github.com | FetchContent, fetch-by-SHA | yes (fetch-by-SHA run succeeded) | — | `FETCHCONTENT_SOURCE_DIR_*` overrides (README documents this) |
| OSD checkout | consumer-pattern reference | yes at `~/conductor/repos/openspatialdelay` | read-only reference | — |

**Missing dependencies with no fallback:** none.
**Missing dependencies with fallback:** ninja (not needed).

## Validation Architecture

(`.planning/config.json` has no `workflow.nyquist_validation` key, so this section is required.)

### Test Framework
| Property | Value |
|----------|-------|
| Framework | Catch2 v3.7.1 (FetchContent); executables `SpatialCoreTests` (387 cases) and `SpatialCoreUITests` (16 cases, ctest names prefixed `ui:`) |
| Config file | `tests/CMakeLists.txt` (explicit source lists; CRLF endings; new `.cpp` files must be added to it) |
| Quick run command | `./build-release/tests/SpatialCoreTests "[intg02],[osd50],[osd89]"` (new tags, per-case) |
| Full suite command | README "Local test gate": Debug build + both executables; Release build + `ctest --test-dir build-release/tests --output-on-failure` (403 tests, 126 s serial measured) |

### Phase Requirements to Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| INTG-01 | Clean consumer submodules the SHA under test, links both targets, includes the two umbrellas, builds Release, sound check passes (finite, non-silent, counter 0, HRTF reports Embedded/Ready) | integration script | `examples/harness/scripts/build-consumer.sh "$(git rev-parse HEAD)"` | no, Wave 0 |
| INTG-01 | Both umbrellas include every public header (or allow-list) and each compiles stand-alone | unit (source scan + TU) | `./build-release/tests/SpatialCoreTests "[umbrella]"`; `./build-release/tests/SpatialCoreUITests "[umbrella]"` | no, Wave 0 |
| INTG-02 | Flag off + active objects + zero gains on a gain-reading path -> counter increments; flag on -> 0; HRTF/Ambisonics flag-off -> 0; `engineSelectsHRTF` on binaural -> 0 | unit | `./build-release/tests/SpatialCoreTests "[intg02]"` | no, Wave 0 |
| INTG-02 | Debug assert fires on the misuse and not on valid configurations | unit, Debug only | `./build/tests/SpatialCoreTests "[intg02]"` (look for the logged assertion count) | no, Wave 0 |
| INTG-02 | Detector adds no allocation or lock | trap | `./build-release/tests/SpatialCoreTests "[rt]"` (existing matrix covers all 8 flag sets for all 23 formats) | yes |
| CI-01 | macOS job green on JUCE 9.0.0 and runs >0 tests including `ui:` | CI run | `gh run list -R AndrewRahman/SpatialCore --workflow ci.yml --limit 1`; step log contains `100% tests passed out of 403` | no, Wave 0 (workflow edit) |
| CI-01 | A root `ctest` finds the tests locally | local | `cmake -S . -B build-x && ctest --test-dir build-x -N` prints `Total Tests: 403` | no, Wave 0 (`enable_testing()`) |
| TEST-01 | Coverage report produced with per-area numbers | script | `python3 -I scripts/coverage-report.py core-summary.json "$PWD"` | no, Wave 0 |
| TEST-01 | `PartitionedConvolver.cpp`, `BinauralRenderer.cpp` have dedicated tests | existing | `./build-release/tests/SpatialCoreTests "[convolver],[bug02]"`, tests in `BinauralRendererTests.cpp` | yes (verified; coverage 96.1% and 100%) |
| TEST-01 | OSD#50 regression fails against old behaviour | unit + mutation | `./build-release/tests/SpatialCoreTests "[osd50]"` green; mutation (hard-swap `setIR`) red; evidence in SUMMARY | no, Wave 0 |
| TEST-01 | OSD#89 regression (onset fallback; pole threshold) | unit + mutation | `"[osd89]"` | no, Wave 0 |
| TEST-01 | OSD#96, #131, #234 tests exist and have recorded red evidence | existing | `"[multi][isolation]"`, `SpatialCoreUITests "[ui][multi]"`, `"[bug02]"` | yes |

### Sampling Rate
- **Per task commit:** the touched area's tags (quick run) plus a Release build of the changed targets.
- **Per wave merge:** full local gate (Debug both executables + Release `ctest`), `build-consumer.sh` once Wave 3+ lands.
- **Phase gate:** full local gate green, the macOS CI job green on a pushed commit, the README realtime-safety check (steps 1-5) once at phase end, nightly workflow dispatched once manually and green.

### Wave 0 Gaps
- [ ] `enable_testing()` at the top level (`CMakeLists.txt`) and the `SPATIALCORE_BUILD_TESTS` default change.
- [ ] `tests/Engine/UnsuppliedGainsTests.cpp`, `tests/Core/UmbrellaTests.cpp`, `[osd50]`/`[osd89]` cases; add the new files to `tests/CMakeLists.txt` (CRLF).
- [ ] `examples/harness/**` and `scripts/coverage-report.py`.
- [ ] `.gitignore`: `build-cov/`.
- [ ] No new framework install.

## Security Domain

`security_enforcement` is absent in `.planning/config.json`, so it is enabled.

### Applicable ASVS Categories

| ASVS Category | Applies | Standard Control |
|---------------|---------|-----------------|
| V2 Authentication | no | no user auth in scope |
| V3 Session Management | no | — |
| V4 Access Control | yes | Branch protection on `main` (D-11); workflow `permissions: contents: read`; no secrets in PR workflows; use `pull_request`, never `pull_request_target` |
| V5 Input Validation | yes | Workflow/script inputs come from untrusted refs and SHAs: pass `${{ github.sha }}` and `${{ github.event.pull_request.head.ref }}` through `env:` and quote them in bash, never interpolate into `run:`; `build-consumer.sh` validates `SHA` against `^[0-9a-f]{40}$` |
| V6 Cryptography | no | none |
| V14 Configuration / supply chain | yes | Pin actions to a major tag at minimum (or full SHA); FetchContent tags are pinned (JUCE 9.0.0, Catch2 v3.7.1, libmysofa); HDF5-signature guard (`CMakeLists.txt:92-99`) already rejects an LFS pointer or a swapped non-SOFA file |

### Known Threat Patterns for this stack

| Pattern | STRIDE | Standard Mitigation |
|---------|--------|---------------------|
| Script injection through branch name or PR title in a workflow `run:` | Tampering | `env:` indirection, no `${{ }}` inside shell bodies |
| Malicious fork PR running with write token or secrets | Elevation of privilege | `pull_request` trigger only, read-only token, no secrets used |
| Tag-moved dependency (FetchContent tag re-pointed) | Tampering | Existing tag pins; optionally pin commit hashes later (not required now) |
| Coverage artifact leaking paths | Information disclosure | Artifact holds only percentages and the llvm-cov JSON summary; repo is public, so no private data |
| Cache poisoning of `.git/lfs` | Tampering | Configure-time HDF5 signature check plus the existing size check in CI |
| Audio-thread DoS through the detector | Denial of service | One relaxed atomic add per block, no allocation, no log (D-06) |

## Sources

### Primary (HIGH confidence)
- This repo, read this session: `CLAUDE.md`, `.planning/phases/06-*/06-CONTEXT.md`, `06-DISCUSSION-LOG.md`, `.planning/REQUIREMENTS.md` (INTG-01/02, CI-01, TEST-01), `.planning/v0.5.0-MILESTONE-AUDIT.md`, `.planning/STATE.md`, `.github/workflows/ci.yml`, `CMakeLists.txt`, `tests/CMakeLists.txt`, `examples/CMakeLists.txt`, `README.md` (gate and realtime-safety sections), `include/SpatialCore/SpatialCore.h`, `RenderEngine.h` (context struct, counters, HRTF section), `RenderEngine.cpp` (renderBlock, render paths, counters), `HRTFProfile.h`, `OutputFormat.h`, `OutputFormatRegistry.h`, `FloatSemanticsGuard.h`, `SpatialMath.h`, `PartitionedConvolver.cpp`, `HRTFDatabase.cpp:300-360`, `BinauralRenderer.cpp:170-215`, test files for #50/#89/#96/#131/#234, Phase 3 and 5 SUMMARY files for red evidence.
- Local experiments (all under `/tmp`, nothing written to the repo): instrumented Debug build and full-suite coverage run; manual UI relink with extracted objects; prototype consumer plugin and console check in two configurations; Release build and serial `ctest`; fetch-by-SHA against the GitHub remote; `ctest` exit codes.
- `gh issue view` for OpenSpatialDelay #50, #89, #96, #131, #234 and SpatialCore #9, #14; `gh run list`/`gh run view` for CI history; `gh api` for action releases and branch-protection status.
- OSD `CMakeLists.txt` (JUCE submodule, libmysofa v1.3.2, `-ffast-math`, arm64) at `~/conductor/repos/openspatialdelay`.

### Secondary (MEDIUM confidence)
- GitHub runner-images README and macOS 26 arm64 readme (labels, Xcode 26.6, CMake 4.4.3, Git LFS 3.8.0, Python 3.14.7).
- docs.github.com: GitHub-hosted runners table (macOS arm64: 3 cores M1, 7 GB, free on public repos), events that trigger workflows (`schedule`), about protected branches, Git LFS billing (10 GiB / 10 GiB, Actions downloads count).
- cmake.org `enable_testing` documentation.
- github.blog changelog 2026-05-14 (image migrations) and runner-images release notes (macOS 14 deprecation) via search summaries.

### Tertiary (LOW confidence)
- Third-party mirrors of runner-image release notes (newreleases.io) used only to corroborate the deprecation date.

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH, versions read from official repos and the runner readme; runner timings unmeasured (A2).
- Architecture: HIGH for the harness, coverage and detector designs (prototyped or read in source); MEDIUM for workflow behaviour that cannot run until pushed (cache hit rates, fork PR SHAs, branch protection effects).
- Pitfalls: HIGH for the four verified defects (ctest root, consumed-mode tests, UI coverage linkage, libmysofa policy), MEDIUM for Linux/Windows first-run outcomes.

**Research date:** 2026-10-17
**Valid until:** 2026-11-17 for tool and runner versions (runner images and action majors move monthly; re-check `macos-26` contents and action releases when planning executes); the repo-internal findings stay valid until the files named change.
