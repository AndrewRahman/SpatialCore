# Technology Stack

**Analysis Date:** 2026-08-10

> Verified against branch `gsd-remap` (HEAD `8d1868e`, merge of `origin/spatialcore-v2-extraction` + `origin/main`) by reading `CMakeLists.txt`, `tests/CMakeLists.txt`, `.github/workflows/ci.yml`, and running `ls`/`find`/`file` against the tree. Where CLAUDE.md/README.md disagree with the tree, the tree wins and the contradiction is flagged below.

## Languages

**Primary:**
- C++17 — all DSP/engine code under `src/` and `include/SpatialCore/` (`CMakeLists.txt:7-8`: `set(CMAKE_CXX_STANDARD 17)`, `set(CMAKE_CXX_STANDARD_REQUIRED ON)`; also `target_compile_features(SpatialCore PUBLIC cxx_std_17)` at `CMakeLists.txt:163`)
- CMake (build DSL) — `CMakeLists.txt` (top level), `tests/CMakeLists.txt`

**Secondary:**
- None detected. No Python, JavaScript, Swift, or Objective-C sources found (`find . -name "*.py" -o -name "*.swift" -o -name "*.m" -o -name "*.mm"` under `src/`/`include/` returns nothing beyond JUCE's own `.mm` files, which are excluded from analysis).

## Runtime

**Environment:**
- Native C++ static library, no interpreted runtime. Builds to `libSpatialCore.a` and `libSpatialCoreUI.a` (`CMakeLists.txt:70` `add_library(SpatialCore STATIC ...)`, `CMakeLists.txt:187` `add_library(SpatialCoreUI STATIC ...)`).
- Target platforms per CI: macOS (arm64 pinned, `CMakeLists.txt:11-13`: `CMAKE_OSX_ARCHITECTURES "arm64"`, `CMAKE_OSX_DEPLOYMENT_TARGET "12.0"`) and Linux (`ubuntu-latest` in `.github/workflows/ci.yml:12`). No Windows CI job present, though MSVC-specific compile flags exist (`CMakeLists.txt:16-19`).

**Package Manager:**
- No Node/Python/Rust package manager — dependencies are fetched via CMake `FetchContent` (JUCE, libmysofa, Catch2) or resolved via `find_package(ZLIB REQUIRED)` (`CMakeLists.txt:60`).
- Lockfile: none (CMake FetchContent pins by Git tag/commit, not a lockfile format).

## Frameworks

**Core:**
- JUCE **9.0.0** (FetchContent fallback) — `CMakeLists.txt:31-34`: `GIT_REPOSITORY https://github.com/juce-framework/JUCE.git`, `GIT_TAG 9.0.0`. **This contradicts CLAUDE.md, which states "JUCE 8"** — the tree/CI confirm 9.0.0 (also stated explicitly in `.github/workflows/ci.yml:13`: `name: Build + Test (standalone, JUCE 9.0.0 fallback)`). CLAUDE.md is out of date on this point.
  - JUCE resolution order (`CMakeLists.txt:20-40`): `TARGET juce::juce_core` already defined by consumer → sibling `../JUCE` checkout → local `JUCE/` subdir → `find_package(JUCE)` → FetchContent fallback pinned to 9.0.0.
  - JUCE modules linked into `SpatialCore` (DSP-only): `juce_core`, `juce_audio_basics`, `juce_audio_formats`, `juce_dsp`, `juce_osc` (`CMakeLists.txt:135-141`).
  - JUCE modules linked into `SpatialCoreUI` (GUI-only): `juce_gui_basics`, `juce_graphics` (`CMakeLists.txt:227-230`). No `juce_audio_plugin_client` or `juce_gui_extra` linked by SpatialCore itself — this is a library, not a plugin wrapper.
  - `JUCE_USE_CURL=0` set on both targets to avoid libcurl link errors on Linux (`CMakeLists.txt:159`, `CMakeLists.txt:242`).

**Testing:**
- Catch2 **v3.7.1** (FetchContent) — `tests/CMakeLists.txt:2-6`: `GIT_REPOSITORY https://github.com/catchorg/Catch2.git`, `GIT_TAG v3.7.1`. Linked via `Catch2::Catch2WithMain` (`tests/CMakeLists.txt:27`).
- CTest + `catch_discover_tests` for test registration (`tests/CMakeLists.txt:38-40`).

**Build/Dev:**
- CMake **>= 3.22** (`CMakeLists.txt:1`: `cmake_minimum_required(VERSION 3.22)`).
- `CMAKE_POLICY_VERSION_MINIMUM 3.5` forced (`CMakeLists.txt:3`) — compatibility shim for FetchContent'd dependencies using older CMake minimums.

## Key Dependencies

**Critical:**
- **libmysofa v1.3.2** — SOFA HRTF file parsing, `CMakeLists.txt:44-56`: `GIT_REPOSITORY https://github.com/hoene/libmysofa.git`, `GIT_TAG v1.3.2`, built as static lib (`BUILD_STATIC_LIBS ON`, `BUILD_SHARED_LIBS OFF`, `BUILD_TESTS OFF`). Guarded by `if(NOT TARGET mysofa-static)` to avoid duplicate-target errors when consumed by a parent project (e.g. OpenSpatialDelay) that already fetches the same tag.
- **zlib** (system) — `find_package(ZLIB REQUIRED)` (`CMakeLists.txt:60`), linked `PRIVATE` into `SpatialCore` (`CMakeLists.txt:145-149`). Required transitively by libmysofa's HDF5-lite SOFA reader.

**Infrastructure:**
- None (no database, no message queue, no HTTP client library — `JUCE_USE_CURL=0` explicitly disables JUCE's optional libcurl-backed URL support since it's unused).

## Configuration

**Environment:**
- No `.env` files present (`ls -la .env* 2>/dev/null` returns nothing at repo root).
- Build-time configuration is via CMake cache variables/options, not environment variables. One test-only compile definition: `SPATIALCORE_HRTF_DIR` (absolute path to `HRTF/` for test fixtures, `tests/CMakeLists.txt:34`) and `JUCE_UNIT_TESTS=1` (exposes `ADMOSCReceiver::testProcessOSCMessage()`, `tests/CMakeLists.txt:35`).

**Build:**
- `CMakeLists.txt` (repo root) — defines `SpatialCore` (DSP-only static lib) and `SpatialCoreUI` (GUI static lib) targets plus dependency fetches.
- `tests/CMakeLists.txt` — defines `SpatialCoreTests` executable, fetches Catch2, registers 17 test source files across `Algorithms/`, `IO/`, `OSC/`, `Trajectory/`, `Core/`, `Binaural/`, `Engine/` subdirectories.
- `.github/workflows/ci.yml` — single job `build-and-test`, runs on `ubuntu-latest`, triggers on push/PR to `main` and `spatialcore-v2-extraction`, plus manual `workflow_dispatch`.

## Platform Requirements

**Development:**
- CMake 3.22+, a C++17 toolchain, Git LFS (for `HRTF/*.sofa` — see INTEGRATIONS.md), and system packages for JUCE's Linux GUI backend if building `SpatialCoreUI` locally (ALSA, JACK, X11, WebKit2GTK, etc. — full list in `.github/workflows/ci.yml:37-42`: `libasound2-dev libjack-jackd2-dev ladspa-sdk libcurl4-openssl-dev libfreetype6-dev libx11-dev libxcomposite-dev libxcursor-dev libxext-dev libxinerama-dev libxrandr-dev libxrender-dev libwebkit2gtk-4.1-dev libglu1-mesa-dev mesa-common-dev zlib1g-dev`).
- macOS builds pin `arm64` only (no `x86_64` in `CMAKE_OSX_ARCHITECTURES`), deployment target 12.0 (`CMakeLists.txt:11-13`).

**Production:**
- SpatialCore is not deployed standalone — it is consumed as a CMake subdirectory / git submodule by host plugins (per CLAUDE.md's "Consumer Plugins" section — this describes the intended packaging model, consistent with the `PROJECT_IS_TOP_LEVEL` branching throughout `CMakeLists.txt` that changes link visibility (`PUBLIC` vs `INTERFACE`) depending on whether SpatialCore is built standalone or `add_subdirectory()`'d into a consumer).
- CI (`.github/workflows/ci.yml`) only validates the standalone build (library + `SpatialCoreUI` + test suite via `ctest`); no consumer-plugin integration build is present in this repo.

## Build Targets

| Target | Type | Sources | Purpose |
|--------|------|---------|---------|
| `SpatialCore` | STATIC lib | `src/Algorithms/*.cpp`, `src/Core/SpatialMath.cpp`, `src/Binaural/*.cpp`, `src/IO/*.cpp`, `src/OSC/*.cpp`, `src/Trajectory/*.cpp`, `src/Engine/RenderEngine.cpp` | DSP-only engine, no GUI modules (`CMakeLists.txt:70-92`) |
| `SpatialCoreUI` | STATIC lib | `src/UI/*.cpp` (10 files: SMLLookAndFeel, ReverseSlider, IndicatorToggle, StyledButton, SpatialMapComponent, GlobalTapDrawer, PresetBrowser, IOSectionComponent, OSCSectionComponent, ObjectPanel) | GUI widget library, separate from DSP-only target (`CMakeLists.txt:184-196`) |
| `SpatialCoreUIFontData` | BinaryData lib | `fonts/*.ttf` (DM Sans x4, JetBrains Mono x3, Roboto Medium) | Embedded font binary data for `SMLLookAndFeel` (`CMakeLists.txt:207-219`) |
| `SpatialCoreTests` | executable | 17 files under `tests/` | Catch2 test suite, registered with CTest (`tests/CMakeLists.txt:9-25`) |

---

*Stack analysis: 2026-08-10*
