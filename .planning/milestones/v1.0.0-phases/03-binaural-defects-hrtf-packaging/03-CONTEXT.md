# Phase 3: Binaural Defects & HRTF Packaging - Context

**Gathered:** 2026-10-04
**Status:** Ready for planning

<domain>
## Phase Boundary

The binaural path renders correct spatial cues, and a consumer gets HRTF data by linking rather
than by hand-copying a directory. Requirements: EXTR-02, DATA-01, BUG-01, BUG-02.

Concretely, this phase delivers:

1. **BUG-01 (SpatialCore#15):** the Simple (Woodworth) binaural path gains elevation and
   front/back cues; overhead ≠ ahead and front ≠ back are asserted on both binaural paths.
2. **BUG-02 (`Spatial-Media-Lab/OpenSpatialDelay#234`):** SpatialCore's convolver is proven
   artifact-free at 32/64/128-sample and irregular block sizes.
3. **DATA-01:** SpatialCore embeds the 5 SOFA profiles as BinaryData, resolves each profile through
   shared folder → embedded → reported error, and owns profile switching end to end behind one
   engine call.
4. **EXTR-02:** dedicated test files for `PartitionedConvolver` and `BinauralRenderer`.
5. Click-free profile switching while audio runs (roadmap criterion 4).

The resolution-chain architecture, the `SPATIALCORE_EMBED_ALL_HRTF` option (default ON), the shared
folder paths, and the rejection of convert-then-embed are **already locked** (OQ-6, 2026-08-10;
`.planning/REQUIREMENTS.md` DATA-01). This discussion did not reopen them.

</domain>

<decisions>
## Implementation Decisions

### Simple-mode elevation and front/back fix (BUG-01, SpatialCore#15)
- **D-01:** Simple (Woodworth) mode gains lightweight **spectral cues**: a rear high-cut / head-shadow
  filter for sources behind the listener and a pinna-style elevation cue for sources above and
  below. Simple mode stays HRTF-free and cheap. Rejected: front/back only (leaves criterion 1
  unmet), routing Simple through the embedded KEMAR profile (CPU cost, Simple stops being its own
  mode), and leaving Simple mode left/right-only. Note: `computeBinauralGains` today returns gains
  and delays only, so the filter state lives on the engine's simple-binaural render path (or a
  new per-source filter), not in the stateless algorithm (Design Principle: stateless algorithms).
  — **Reversibility:** costly — this is an audible change to a shipping product; reverting after
  OSD releases on it changes users' sound a second time.
- **D-02:** **One behaviour, no legacy flag.** OSD does not get a switch back to the flat Simple
  sound. This follows the Phase 2 VBIP precedent (P2-D14): a defect fix that changes the sound is
  shipped with an OSD release note, not hidden behind an option. Add the release-note item to the
  OSD-side follow-ups in PROJECT.md External Dependencies.
- **D-03:** SpatialCore#15 closes on an **automated test**: on both the Simple path and the HRTF
  path, a source at elevation +90° is measurably different from 0°, and azimuth 0° from 180°.
  This replaces the deliberately unasserted DirectBinaural elevation/front-back cases in
  `tests/Algorithms/PanningLawTests.cpp`. A headphone listening check goes to the ROADMAP
  backlog next to 999.1/999.2. It is **not** a phase gate.

