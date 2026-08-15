# Requirements: SpatialCore

**Defined:** 2026-08-09
**Rewritten:** 2026-08-10 — re-derived against the real code branch, with all 14 open GitHub issues triaged
**Milestone:** v1 — "OpenSpatialDelay ships on SpatialCore as a submodule with zero regressions"
**Core Value:** A Spatial Media Lab plugin author gets production-grade spatial rendering by linking one library, so the only audio code they write is their own effect.

Source intel: `.planning/intel/requirements.md`, `.planning/codebase/` (re-mapped 2026-08-10 on
branch `gsd-remap`), and the GitHub issue trackers of both this repo and
`Spatial-Media-Lab/OpenSpatialDelay`.

---

## Why this document was rewritten

The 2026-08-09 version was derived from a codebase map generated against a branch that was
missing 42 commits of work. Verified corrections now folded in:

| 2026-08-09 claim | Verified 2026-08-10 |
|---|---|
| "~5% test coverage; binaural, HRTF, convolver, OSC, Ambisonics untested" | Suite **builds and passes: 144 TEST_CASEs, 1585 assertions**, 16 files covering Core, Algorithms, IO, OSC, Trajectory, Binaural, Engine |
| "No `.sofa` file exists anywhere in the tree — DATA-01 unstarted" | 5 real HDF5 SOFA files, 1.2–36.6 MB, LFS-tracked, with a CI guard against pointer stubs |
| JUCE 8 | JUCE **9.0.0** (`CMakeLists.txt:33`) |
| 22 output formats, 13 layouts | **23** formats, **15** layouts |
| Existing `src/` is "partial work, treat as unstarted" | Extraction is substantially complete and test-covered; remaining work is **named defects**, not re-implementation |
| Issue refs `#50/#89/#96/#131` unresolvable | Real issues in `Spatial-Media-Lab/OpenSpatialDelay` (tracker runs to #235) |

*Corrected 2026-08-11: the earlier pair in the row above was a mapper miscount propagated by
commit `f0b14f4`; the tree has been unchanged since `149d50d` (2026-07-05). See API-04.*

The user's ruling that "the tree is partial until proven otherwise" has now been **discharged by
evidence**: the proof was run. Requirements below are scoped to what execution showed is missing.

## Consumer ruling (2026-08-10)

**OpenSpatialDelay is the v1 consumer.** It is the plugin SpatialCore was extracted from, it ships
publicly at v1.0.0, and it is therefore the only tree that can prove zero regressions.
OpenSpatialPanner is early-stage; its issues are triaged below but do not shape v1 phases.

**Verified:** OSD does **not** yet consume SpatialCore. Both checkouts
(`~/conductor/repos/openspatialdelay`, `…-v1`) carry only a JUCE submodule; SpatialCore appears
solely in planning docs and one archived `Archive/v0.1/PluginProcessor_v0.1.h`. OSD's DSP still
lives in its own `Source/` (14 files), last commit 2026-05-06. The migration is unstarted — which
is exactly what the v1 milestone exists to deliver.

---

## v1 Requirements

### Public Contract Resolution

Frozen under DR-7, gated by DR-2. Three of the four 2026-08-09 open questions turned out to be
doc-counting errors, answerable from the tree without a user decision.

- [x] **API-01**: The public algorithm count is one number across headers and docs
  - **Resolved by evidence: 8.** `grep -l "public SpatializationAlgorithm"` returns 8 concrete
    implementations. OQ-1 ("6 vs 7 vs 8") is closed.

  - Acceptance: `SpatialCore.h` and `AllAlgorithms.h` expose exactly 8; no source doc states a
    different count.

  - **Complete 2026-08-15 (phase 01, plan 03):** `AllAlgorithms.h` pins `NUM_ALGORITHMS = 8` via
    `static_assert` (plan 01-01). `docs/integration-guide.md` — the last source doc still stating
    "7 spatialization algorithms" — corrected in plan 01-03. All five Tier A surfaces (CLAUDE.md,
    README.md, docs/integration-guide.md, and both auto-loading skill files) now state 8 with no
    residual stale count.

- [x] **API-02**: `spatialcore::outputLimiter()` has one defined transfer function
  - There is no `DSP` namespace and no `include/SpatialCore/DSP/` directory — verified:
    `include/SpatialCore/Core/SpatialMath.h:7` opens `namespace spatialcore` and `:100` declares
    `inline float outputLimiter (float x)`, with no intervening namespace (plan-checker Warning 3,
    2026-08-11; same phantom-`DSP/` defect class as D-09).
  - **OQ-2 RESOLVED 2026-08-10 (user decision): keep the shipped `tanh` soft ceiling.**
    Rationale: OpenSpatialDelay shipped publicly at v1.0.0 with this curve, so it is the known
    sound. Changing it during a migration whose gate is "zero regressions" would alter output
    for existing users. The SPEC is amended to describe the implementation, not the reverse.

  - Acceptance: the SPEC's hard-clamp-at-1.2589f language is replaced with the tanh soft ceiling.
    Implementation unchanged. A test pins the curve at, below, and above the ceiling, and for
    non-finite input (returns 0.0f).

  - **Deferred, not discarded:** a selectable hard-clamp mode is recorded as a v2 candidate
    (LIMIT-01, Future Milestones). Adding a mode later is additive and does not break DR-2.

- [x] **API-03**: The HRTF profile count is one number across docs, headers, and shipped data
  - **Resolved by evidence: 5.** `HRTF/` holds exactly 5 `.sofa` files; the identical 5 exist in
    OSD's `HRTF/`, confirming provenance. OQ-3 ("5 vs 6") is closed.

  - Acceptance: CLAUDE.md, `docs/development-roadmap.md`, and the header exposing `profileIndex`
    all state 5.

  - **Complete 2026-08-15 (phase 01, plan 03):** CLAUDE.md already stated 5 (verified, unedited).
    `docs/development-roadmap.md` carries a supersession note pointing to CLAUDE.md/README.md/
    integration-guide.md as current truth; its own "5 profiles" / "5 SOFA files" mentions were
    never wrong. README.md and the spatialcore-architecture skill now carry the D-05 canonical
    two-number sentence (5 SOFA profiles ship; `profileIndex` 0-5, 0 = Simple Woodworth) instead
    of a bare or conflated count.

- [x] **API-04**: Output-format and speaker-layout counts match the tree
  - **New.** `OutputFormat` has **23** values (`OutputFormat.h:9-28`, cross-checked against 23
    `OutputFormat::` rows in `OutputFormatRegistry.cpp`); `SpeakerLayout.h`'s `LayoutID` has **15**
    named layouts plus a `NUM_LAYOUT_DEFS` sentinel (`SpeakerLayout.h:42-45`). Every doc says 22
    and 13.

  - Acceptance: CLAUDE.md, the SPEC, and the integration guide state 23 and 15, or the extra
    entries are documented as internal.

  - **Corrected 2026-08-11:** this requirement previously read 25 / 14. Those numbers came from a
    mapper miscount in `.planning/codebase/ARCHITECTURE.md:107` that `f0b14f4` copied here under a
    "verified from the tree" label; the tree says 23 / 15. See CONTEXT.md D-04.

- [x] **API-05**: CLAUDE.md describes the library that actually exists
  - **New.** Five verified inaccuracies in the project's own context file:
    1. JUCE 8 (is 9.0.0)
    2. SOFA files "embedded as BinaryData" (they load from disk) — **already closed, `87cb7a3`:**
       CLAUDE.md already states runtime `HRTFDatabase::loadFromFile`, no BinaryData compilation
       step, before this phase started.
    3. the component table omits the `Engine/` module entirely — **already closed, `2e3b090`:**
       CLAUDE.md's Architecture table already carries the `Engine` row before this phase started.
    4. format/layout counts wrong
    5. the architecture table's row for `DSP/`, a module directory that does not exist (D-09) —
       found during this phase's discussion, not part of the 2026-08-10 rewrite's original list.

  - Acceptance: CLAUDE.md's Architecture table lists `Engine/`, carries no `DSP/` row, and every
    factual claim in it is reproducible from the tree.

  - Rationale: CLAUDE.md is loaded into every session in this repo. A wrong CLAUDE.md
    mis-steers every future planning pass — this is the root cause of the 2026-08-09 rewrite.

### Extraction Verification

**Reframed.** The 2026-08-09 requirements assumed these were unwritten. The test suite proves
otherwise. What remains is verifying the named gaps, not building the modules.

- [ ] **EXTR-01**: Every public algorithm computes correct gains
  - Acceptance: the existing algorithm tests continue to pass, and the 3D triplet fallback
    (`jassertfalse` → nearest-speaker heuristic in VBAP/VBIP/MDAP) is either eliminated or fails
    loudly rather than silently degrading.

- [ ] **EXTR-02**: Binaural rendering produces measured-correct output
  - Acceptance: already covered by binaural tests running against all 5 real SOFA profiles plus
    the Woodworth fallback. Remaining gap: `PartitionedConvolver.cpp` and `BinauralRenderer.cpp`
    have **no dedicated test files** — they are only exercised indirectly.

- [ ] **EXTR-03**: Layouts, format registry, and Ambisonics codec return real data
  - Acceptance: all 23 `OutputFormat` entries resolve; all 15 layouts return populated channel
    indices and LFE placement; Ambisonics encodes/decodes to order 6.

- [ ] **EXTR-04**: ADM-OSC and the trajectory engine drive real object motion
  - Acceptance: covered by existing OSC and Trajectory tests. Bounds checking at
    `ADMOSCReceiver.cpp:43` was audited and confirmed correct — the 2026-08-09 RTSF-04 concern
    is **closed as a non-finding**.

- [ ] **EXTR-05**: UI components render and interact for real
  - Acceptance: `SpatialMapComponent` renders objects, rings, and elevation-as-opacity and
    supports drag; no component holds a concrete processor pointer (DR-16).

  - Note: all 10 `src/UI/*.cpp` files are untested, by design — UI lives in the separate
    `SpatialCoreUI` target which `SpatialCoreTests` does not link.

### Packaging and Consumability

- [ ] **DATA-01**: HRTF data resolves through a lookup chain, with an embedded set that cannot fail
  - **OQ-6 RESOLVED 2026-08-10 (final): build the shared-folder lookup chain now, ship v1 with
    the embedded set active, flip the default when a signed installer exists.**

  - **Target architecture** (user proposal, adopted): the 5 profiles live on disk once per machine
    in a shared location, and every SML plugin references them rather than carrying its own copy.

    - macOS: `/Library/Application Support/Spatial Media Lab/HRTF/`
    - Windows: `%ProgramData%\Spatial Media Lab\HRTF\`
  - **Why this is better than what OSD does today** (OSD embeds all 5 raw via
    `juce_add_binary_data(HRTFData ...)` at its `CMakeLists.txt:50-58`, shipping a 64 MB VST3,
    64 MB AU, and 73 MB macOS zip):

    1. **User-supplied HRTFs become possible.** Anyone with their own measured SOFA file drops it
       in the folder and it appears. Embedding makes this impossible without a rebuild. For a
       spatial audio library this is close to a feature requirement.

    2. Profiles can be fixed or added without rebuilding and re-shipping every plugin.
    3. Suite-wide size: 5 plugins x 2 formats x 64 MB is ~640 MB embedded, vs 58 MB once.
    4. 58 MB of generated BinaryData arrays slows every clean build of every consumer.
  - **Resolution chain** (`HRTFDatabase`, in order): shared platform folder -> embedded
    BinaryData fallback -> loud error. Never a silent failure to render.

  - **v1 ships with all 5 embedded.** Rationale: the shared folder depends on a signed installer
    that does not exist yet, and OSD ships drag-and-drop today. Cutting OSD's bundle before the
    installer lands would mean an existing user who updates by drag-and-drop silently loses 4 of
    5 profiles — a regression, in a milestone whose gate is zero regressions. Migrating OSD and
    repackaging OSD are separately risky and must not ride together.

  - Acceptance: `juce_add_binary_data(HRTFData ...)` moves into SpatialCore over the 5
    `HRTF/*.sofa` files; `HRTFDatabase` gains `loadFromBinaryData()` **and** the shared-folder
    lookup; a `SPATIALCORE_EMBED_ALL_HRTF` CMake option selects all-5 (default, v1) vs
    default-profile-only. A freshly cloned consumer renders through any of the 5 profiles with no
    install step. `loadFromFile()` / `loadFromMemory()` survive as secondary APIs.

  - **The flip is a build flag, not a redesign.** When the signed installer lands, set
    `SPATIALCORE_EMBED_ALL_HRTF=OFF` — embedding drops to `mit_kemar_large_pinna` (1.1 MB) and the
    installer supplies the rest. No SpatialCore code changes. Tracked as SUITE-01.

  - **Rejected:** convert-then-embed (min-phase/int16 to ~4 MB). It introduces DSP work that can
    change the sound during a zero-regressions migration, to solve a size problem the shipping
    product proves it does not have. `convertToMinPhase()` returns to out-of-scope.

  - The build already fails loudly on LFS pointer stubs via a CI guard. That half is done.
  - Satisfies DR-5 as written, and leaves a designed path off it.

- [ ] **DATA-02**: `SMLLookAndFeel` has the font BinaryData it needs
  - **Verified done.** `fonts/*.ttf` are compiled in as JUCE BinaryData. Retained only to confirm
    rendering on a host with no SML font installed.

- [ ] **INTG-01**: A consumer plugin can submodule, link, and build against SpatialCore
  - **Corrected.** The 2026-08-09 acceptance named only
    `target_link_libraries(... PRIVATE SpatialCore)`. There are **two** targets: `SpatialCore`
    (DSP-only) and `SpatialCoreUI` (GUI). OSD needs both, so the old criterion would have passed a
    harness that OSD could not actually use.

  - Acceptance: a minimal harness adds SpatialCore as a submodule, links **both** targets,
    includes only `<SpatialCore/SpatialCore.h>`, resolves HRTF data per DATA-01, and builds clean
    in Release on macOS. From a clean clone, `cmake -B build -DCMAKE_BUILD_TYPE=Release`,
    `cmake --build build --config Release`, and the test binary all succeed.

- [ ] **INTG-02**: `engineComputesGains` does not silently no-op for a migrating consumer
  - **New.** SC-13 (issue #14, closed) moved panning-gain computation into `RenderEngine`, but
    behind an **opt-in** `engineComputesGains` flag. A consumer that doesn't set it still gets the
    old pass-through path where `RenderEngine` only reads `objChannelGains` — the exact bug #14
    was filed for.

  - Acceptance: the flag defaults safely or the engine fails loudly when gains are neither
    computed nor supplied; the integration guide states which mode a consumer must choose.

### Realtime Safety

Breaches of locked rule DR-1. Two of the 2026-08-09 items survived audit, one widened, one closed.

- [ ] **RTSF-01**: `BinauralRenderer` cannot allocate on the audio thread in a Release build
  - Tracked by **SpatialCore#19** (filed 2026-08-10).
  - **Widened — there are two sites, not one.**
    - `src/Binaural/BinauralRenderer.cpp:196-209` (`renderSourceBuffers`)
    - `src/Binaural/BinauralRenderer.cpp:134,158-163` (`updateSourceHRIR`), reached from
      `src/Engine/RenderEngine.cpp:178` in the per-block HRIR update loop

  - Both use `jassertfalse` — a **no-op in Release** — guarding a `resize()` that then executes
    unconditionally in shipping builds.

  - Acceptance: neither site can allocate in Release. Prefer one shared guarded helper over two
    copies of the fallback. Verified in a Release build.

- [ ] **RTSF-02**: TrajectoryEngine final positions are safe to read from the audio thread
  - Tracked by **SpatialCore#18** (filed 2026-08-10).
  - **Confirmed real.** `TrajectoryEngine.h:30` documents a
    60 Hz-timer-writes / audio-thread-reads contract, but only `active_` is atomic
    (`TrajectoryEngine.h:106`). `finalAz_`, `finalEl_`, `finalDist_` (lines 103-106) are plain
    `float[]`, written at `src/Trajectory/TrajectoryEngine.cpp:76-90,143-145`. A data race under
    the C++ memory model.

  - Acceptance: atomic floats or an atomically-swapped struct. Clean under thread sanitizer.

- [ ] **RTSF-03**: `HRTFDatabase` access to the libmysofa handle is thread-safe
  - Acceptance: `setProfile()` from a UI timer during audio processing does not race on
    `easyHandle` or `loaded`.

- [ ] ~~**RTSF-04**: `ADMOSCReceiver` rejects out-of-range object indices~~
  - **CLOSED as a non-finding.** `src/OSC/ADMOSCReceiver.cpp:43` already validates
    `if (objNum < 1 || objNum > MAX_SOURCES) return;` on every `/adm/obj/` and `/osd/obj/` path.
    No out-of-bounds write exists. The 2026-08-09 requirement was written from a stale map.

- [ ] **RTSF-05**: The whole audio path is verified lock-free and realtime-safe
  - Acceptance: thread sanitizer plus an allocation detector over every function reachable from
    `processBlock`, reporting zero allocations, locks, and races.

  - Scope correction: the `DBG()` calls at `BinauralRenderer.cpp:127` and
    `HRTFDatabase.cpp:35,58,72` were audited and are **configuration-thread only**, and `DBG`
    compiles out in Release regardless. Not a violation — removed from scope.

  - `SharedFFTCache` was audited and **is** correctly spinlock-guarded
    (`PartitionedConvolver.cpp:16`) and called only from `prepare()`-time paths. See VERIFY-02 for
    the residual multi-instance question.

### Inherited and Filed Defects

New category. These are real bugs in code SpatialCore now owns.

- [ ] **BUG-01**: DirectBinaural produces no elevation cue and cannot distinguish front from rear
  - `src/Algorithms/DirectBinauralAlgorithm.cpp:26` — `lateral = sinAz * cosEl` collapses to 0 at
    elevation ±90° *and* at azimuth 0°/180°. Both the Woodworth ITD (line 30) and the ILD
    (line 34) derive from this single value, so neither carries elevation.

  - Tracked by **SpatialCore#15**. The issue states this code was extracted verbatim, so
    **OpenSpatialDelay carries the identical defect** — fixing it here fixes both.

  - Acceptance: a source at elevation +90° is measurably distinguishable from one at 0°, and
    azimuth 0° from 180°.

- [ ] **BUG-02**: Audio artifacts at buffer sizes below 256 samples
  - `Spatial-Media-Lab/OpenSpatialDelay#234` — **still open** — is cited in live convolver code at
    `src/Binaural/PartitionedConvolver.cpp:120-124`. SpatialCore inherited an unfixed OSD defect
    during extraction.

  - Acceptance: a regression test renders at 32/64/128 sample blocks without artifacts.
  - Directly blocks the v1 "zero regressions" gate: OSD ships to users who run small buffers.

- [x] **BUG-03**: Cross-repo issue references in code comments are qualified
  - **Full inventory** (repo-wide sweep during Phase 1 planning, superseding the earlier four-site
    sample): **39 occurrences across 38 lines in 17 files**, spanning **15 distinct issue numbers**,
    including sites under `tests/`. Per-file table: see Plan 02's SUMMARY
    (`.planning/phases/01-documentation-truth-contract-freeze/01-02-SUMMARY.md`). All OSD issues
    cited in bare `#N` form, which resolves to the wrong tracker.
  - **Motivating finding:** at one site, `#2` resolves in **both** trackers to different issues —
    `Spatial-Media-Lab/OpenSpatialDelay#2` ("User Presets folder missing after build") vs
    `AndrewRahman/SpatialCore#2` ("[OpenSpatialPanner] Preset system") — so the bare form there is
    not merely ambiguous, it is actively wrong.

  - Acceptance: each rewritten as `Spatial-Media-Lab/OpenSpatialDelay#N`. Comment-only, low risk.

### Verification Tasks

Open issues that close with evidence rather than a code change.

- [ ] **VERIFY-01**: Ambisonics channel-order and normalisation convention is stated
  - **SpatialCore#11.** Inspect `AmbisonicsCodec` encode/decode against ACN ordering and SN3D
    normalisation; record the answer in code and docs, or correct it if mixed.

- [ ] **VERIFY-02**: `SharedFFTCache` is safe across concurrent plugin instances
  - **SpatialCore#12.** The spinlock is present and correct; what is unverified is behaviour with
    several instances in one host process.

  - **This is the highest-value item in v1.** It is the same failure mode as two closed OSD bugs:
    `OpenSpatialDelay#96` ("Two plugin instances on same track causes buzzing and Reaper crash")
    and `#131` ("CoreGraphics crash with multiple plugin instances loaded simultaneously"). Those
    were fixed in OSD; the fix now lives in SpatialCore. If it regressed during extraction, the
    migration reintroduces two crashes into a shipping plugin.

  - Acceptance: a concurrent-instance stress test, plus regression tests reproducing OSD#96 and
    OSD#131 against the old behaviour.

  - Note: the cache is create-once-never-destroy by design — a deliberate permanent allocation,
    not a leak bug. Document it as such.

- [ ] **CI-01**: SpatialCore is CI-verified against JUCE 9.0.0
  - **SpatialCore#9.** SpatialCore's own CI builds on Linux only. The tree is pinned to JUCE 9.0.0
    while CLAUDE.md claims 8 — the mismatch must be settled in CI, not prose.

  - Acceptance: a green macOS CI run building against JUCE 9.0.0 and running the Catch2 suite.
  - Scope note: this supersedes the 2026-08-09 "Windows/Linux CI is out of scope" line only for
    macOS + JUCE 9. Cross-platform matrices remain out of scope.

### Test Coverage

- [ ] **TEST-01**: Coverage tooling exists and the untested modules are covered
  - **Reframed.** No coverage tooling is configured anywhere in the build, so no percentage can be
    measured — the old "~5%" figure was invented, and OQ-4 ("set a coverage target") cannot be
    answered until measurement exists.

  - Acceptance: coverage instrumentation is wired into CMake and reports a real number. The
    modules confirmed to have **no dedicated tests** get them:
    `src/Binaural/PartitionedConvolver.cpp` and `src/Binaural/BinauralRenderer.cpp`.

  - Regression tests required for: `OpenSpatialDelay#50`, `#89`, `#96`, `#131` (all closed there,
    all in code SpatialCore now owns) and `#234` (open — see BUG-02).

  - `src/UI/*.cpp` is excluded by design: UI is in the `SpatialCoreUI` target, which
    `SpatialCoreTests` does not link. Covering it needs a separate UI test target — post-v1.

---

## Open Questions

| ID | Question | Status |
|----|----------|--------|
| OQ-1 | Algorithm count 6 / 7 / 8 | **Closed — 8**, verified from tree |
| OQ-2 | `outputLimiter` hard clamp vs tanh | **Closed 2026-08-10 — keep tanh.** Hard clamp deferred to v2 as LIMIT-01 |
| OQ-3 | HRTF profile count 5 vs 6 | **Closed — 5**, verified from tree |
| OQ-4 | Test coverage target | **Blocked** — no coverage tooling exists to measure against |
| OQ-6 | HRTF packaging | **Closed 2026-08-10 — build the shared-folder lookup chain, ship v1 embedded, flip via CMake flag when an installer exists.** See DATA-01 |

---

## Issue Triage — all 14 open SpatialCore issues

Every open issue is accounted for. None is silently dropped.

### In v1 (5)

| Issue | Requirement | Why it's v1 |
|-------|-------------|-------------|
| #12 SharedFFTCache concurrency | VERIFY-02 | Same failure mode as OSD#96 and OSD#131 — two crashes in a shipping plugin |
| #15 Binaural elevation collapse | BUG-01 | Extracted verbatim; OSD carries the identical defect |
| #11 Ambisonics ACN/SN3D | VERIFY-01 | Cheap verification; OSD uses Ambisonics output |
| #9 CI vs JUCE 9.0.0 | CI-01 | OSD must build against the same JUCE the tree is pinned to |
| #14 (closed) RenderEngine gains | INTG-02 | Closed, but the opt-in flag leaves the trap armed for migration |

### Deferred to v2 (9)

Not needed for OSD parity. OSD already implements #4 and #5 internally and fills
`RenderSources::distGainPerSample[]` itself, so migration does not require them in SpatialCore.

| Issue | Reason for deferral |
|-------|---------------------|
| #3 MAX_SOURCES 12→32 | OSD uses 12. Prerequisite: `DopplerVelocity.h:17`'s independent `kMaxObjects = 12` must move in lockstep or arrays silently desync |
| #4 Distance attenuation curves | Consumer-side today; OSD keeps its own through migration |
| #5 Air absorption | Same |
| #6 Global rotate-all transform | OSP feature |
| #7 Per-object RMS visualisation | OSP feature |
| #10 Spread API | Needs a `computeGains()` signature change — a DR-2 major-version break. Not a v1 add |
| #13 PresetBrowser API docs | OSP Phase 8 |
| #16 Metres as a real unit | New design work; changes parameter ranges and invalidates saved automation |
| #17 Doppler pitch-shift path | Real DSP work; OSD keeps its own pitch-shift engine consumer-side |
| #2 OSP preset system | OSP-level feature |

---

## Traceability

| Requirement | Source | Phase | Status |
|-------------|--------|-------|--------|
| API-01 | OQ-1, resolved by re-map | 1 | Complete |
| API-02 | OQ-2 (resolved: keep tanh) | 1 | Complete |
| API-03 | OQ-3, resolved by re-map | 1 | Complete |
| API-04 | Re-map (23 formats / 15 layouts) | 1 | Complete |
| API-05 | Re-map (CLAUDE.md inaccuracies) | 1 | Complete |
| EXTR-01 | REQ-extract-algorithms | 2 | Largely verified by tests |
| EXTR-03 | REQ-extract-speaker-layouts | 2 | Largely verified by tests |
| EXTR-02 | REQ-extract-binaural-rendering | 3 | Largely verified by tests |
| DATA-01 | REQ-embed-hrtf-binarydata | 3 | OQ-6 resolved: lookup chain + embedded default |
| EXTR-04 | REQ-extract-adm-osc-and-trajectory | 4 | Largely verified by tests |
| EXTR-05 | REQ-extract-ui-rendering | 4 | Pending |
| DATA-02 | REQ-smllookandfeel-font-binarydata | 4 | Verified done |
| RTSF-01 | SpatialCore#19 — 2 sites | 5 | Pending |
| RTSF-02 | SpatialCore#18 | 5 | Pending |
| RTSF-03 | CONCERNS.md | 5 | Pending |
| ~~RTSF-04~~ | — | — | **Closed, non-finding** |
| RTSF-05 | REQ-verify-lockfree-realtime-safe | 5 | Pending |
| BUG-01 | SpatialCore#15 | 3 | Pending |
| BUG-02 | OpenSpatialDelay#234 | 3 | Pending |
| BUG-03 | CONCERNS.md | 1 | Complete |
| VERIFY-01 | SpatialCore#11 | 2 | Pending |
| VERIFY-02 | SpatialCore#12 | 5 | Pending |
| CI-01 | SpatialCore#9 | 6 | Pending |
| INTG-01 | Build gate + v1 metric | 6 | Pending |
| INTG-02 | SpatialCore#14 residual | 6 | Pending |
| TEST-01 | REQ-test-port-and-coverage | 6 | Reframed; OQ-4 blocked |

**Coverage:** 25 active v1 requirements, all mapped. 1 closed as non-finding (RTSF-04).
14 open issues triaged: 4 into v1 requirements, 9 deferred to v2, 1 (#14) closed with residual
work captured as INTG-02. Unmapped: 0.

---

## Future Milestones

### v2 — A second plugin is built on SpatialCore from scratch

OpenSpatialPanner is the presumptive v2 consumer. The 9 deferred issues above are its
gate list, alongside:

| ID | Requirement |
|----|-------------|
| REQ-plugin-openspatialpanner | OpenSpatialPanner — object-based panner with energy distribution |
| REQ-plugin-openspatialreverb | OpenSpatialReverb — algorithmic reverb with spatial reflections |
| REQ-plugin-openspatialgranular | OpenSpatialGranular — granular synthesis with 3D grain positioning |
| REQ-plugin-openspatialchorus | OpenSpatialChorus — chorus/flanger with spatially distributed voices |
| LIMIT-01 | Selectable hard-clamp limiter mode alongside the default tanh soft ceiling. Deferred from OQ-2 2026-08-10 — additive, does not break DR-2 |
| SUITE-01 | Signed installers that populate the shared HRTF folder, then `SPATIALCORE_EMBED_ALL_HRTF=OFF`. The SpatialCore side ships in v1 (DATA-01); this is the installer + the flag flip. Deferred from OQ-6 2026-08-10 |

### v3 — Public release under the org with docs and a tagged v1.0

| ID | Requirement |
|----|-------------|
| REQ-create-org-repo | Create `github.com/Spatial-Media-Lab/SpatialCore` with squashed history |
| REQ-set-dual-license | Land the GPL-3.0 + commercial dual license |
| REQ-tag-v1-0-0 | Tag v1.0.0 (gated by DR-4: full test suite passes first) |
| REQ-osd-repoint-submodule-to-org | Point OSD's submodule at the public org repo (external) |

## Out of Scope

| Feature | Reason |
|---------|--------|
| OpenSpatialDelay repository work (7 × REQ-osd-*) | Lives in another repo. External dependency of v1 |
| Windows / Linux CI matrices | v1 verifies macOS only. CI-01 covers macOS + JUCE 9 |
| Custom / vendor-specific speaker layouts | Only the 14 built-in layouts |
| Replacing the JUCE FFT dependency | Disproportionate to v1 |
| `convertToMinPhase()` integration | Implemented but unused. Considered for HRTF compaction under OQ-6 and rejected — no size problem to solve |
| Configurable convolver crossfade duration | Hardcoded `kCrossfadeBlocks = 4` works |
| UI test target for `src/UI/*.cpp` | Needs a separate target linking `SpatialCoreUI`. Post-v1 |

---
*Requirements defined: 2026-08-09*
*Rewritten 2026-08-10 against branch `gsd-remap` after the original map was found to describe the wrong branch*
