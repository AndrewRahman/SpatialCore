# Technology Stack

**Analysis Date:** 2026-08-09

## Languages

**Primary:**
- C++ (C++17) — Core library implementation, audio algorithms, spatial rendering

**Standards:**
- C++17 standard with `-std=c++17` enforcement

## Runtime

**Environment:**
- JUCE 8 (C++17 audio framework) — Cross-platform audio plugin and application framework

**Compiler Support:**
- Apple Clang (macOS, arm64)
- MSVC (Windows)
- GCC/Clang (Linux, inferred)

## Build System

**Build Tool:**
- CMake 3.22+ — Required minimum version
- Policy: CMP0077 enabled for CMake 3.22+ features

**Build Configuration:**
- Debug and Release builds supported
- Platform-specific optimizations enabled

## Frameworks

**Core:**
- **JUCE 8.0.4** (GIT_TAG v8.0.4) — Audio plugin framework with GUI, DSP, and OSC support
  - Fetched via FetchContent from https://github.com/juce-framework/JUCE.git
  - Fallback: Local JUCE checkout at `../JUCE/` or `./JUCE/` or via `find_package(JUCE)`
  - Public exports: `juce::juce_core`, `juce::juce_audio_basics`, `juce::juce_audio_formats`, `juce::juce_dsp`, `juce::juce_gui_basics`, `juce::juce_osc`

**Testing:**
- **Catch2 3.7.1** (GIT_TAG v3.7.1) — Unit testing framework
  - Fetched via FetchContent from https://github.com/catchorg/Catch2.git
  - Integrated with CTest via `include(Catch)` and `catch_discover_tests()`
  - Tests only (not shipped with library)

**Build/Dev:**
- CMake FetchContent — Dependency management and configuration

## Key Dependencies

**Critical:**
- **libmysofa v1.3.2** (GIT_TAG v1.3.2) — SOFA file parsing and HRTF loading
  - Source: https://github.com/hoene/libmysofa.git
  - Fetched via FetchContent, linked as static library
  - Purpose: Loads Head-Related Transfer Functions (HRTF) from SOFA files for binaural rendering
  - Configuration: `BUILD_STATIC_LIBS=ON`, `BUILD_SHARED_LIBS=OFF`, `BUILD_TESTS=OFF`

**Infrastructure:**
- **zlib** (system package) — Compression for SOFA files
  - Located via `find_package(ZLIB REQUIRED)`
  - Linked as `ZLIB::ZLIB` (PRIVATE)

## Platforms

**Development:**
- **macOS 12.0+** (arm64 architecture, primary)
  - CMAKE_OSX_ARCHITECTURES: `arm64`
  - CMAKE_OSX_DEPLOYMENT_TARGET: `12.0`
  - Compiler flags: `-ffast-math -Wno-nan-infinity-disabled` (enables fast math for DSP)

- **Windows** (MSVC)
  - Compiler flags: `/Zc:preprocessor /fp:fast`
  - Suppresses: `_CRT_SECURE_NO_WARNINGS`

- **Linux** (likely, via GCC/Clang)
  - Inferred from CMakeLists.txt structure (no specific flags, inherits standard C++17 build)

**Production/Deployment:**
- Plugins link SpatialCore as a CMake subdirectory: `add_subdirectory(SpatialCore)`
- Static library (STATIC) — embedded into plugin binaries, no runtime shared object dependency
- Dual-licensed (GPL-3.0 + commercial)

## Build Output

**Library Type:**
- Static library (`libSpatialCore.a` / `SpatialCore.lib`)

**Artifacts:**
- Main library: `libSpatialCore.a` (or Windows equivalent)
- Test executable: `SpatialCoreTests` (when `SPATIALCORE_BUILD_TESTS=ON`, default)
- Header-only includes in `include/SpatialCore/**`

## Compilation Features

**Optimization:**
- `-ffast-math` (macOS) — Enables fast floating-point operations for DSP
- `/fp:fast` (MSVC) — Equivalent Windows fast-math flag

**Compiler Definitions:**
- `_CRT_SECURE_NO_WARNINGS` (MSVC only) — Suppresses deprecation warnings

## Dependencies Summary

| Dependency | Version | Link | Purpose |
|----------|---------|------|---------|
| JUCE | 8.0.4 | Fetched (GitHub) | Audio framework, GUI, OSC, DSP |
| libmysofa | 1.3.2 | Fetched (GitHub, static) | HRTF/SOFA file parsing |
| zlib | system | System package | SOFA compression |
| Catch2 | 3.7.1 | Fetched (GitHub, tests only) | Unit testing |

## Configuration Files

**Build Configuration:**
- `CMakeLists.txt` — Main build configuration
- `tests/CMakeLists.txt` — Test build configuration

**Options:**
- `SPATIALCORE_BUILD_TESTS` (ON by default) — Enable/disable test suite

**Platform Defaults:**
- No `.env` files or environment variable configuration used
- All configuration via CMakeLists.txt

---

*Stack analysis: 2026-08-09*