### Profile switching ownership (DATA-01, criterion 3 + 4)
- **D-04:** **SpatialCore owns "switch to profile N" behind one call** on `RenderEngine` (e.g.
  `setHRTFProfile(int)`, name at planner's discretion). It runs the resolution chain, loads into
  the inactive renderer, and performs the click-free double-buffered swap. Consumers delete their
  glue. The reference for what moves into the library is OSD's
  `OpenSpatialDelayProcessor::loadHRTFProfileIntoRenderer` / `loadHRTFProfile` /
  `timerCallback` (OSD `Source/PluginProcessor.cpp` ~1495-1600). The existing escape hatches
  (`getBinauralRenderer`, `getPrepareRendererIndex`, `swapActiveRenderer`) stay public (D-02 from
  Phase 1/2: leaf classes stay public), so existing consumers are unaffected. This is an additive
  API (minor bump, DR-6).
  — **Reversibility:** costly — once OSD and OpenSpatialPanner delete their glue and call this,
  moving it back means re-adding consumer code in two repos.
- **D-05:** **Background loading.** The call returns immediately and never blocks the UI. The SOFA
  load (SADIE is 36 MB) runs off the message thread and off the audio thread. Audio keeps playing
  the current profile, then crossfades (existing `kRendererXfadeBlocks` machinery) when the new
  one is ready. If the user requests profiles faster than they load, the most recent request
  wins (Claude's discretion on the mechanism). The loader writes only the **inactive** renderer.
  It must not widen the existing `HRTFDatabase` `easyHandle` race (RTSF-03, owned by Phase 5).
- **D-06:** **A failed load never causes a dropout.** If a profile cannot be resolved anywhere, audio
  keeps the current profile (or Simple mode if nothing is loaded yet), and SpatialCore exposes a
  readable status (e.g. "profile 3 failed: file missing") that the plugin can show in its UI. This
  is how the roadmap's "loud error, never silent" is implemented: loud to the plugin and the user,
  not a mute. The status must be readable without the audio thread logging, allocating or locking
  (DR-1).
- **D-07:** **Profile numbering is OSD's existing numbering**, so saved sessions reopen on the same
  profile: 0 = Simple (Woodworth), 1 = `sadie_d2_ku100`, 2 = `cipic_subject_003`,
  3 = `hutubs_pp2`, 4 = `bernschuetz_ku100`, 5 = `mit_kemar_large_pinna` (source: OSD
  `loadHRTFProfileIntoRenderer` switch; `BinauralRenderer::isSimpleMode()` = profile 0; KEMAR LF
  shelf keyed on index 5 at `src/Binaural/BinauralRenderer.cpp:106`).
  — **Reversibility:** one-way — the indices are persisted in OSD session state, and renumbering
  silently changes the profile a saved session reopens with.

### Shared-folder rules (DATA-01, criterion 3)
- **D-08:** **Same filename wins.** A file in the shared folder whose name matches (case rule: see D-18) a built-in
  profile file (e.g. `hutubs_pp2.sofa`) replaces that profile. Any other `.sofa` in the folder is
  ignored in v1. User-supplied extra profiles are deferred.
- **D-09:** **A broken shared file falls back to embedded and reports it.** If a same-name file
  exists but fails to load (corrupt, or a Git LFS pointer stub), SpatialCore uses the embedded copy
  and reports a status ("shared file X unreadable, used built-in") through the same channel as D-06.
  A missing **folder** still falls back silently, as the roadmap says.
- **D-10:** **System folder only**: `/Library/Application Support/Spatial Media Lab/HRTF/` (macOS),
  `%ProgramData%\Spatial Media Lab\HRTF\` (Windows). No per-user `~/Library` folder in v1.
- **D-11:** The folder is checked **on every profile load** (not cached once at startup), so a newly
  dropped file takes effect on the next switch without restarting. The Windows path is
  implemented but unverified (v1 verification is macOS-only, PROJECT.md Out of Scope).

### Small-block correctness (BUG-02, `Spatial-Media-Lab/OpenSpatialDelay#234`, criterion 2)
- **D-12:** **Automated match test is the bar.** Render identical input through the binaural path
  at 32, 64 and 128 samples, plus irregular/variable host block sizes (e.g. 37, alternating sizes),
  and require output that matches a 512-sample reference render to within float rounding. Runs in
  the suite every time. No DAW listening gate.
- **D-13:** **Comment on OSD#234, leave it open.** When Phase 3 passes, post a comment on
  `Spatial-Media-Lab/OpenSpatialDelay#234` stating that SpatialCore's convolver is proven clean at
  32-128 and that OSD's delay line and pitch shifter (the issue's other two named causes) are
  still unchecked. Posting is an outward action, so confirm the exact text with the user at the
  time (CLAUDE.md: outward-facing actions are confirmed first).

### Cross-profile level measurement (EXTR-02, added 2026-10-04 follow-up)
- **D-14:** **Measure now, change later.** Phase 3 adds a test that measures each of the 5 built-in
  profiles' **perceived loudness** (pink noise rendered through each profile and averaged over
  directions, ear-weighted loudness in the K-weighted / ITU-R BS.1770 sense) and reports the
  spread. **No change to the built-ins' level in Phase 3**, so OSD's sound is unchanged. The
  current rule (`src/Binaural/BinauralRenderer.cpp:43-79`) equalises raw broadband energy over
  6 reference directions to `targetRMS = 1/sqrt(irLen)`. It is not frequency-weighted, and no
  test today checks that the built-ins actually land at the same level. The test's pass/fail
  tolerance is set **after** the first measurement and approved by the user. Until then it
  records and prints the numbers. The measured spread feeds the future loudness standard
  (Deferred → custom HRTFs).

