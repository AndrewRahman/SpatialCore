# Codebase Structure

**Analysis Date:** 2026-08-09

## Directory Layout

```
/Users/andrewrahman/conductor/workspaces/SpatialCore/kelowna/
├── .claude/                          # Claude Code project config
│   └── skills/                       # 15 JUCE/DSP project skills (auto-load)
├── .planning/                        # GSD planning documents
│   └── codebase/                     # Architecture/structure/concerns maps
├── .context/                         # Context database
├── include/SpatialCore/              # Public API headers (distributed with library)
│   ├── Algorithms/                   # 8 spatialization algorithms
│   ├── Binaural/                     # HRTF rendering (HRTFDatabase, PartitionedConvolver, BinauralRenderer)
│   ├── Core/                         # Fundamental types and math (Types.h, SourcePosition.h, SpatialMath.h)
│   ├── DSP/                          # Utility DSP (softClip, outputLimiter)
│   ├── IO/                           # Output formats and speaker layouts (22 formats, 13 ITU-R layouts)
│   ├── OSC/                          # ADM-OSC integration (receiver/sender)
│   ├── Trajectory/                   # Animation engine (13 shapes)
│   ├── UI/                           # JUCE GUI components (SpatialMapComponent, SMLLookAndFeel, etc.)
│   └── SpatialCore.h                 # Umbrella header
├── src/                              # Implementation files (compiled into libSpatialCore.a)
│   ├── Algorithms/                   # Algorithm implementations (8 .cpp files)
│   ├── Binaural/                     # Binaural rendering implementations
│   ├── Core/                         # Core math/utility implementations
│   ├── DSP/                          # No .cpp (header-only utilities)
│   ├── IO/                           # Format registry and layout implementations
│   ├── OSC/                          # ADM-OSC implementation
│   ├── Trajectory/                   # Trajectory engine implementation
│   └── UI/                           # UI component implementations (8 .cpp files)
├── tests/                            # Unit tests (Catch2 v3.7.1)
│   ├── Algorithms/                   # Algorithm tests
│   ├── DSP/                          # DSP utility tests
│   ├── IO/                           # I/O layer tests
│   ├── Trajectory/                   # Trajectory engine tests
│   ├── CMakeLists.txt                # Test build configuration
│   └── [4 .cpp test files]           # Test implementations
├── docs/                             # Documentation
│   ├── integration-guide.md          # Plugin integration tutorial
│   └── superpowers/                  # GSD superpowers documentation
├── CMakeLists.txt                    # Build configuration (C++17, JUCE 8, libmysofa, Catch2)
├── README.md                         # Project overview and usage
├── CLAUDE.md                         # Project context for Claude (architecture, skills, rules)
└── .gitignore                        # Exclude build/, .cmake, etc.
```

## Directory Purposes

