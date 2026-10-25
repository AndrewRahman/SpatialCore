---
phase: 261024-qlw-ambisonics-plain-ambix
plan: 01
type: execute
wave: 1
depends_on: []
files_modified:
  - tests/reference/gen_sh_reference.py
  - tests/reference/ShBackHalfReference.h
  - tests/reference/README.md
  - tests/IO/AmbisonicsCodecTests.cpp
  - tests/Engine/AmbisonicsRenderDirectionTests.cpp
  - tests/Engine/RealtimeSafetyTests.cpp
  - tests/Engine/RenderEngineTests.cpp
  - src/Engine/RenderEngine.cpp
  - include/SpatialCore/Engine/RenderEngine.h
  - CLAUDE.md
  - README.md
  - docs/integration-guide.md
  - .claude/skills/spatial-audio-dsp/SKILL.md
  - .planning/codebase/ARCHITECTURE.md
  - .planning/PROJECT.md
  - .planning/STATE.md
  - .planning/ROADMAP.md
  - .planning/quick/261024-qlw-make-spatialcore-s-ambisonics-output-pla/evidence/red-old-code.txt
  - .planning/quick/261024-qlw-make-spatialcore-s-ambisonics-output-pla/261024-qlw-SUMMARY.md
autonomous: true
requirements: [SpatialCore#32]   # quick task: no REQUIREMENTS.md ID exists; the issue is the requirement
user_setup: []

estimate:
  tokens: 150000
  raw_tokens: 150000
  tasks: 3
  confidence: low

must_haves:
  truths:
    - "D-01: for any source, the Ambisonics output channels equal evalSH(acn, az, el) x signal x the block's distance gain x tap fade, and match the independent scipy tables (ShBackHalfReference.h) within 1e-4 at orders 1, 3 and 6"
    - "D-01: channel ratios to W are identical at 0.5, 1.5, 5 and 10 m, at DC and at 50 Hz, and the 50 Hz stream has no phase shift between channels (least-squares residual below 1e-3)"
    - "D-01, D-02: the output stream has no near-field filter and no max-rE weighting; AmbisonicsAlgorithm::computeGains, AmbisonicsCodec::applyMaxREWeights and the [ambi-pin] decode still use max-rE and their tests are untouched"
    - "D-03: per-block SH interpolation (prevSHCoeffs, snapInterpolation_) is unchanged; the SC-19 order-change tests and the SC-20 fade tests still pass"
    - "D-04: no public declaration in include/ is added, removed or changed; only private members, a private friend declaration and comments go"
    - "D-05: renderAmbisonicsOutput allocates, locks and logs nothing: [rt] in Release reports zero allocations and zero locks, and the build-tsan whole-suite, [tsan] and UI runs report exit 0 and zero ThreadSanitizer warnings"
    - "Every test that pinned the old output is listed with a reason (Task 2 register, copied into the SUMMARY); nothing unrelated is weakened; the new tests fail on the old code with the output recorded verbatim"
  artifacts:
    - path: "tests/reference/ShBackHalfReference.h"
      provides: "the seven back-half scipy SH tables plus kBackHalfRefs, shared by the codec and engine tests"
    - path: "tests/Engine/AmbisonicsRenderDirectionTests.cpp"
      provides: "engine-stream golden test (orders 1, 3, 6) and the distance and 50 Hz independence test, tag [plain-ambix]"
    - path: "src/Engine/RenderEngine.cpp"
      provides: "plain AmbiX renderAmbisonicsOutput"
    - path: ".planning/quick/261024-qlw-make-spatialcore-s-ambisonics-output-pla/evidence/red-old-code.txt"
      provides: "verbatim failing output of the new tests on the old code"
    - path: ".planning/quick/261024-qlw-make-spatialcore-s-ambisonics-output-pla/261024-qlw-SUMMARY.md"
      provides: "gate counts verbatim, RED evidence, rebaseline register"
  key_links:
    - from: "tests/Engine/AmbisonicsRenderDirectionTests.cpp"
      to: "tests/reference/ShBackHalfReference.h"
      via: "#include and spatialcore_ref::kBackHalfRefs"
    - from: "tests/IO/AmbisonicsCodecTests.cpp"
      to: "tests/reference/ShBackHalfReference.h"
      via: "same include; the codec back-half test keeps its assertions"
    - from: "src/Engine/RenderEngine.cpp renderAmbisonicsOutput"
      to: "evalSH in Core/SpatialMath.h"
      via: "objSHCoeffs[t][c] = evalSH (c, azRad, elRad), nothing multiplied in"
---

<objective>
Make the Ambisonics output-format stream of `RenderEngine` plain AmbiX SN3D (SpatialCore#32), the
spec being `.planning/HANDOFF-ambisonics-fix.md` "Session A". Tests first (red on the old code),
then the fix, then docs and the full realtime-safety gate.

Purpose: the stream carries two things standard AmbiX encoders (IEM StereoEncoder, MultiEncoder) do
not: a per-order near-field shelf whose low-frequency gain is r / 1.5 (inverted against Daniel 2003,
no per-order power) and max-rE weights that IEM decoders apply a second time. Both degrade the image
of a decoded field. Output: plain evalSH x signal x existing distance and tap gains.

Decisions (locked by the orchestrator task; cited as D-NN below):
- D-01: the Ambisonics output stream = evalSH x signal x the existing distance and tap gains. No near-field shelf, no max-rE on that stream.
- D-02: max-rE stays wherever SpatialCore itself decodes (AmbisonicsAlgorithm speaker decode, `[ambi-pin]`, `AmbisonicsCodec::applyMaxREWeights`). Do not touch those.
- D-03: keep the per-block SH interpolation.
- D-04: public API stays source-compatible.
- D-05: the audio path stays allocation-, lock- and logging-free.
- D-06: no push, no GitHub comment, no version bump (the orchestrator pushes and comments; #32 stays open until openspatialdelay#22 ships and the speaker check passes).

Tracer note: this is a behaviour change inside one function reached through one entry point
(`RenderEngine::renderBlock`). The end-to-end tracer is Task 1: `renderBlock` to the Ambisonics
channels to the scipy golden values, starting red as the TDD composition requires. Task 2 turns it
green. There are no further layers to thread.

## Multi-source coverage audit

| Source | Item | Covered by |
|--------|------|-----------|
| GOAL | Ambisonics output becomes plain AmbiX SN3D (SpatialCore#32) | Task 2 |
| REQ | SpatialCore#32 (quick task, no REQUIREMENTS.md ID) | Tasks 1-3 |
| RESEARCH | nfc-check.md: shelf inverted, no per-order power; remove, do not repair | Task 2 (D-01) |
| RESEARCH | iem-ambisonics-math.md: IEM encoders emit no max-rE, decoders add it; port nothing | Task 2 (D-01, D-02) |
| RESEARCH | ring-sim.md: image degrades, does not move; direction tests must keep passing | Task 2 verify |
| RESEARCH | UNTESTED-partial diff: reference only, never applied; its bare issue refs would fail `[bug03]` | Tasks 1 and 2 (reused by hand, qualified) |
| CONTEXT | D-01 plain stream; golden tests at orders 1, 3, 6; ratios identical at 0.5/1.5/5/10 m at DC and 50 Hz | Tasks 1, 2 |
| CONTEXT | D-02 max-rE stays in the decode | Task 2 (register: untouched), Task 3 docs |
| CONTEXT | D-03 interpolation kept | Task 2 |
| CONTEXT | D-04 source-compatible API | Task 2 gate on include/ diff |
| CONTEXT | D-05 realtime-safe | Tasks 2, 3 (`[rt]`, build-tsan) |
| CONTEXT | rebaseline `[nfc]`, `[d10]`, Phase 5 captures, with reasons | Task 2 register |
| CONTEXT | docs: CLAUDE.md, README, HOA doc, decisions table (#32, closed #21), ROADMAP 999.5 tick | Task 3 |
| CONTEXT | gate counts, RED evidence, rebaselined tests reported | Task 3 SUMMARY |

Excluded (not gaps): OpenSpatialDelay AU / format fallback (openspatialdelay#22, Session B), REAPER
speaker check, GitHub comment and push (D-06), version bump (D-06).
</objective>

<execution_context>
@~/.claude/gsd-core/workflows/execute-plan.md
@~/.claude/gsd-core/templates/summary.md
</execution_context>

<context>
@.planning/HANDOFF-ambisonics-fix.md
@.planning/quick/261024-mhh-re-check-the-ambisonics-maths-end-to-end/261024-mhh-FINDINGS.md
@.planning/quick/261024-mhh-re-check-the-ambisonics-maths-end-to-end/research/nfc-check.md
@.planning/quick/261024-mhh-re-check-the-ambisonics-maths-end-to-end/research/iem-ambisonics-math.md
@.planning/quick/261024-mhh-re-check-the-ambisonics-maths-end-to-end/research/ring-sim.md
@.planning/quick/261024-mhh-re-check-the-ambisonics-maths-end-to-end/research/UNTESTED-partial-plain-ambix-fix.diff
@.planning/STATE.md
@CLAUDE.md
@src/Engine/RenderEngine.cpp
@include/SpatialCore/Engine/RenderEngine.h
@tests/Engine/AmbisonicsRenderDirectionTests.cpp
@tests/IO/AmbisonicsCodecTests.cpp
@tests/reference/gen_sh_reference.py

Facts found while planning (verified against HEAD a43a8aa, 2026-10-24):
- `[bug03]` (tests/Core/IssueReferenceTests.cpp) scans every .h/.cpp under include/, src/, tests/ for a bare issue number (a hash and digits not preceded by a tracker name). It FAILS at HEAD: tests/IO/AmbisonicsCodecTests.cpp lines 588 and 605 (two TEST_CASE titles written "(999.5, <hash>32)" in commit c292b47). Not caused by this task; Task 1 fixes it. Every new comment, test title and string in include/, src/, tests/ must say `SpatialCore#32` or `AndrewRahman/SpatialCore#21`, never a bare number. The reference diff's test titles use the bare form: do not copy them verbatim.
- `tests/reference/ShBackHalfReference.h` does not exist. The seven tables and the BackHalfRef array live in an anonymous namespace in tests/IO/AmbisonicsCodecTests.cpp (about lines 200-330 and 575-590), pasted from `gen_sh_reference.py --back-half`. A copy of the header from the interrupted attempt sits in the git-ignored `.context/partial-fix-files/`: reference only, do not depend on it.
- This machine has Python 3.9.6 with scipy 1.13.1 (user site). `gen_sh_reference.py` imports `scipy.special.sph_harm_y`, which only exists from scipy 1.15, and `.context/venv` (README pin: scipy 1.18.1) does not exist here. Do NOT pip install anything.
- Baselines at HEAD, computed while planning: code-only matches of the removed identifiers in RenderEngine.cpp plus RenderEngine.h = 29; in RealtimeSafetyTests.cpp = 7; the body of renderAmbisonicsOutput has 5 code lines matching the G3 pattern (cos, tan, the filter call, the weighted evalSH); `grep -c plain-ambix` in the direction tests = 0. Previous full-suite counts (quick 261024-mhh): Debug SpatialCoreTests 392 cases / 330733 assertions, SpatialCoreUITests 16 cases / 150 assertions, Release ctest 408 tests. Expected case delta of this task: 2 NFC cases removed, 2 new cases added.
- Build trees: `build` (Debug) and `build-release` exist and are configured; `build-tsan` does not (git-ignored). Sources for an offline TSan configure are in `build/_deps/{juce,catch2,mysofa}-src`.
- Long output: pipe builds and test runs to a log under /tmp and `tail`/`grep` it; keep full logs for the SUMMARY.
</context>

<tasks>

<task type="tracer" tdd="true">
  <name>Task 1: shared scipy tables, then the end-to-end tests that fail on the old code</name>
  <files>tests/reference/gen_sh_reference.py, tests/reference/ShBackHalfReference.h, tests/reference/README.md, tests/IO/AmbisonicsCodecTests.cpp, tests/Engine/AmbisonicsRenderDirectionTests.cpp, .planning/quick/261024-qlw-make-spatialcore-s-ambisonics-output-pla/evidence/red-old-code.txt</files>
  <read_first>
    .planning/HANDOFF-ambisonics-fix.md (Session A, Scope 2)
    tests/Engine/AmbisonicsRenderDirectionTests.cpp (helper renderDc, constants, existing three cases)
    tests/IO/AmbisonicsCodecTests.cpp (the back-half block and the two TEST_CASE titles with the bare issue number)
    tests/reference/gen_sh_reference.py and tests/reference/README.md
    .planning/quick/261024-mhh-re-check-the-ambisonics-maths-end-to-end/research/UNTESTED-partial-plain-ambix-fix.diff (the added tests near its end: reuse the renderFit idea by hand, reference only)
  </read_first>
  <behavior>
    - Golden: for FOA (4 ch), 3OA (16 ch) and 6OA (49 ch), each of the seven kBackHalfRefs directions, a DC source of level 0.5 gives out[n] / 0.5 equal to the scipy table value for every ACN n, within 1e-4 absolute (this includes W = 1, so overall gain is pinned too).
    - Distance independence: for FOA, 3OA and 6OA, direction az -100 el -25 (all 49 channels clearly non-zero), input DC and input a 50 Hz sine, distance values 0.05, 0.15, 0.5 and 1.0 (0.5, 1.5, 5 and 10 m): (a) channel n over W equals table[n] / table[0] within 1e-4 [golden]; (b) channel n over W at each distance equals the same ratio at distance 0.15 within 1e-4 [distance independence]; (c) the least-squares residual of each channel against the input is below 1e-3 of the channel RMS [no filter phase].
    - On the old code both new cases fail: golden because of max-rE (orders 1+ scaled by cos(n pi / (2N + 2))), distance independence because the shelf gain is r / 1.5 at DC and has phase at 50 Hz.
    - On both old and new code, every existing test in the file and `[bug03]` and `[sn3d][golden]` pass.
  </behavior>
  <action>
    Work on branch AndrewRahman/ambisonics-plain-ambix (do not create another). Stage files by name, never `git add -A`. Commit trailer on every commit: `Co-Authored-By: Claude Opus 5.5 (1M context) <noreply@anthropic.com>`. Issue references in include/, src/, tests/ are always `SpatialCore#32` (see context).

    Step 0, baseline on the untouched tree: record `git rev-parse HEAD` (expect a43a8aa). Run `cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug` then `cmake --build build -j8` (log to /tmp/qlw-build-debug-0.log; if configure fails offline on FetchContent, add `-DFETCHCONTENT_FULLY_DISCONNECTED=ON`). Run `./build/tests/SpatialCoreTests "[ambi][sn3d][golden]"` and note its assertion count (the moved tables must keep exactly this count), then the full `./build/tests/SpatialCoreTests` once and keep its last 4 lines as BASELINE for the SUMMARY. Expect exactly one failing case, `[bug03]`, naming AmbisonicsCodecTests.cpp lines 588 and 605. Anything else failing at HEAD is a finding to record in the SUMMARY, not to fix here.

    Step A, shared tables (commit 1). In tests/reference/gen_sh_reference.py add a `--back-half-header` flag that prints the complete header: pragma once, a comment saying these are the back-half SH tables (999.5, SpatialCore#32) shared by the codec and engine tests and generated by this script, `namespace spatialcore_ref`, the seven tables, a `BackHalfRef` struct (azDeg, elDeg, table pointer) and an `inline constexpr BackHalfRef kBackHalfRefs[]` listing the BACK_HALF directions in their existing order. Give `back_half()` a qualifier parameter so `--back-half` keeps printing exactly what it prints today (byte for byte) while the header form prints `inline constexpr float` tables. Produce tests/reference/ShBackHalfReference.h by running the generator, never by hand. To run it on this machine write a throwaway wrapper OUTSIDE the repo (for example /tmp/qlw-gen/run_gen.py) that defines `scipy.special.sph_harm_y (n, m, polar, azimuth)` as `scipy.special.sph_harm (m, n, azimuth, polar)` before `runpy.run_path`-ing the generator with `--back-half-header`; the generator's own self-checks (two routes agree below 1e-12) stay active. Prove the header's table values are the ones already checked in: the numeric lines of each table must be identical to the ones in tests/IO/AmbisonicsCodecTests.cpp. If the wrapper does not work in one honest attempt, fall back to moving the tables verbatim from AmbisonicsCodecTests.cpp into the header with the same layout and say so in the SUMMARY ("generator not runnable here: scipy 1.13.1 has no sph_harm_y; tables byte-identical to c292b47"); never invent or retype values. Compile-check the header with `c++ -std=c++17 -fsyntax-only -x c++ tests/reference/ShBackHalfReference.h`.

    In tests/IO/AmbisonicsCodecTests.cpp replace the anonymous-namespace block (tables, struct, array) with an include of "../reference/ShBackHalfReference.h" next to the existing ShReference.h include and `using spatialcore_ref::kBackHalfRefs;`, and update the comment above it to point at the header. Change the two TEST_CASE titles that carry the bare issue number to `SpatialCore#32`. Add a row for ShBackHalfReference.h to tests/reference/README.md (generator `gen_sh_reference.py --back-half-header`, tests `[sn3d][golden]` and `[plain-ambix]`, include path from tests/Engine and tests/IO). Verify: build, `[ambi][sn3d][golden]` has the same assertion count as in Step 0, `[bug03]` passes. Commit: `test(reference): share the back-half SH tables as ShBackHalfReference.h and qualify two issue references (SpatialCore#32)`.

    Step B, the failing tests (commit 2). In tests/Engine/AmbisonicsRenderDirectionTests.cpp include the new header and add two TEST_CASEs tagged `[ambi][engine][ambix][plain-ambix]`, with titles ending in `(SpatialCore#32)`. Use two new constants local to them: `kStreamTol = 1.0e-4f`, `kResidualTol = 1.0e-3f` (Task 2 folds `kStreamTol` into `kRatioTol`).
    Test 1 (golden): loop the three formats (OutputFormat::AmbisonicsFOA order 1, AmbisonicsHOA order 3, Ambisonics6OA order 6) and the seven kBackHalfRefs; DYNAMIC_SECTION per format and direction; render with the existing `renderDc` and CHECK `out[n] / kDcLevel` against `ref.table[n]` for n below (order + 1) squared, with INFO naming the check "golden", format, direction and ACN. SN3D channels do not depend on the stream order, so the 49-entry tables serve all three formats by their first 4, 16 and 49 entries.
    Test 2 (distance and 50 Hz): add a helper `renderFit` modelled on the one in the reference diff: it takes format, channel count, az, el, distance and a frequency (0 means DC, otherwise a sine continuous across blocks), renders kSettleBlocks blocks of a unit-amplitude input through a fresh engine with the same wiring as `renderDc` (engineComputesGains and engineDerivesDispatch true, distGain and tapFade 1), and over the last 100 blocks returns per channel the least-squares gain onto the input and the relative residual of what the gain cannot explain. Use direction az -100 el -25, taken from the kBackHalfRefs entry with those angles (expected values come from the scipy table, not from evalSH: the earlier mhh test only checked the engine against its own evalSH). One DYNAMIC_SECTION per format and frequency; inside it render the four distances and, for every channel n of ACN 1 and above, run the three checks from `<behavior>` with INFO text naming each check ("golden", "distance independence", "phase residual"), the distance and the frequency. REQUIRE W (channel 0) positive first.

    Step C, red evidence. Build the Debug test target. Run `./build/tests/SpatialCoreTests "[plain-ambix]"` and save the full output to .planning/quick/261024-qlw-make-spatialcore-s-ambisonics-output-pla/evidence/red-old-code.txt (create the directory). Both cases must FAIL, for the reasons in `<behavior>`; look at the first failing lines of each case in the file to confirm the "distance independence" and "phase residual" checks fail at 5 m and 10 m and at 50 Hz, not only the golden check. If a case passes on the old code or fails for a different reason (crash, wrong index, tolerance too loose), fix the test, not the engine. Also run `./build/tests/SpatialCoreTests "[ambix]~[plain-ambix]"` (the pre-existing direction tests): all pass on the old code. Commit the test file and the evidence file: `test(engine): pin the Ambisonics output stream to plain AmbiX, red on the old shelf and max-rE code (SpatialCore#32)`. Do not touch src/ or include/ in this task (D-01 is implemented in Task 2).
  </action>
  <verify>
    <automated>cmake --build build -j8 --target SpatialCoreTests > /tmp/qlw-build-t1.log 2>&1 && ./build/tests/SpatialCoreTests "[bug03]" && ./build/tests/SpatialCoreTests "[ambi][sn3d][golden]" && ./build/tests/SpatialCoreTests "[ambix]~[plain-ambix]" && ! ./build/tests/SpatialCoreTests "[plain-ambix]" > /tmp/qlw-red-verify.txt 2>&1 && grep -c "FAILED" /tmp/qlw-red-verify.txt && c++ -std=c++17 -fsyntax-only -x c++ tests/reference/ShBackHalfReference.h && git diff --quiet a43a8aa -- src include; echo "src_include_untouched_exit=$?"</automated>
  </verify>
  <done>ShBackHalfReference.h exists and the codec back-half test keeps its assertion count; `[bug03]` passes; the two `[plain-ambix]` cases fail on the old code and the full output is committed in evidence/red-old-code.txt; all other `[ambix]` cases pass; src/ and include/ are untouched; two atomic commits exist.</done>
</task>

<task type="auto" tdd="true">
  <name>Task 2: implement plain AmbiX in renderAmbisonicsOutput and rebaseline what pinned the old output</name>
  <files>src/Engine/RenderEngine.cpp, include/SpatialCore/Engine/RenderEngine.h, tests/Engine/AmbisonicsRenderDirectionTests.cpp, tests/Engine/RealtimeSafetyTests.cpp, tests/Engine/RenderEngineTests.cpp</files>
  <read_first>
    src/Engine/RenderEngine.cpp (prepare() near line 490-560; renderAmbisonicsOutput near 1605-1745)
    include/SpatialCore/Engine/RenderEngine.h (top comment ~43, RenderBlockContext ~157, friend ~606, acnToOrder ~703, state ~831)
    tests/Engine/AmbisonicsRenderDirectionTests.cpp, tests/Engine/RealtimeSafetyTests.cpp (the NFC cases ~551-701 and MovingScene comment ~57), tests/Engine/RenderEngineTests.cpp (~811 NaN-distance title, ~1750 SC-19 comment)
    .planning/quick/261024-mhh-re-check-the-ambisonics-maths-end-to-end/research/UNTESTED-partial-plain-ambix-fix.diff (reference only; do not apply it with git apply, line numbers and context differ)
  </read_first>
  <behavior>
    - Both `[plain-ambix]` cases from Task 1 pass.
    - The rewritten `[ambix]` direction tests pass with kRatioTol = 1e-4 and plain evalSH expectations.
    - `[ambi-pin]`, `[ambi][maxre][d10]`, `[sc19]`, `[sc20]`, `[sanitize]`, `[rt]`, `[sn3d][golden]`, `[bug03]` and the whole suite pass in Debug and Release; `[rt]` reports zero allocations and zero locks.
  </behavior>
  <action>
    Implement D-01 in src/Engine/RenderEngine.cpp `renderAmbisonicsOutput`: each live object contributes evalSH (acn, az, el) x mono x distGainPerSample x tapFade, interpolated linearly per block from prevSHCoeffs to the block's coefficients exactly as today (D-03). Concretely: (1) change the function's header comment to say it is plain AmbiX SN3D per SpatialCore#32, that IEM-style decoders assume plane-wave encoding and apply their own max-rE, so none is applied here; (2) delete the cached order weights block, the whole near-field block (smoothing constant, the sample-rate local, the order-change invalidation, the per-object coefficient update loop); (3) store plain `evalSH (c, azRad, elRad)` in objSHCoeffs; (4) in the per-sample loop replace the W special case and the per-order filter loop with one loop over c below numAmbiCh that adds scaledMono times the interpolated coefficient to ambiAccum[c]; (5) keep everything else byte for byte: the snapInterpolation_ block, invN, the ambiAccum zeroing, the `c < numOutCh` and null-pointer write guard, the prevSHCoeffs carry-forward. In `prepare()` delete the per-object, per-order filter preparation block (its comment starts "v0.5: Prepare NFC-HOA filters" and ends with the reset of the last-order variable) and keep the LFE filter lines above it and the SC-20 fadeInPending_ reset below it. No new members, no allocation, no logging, no lock (D-05).

    In include/SpatialCore/Engine/RenderEngine.h (D-04: private members, one private friend declaration and comments only): update the top comment so it says Ambisonics SH encode (plain AmbiX SN3D); update the RenderBlockContext comment above `ambiOrder` / `sampleRate` to say the Ambisonics stream no longer reads the sample rate and the field stays for the other paths and for source compatibility (do NOT remove or retype `sampleRate`); delete the test-only friend declaration with its comment; delete the private `acnToOrder` helper with its comment; replace the NFC and max-rE state block with a short comment that the output is plain AmbiX (SpatialCore#32) and keep only `kMaxAmbiOrder`, `kMaxAmbiChannels` and `prevSHCoeffs`. Delete the per-object filter array, the smoothed and previous distance arrays, the last-order marker, the cached order and weight table and the reference-radius constant (Task 2 gate G1 lists them). Do not touch AmbisonicsAlgorithm, AmbisonicsCodec or any decode code (D-02).

    Rebaseline, deliberately and only these (everything else must pass unchanged; if the full suite shows another test pinning the old output, add it to this register with its reason, never loosen a tolerance outside it):
    R1 tests/Engine/AmbisonicsRenderDirectionTests.cpp: header comment (drop the shelf-neutral-at-1.5-m reasoning, say plain AmbiX, SpatialCore#32); rename the neutral-distance constant to a plain default distance; set kRatioTol to 1.0e-4f (2 percent was the old shelf dead band) and fold the Task 1 kStreamTol into it; remove the max-rE helper; the higher-order case now expects evalSH (n) / evalSH (0) with no weights and keeps its sign check; the elevated-source comment no longer mentions a dead band; retitle the higher-order case "plain evalSH, relative to W (no max-rE)". Reason: these asserted the max-rE-weighted, shelf-affected stream that D-01 removes.
    R2 tests/Engine/RealtimeSafetyTests.cpp: delete the case "Ambisonics NFC: the in-place array assignment stores the old coefficients bit for bit" (`[ambi][nfc][d10]`), the case "...the engine stores the formula's coefficients for the distance it last updated with" (`[ambi][nfc][d10][stored]`) with its `RenderEngineTestAccess` struct, and the hidden capture case `[.][ambi-nfc-capture]` with the banner comments above each. Reason: they pin the shelf coefficients and the Phase 5 D-10 byte-identity of the shelf fix; the shelf no longer exists, the capture was the before/after tool for that fix, and the new golden tests pin the new output. Reword the MovingScene comment that mentions the smoothed near-field distance. Keep the `[rt]` case "every Ambisonics format renders moving distance without allocating" unchanged: it is the realtime proof of the new loop. Keep `<cstring>` and `<cstdlib>` (still used by the max-rE cases).
    R3 tests/Engine/RenderEngineTests.cpp: retitle the `[engine][sanitize]` NaN-distance Ambisonics case to say the output stays finite and equals the last good distance (no filter-state wording), assertions unchanged; reword the SC-19 section comment and the tolerance comment (no shelf, the stream is exact, 5e-3 stays as generous margin), tolerance value unchanged. Reason: wording tied to deleted state; the order-change guard itself stays.
    Deliberately NOT rebaselined (D-02): `[ambi][maxre][d10]` and `[.][ambi-maxre-capture]` (AmbisonicsAlgorithm::computeGains), `[ambi-pin]`, the AmbisonicsCodec max-rE test, the SH golden tests.

    Then run the local gate on the fix tree and keep the logs: Debug `cmake --build build -j8`, `./build/tests/SpatialCoreTests`, `./build/tests/SpatialCoreUITests`; Release `cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release`, `cmake --build build-release -j8`, `ctest --test-dir build-release/tests --output-on-failure`, `ctest --test-dir build-release/tests -N | grep 'ui:'`, then `./build-release/tests/SpatialCoreTests "[rt]"` (README "Local test gate" and "Realtime-safety check" step 1). Save the last lines of each (case, assertion, test counts and the `[rt]` summary) for the SUMMARY. The case delta against the BASELINE from Task 1 should be 0 (2 removed, 2 added; the removed hidden capture is not run by default); explain any other delta. Check the public-API gate below. Commit as one green commit: `fix(engine)!: the Ambisonics output is plain AmbiX SN3D, no near-field shelf and no max-rE (SpatialCore#32)` with a body that lists R1-R3.
  </action>
  <verify>
    <automated>cmake --build build -j8 --target SpatialCoreTests > /tmp/qlw-build-t2.log 2>&1 && ./build/tests/SpatialCoreTests "[plain-ambix],[ambix],[ambi],[ambi-pin],[sc19],[sc20],[sanitize],[sn3d][golden],[bug03]" | tail -3 && test "$(cat src/Engine/RenderEngine.cpp include/SpatialCore/Engine/RenderEngine.h | grep -v '^[[:space:]]*//' | grep -Ec '[n]fcFilters|[s]moothedNfcDistance|[p]revNfcDistance|[l]astNfcAmbiOrder_|[c]achedMaxrE|[k]NfcReferenceRadius|[a]cnToOrder|[R]enderEngineTestAccess')" = 0 && test "$(grep -v '^[[:space:]]*//' tests/Engine/RealtimeSafetyTests.cpp | grep -Ec '[R]enderEngineTestAccess|[n]fcFilters|ambi-[n]fc-capture|\[[n]fc\]')" = 0 && test "$(awk '/^void RenderEngine::renderAmbisonicsOutput/,/^void RenderEngine::renderDiscreteSurround/' src/Engine/RenderEngine.cpp | grep -v '^[[:space:]]*//' | grep -Ec 'std::(cos|tan)|[p]rocessSample|evalSH \(c, azRad, elRad\) \*')" = 0 && INC_DIFF="$(git diff a43a8aa -- include/)" && test "$(printf '%s\n' "$INC_DIFF" | grep -E '^\+' | grep -vE '^\+\+\+' | grep -vE '^\+[[:space:]]*(//|$)' | wc -l | tr -d ' ')" = 0</automated>
  </verify>
  <done>Gates G1 (identifiers gone from RenderEngine.cpp and RenderEngine.h, baseline 29, now 0), G2 (RealtimeSafetyTests.cpp, baseline 7, now 0), G3 (renderAmbisonicsOutput body, baseline 5, now 0) and G4 (no added non-comment line in include/; baseline 0, a regression guard) all pass. Both `[plain-ambix]` cases are green. Debug SpatialCoreTests and SpatialCoreUITests, Release ctest (with the `ui:` listing) and `[rt]` in Release pass with counts recorded. The register R1-R3 is in the commit body and nothing outside it was touched.</done>
</task>

<task type="auto">
  <name>Task 3: docs, decision record, realtime-safety gate, SUMMARY</name>
  <files>CLAUDE.md, README.md, docs/integration-guide.md, .claude/skills/spatial-audio-dsp/SKILL.md, .planning/codebase/ARCHITECTURE.md, .planning/PROJECT.md, .planning/STATE.md, .planning/ROADMAP.md, .planning/quick/261024-qlw-make-spatialcore-s-ambisonics-output-pla/261024-qlw-SUMMARY.md</files>
  <read_first>
    CLAUDE.md (Engine row, Design Principles)
    README.md lines 10-45 (output table, algorithm list) and the "Local test gate" and "Realtime-safety check" sections
    docs/integration-guide.md "Ambisonics and panning conventions" (~line 226)
    .planning/PROJECT.md Key Decisions table (last rows, DR-18) and .planning/ROADMAP.md Phase 999.5
  </read_first>
  <action>
    Start the realtime-safety tree first, because it takes about an hour and the source is final after Task 2. Configure `build-tsan` with the README "Realtime-safety check" step 2 flags plus the offline flags `-DFETCHCONTENT_FULLY_DISCONNECTED=ON -DFETCHCONTENT_SOURCE_DIR_JUCE="$PWD/build/_deps/juce-src" -DFETCHCONTENT_SOURCE_DIR_CATCH2="$PWD/build/_deps/catch2-src" -DFETCHCONTENT_SOURCE_DIR_MYSOFA="$PWD/build/_deps/mysofa-src"`; build it; then run README steps 3, 4 and 5 verbatim one after another (never in parallel: the timings and timeout scale assume one run at a time), appending each step's `exit=` line and warning count to /tmp/qlw-tsan-summary.txt. Run the whole chain with `run_in_background` (timeout 7200000 ms) and write the docs while it runs. `build-tsan` is git-ignored: never stage it.

    Docs (D-01, D-02; every statement must match the code, in prose that does not echo old identifiers into code comments): CLAUDE.md Engine row and Design Principles gain one statement each: the Ambisonics output-format stream is plain AmbiX SN3D (evalSH x signal x distance and tap gains), with no near-field filter and no max-rE, because IEM-style decoders add their own max-rE; max-rE lives only where SpatialCore decodes (AmbisonicsAlgorithm, `[ambi-pin]`); SpatialCore#32. README.md: the Ambisonics Output row says plain AmbiX with no near-field filter and no max-rE; the line describing the Ambisonics algorithm says its max-rE belongs to the speaker decode, not the output formats. docs/integration-guide.md: under "Ambisonics channel convention" add a short subsection "Ambisonics output stream" stating the above, the behaviour change for existing consumers (far sources no longer boosted at low frequency; orders 1 and above no longer pre-weighted, so a decoder that applies max-rE does it once; overall W level unchanged), that `RenderBlockContext::sampleRate` is no longer read on this path, and that a consumer which decodes the stream itself applies any weighting on its decode side. .claude/skills/spatial-audio-dsp/SKILL.md: one note in the Ambisonics section that the weighted encoding described there is the speaker-decode algorithm, and the output formats are unweighted. .planning/codebase/ARCHITECTURE.md: fix the dispatch line that says the path uses near-field filters, and the state-management line that lists NFC filter state. tests/reference/README.md was done in Task 1. Do not edit historical phase records under .planning/phases/.

    Decision record: .planning/PROJECT.md Key Decisions table, new row `P999.5-D01`: the Ambisonics output-format stream is plain AmbiX SN3D, no near-field shelf, no max-rE on that stream; max-rE stays in SpatialCore's own decode; per-block SH interpolation kept; public API source-compatible; why (shelf inverted against Daniel 2003, max-rE applied twice with IEM decoders; research in .planning/quick/261024-mhh-.../research); references SpatialCore#32 (open until openspatialdelay#22 ships and the pre-release speaker check passes) and the closed SpatialCore#21 (the order-change defect of the very filters now removed; its `[sc19]` tests stay as order-change guards); outcome "Decided 2026-10-24, quick 261024-qlw". Update the footer date. Add one bullet to the Decisions list in .planning/STATE.md in the style of the existing "[Phase 05]" bullets. Commit (docs only, no file under include/, src/ or tests/): `docs: the Ambisonics output is plain AmbiX SN3D (SpatialCore#32)`.

    Wait for the realtime-safety chain. A run passes only when exit is 0 and its warning count is 0 (README). The 6OA and the Ambisonics cases are inside the whole-suite run. If a run fails, diagnose before touching anything: a ThreadSanitizer warning in renderAmbisonicsOutput is a real defect to fix in Task 2's code; a timeout is a load problem, rerun alone.

    SUMMARY (.planning/quick/261024-qlw-make-spatialcore-s-ambisonics-output-pla/261024-qlw-SUMMARY.md, frontmatter in the style of 261024-mhh-SUMMARY.md): result in two sentences; commits (hash and subject); BASELINE vs final counts verbatim (Debug SpatialCoreTests cases and assertions, SpatialCoreUITests, Release ctest line "N tests passed", the `ui:` listing, `[rt]` summary line with zero allocations and locks, build-tsan: the three exit codes, the three warning counts, case counts, skipped count); the RED evidence (path of evidence/red-old-code.txt and the first failing lines of each new case quoted verbatim, naming which check failed at which distance and frequency); the rebaseline register R1-R3 with reasons plus anything added; the pre-existing `[bug03]` failure at HEAD and its fix; whether the generator or the verbatim-move fallback produced ShBackHalfReference.h; anything unexpected. Then tick the SpatialCore follow-up under Phase 999.5 in .planning/ROADMAP.md (change its `- [ ]` to `- [x]` and append "done 2026-10-24, quick task 261024-qlw; SpatialCore#32 stays open until openspatialdelay#22 ships and the speaker check passes"); leave the other two follow-ups unchanged. Commit: `docs(planning): record quick task 261024-qlw, tick the Phase 999.5 SpatialCore follow-up (SpatialCore#32)`. Do not push, do not comment on GitHub, do not bump the version (D-06).
  </action>
  <verify>
    <automated>FIX="$(git log --format=%H --grep='^fix(engine)!: the Ambisonics output is plain AmbiX' -n 1)" && test -n "$FIX" && git diff --quiet "$FIX" -- include src tests && CHANGED="$(git diff --name-only a43a8aa -- include src)" && test "$(printf '%s\n' "$CHANGED" | grep -v 'src/Engine/RenderEngine.cpp\|include/SpatialCore/Engine/RenderEngine.h' | wc -l | tr -d ' ')" = 0 && grep -q 'P999.5-D01' .planning/PROJECT.md && grep -q '\[x\] SpatialCore: Ambisonics output becomes plain AmbiX' .planning/ROADMAP.md && grep -qi 'plain AmbiX' CLAUDE.md README.md docs/integration-guide.md && test "$(cat /tmp/sc-tsan.log /tmp/sc-tsan-ui.log /tmp/sc-tsan-full.log | grep -c 'WARNING: ThreadSanitizer')" = 0 && test "$(grep -n 'exit=' /tmp/qlw-tsan-summary.txt | grep -vc 'exit=0')" = 0 && test "$(grep -n 'exit=' /tmp/qlw-tsan-summary.txt | wc -l | tr -d ' ')" -ge 3 && STATUS="$(git status --short)" && test "$(printf '%s\n' "$STATUS" | grep -v '^??' | wc -l | tr -d ' ')" = 0</automated>
  </verify>
  <done>Docs, the PROJECT.md decision row (referencing SpatialCore#32 and the closed SpatialCore#21), the STATE.md bullet, ARCHITECTURE.md and the skill note are committed with no code drift after the Task 2 commit; build-tsan whole suite, `[tsan]` subset and UI suite are all exit 0 with 0 ThreadSanitizer warnings; the SUMMARY has every count verbatim, the RED evidence, the rebaseline register; the Phase 999.5 SpatialCore follow-up is ticked; five atomic commits on AndrewRahman/ambisonics-plain-ambix, nothing pushed.</done>
</task>

</tasks>

<threat_model>
## Trust Boundaries

| Boundary | Description |
|----------|-------------|
| host audio thread to RenderEngine::renderBlock | the render loop runs under the host's realtime deadline; any allocation, lock or log is a dropout |
| consumer plugin to RenderEngine public API | consumers (OpenSpatialDelay, OpenSpatialPanner) compile against the header; an API break reaches a shipping plugin |
| test suite to the claim "nothing else changed" | rebaselining tests is where a regression can be hidden |

## STRIDE Threat Register

| Threat ID | Category | Component | Severity | Disposition | Mitigation Plan |
|-----------|----------|-----------|----------|-------------|-----------------|
| T-QLW-01 | Denial of Service | renderAmbisonicsOutput per-sample loop (up to 32 objects x 49 channels) | high | mitigate | one stack accumulator loop, no new members; `[rt]` trap in Release (0 allocations, 0 locks) and build-tsan whole suite plus `[tsan]` subset with 0 warnings (Task 2 gate, Task 3) |
| T-QLW-02 | Tampering | rebaselined tests hide a regression | high | mitigate | register R1-R3 with reasons; `[ambi][maxre][d10]`, `[ambi-pin]` and the SH golden tests untouched (D-02); new tests compare to external scipy values at 1e-4; whole suite must otherwise pass unchanged; `git diff` of tests reviewed for tolerance changes outside R1 |
| T-QLW-03 | Tampering | public header change breaks a consumer build | high | mitigate | gate G4: no added non-comment line in include/; `sampleRate`, `ambiOrder` and every public declaration stay; only private members and a private friend declaration are removed (D-04) |
| T-QLW-04 | Repudiation | red evidence not reproducible | medium | mitigate | red output committed as evidence/red-old-code.txt with the old-code commit hash a43a8aa recorded in the SUMMARY |
| T-QLW-05 | Information Disclosure | none: no new input, file or network path | low | accept | nothing parsed or exposed by this change |
| T-QLW-SC | Tampering | package-manager installs | high | accept | none in this plan: no pip, npm or cargo; the TSan tree configures offline from build/_deps; the generator wrapper only aliases an existing scipy function |
</threat_model>

<verification>
- `[plain-ambix]` red on a43a8aa (evidence committed), green after Task 2.
- Full local gate on the fix tree: Debug SpatialCoreTests and SpatialCoreUITests, Release `ctest --test-dir build-release/tests` with 16 `ui:` tests listed, `[rt]` in Release.
- Realtime-safety check (README): build-tsan `[tsan]`, UI suite and whole suite, each exit 0 and 0 warnings.
- G1-G4 grep and diff gates pass; `[bug03]` passes.
- Orchestrator-only afterwards: push, comment on SpatialCore#32 (stays open).
</verification>

<success_criteria>
Every locked decision D-01 to D-05 is implemented and evidenced; the new golden and distance tests fail on the old code and pass on the new; the rebaseline register accounts for every test that pinned the old behaviour; gate counts, red evidence and the register are recorded verbatim in the SUMMARY; ROADMAP Phase 999.5 SpatialCore follow-up is ticked; #32 is left open.
</success_criteria>

<output>
Create `.planning/quick/261024-qlw-make-spatialcore-s-ambisonics-output-pla/261024-qlw-SUMMARY.md` when done.
</output>