### Post-research decisions (2026-10-04, after 03-RESEARCH.md)
- **D-15:** **Simple ↔ HRTF switching is smooth too.** Profile 0 (Simple) counts as a profile for
  criterion 4. Switching between Simple and any HRTF profile crossfades the two render paths, so no
  click. The dual-path crossfade sits behind a new **default-off** `RenderBlockContext` flag, so
  existing consumers are unaffected until they opt in.
- **D-16:** **SADIE ITD wrap: pin, don't fix.** The 64-sample ITD delay line wraps SADIE's absolute
  delays (up to 123 samples). Phase 3 adds a characterisation test locking today's behaviour at
  44.1 and 48 kHz and files a GitHub issue for 88.2 kHz and above. No change to shipped timing.
  (The KEMAR audio-thread resize at `BinauralRenderer.cpp:162` is a separate defect and **is** fixed
  in Phase 3.)
- **D-17:** **Upgrade libmysofa to v1.3.5** for its malformed-input hardening, since Phase 3 makes
  user-supplied SOFA files loadable. If the bump moves any golden checksum, the plan must surface
  the before/after numbers and get user sign-off before re-baselining — never silently update a
  golden.
- **D-18:** **Shared-folder filename matching follows the filesystem.** Supersedes the word
  "exactly" in D-08: the override check uses a normal file-exists lookup, so it is case-insensitive
  on default macOS/Windows volumes and case-sensitive on case-sensitive volumes. No directory-listing
  exact-case comparison.

### Claude's Discretion
- **Filter design and strength for D-01** (user: "you decide"). Pick literature-grounded values
  (e.g. Brown & Duda structural model for head shadow / pinna cues). The test must show a clear
  measurable difference while Simple mode stays recognisably close to today's sound at ear level,
  front. Record the chosen values and their source in the plan.
- The embedded BinaryData namespace/target name. It must not collide with OSD's own
  `juce_add_binary_data(HRTFData ...)` / `namespace HRTFData` while OSD still has it. Follow the
  `SpatialCoreUIFontData` precedent at `CMakeLists.txt:233-236` (e.g. `SpatialCoreHRTFData`).
- The API name and the status-reporting shape (enum/atomic/struct) for D-04/D-06/D-09.
- How `SPATIALCORE_EMBED_ALL_HRTF=OFF` selects KEMAR-only (profile 5) at build time.
- Fixing the `tests/Binaural/HutubsPP2Tests.cpp:47` Debug-only golden failure (Phase 2 deferred
  item; Phase 3 owns the binaural goldens).
- Whether to address the `SharedFFTCache` "Leaked objects detected: 1 FFT" false positive here or
  leave it for Phase 5.
