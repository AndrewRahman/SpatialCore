---
phase: 04-control-surface-ui
plan: 06
subsystem: ui
tags: [ui, fonts, embedded-binarydata, jetbrains-mono, dm-sans, spatial-map, preset-overlay, catch2, juce-lookandfeel]

requires:
  - phase: 04-control-surface-ui
    provides: "SpatialCoreUITests target, UITestSupport.h (renderToImage), headless GUI listener (04-05)"
provides:
  - "SpatialMapComponent owns embedded JetBrains Mono Regular/Medium/Bold and draws all five label sites with them under any look-and-feel"
  - "PresetSaveOverlay SAVE PRESET title in an explicit embedded DM Sans Bold typeface"
  - "Font provenance tests: spy look-and-feel, embedded-bytes equality, SMLLookAndFeel member check, no-family-name code rule"
  - "SPATIALCORE_SOURCE_DIR compile definition on SpatialCoreUITests"
affects: [04-07, 04-08]

plan_head_before: ff7d755ecdcc2cebb6b9c21e0dbd4b9c487f7121
plan_head_after: bb40ec83f862ff8b28a276aa9896c6545fe0555c

actuals:
  tokens: 4000
  tasks: 2
  commits: 2

tech-stack:
  added: []
  patterns:
    - "A component that must look the same under any look-and-feel loads its own typefaces from SpatialCoreUIFontData and builds Fonts from FontOptions (typeface), which bypasses the default look-and-feel lookup"
    - "Font provenance is proven with a spy LookAndFeel_V4 installed as the JUCE default (counts getTypefaceForFont) after Typeface::clearTypefaceCache(), never with a screenshot on a machine that has the fonts installed"
    - "A source-scan test with positive-control strings keeps a code rule live"

key-files:
  created:
    - tests/UI/FontProvenanceTests.cpp
  modified:
    - include/SpatialCore/UI/SpatialMapComponent.h
    - src/UI/SpatialMapComponent.cpp
    - include/SpatialCore/UI/PresetBrowser.h
    - src/UI/PresetBrowser.cpp
    - tests/CMakeLists.txt

key-decisions:
  - "The map's typefaces are private members loaded in its constructor, not read from SMLLookAndFeel, so the render no longer depends on which look-and-feel is set (D-06)"
  - "SMLLookAndFeel.h and SMLLookAndFeel.cpp are untouched; its seven public typeface members stay for consumers"
  - "The overlay title uses FontOptions (typeface).withHeight (13.0f) and needed no bold style: the DM Sans Bold file is already bold, and under SML as the default look-and-feel the render is byte-identical to BASE"

patterns-established:
  - "SpyLookAndFeel / ScopedSpyDefault / makeFontTestMap (test-local in FontProvenanceTests.cpp)"
  - "Hidden [.][ui-capture] case writes PNGs to $SC_UI_CAPTURE_DIR for before/after cmp"

requirements-completed: [DATA-02, EXTR-05]

coverage:
  - id: D1
    description: "Painting the map with no SML look-and-feel and with SMLLookAndFeel on the component makes 0 typeface requests to the default look-and-feel (2 at BASE, both '<Sans-Serif>'), so no system font lookup happens"
    requirement: DATA-02
    verification:
      - kind: unit
        ref: "tests/UI/FontProvenanceTests.cpp#Fonts: the map asks the default look-and-feel for no font, with or without SML"
        status: pass
    human_judgment: false
  - id: D2
    description: "The map render with SML on the component equals the render with no look-and-feel, pixel for pixel, and the SML-on-component and SML-default map PNGs are byte-identical before and after the change (OSD's map is unchanged)"
    requirement: EXTR-05
    verification:
      - kind: unit
        ref: "tests/UI/FontProvenanceTests.cpp#Fonts: map render is byte-identical with and without the SML look-and-feel"
        status: pass
      - kind: other
        ref: "cmp build/ui-capture-base/map-sml-component.png build/ui-capture-after/map-sml-component.png; same for map-sml-default.png"
        status: pass
    human_judgment: false
  - id: D3
    description: "Painting the SAVE PRESET overlay makes 0 default look-and-feel typeface requests (1 at BASE)"
    requirement: DATA-02
    verification:
      - kind: unit
        ref: "tests/UI/FontProvenanceTests.cpp#Fonts: the save-preset title asks for no system font"
        status: pass
    human_judgment: false
  - id: D4
    description: "All 8 SpatialCoreUIFontData resources equal their source files in fonts/ byte for byte, and a fresh SMLLookAndFeel has all seven typeface members loaded"
    requirement: DATA-02
    verification:
      - kind: unit
        ref: "tests/UI/FontProvenanceTests.cpp#Fonts: every embedded font resource equals its source file"
        status: pass
      - kind: unit
        ref: "tests/UI/FontProvenanceTests.cpp#Fonts: SMLLookAndFeel loads all seven embedded typefaces"
        status: pass
    human_judgment: false
  - id: D5
    description: "No file under src/UI or include/SpatialCore/UI requests an SML font by family name, and every createSystemTypefaceFor call takes SpatialCoreUIFontData data"
    requirement: DATA-02
    verification:
      - kind: unit
        ref: "tests/UI/FontProvenanceTests.cpp#Fonts: no UI code asks for an SML font by family name"
        status: pass
    human_judgment: false
  - id: D6
    description: "Under a per-component SMLLookAndFeel (OSD's setup) the SAVE PRESET title changes from the system sans to DM Sans Bold; whether that visible change is acceptable for OSD"
    verification: []
    human_judgment: true
    rationale: "D-22 said OSD must look identical, but OSD sets SML per component, where this one title changes; only the user can accept a visible change. Two captures are queued for the end-of-phase review."

