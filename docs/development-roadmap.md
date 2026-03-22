# SpatialCore — Development Roadmap

Last updated: 2026-03-22

---

## Current State

SpatialCore is currently a **documentation-only shell**. The architecture, components, and integration patterns are fully designed, but no source code has been extracted yet. All spatial audio code still lives inside OpenSpatialDelay v1.0.

### What Exists
- Git repository with GitHub remote (`github.com/AndrewRahman/SpatialCore`)
- CLAUDE.md (project context for Claude)
- README.md (public-facing documentation)
- docs/integration-guide.md (how plugins will consume SpatialCore)
- docs/conductor-setup-guide.md (multi-plugin development workflow)
- docs/workflow-tutorials.md (3 parallel development patterns)

### What Does NOT Exist Yet
- No source code (no `.h` or `.cpp` files)
- No CMakeLists.txt or build system
- No tests
- No HRTF data files

---

## Sequencing

SpatialCore extraction happens **after** OpenSpatialDelay v1.0 ships (deadline: 2026-03-31). The order is:

1. **Finish OpenSpatialDelay v1.0** (remaining items below)
2. **Extract SpatialCore** from OpenSpatialDelay
3. **Build future plugins** on top of SpatialCore

### OpenSpatialDelay v1.0 — Remaining Items
| Item | Description |
|---|---|
| Custom SOFA Import | Let users load their own HRTF files |
| ADM-OSC Settings UI | In-plugin UI for configuring OSC ports/settings |
| AAX Format | Pro Tools plugin format support |
| Code Signing | macOS notarization for distribution |
| GitHub Migration | Move to Spatial-Media-Lab/OpenSpatialDelay org |

---

## Phase 1: SpatialCore Library Extraction

**Goal:** Pull the 68% of OpenSpatialDelay marked as `SPATIAL MEDIA LIBRARY` into a standalone static library.

### Modules to Extract

| Module | Source in OpenSpatialDelay | What Gets Extracted |
|---|---|---|
| **Algorithms/** | PluginProcessor.h/cpp | VBAP, VBIP, KNN, DBAP, MDAP, Ambisonics, DirectBinaural — all 7 SpatializationAlgorithm implementations |
| **Binaural/** | PluginProcessor.h/cpp | HRTFDatabase (SOFA/libmysofa), PartitionedConvolver (FFT overlap-save), BinauralRenderer (12 per-source convolvers) |
| **IO/** | PluginProcessor.h/cpp | OutputFormatRegistry (22 formats), SpeakerLayout (13 ITU-R layouts), AmbisonicsCodec (SH eval, decode matrices) |
| **OSC/** | PluginProcessor.cpp | ADM-OSC Receive (parse /adm/obj/N/), ADM-OSC Send (30Hz broadcast) |
| **Trajectory/** | PluginProcessor.cpp | 13 shapes, origin-point architecture, forward/reverse |
| **DSP/** | PluginProcessor.cpp | softClip(), outputLimiter() |
| **UI/** | PluginEditor.h/cpp | SpatialMapComponent, SMLLookAndFeel, ReverseSlider, IndicatorToggle, StyledButton |

### Architecture Pattern
```
Every SML Plugin (~32% plugin-specific DSP)
    |
    v  links via CMake subdirectory
SpatialCore (~68% reusable framework)
    - Algorithms, HRTF, I/O, OSC, Trajectories, UI
```

### Consumer Integration Model
```bash
# Plugin adds SpatialCore as git submodule
git submodule add https://github.com/Spatial-Media-Lab/SpatialCore.git SpatialCore
```
```cmake
# Plugin's CMakeLists.txt
add_subdirectory(SpatialCore)
target_link_libraries(MyPlugin PRIVATE SpatialCore)
```

### Key Interface (Frozen — Major Version Bump Required to Change)
```cpp
class SpatializationAlgorithm {
    virtual void computeGains(const SourcePosition& source,
                              const LayoutContext& ctx,
                              float* outputGains, int numSpeakers) const = 0;
    virtual bool supportsBinauralDirect() const { return false; }
    virtual bool supportsSurround() const { return true; }
    virtual bool supportsSHDomain() const { return false; }
    virtual juce::String getName() const = 0;
};
```

### Deliverables
- [ ] Modular directory structure (Algorithms/, Binaural/, IO/, OSC/, Trajectory/, DSP/, UI/)
- [ ] CMakeLists.txt build system (static library)
- [ ] Catch2 test suite (port existing tests from OpenSpatialDelay)
- [ ] HRTF data embedded as BinaryData (5 SOFA files)
- [ ] All code verified lock-free and realtime-safe
- [ ] OpenSpatialDelay refactored to consume SpatialCore as submodule

---

## Phase 2: GitHub Migration & Public Release

**Goal:** Publish SpatialCore under the Spatial Media Lab organization.

| Task | Details |
|---|---|
| Create org repo | `github.com/Spatial-Media-Lab/SpatialCore` (fresh, squash history) |
| Set license | Dual GPL-3.0 + Commercial |
| Tag v1.0.0 | First public release with all modules extracted and tested |
| Update OpenSpatialDelay | Point submodule to public org repo |

---

## Phase 3: Future Plugin Suite

All future plugins will share SpatialCore and add their own effect-specific DSP (~32% custom code each).

| Plugin | Description | Target |
|---|---|---|
| **OpenSpatialDelay** | Spatial delay with per-tap 3D positioning | v1.0 — Q1 2026 (active) |
| **OpenSpatialReverb** | Algorithmic reverb with spatial reflections | 2027 |
| **OpenSpatialGranular** | Granular synthesis with 3D grain positioning | 2027 |
| **OpenSpatialPanner** | Object-based spatial panner with energy distribution | 2027 |
| **OpenSpatialChorus** | Chorus/flanger with spatially distributed voices | 2027 |

Each plugin follows the same pattern:
1. Create repo with SpatialCore as git submodule
2. Implement effect-specific DSP (~32% custom code)
3. Use SpatialCore's rendering paths, algorithms, UI components
4. Build + test + release

---

## Critical Design Rules

These rules are locked in and apply to all SpatialCore development:

1. **NEVER** allocate memory in any function called from processBlock
2. **NEVER** modify the SpatializationAlgorithm interface without bumping the major version
3. **ALWAYS** maintain backward compatibility with existing plugins when adding features
4. **ALWAYS** run the full test suite before tagging a release
5. HRTF profiles are embedded as BinaryData — adding/removing profiles requires rebuild of ALL consumer plugins
6. Semantic versioning: Major (breaking API), Minor (new features), Patch (bug fixes)