- **Doc correction:** CLAUDE.md "Build System" and "Critical Rules" say HRTF profiles are not
  BinaryData and that OSD resolves them bundle-relative (`<bundle>/Contents/Resources/HRTF/`).
  Introduced by `87cb7a3` (CR-01, 2026-07-06), which predates the OQ-6 ruling. It was already
  wrong about OSD: OSD v1.0.0 embeds all 5 via `juce_add_binary_data(HRTFData ...)` at its
  `CMakeLists.txt:50-58`, and the shipped `.vst3` has no `Resources/HRTF/`. Once DATA-01 lands,
  rewrite those CLAUDE.md lines (and `docs/integration-guide.md` if affected) to describe the
  resolution chain and embedding.

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Scope and locked decisions
- `.planning/ROADMAP.md` §"Phase 3: Binaural Defects & HRTF Packaging": goal, 5 success criteria, OQ-6 resolution, rejected convert-then-embed.
- `.planning/REQUIREMENTS.md` §DATA-01: resolution chain, `SPATIALCORE_EMBED_ALL_HRTF`, `loadFromBinaryData()`, KEMAR-only flip (SUITE-01).
- `.planning/REQUIREMENTS.md` §EXTR-02, BUG-01, BUG-02, RTSF-01, RTSF-03: acceptance text; RTSF-* are Phase 5 but constrain D-05.
- `.planning/PROJECT.md` Key Decisions: DR-1 (no alloc/lock/log on audio path), DR-5 (embed, supersedes DR-15), DR-6 (semver), P2-D14 (VBIP audible-change precedent for D-02).
- `.planning/STATE.md` §Blockers/Concerns and §Deferred Items: HutubsPP2 Debug golden, SharedFFTCache leak report, BUG-01 unasserted cases.
- `CLAUDE.md`: design principles (lock-free audio path, stateless algorithms, facade boundary SC-13). Its HRTF wording is stale; see Claude's Discretion.

### Defect sources
- SpatialCore#15 (`gh issue view 15 -R AndrewRahman/SpatialCore`): lateral-angle collapse; OpenSpatialPanner `Tests/DevFormatTests.cpp` `[binaural][sc12]` pins the defect and must be **inverted, not deleted** when the fix lands (consumer-side).
- `Spatial-Media-Lab/OpenSpatialDelay#234`: artifacts below 256 samples; three candidate causes, only the convolver is SpatialCore's.

### Code
- `src/Algorithms/DirectBinauralAlgorithm.cpp`: Woodworth ITD/ILD, `lateral = sinAz * cosEl` (BUG-01 site).
- `src/Binaural/PartitionedConvolver.cpp:100-140`: per-call overlap-add decoupling labelled OSD#234 (BUG-02 site); `include/SpatialCore/Binaural/PartitionedConvolver.h`.
- `include/SpatialCore/Binaural/HRTFDatabase.h` + `src/Binaural/HRTFDatabase.cpp`: `loadFromMemory` / `loadFromFile` / private `loadFromBytes` (shared parse body, D-09 from earlier extraction).
- `include/SpatialCore/Binaural/BinauralRenderer.h` + `src/Binaural/BinauralRenderer.cpp`: `setProfile`, `isSimpleMode`, KEMAR LF shelf at :106.
- `include/SpatialCore/Engine/RenderEngine.h:283-310, 380-400`: renderer double-buffer, `swapActiveRenderer`, `kRendererXfadeBlocks`, crossfade state.
- `CMakeLists.txt:233-236`: `juce_add_binary_data(SpatialCoreUIFontData ...)`, the precedent for embedded data naming.
- `tests/CMakeLists.txt:38-46`, `tests/Binaural/BinauralTestUtilities.h:47-52`: `SPATIALCORE_HRTF_DIR` source-tree test path.

