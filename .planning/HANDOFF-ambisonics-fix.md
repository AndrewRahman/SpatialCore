Written 2026-10-24 at the end of the session that found the cause. Run A and B in separate sessions; they don't depend on each other.
Done-when lists amended 2026-10-25 to the test tiers (P7-D01, P7-D02): they point at README "Local test gate" instead of naming the full gate. Session A had already shipped, as quick task 261024-qlw, under the earlier gate.

## Session A — SpatialCore (run in a SpatialCore workspace)

```
/gsd-quick Make SpatialCore's Ambisonics output plain AmbiX SN3D (SpatialCore#32)

## How to work
Orchestrate only: no file reading, editing, building or test running in the main session. Haiku for lookups, builds and test runs; Sonnet for code and review. Validate against real test output before calling anything done. Don't ask me routine questions — decide, act, report.

## Context
- Read first: `.planning/quick/261024-mhh-re-check-the-ambisonics-maths-end-to-end/261024-mhh-FINDINGS.md` and its `research/` folder (especially `nfc-check.md`, `iem-ambisonics-math.md`, `ring-sim.md`). Issue comment with the numbers: https://github.com/AndrewRahman/SpatialCore/issues/32#issuecomment-6387069548
- `RenderEngine::renderAmbisonicsOutput` (`src/Engine/RenderEngine.cpp`, about lines 1609-1750) applies two things standard AmbiX encoders (IEM StereoEncoder/MultiEncoder) don't: (1) a per-order NFC shelf on orders ≥ 1 whose low-frequency gain is r / 1.5 — inverted against Daniel 2003 (far sources should lose bass, by about n·20·log10(r/R) on order n) and missing the per-order power; (2) max-rE weights cos(n·π/(2N+2)), which IEM decoders apply again by default.
- Decision: the Ambisonics output-format stream becomes plain AmbiX — evalSH × signal × the existing distance and tap gains, no NFC, no max-rE. max-rE stays wherever SpatialCore itself decodes (the internal Ambisonics speaker decode, `[ambi-pin]`). Keep the per-block SH interpolation. Keep the public API source-compatible.
- `research/UNTESTED-partial-plain-ambix-fix.diff` is an interrupted attempt: reference only, never applied or built.

## Scope
1. Implement the decision. Audio path stays allocation-, lock- and logging-free.
2. Tests: the engine stream matches the independent scipy golden values (`tests/IO/AmbisonicsCodecTests.cpp` back-half tables, `tests/reference/gen_sh_reference.py`) at orders 1, 3 and 6; channel ratios to W are identical at distances 0.5, 1.5, 5 and 10 m, at DC and at 50 Hz. Show these tests fail on the old code. Update `tests/Engine/AmbisonicsRenderDirectionTests.cpp` (drop the max-rE expectation and the "1.5 m is NFC-neutral" reasoning).
3. Rebaseline deliberately, and list with a reason, every test that pinned the old behaviour (`[nfc]`, `[d10]`, Phase 5 captures of the Ambisonics output). Weaken nothing unrelated.
4. Docs: CLAUDE.md, README and any doc describing the HOA path; record the decision in `.planning` (decisions table) referencing #32 and closed #21.

## Done when
- Tests per README "Local test gate": the light tier while iterating (`area=Engine` and `area=IO`, which hold the Ambisonics output and codec tests), then the full Release `ctest` before merging to `main`. The full tier (Debug run, `build-tsan`) is release prep and not part of this handoff.
- Atomic commits on a new branch from this branch, pushed (pushing a non-main branch starts no CI). Comment the result on #32. Leave #32 open until the OSD fix (openspatialdelay#22) has shipped and the pre-release speaker check passes.
- Tick the SpatialCore follow-up in ROADMAP Phase 999.5.
- Report: gate counts in Debug and Release, the fail-on-old-code evidence, the rebaselined tests.
```

## Session B — OpenSpatialDelay (run in an OSD workspace)

```
/gsd-quick Fix the AU 50-channel output and the silent format fallback (openspatialdelay#22)

## How to work
Orchestrate only: no file reading, editing, building or test running in the main session. Haiku for lookups, builds and test runs; Sonnet for code and review. Validate against real output (auval, test logs) before calling anything done. Don't ask me routine questions — decide, act, report. Never open a second REAPER instance.

## Context
- Issue: https://github.com/AndrewRahman/openspatialdelay/issues/22. Full evidence: SpatialCore repo, `.planning/quick/261024-mhh-re-check-the-ambisonics-maths-end-to-end/research/osd-bus-negotiation.md`, `ring-sim-truncation.md`, `osd-ambi-path.md` (on branch AndrewRahman/ambisonic-rendering-issue of AndrewRahman/SpatialCore).
- DAWs give channels in pairs, so 6th order runs on a 50-channel bus (49 used). OSD declares discreteChannels(50), but `isBusesLayoutSupported` rejects discrete main outputs on AU, and JUCE's AU layout tags stop at 5th-order Ambisonics (36 ch). `auval -v aufx O209 Smli` / `O214` FAIL: "Cannot Set Output Num Channels:50 when unit says it can". With fewer than (order+1)² outputs, `resolveEffectiveFormat` silently falls back to 9.1.6 while the UI still shows "6th Order Ambisonics". The show projects load OSD as AU, which matches the heard fault (back-left and back-right wrong in different ways). VST3 is fine.
- Also: the editor timer re-resolve (`PluginEditor.cpp` about 2291-2305) is dead code; `maxBusChannels` is read only in `prepareToPlay`.

## Scope
1. AU accepts 2-in / 50-out (and every count OSD's formats need) without breaking whatever the discrete rejection protected (find why it was added; keep Logic working).
2. A visible warning whenever the effective format differs from the selected one (for example "6th Order Ambisonics needs 50 outputs — host gave N, using 9.1.6"); re-resolve on bus-layout change (message thread only; nothing new on the audio thread).
3. Wiki: fix the azimuth sign in `docs/wiki/spatial-map.md:13-14, 21-22`, `controls-reference.md:95`, `adm-osc.md:26`, `creative-tips.md:82-85, 133`, the side comments in `Source/PresetData.cpp`, and the map ring distance labels vs the engine's metres.
4. Work on a new branch from the current dev branch, in its own worktree; don't build inside another session's workspace.

## Done when
- `auval -v` PASS with 2→50 settable, verbatim output captured. Test bundles get a distinct subtype and are removed from ~/Library/Audio/Plug-Ins/Components afterwards; never overwrite an installed dev build.
- VST3 still builds; OSD's tests for the code you changed pass (the light tier; whether OSD adopts SpatialCore's tiers is OSD's decision, P7-D02).
- Atomic commits, pushed if pushing that branch starts no CI (check the workflow triggers first; if it would, ask). Comment the result on #22 and on SpatialCore#32.
- Report: auval before/after, test counts, and the one remaining step for me: the speaker-rig check at the pre-release review.
```