**`.claude/skills/`:**
- Purpose: 15 project-level JUCE/DSP skills (auto-loaded by Claude Code)
- Contains: Skill definitions for spatial audio DSP patterns, JUCE best practices, plugin architecture
- Key files: SKILL.md for each skill, rules/*.md for detailed patterns

**`.planning/codebase/`:**
- Purpose: GSD-generated architecture/structure/concerns analysis documents
- Contains: ARCHITECTURE.md, STRUCTURE.md, CONVENTIONS.md, TESTING.md, CONCERNS.md (when generated)

**`include/SpatialCore/`:**
- Purpose: Public API headers distributed with library (installed as #include <SpatialCore/...>)
- Structure: One subdirectory per component (Algorithms, Binaural, Core, etc.)
- Headers: All `.h` files; implementation details kept in src/

**`include/SpatialCore/Algorithms/`:**
- Purpose: Spatialization algorithm interface and 8 concrete implementations
- Key files:
  - `SpatializationAlgorithm.h` — Abstract base class, LayoutContext, BinauralContext
  - `VBAPAlgorithm.h`, `VBIPAlgorithm.h`, `KNNAlgorithm.h`, `DBAPAlgorithm.h`, `MDAPAlgorithm.h` — Speaker-based algorithms
  - `AmbisonicsAlgorithm.h` — Spherical harmonic encoding
  - `DirectBinauralAlgorithm.h`, `ConstantPowerAlgorithm.h` — Binaural/stereo algorithms
  - `AllAlgorithms.h` — Convenience aggregator

**`include/SpatialCore/Binaural/`:**
- Purpose: HRTF-based binaural audio rendering
- Key files:
  - `SharedFFTCache.h` — Process-global FFT singleton
  - `HRTFDatabase.h` — SOFA loading, KD-tree HRIR lookup
  - `PartitionedConvolver.h` — FFT overlap-save convolution
  - `BinauralRenderer.h` — 12 per-source convolvers, profile management

**`include/SpatialCore/Core/`:**
- Purpose: Fundamental data structures and math utilities
- Key files:
  - `Types.h` — ObjectState, TrajectoryState, BinauralProfile, constants (MAX_SOURCES=12, MAX_SPEAKERS=16)
  - `SourcePosition.h` — Spatial position input struct (azimuth, elevation, distance in radians)
  - `SpatialMath.h` — Coordinate conversion, spatial math declarations
  - `BinauralGains.h` — ITD/ILD gains for binaural rendering

**`include/SpatialCore/IO/`:**
- Purpose: Output format definitions, speaker layouts, Ambisonics codec
- Key files:
  - `OutputFormat.h` — Enum of 22 formats (binaural, stereo, 7 surround, 7 Atmos, 6 Ambisonics)
  - `OutputFormatRegistry.cpp` — Format metadata (channel count, LFE, height, Ambi order)
  - `SpeakerLayout.h` — Speaker position struct, 13 ITU-R layout factory functions, VBAP triplet structure
  - `AmbisonicsCodec.h` — Spherical harmonic encoding/decoding, ACN/SN3D, max-rE weighting

**`include/SpatialCore/OSC/`:**
- Purpose: ADM-OSC protocol for spatial audio object positioning
- Key files:
  - `ADMOSCReceiver.h` — Listen on port 4002, parse `/adm/obj/N/azim|elev|dist|aed|xyz`, Listener interface
  - `ADMOSCSender.h` — Broadcast object positions at 30Hz

**`include/SpatialCore/Trajectory/`:**
- Purpose: Animated source trajectories with 13 parameterized shapes
- Key files:
  - `TrajectoryEngine.h` — Shape enum (None, Bounce, Circle, Cross, Figure8, Heart, Helix, Infinity, Line, Orbit, Random, Spiral, Square, Triangle), tick(), randomness state

**`include/SpatialCore/DSP/`:**
- Purpose: Utility DSP functions (saturation, limiting)
- Key files:
  - `Utilities.h` — Header-only: softClip() (asymptotic saturation), outputLimiter() (+2dB ceiling tanh)

**`include/SpatialCore/UI/`:**
- Purpose: JUCE GUI components for spatial audio editing
- Key files:
  - `SpatialMapComponent.h` — 2D top-down spatial map, object dragging, distance rings, elevation opacity encoding
  - `SMLLookAndFeel.h` — Dark theme styling, cyan/purple/amber color palette
  - `ReverseSlider.h` — Azimuth knob (clockwise rotation = clockwise on map)
  - `IndicatorToggle.h` — Toggle pill with indicator dot
  - `StyledButton.h` — Centered-text button

**`src/`:**
- Purpose: Implementation files compiled into libSpatialCore.a static library
- Structure: Mirrors include/SpatialCore/ directory layout
- Compiled for: Linked into consumer plugins via `target_link_libraries(MyPlugin PRIVATE SpatialCore)`

**`src/Algorithms/`:**
- Files: 8 .cpp files implementing computeGains() for each algorithm
- No headers (all declarations in include/)

**`src/Binaural/`:**
- Files: HRTFDatabase.cpp, PartitionedConvolver.cpp, BinauralRenderer.cpp
- Includes: libmysofa integration, FFT state management, ITD/ILD computation

**`src/Core/`:**
- Files: SpatialMath.cpp (coordinate conversion, VBAP gain computation, spherical harmonic evaluation)

**`src/IO/`:**
- Files: OutputFormatRegistry.cpp, SpeakerLayout.cpp, AmbisonicsCodec.cpp
- Contains: ITU-R layout definitions, Ambi decode matrix generation, max-rE weighting

**`src/OSC/`:**
- Files: ADMOSCReceiver.cpp, ADMOSCSender.cpp
- JUCE OSC listener/sender implementation

**`src/Trajectory/`:**
- Files: TrajectoryEngine.cpp
- Contains: All 13 shape evaluation functions, random noise generation

**`src/UI/`:**
- Files: 8 .cpp files implementing paint(), mouseDown(), mouseDrag(), etc.

**`tests/`:**
- Purpose: Catch2 unit test suite
- Test Framework: Catch2 v3.7.1 (fetched via FetchContent)
- Test Runner: `cmake --build build --target SpatialCoreTests` or via CTest

**`tests/Algorithms/`:**
- Files: SpatializationAlgorithmTests.cpp
- Coverage: All 8 algorithms instantiate, capability flags (supportsBinauralDirect, supportsSurround, supportsSHDomain), computeGains produces valid gains

**`tests/IO/`:**
- Files: SpeakerLayoutTests.cpp
- Coverage: Speaker layout creation, VBAP triplet generation, format registry lookups

**`tests/Trajectory/`:**
- Files: TrajectoryEngineTests.cpp
- Coverage: All 13 shapes evaluate, forward/reverse, random noise generation

**`tests/DSP/`:**
- Files: UtilitiesTests.cpp
- Coverage: softClip(), outputLimiter() clipping thresholds, NaN safety

**`docs/`:**
- integration-guide.md — Step-by-step plugin integration tutorial
- superpowers/plans/ — GSD planning documents (generated during workflows)

**`CMakeLists.txt` (root):**
- Purpose: Main build configuration
- Fetches: JUCE 8.0.4 (or uses local/installed), libmysofa v1.3.2, zlib
- Defines: SpatialCore static library target with all 23 .cpp files
- Public interface: `target_include_directories(SpatialCore PUBLIC include/)`
- Compiler flags: C++17, fast-math on Apple/MSVC

**`tests/CMakeLists.txt`:**
- Purpose: Test build configuration
- Fetches: Catch2 v3.7.1
- Defines: SpatialCoreTests executable linking against Catch2 and SpatialCore
- Test discovery: CTest integration with Catch2 discover_tests macro

## Key File Locations

**Entry Points:**
- `include/SpatialCore/SpatialCore.h` — Umbrella header (include this to get all APIs)
- `CMakeLists.txt` — Build entry point for consumer plugins (`add_subdirectory(SpatialCore)`)

**Configuration:**
- `CMakeLists.txt` — C++ version (17), compiler flags, dependency versions, output target (libSpatialCore.a)
- `.claude/skills/` — Project-specific skill definitions

**Core Logic:**
- `include/SpatialCore/Algorithms/SpatializationAlgorithm.h` — Abstract base class for all algorithms
- `src/Algorithms/*.cpp` — 8 algorithm implementations
- `src/Binaural/BinauralRenderer.cpp` — HRTF convolution orchestration
- `src/IO/OutputFormatRegistry.cpp` — 22 output format definitions
- `include/SpatialCore/Trajectory/TrajectoryEngine.h` — 13 animation shapes

**Testing:**
- `tests/CMakeLists.txt` — Test build configuration
- `tests/Algorithms/SpatializationAlgorithmTests.cpp` — Algorithm capability tests
- `tests/IO/SpeakerLayoutTests.cpp` — Layout and format tests
- `tests/Trajectory/TrajectoryEngineTests.cpp` — Shape evaluation tests
- `tests/DSP/UtilitiesTests.cpp` — DSP function tests

## Naming Conventions

**Files:**
- Headers: `ComponentName.h` (e.g., `VBAPAlgorithm.h`, `BinauralRenderer.h`)
- Implementations: `ComponentName.cpp` (e.g., `VBAPAlgorithm.cpp`)
- Catch2 tests: `ComponentNameTests.cpp` (e.g., `SpatializationAlgorithmTests.cpp`)
- No separate files for interfaces (base classes defined inline in headers)

**Directories:**
- Component names are singular or plural as group (e.g., `Algorithms/` for multiple, `Binaural/` for category, `Core/` for fundamentals)
- No abbreviations (no `algo/`, `hrtf/`, `fft/`)
- Public headers: `include/SpatialCore/{Component}/`
- Implementation: `src/{Component}/`

**Code Organization:**
- One public class per header (e.g., `VBAPAlgorithm` lives in `VBAPAlgorithm.h`)
- All code in `spatialcore::` namespace to avoid consumer plugin conflicts
- Test suite uses `using namespace spatialcore` for brevity

## Where to Add New Code

**New Spatialization Algorithm:**
- Implementation: `src/Algorithms/NewAlgorithm.cpp`
- Header: `include/SpatialCore/Algorithms/NewAlgorithm.h` (inherit SpatializationAlgorithm, override computeGains)
- Register: Add `#include` to `include/SpatialCore/Algorithms/AllAlgorithms.h`
- Tests: `tests/Algorithms/SpatializationAlgorithmTests.cpp` (add TEST_CASE for new algorithm)
- Update: CMakeLists.txt add_library() source list

**New Output Format:**
- Metadata: `src/IO/OutputFormatRegistry.cpp` (add OutputFormatInfo entry)
- Enum: `include/SpatialCore/IO/OutputFormat.h` (add format to OutputFormat enum)
- Layout: `src/IO/SpeakerLayout.cpp` (add factory function if new speaker count)
- Tests: `tests/IO/SpeakerLayoutTests.cpp` (test format discovery)

**New Trajectory Shape:**
- Implementation: `src/Trajectory/TrajectoryEngine.cpp` (add shape evaluation function)
- Enum: `include/SpatialCore/Trajectory/TrajectoryEngine.h` (add to TrajectoryShape enum)
- Tests: `tests/Trajectory/TrajectoryEngineTests.cpp` (test shape bounds and continuity)

**New UI Component:**
- Header: `include/SpatialCore/UI/NewComponent.h` (inherit juce::Component)
- Implementation: `src/UI/NewComponent.cpp` (implement paint, mouse events)
- Styling: Use SMLLookAndFeel color palette (define colors in SMLLookAndFeel.h)
- Tests: Not currently covered (consider adding to test suite if critical)

**New DSP Utility:**
- Header: `include/SpatialCore/DSP/Utilities.h` (add inline function; no .cpp needed)
- Tests: `tests/DSP/UtilitiesTests.cpp` (test edge cases, NaN handling)

**Bug Fix / Refactor:**
- Locate component in src/ and include/
- Update tests/ to add regression tests
- Update CONCERNS.md if resolving a known issue

## Special Directories

**`.claude/`:**
- Purpose: Claude Code project settings and skills
- Generated: Partially by gsd-map-codebase (planning documents)
- Committed: Yes (skills, settings, context database)

**`.planning/codebase/`:**
- Purpose: Generated architecture analysis documents
- Generated: Yes, by gsd-map-codebase focus modes
- Committed: Yes (read by gsd-plan-phase, gsd-execute-phase)

**`build/` (not shown):**
- Purpose: CMake build output
- Generated: Yes, by cmake -B build
- Committed: No (in .gitignore)

**HRTF Profiles (embedded as BinaryData):**
- Source: 5 SOFA files (Simple Woodworth, MIT KEMAR, SADIE II D2, CIPIC003, HUTUBS PP2, Bernschuetz KU100)
- Embedded: Via JUCE BinaryData (Git LFS tracked)
- Impact: Adding/removing profiles requires rebuild of all consumer plugins
- Location: Not visible in source tree; managed by build system

---

*Structure analysis: 2026-08-09*
