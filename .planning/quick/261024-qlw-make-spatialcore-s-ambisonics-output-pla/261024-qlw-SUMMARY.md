---
quick_id: 261024-qlw
phase: 261024-qlw-ambisonics-plain-ambix
plan: 01
description: Make SpatialCore's Ambisonics output plain AmbiX SN3D (SpatialCore#32)
date: 2026-10-24
status: complete
requirements: [SpatialCore#32]
commits: 4
plan_head_before: 6d9723a791d89326a686f69d16e193a95157a6b8
plan_head_after: 1c905108db83990367bbcaeee4eda72a462b3b5e
actuals:
  tokens: 173427    # chars/4 over `git diff plan_head_before..HEAD`, evidence file excluded
  tasks: 3
  commits: 4        # measured at write time; the fifth commit (this SUMMARY and the ROADMAP tick) follows
key-files:
  created:
    - tests/reference/ShBackHalfReference.h
    - .planning/quick/261024-qlw-make-spatialcore-s-ambisonics-output-pla/evidence/red-old-code.txt
  modified:
    - src/Engine/RenderEngine.cpp
    - include/SpatialCore/Engine/RenderEngine.h
    - tests/Engine/AmbisonicsRenderDirectionTests.cpp
    - tests/Engine/RealtimeSafetyTests.cpp
    - tests/Engine/RenderEngineTests.cpp
    - tests/IO/AmbisonicsCodecTests.cpp
    - tests/reference/gen_sh_reference.py
    - tests/reference/README.md
decisions:
  - "P999.5-D01: the Ambisonics output-format stream is plain AmbiX SN3D; no near-field shelf, no max-rE on that stream; max-rE stays in SpatialCore's own decode"
---

# Quick Task 261024-qlw: Plain AmbiX Ambisonics output (SpatialCore#32) - Summary

## Result

`RenderEngine::renderAmbisonicsOutput` now writes `evalSH x signal x distance gain x tap fade`, with the per-block SH interpolation kept. The inverted per-order near-field shelf and the encoder-side max-rE weights are gone from the output stream; max-rE stays in `AmbisonicsAlgorithm`, `AmbisonicsCodec::applyMaxREWeights` and `[ambi-pin]`. The new engine-stream tests match the independent scipy tables to 1e-4 at orders 1, 3 and 6 and fail on the old code. `include/` changed in comments and private members only (gate G4: no added non-comment line).

## Scope change during execution

The orchestrator narrowed the gate mid-task. The runs below are **Release only**:

- Debug test runs: deferred to the pre-release check per D-19 (SpatialCore#42). The only Debug run is the BASELINE on the untouched tree (before any change), shown below. Debug was built once at baseline; no Debug test ran on the fix.
- build-tsan (ThreadSanitizer realtime-safety check): deferred to the pre-release realtime-safety check (D-18/D-19, SpatialCore#42: the Debug run of both executables and the TSan realtime-safety check run only when preparing a public release, not per fix or at phase end). No TSan tree was configured or run, so there is no TSan evidence for this change. The `[rt]` trap in Release is the only realtime evidence.
- The red run on the old code used `build-release` (old `src/` and `include/`, commit fe23cba).

## Commits

| Hash | Subject |
|------|---------|
| b6348c9 | test(reference): share the back-half SH tables as ShBackHalfReference.h and qualify two issue references (SpatialCore#32) |
| fe23cba | test(engine): pin the Ambisonics output stream to plain AmbiX, red on the old shelf and max-rE code (SpatialCore#32) |
| 06982d6 | fix(engine)!: the Ambisonics output is plain AmbiX SN3D, no near-field shelf and no max-rE (SpatialCore#32) |
| 1c90510 | docs: the Ambisonics output is plain AmbiX SN3D (SpatialCore#32) |
| (this commit) | docs(planning): record quick task 261024-qlw, tick the Phase 999.5 SpatialCore follow-up (SpatialCore#32) |

Nothing pushed, no GitHub comment, no version bump (D-06).

## Gate counts

### BASELINE (Debug, untouched tree, HEAD 6d9723a, before any change)

```
test cases:    394 |    393 passed | 1 failed
assertions: 331517 | 331516 passed | 1 failed
```

The one failure is the pre-existing `[bug03]` (see below). `[ambi][sn3d][golden]` at baseline: `All tests passed (784 assertions in 2 test cases)`. The Debug run also prints the JUCE leaked-object line for 7 FFT instances at exit, the deliberate process-lifetime SharedFFTCache (D-08); it was already present in quick 261024-mhh.

### FINAL (Release, fix tree, code at 06982d6; 1c90510 changed docs only)

Targeted run, `./build-release/tests/SpatialCoreTests "[plain-ambix],[ambix],[ambi],[ambi-pin],[sc19],[sc20],[sanitize],[sn3d][golden],[bug03]"`:

```
All tests passed (5638 assertions in 29 test cases)
```

`[plain-ambix]` alone: `All tests passed (2118 assertions in 2 test cases)`. `[ambi][sn3d][golden]` after the table move: `All tests passed (784 assertions in 2 test cases)`, the same count as the baseline. `[bug03]`: `All tests passed (5 assertions in 1 test case)`.

Full Release run, `ctest --test-dir build-release/tests --output-on-failure` (154.63 s):

```
100% tests passed out of 410
```

`ctest --test-dir build-release/tests -N` reports `Total Tests: 410`, and `grep -c 'ui:'` gives 16. Case delta against the baseline is 0 (394 `SpatialCoreTests` cases + 16 `ui:` = 410 before; 2 removed, 2 added).

`./build-release/tests/SpatialCoreTests "[rt]"`:

```
All tests passed (2514 assertions in 46 test cases)
```

This includes the unchanged case "every Ambisonics format renders moving distance without allocating", which is the realtime proof of the new loop (the trap counts allocations, frees and pthread locks; it passed, so zero of each).

Greps: G1 (removed identifiers in RenderEngine.cpp and RenderEngine.h, baseline 29) now 0; G2 (RealtimeSafetyTests.cpp, baseline 7) now 0; G3 (renderAmbisonicsOutput body, baseline 5) now 0; G4 (added non-comment lines in `include/`, baseline 0) 0.

### Not run (deferred to the pre-release check)

- Debug `SpatialCoreTests` and `SpatialCoreUITests` on the fix tree.
- build-tsan: whole suite, `[tsan]` subset, UI suite. The code removes per-sample filter state and adds no shared state, but this is a statement about the diff, not evidence.

## Fail-on-old-code evidence

Full output: `.planning/quick/261024-qlw-make-spatialcore-s-ambisonics-output-pla/evidence/red-old-code.txt`, from `./build-release/tests/SpatialCoreTests "[plain-ambix]"` on the old `src/` and `include/` (commit fe23cba, `git diff --quiet HEAD -- src include` clean). The pre-existing `[ambix]~[plain-ambix]` cases (695 assertions in 3 test cases) pass on the old code, as does `[bug03]` after the title fix.

Tail of the evidence file, verbatim:

```
test cases:    2 |   0 passed |    2 failed
assertions: 2118 | 645 passed | 1473 failed
```

The 1473 failures split into 1473 "golden", 660 "distance independence" and 264 "phase residual" messages (a failed assertion can carry more than one message group, so these do not sum to 1473). First failing lines, verbatim:

Golden case (max-rE scaling, FOA, az 135 el 0, order 1 scaled by cos(pi/4) = 0.7071):

```
  CHECK( std::abs (out[(size_t) n] / kDcLevel - ref.table[n]) < kRatioTol )   [kStreamTol at fe23cba]
with expansion:
  0.209872574f < 0.0001f
with message:
  golden: 1st order (4 ch), az 135 el 0, ACN 1 expected 0.707107 got 0.497234
```

Distance case, DC (shelf gain r / 1.5 at low frequency; distance 1.0 is 10 m):

```
  golden: 1st order (4 ch), 0 Hz, distance 1, ACN 1 expected -0.892539 got -4.20319
  distance independence: 1st order (4 ch), 0 Hz, distance 1, ACN 1 ratio -4.20319 vs ratio at 1.5 m -0.627629
```

Distance case, 50 Hz (filter phase):

```
  distance independence: 1st order (4 ch), 50 Hz, distance 0.05, ACN 1 ratio -0.281095 vs ratio at 1.5 m -0.629934
  phase residual: 1st order (4 ch), 50 Hz, distance 1, ACN 1 residual 0.504926
```

So "distance independence" fails at 0.5 m, 5 m and 10 m at DC and at 50 Hz (counts per distance in the log: 0.05, 0.5 and 1.0 for every format at both frequencies), "phase residual" fails at 50 Hz at every distance including 1.5 m, and "golden" fails everywhere because of max-rE. Old code commit for reproduction: 6d9723a (code identical to a43a8aa; the plan commit only added planning files).

## Rebaseline register

Deliberate and complete; nothing was added to the plan's list.

| ID | File | Change | Reason |
|----|------|--------|--------|
| R1 | tests/Engine/AmbisonicsRenderDirectionTests.cpp | header comment (shelf-neutral-at-1.5-m reasoning dropped); neutral-distance constant renamed `kDefaultDistance`; `kRatioTol` 2e-2 to 1e-4 and `kStreamTol` folded into it; max-rE helper and the now-unused `acnOrder` removed; higher-order case expects `evalSH(n) / evalSH(0)` with no weights, sign check kept, retitled "plain evalSH, relative to W (no max-rE)"; elevated-source comment no longer mentions a dead band | these asserted the max-rE-weighted, shelf-affected stream that D-01 removes; 2 percent was the old shelf dead band |
| R2 | tests/Engine/RealtimeSafetyTests.cpp | deleted `[ambi][nfc][d10]` (in-place assignment bit-for-bit), `[ambi][nfc][d10][stored]` with `RenderEngineTestAccess`, and the hidden `[.][ambi-nfc-capture]`; MovingScene comment reworded | they pinned shelf coefficients and the Phase 5 D-10 byte identity of the shelf fix; the shelf no longer exists, the capture was the before/after tool for it, and the new golden tests pin the new output. `[rt]` moving-distance case unchanged |
| R3 | tests/Engine/RenderEngineTests.cpp | `[sanitize]` NaN-distance Ambisonics case retitled (finite output, equals last good distance); SC-19 section and tolerance comments reworded | wording tied to deleted state. All assertions and the 5e-3 tolerance are unchanged; the order-change guard stays. The SC-19 case title that says "filters the new orders" was left as is (renaming it renames a ctest name for no behavioural reason) |

Deliberately not rebaselined (D-02): `[ambi][maxre][d10]` and `[.][ambi-maxre-capture]`, `[ambi-pin]`, the `AmbisonicsCodec` max-rE test, the SH golden tests.

## Pre-existing `[bug03]` failure at HEAD, and its fix

At baseline the full Debug run failed exactly one case, `[bug03]` ("Issue references in include, src and tests name their tracker"), naming `tests/IO/AmbisonicsCodecTests.cpp:588` and `:605`: two TEST_CASE titles written "(999.5, #32)" in commit c292b47. Not caused by this task. Commit b6348c9 qualifies both as `SpatialCore#32`; `[bug03]` passes from then on.

## ShBackHalfReference.h: generator, not the fallback

The header was produced by `python3 tests/reference/gen_sh_reference.py --back-half-header` (new flag), run through a throwaway wrapper outside the repo (`/tmp/qlw-gen/run_gen.py`) that aliases `scipy.special.sph_harm_y` to `sph_harm` because this machine has scipy 1.13.1 (numpy 2.0.2) and the generator wants `sph_harm_y` from 1.15. The generator's own self-check (two scipy routes agree below 1e-12) ran. Checks: the 343 numeric table lines of the header are identical to the ones previously inline in `AmbisonicsCodecTests.cpp`; `--back-half` output is byte-identical before and after the generator edit (cmp); `[ambi][sn3d][golden]` keeps 784 assertions. The header's provenance comment records scipy 1.13.1, not the README pin (1.18.1); the values are identical, the oracle is the same math. Nothing was pip installed.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] `python -I` hid the user-site scipy**
- **Found during:** Task 1 Step A
- **Issue:** the wrapper failed with `ModuleNotFoundError: No module named 'scipy'` under `-I` (isolated mode ignores user site-packages, where scipy lives)
- **Fix:** ran the wrapper without `-I`; the script is mine, in `/tmp`, and reads no downloaded data
- **Files modified:** none

**2. [Rule 1 - Bug] README.md line endings**
- **Found during:** Task 3
- **Issue:** a Python read/write of the CRLF README normalised every line ending (414-line diff)
- **Fix:** restored the file with `git checkout -- README.md` and re-applied the two edits with newline-preserving I/O; the diff is 4 lines
- **Files modified:** README.md
- **Commit:** 1c90510

### Scope changes from the orchestrator (not plan deviations by the executor)

Release-only gate; Debug and TSan deferred (see above). The plan's Task 3 TSan chain and the Debug gate were therefore not run.

### Other notes

- The plan's baseline HEAD was a43a8aa; the tree started at 6d9723a (plan commit on top). Code is identical.
- Tracer gate: Task 1 is the tracer; it was verified end to end on the old code (red) and again after Task 2 (green). No halt.
- `Co-Authored-By` trailer on every commit as instructed by the orchestrator.

## Known Stubs

None.

## Threat Flags

None. No new input, file or network path; the audio loop is smaller and allocates nothing.

## Self-Check

- Files: `tests/reference/ShBackHalfReference.h`, the evidence file, and this SUMMARY exist (checked below).
- Commits b6348c9, fe23cba, 06982d6, 1c90510 are ancestors of HEAD.
- Not covered by this self-check: Debug and TSan, deferred by the orchestrator.

## Self-Check: PASSED