### Consumer reference (OSD, read-only)
- OSD `origin/main` `CMakeLists.txt:48-58`: current `juce_add_binary_data(HRTFData ...)` over the 5 files.
- OSD `origin/main` `Source/PluginProcessor.cpp` ~1495-1600: `loadHRTFProfileIntoRenderer` / `loadHRTFProfile`, the profile index map and swap glue that D-04 moves into SpatialCore. Local clone: `~/conductor/repos/openspatialdelay`.

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- `HRTFDatabase::loadFromBytes`: one parse body behind both loaders; `loadFromBinaryData` / resolution chain can feed it without touching SOFA parsing.
- `RenderEngine` renderer double-buffer + 8-block crossfade: the click-free swap machinery for D-04/D-05 already exists; it currently waits for a consumer to load and call `swapActiveRenderer()`.
- `SPATIALCORE_HRTF_DIR` + the 5 per-profile test files in `tests/Binaural/`: existing golden harness to extend with dedicated convolver/renderer tests (EXTR-02).
- LFS pointer-stub CI guard: already fails the build on stub `.sofa` files (DATA-01 "half done").

### Established Patterns
- Embedded data via `juce_add_binary_data` with a `SpatialCore`-prefixed namespace (fonts).
- Three-slot/double-buffer handoffs with message-thread-only writers (SC-16 layout handoff, renderer swap). The background loader must fit this single-writer model.
- Non-finite guards in `.cpp` with the fast-math build error (`src/Core/FloatSemanticsGuard.h`). New filter code in algorithm/engine TUs keeps it.
- Offline oracles in `tests/reference/` never run in CI (Phase 2).

### Integration Points
- `RenderEngine::renderSimpleBinauralWoodworth`: where D-01's per-source filters run.
- New engine profile API: called from consumer UI/message thread; replaces OSD's timer-driven glue.
- Top-level `CMakeLists.txt`: new `SPATIALCORE_EMBED_ALL_HRTF` option + HRTF BinaryData target linked into `SpatialCore`.
- OSD duplicate `mysofa-static` guard (`CMakeLists.txt:40-55`) shows how to coexist with a consumer that still declares the same resources.

</code_context>

<specifics>
## Specific Ideas

- "Loud error" means a status the plugin can show the user. It does not mean silence.
- A broken override file should never strand the user: fall back to built-in, but say so.
- Treat the Simple-mode sound change exactly like the Phase 2 VBIP change: fix it, document it in OSD release notes, no legacy switch.

</specifics>

<deferred>
## Deferred Ideas

- **Custom HRTFs: later milestone (ROADMAP backlog 999.3).** The user has their own measured
  `.sofa` file. Decisions already made for that phase (2026-10-04):
  - Import happens through the plugin UI: the **last item in the binaural profile dropdown is
    "Import…"**. It is not a drop-a-file-in-a-folder workflow, which is only a prototype path.
  - Import **copies** the file into a personal library so it stays available in future sessions.
  - The user **types a display name at import**; delivered filenames are unreadable.
  - Session opened without the custom file: play a built-in (KEMAR) + warning; the session keeps
    the custom choice so it returns when the file is present.
  - **Auto-level** custom profiles to a **new perceived-loudness standard** that SpatialCore
    must define (none exists today; see D-14). Built-ins move onto it in the same phase (an
    audible change, so it needs an OSD release note).
  - Open for that phase: whether the dropdown/import widget is a shared `SpatialCoreUI`
    component or per-plugin (leaning shared so every SML plugin sees the same library).
  - **Interim prototype path that works after Phase 3:** name the custom file like a built-in
    (e.g. `hutubs_pp2.sofa`) and place it in `/Library/Application Support/Spatial Media Lab/HRTF/`
    (D-08). It replaces that profile.
- **Per-user HRTF folder** (`~/Library/Application Support/Spatial Media Lab/HRTF/`): subsumed by
  the import library above.
- **Headphone listening check** of the new Simple-mode cues: ROADMAP backlog 999.4.
- **OSD-side:** release note for the Simple-mode sound change; delete OSD's HRTF glue and its own `HRTFData` BinaryData at migration; check OSD's delay line and pitch shifter at small block sizes (rest of OSD#234). Track in PROJECT.md External Dependencies.
- **OpenSpatialPanner-side:** invert (not delete) the `[binaural][sc12]` assertions after the submodule bump.

</deferred>

---

*Phase: 03-binaural-defects-hrtf-packaging*
*Context gathered: 2026-10-04*