duration: 10min
completed: 2026-10-05
status: complete
---

# Phase 4 Plan 06: Embedded Font Provenance Summary

**The spatial map now loads its own embedded JetBrains Mono and the SAVE PRESET title its own embedded DM Sans Bold, so neither asks the system for a font (spy look-and-feel: 0 requests, was 2 and 1), with OSD's map render byte-identical and DATA-02 proven by four test tags.**

## Performance

- **Duration:** about 10 min (almost all of it the build; the plan's compile-definition change rebuilt JUCE once in the Debug tree)
- **Started:** 2026-10-05T09:08:14Z
- **Completed:** 2026-10-05T09:18:29Z
- **Tasks:** 2
- **Files modified:** 6 (1 created)

## BASE and measurements

- **BASE:** `ff7d755ecdcc2cebb6b9c21e0dbd4b9c487f7121` (HEAD when this plan started).
- **BASE spy counts** (spy as default look-and-feel, cache cleared):
  - map with no SML look-and-feel: **2** requests, `<Sans-Serif>, <Sans-Serif>`
  - map with SML on the component: 0
  - `PresetSaveOverlay` title paint: **1** request, `<Sans-Serif>`
  - After this plan: 0 in all three cases.
- **cmp results (base vs after):**

| Capture | Result | Binding? |
|---|---|---|
| `map-sml-component.png` (OSD's setup) | identical | yes (D-06) |
| `map-sml-default.png` | identical | yes (D-06) |
| `overlay-sml-default.png` | identical (as expected: the old title already resolved to SML's DM Sans Bold there) | advisory (D-22) |
| `overlay-sml-component.png` (OSD's setup) | **differs** (expected: system sans before, DM Sans Bold after) | for the D-22 review |

- At BASE the SML-on-component and SML-default map captures were also identical to each other, and after the change the no-look-and-feel render equals the SML render pixel for pixel (`[ui][fonts][identical]`).

## Accomplishments

- `SpatialMapComponent` has private `monoRegular_`, `monoMedium_`, `monoBold_`, created in its constructor from `SpatialCoreUIFontData::JetBrains_Mono*_ttf` (the `SMLLookAndFeel` loader). The per-paint `dynamic_cast<SMLLookAndFeel*>` and all five typeface-less fallback branches are gone (ring labels Regular 9, cardinals Medium 14 with kerning 0.2, dot numbers Bold 10, elevation label Regular 11, OSC label Bold 10). The `SMLLookAndFeel.h` include is dropped from the map.
- `PresetSaveOverlay` owns `titleTypeface_` (embedded DM Sans Bold) and paints `SAVE PRESET` with `FontOptions (titleTypeface_).withHeight (13.0f)`. The name editor and buttons are untouched (consumer look-and-feel, outside D-22).
- Six `[ui][fonts]` cases, all passing in Debug and Release.

## Task Commits

1. **Task 1: Tracer, map draws its labels in embedded JetBrains Mono with no system lookup** - `1fe36c8` (feat)
2. **Task 2: Overlay title, embedded-bytes proof, SML typefaces, code-rule test** - `bb40ec8` (feat; the red overlay spy case and the three provenance cases were written first and verified failing/passing before the fix, in the same commit)

**Plan metadata:** the docs commit that follows this file.

## Files Created/Modified

- `tests/UI/FontProvenanceTests.cpp` - `SpyLookAndFeel`, `ScopedSpyDefault`, `makeFontTestMap`, the six durable cases and the hidden `[.][ui-capture]` PNG capture
- `include/SpatialCore/UI/SpatialMapComponent.h` - three private typeface members with the D-06 comment
- `src/UI/SpatialMapComponent.cpp` - constructor loads the typefaces; five label sites use them
- `include/SpatialCore/UI/PresetBrowser.h` / `src/UI/PresetBrowser.cpp` - `titleTypeface_` and the title font
- `tests/CMakeLists.txt` - `UI/FontProvenanceTests.cpp` in `SpatialCoreUITests`; `SPATIALCORE_SOURCE_DIR` define (CRLF preserved)

## DATA-02 evidence (for phase VERIFICATION, D-16)

| Tag | What it proves |
|---|---|
| `[ui][fonts][spy]` (with `[tracer]` for the map, and the overlay case) | 0 typeface requests to the default look-and-feel from the map (with and without SML) and from the overlay title; 2 and 1 at BASE |
| `[ui][fonts][bytes]` | `SpatialCoreUIFontData::namedResourceListSize == 8` and every resource's bytes equal `fonts/<original filename>` |
| `[ui][fonts][rule]` | no UI source line requests a font by family name, every `createSystemTypefaceFor` takes `SpatialCoreUIFontData::` data; positive-control strings prove the regex is live |
| `[ui][fonts][identical]` | the map render is pixel-identical with and without the SML look-and-feel (D-06) |

Also `[ui][fonts][sml]`: all seven `SMLLookAndFeel` typeface members are loaded. No clean-machine run was made (D-05): this Mac has DM Sans, JetBrains Mono and Roboto installed, so a screenshot cannot prove DATA-02 and the claim rests on provenance.

## For the end-of-phase D-22 review (user decision) and the OSD release note

Under a per-component `SMLLookAndFeel` (how OSD installs it) the SAVE PRESET title changes from the computer's system sans to DM Sans Bold; everything else in the overlay is unchanged. Compare:

- before: `/Users/andrewrahman/conductor/workspaces/SpatialCore/gwangju-v1/build/ui-capture-base/overlay-sml-component.png`
- after: `/Users/andrewrahman/conductor/workspaces/SpatialCore/gwangju-v1/build/ui-capture-after/overlay-sml-component.png`

(Both gitignored under `build/`; the base images exist only until `build/` is cleaned.) Plan 04-08 lists this in the OSD release note.

## Decisions Made

- Map typefaces are owned by the map, loaded from the embedded data in its constructor, rather than borrowed from `SMLLookAndFeel` (D-06).
- No bold style was added to the overlay title options: the embedded DM Sans Bold file is already bold and the default-SML render matches BASE byte for byte.

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered

- Adding the `SPATIALCORE_SOURCE_DIR` definition changed the compile flags of every translation unit in `SpatialCoreUITests`, so the Debug build recompiled all JUCE modules once. No action needed.
- Expected noise only: the `juce_LeakedObjectDetector` assertion at exit of `SpatialCoreTests` (3 FFT instances, pre-existing).

## Verification Results

- Debug `SpatialCoreTests`: 303/303 passed. Debug `SpatialCoreUITests`: 13/13 passed (was 7; +6 `[ui][fonts]`).
- Release `ctest --test-dir build-release/tests`: 100% passed, 316/316 (303 core + 13 `ui:`); 13 `ui:` tests listed.
- Acceptance greps: `dynamic_cast<SMLLookAndFeel` 0; `FontOptions (9|10|11|14.0f)` 0 in the map; three `JetBrains_Mono*_ttfSize` symbols in the map; `FontOptions (13.0f)` 0 and `SpatialCoreUIFontData::DM_SansBold_ttf` 1 in `PresetBrowser.cpp`; D-05 grep gate prints nothing; non-embedded `createSystemTypefaceFor` 0; `git diff BASE..HEAD` on `SMLLookAndFeel.h/.cpp` is empty.

## Known Stubs

None.

## Threat Flags

None. T-04-14 (font substitution) is mitigated: explicit embedded typefaces only, spy shows 0 lookups, code rule shows no family-name request.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

Ready for 04-07 (screenshots). One open human item: the user accepts or rejects the SAVE PRESET title change (D-22) at the end-of-phase review.

## Self-Check: PASSED

- Created/modified files exist on disk (`tests/UI/FontProvenanceTests.cpp` and the five modified sources).
- Commits `1fe36c8` and `bb40ec8` exist (`git log --grep "04-06"`).
- All task acceptance criteria and the plan-level verification re-run green (results above).

---
*Phase: 04-control-surface-ui*
*Completed: 2026-10-05*
