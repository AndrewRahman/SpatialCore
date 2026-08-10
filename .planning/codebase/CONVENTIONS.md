# Coding Conventions

**Analysis Date:** 2026-08-10

Scope: `src/`, `include/SpatialCore/`, `tests/` (excludes `build/`, `build-map/`, `JUCE/`, `HRTF/`).

## Naming Patterns

**Files:**
- One class/module per header+source pair, matching class name exactly: `PartitionedConvolver.h` / `PartitionedConvolver.cpp`, `RenderEngine.h` / `RenderEngine.cpp`.
- Headers live under `include/SpatialCore/<Category>/`, mirrored by `src/<Category>/` for implementation. Categories: `Algorithms/`, `Binaural/`, `Core/`, `Engine/`, `IO/`, `OSC/`, `Trajectory/`, `UI/`.
- A single umbrella header `include/SpatialCore/SpatialCore.h` re-exports the public API; `Algorithms/AllAlgorithms.h` aggregates all algorithm headers.

**Functions:**
- `camelCase` for member/free functions: `computeGains`, `setOutputFormat`, `evalSH`, `loadIRIntoSlot`.
- Getter/query predicates use `is`/`supports`/`has` prefixes: `supportsBinauralDirect()`, `supportsSHDomain()`, `layoutHasHeight()`.

**Variables:**
- `camelCase` locals and members, no Hungarian prefixes: `fftOrder`, `blockSize`, `irLen`, `activeSlot`.
- Trailing underscore used to disambiguate constructor/setter parameters from same-named members (`irLength_` param vs `irLen` member) — see `PartitionedConvolver::prepare(int maxBlockSize, int irLength_)` in `src/Binaural/PartitionedConvolver.cpp`.
- Constants use `k`-prefixed camelCase in test/anon-namespace scope: `kBinauralProfiles` in `tests/Binaural/WoodworthFallbackTests.cpp`.

**Types:**
- `PascalCase` for classes/structs: `SpatializationAlgorithm`, `SourcePosition`, `LayoutContext`, `BinauralGains` (`include/SpatialCore/Core/Types.h`).
- Enums/state machines use `PascalCase` values inside a scoped enum-like class state, e.g. `PartitionedConvolver::State::{Idle, Warmup, Crossfading}`.

**Namespace:**
- All library code lives in a single `namespace spatialcore { ... }` — no nested namespaces per module. Files open/close the namespace explicitly (not `spatialcore::Foo::bar` qualification).

## Code Style

**Formatting:**
- No `.clang-format` or `.editorconfig` file is present in the repo — formatting is by convention/reviewer discipline, not tool-enforced.
- JUCE-house style is followed throughout: space before parens in calls (`computeGains (source, ctx, ...)`), Allman-ish brace placement (opening brace on its own line for functions/namespaces/control blocks), 4-space indentation, `//====...====` banner comments (80 chars) to separate logical sections within a file.
- Doc comments use JUCE-style `/** ... */` immediately above declarations (see `include/SpatialCore/Algorithms/SpatializationAlgorithm.h`).

**Linting:**
- No ESLint/clang-tidy/cpplint config found in the repo root or `tests/`. Compiler warnings are the only static check (MSVC gets `/Zc:preprocessor` and `_CRT_SECURE_NO_WARNINGS` in `CMakeLists.txt`).

## Import Organization

**Order:**
1. The header matching the current `.cpp` file's own class, via `<SpatialCore/...>` angle-bracket path (not `"..."`), e.g. `#include <SpatialCore/Binaural/PartitionedConvolver.h>` as the first line of `PartitionedConvolver.cpp`.
2. Standard library headers next (`#include <cstring>`, `#include <array>`).
3. No relative includes (`"../Foo.h"`) observed; all cross-module includes go through the `SpatialCore/<Category>/<File>.h` public path, even for internal-only headers.

**Path Aliases:**
- CMake `target_include_directories` exposes `include/` as the public include root (`$<BUILD_INTERFACE:.../include>`), so all consumers (including tests) use `#include <SpatialCore/...>` uniformly — see `CMakeLists.txt`.

## Error Handling

**Patterns:**
- Realtime/audio-path code favors silent clamping over exceptions: `distanceAttenuation` clamps its denominator floor at 0.1 rather than throwing (`src/Core/SpatialMath.cpp`, verified by `tests/Core/SpatialMathTests.cpp:112`).
- `softClip`/`outputLimiter` explicitly guard against non-finite input and return `0.0` rather than propagating NaN/Inf (`src/Core/SpatialMath.cpp`; asserted in `tests/Core/SpatialMathTests.cpp:20` and `:66`).
- `jassert`/`jassertfalse` (JUCE's debug-only assert macros) are used for internal invariant checks that should never fire in correct usage, not for user input validation — e.g. `src/Engine/RenderEngine.cpp:197` (`jassertfalse` on an unreachable branch) and `:638` (`jassert` on a layout/triplet invariant). These compile out in release builds, so they are NOT a substitute for input validation on the audio path.
- No C++ exceptions are thrown from DSP/audio-path code; OSC input parsing (`src/OSC/ADMOSCReceiver.cpp`) silently ignores malformed/out-of-range addresses rather than throwing (verified by `tests/OSC/ADMOSCReceiverTests.cpp` "Malformed address..." and "Out-of-range object number..." cases).
- Zero TODO/FIXME/HACK/XXX comments found anywhere under `src/` or `include/` — deferred work is not left as inline markers in this codebase; it is tracked externally (issue numbers appear in comments instead, e.g. `// Process-global FFT cache (issue #131)` and `// v1.0.5: Dual-convolver IR loading strategy (issue #50)` in `src/Binaural/PartitionedConvolver.cpp`).

## Realtime-Safety Idioms

This is the most consequential convention in the codebase (per `CLAUDE.md`: "NEVER allocate memory in any function called from processBlock").

- **Pre-allocate in `prepare()`, never in `process()`:** all `std::vector` sizing (`.assign`, `.resize`) for FFT/convolution buffers happens inside `PartitionedConvolver::prepare()`, called once at setup time — not inside the per-block processing path (`src/Binaural/PartitionedConvolver.cpp:31-68`).
- **Process-global singleton for shared expensive state:** `SharedFFTCache` (`include/SpatialCore/Binaural/SharedFFTCache.h`, impl in `src/Binaural/PartitionedConvolver.cpp:14-29`) is a lazily-populated, lock-protected (`juce::SpinLock`) cache of `juce::dsp::FFT` instances keyed by FFT order, held via `shared_ptr` for the lifetime of the process. Comment explains the rationale: Apple's vDSP shares twiddle-factor memory across same-order FFT setups, so destroying the last user of an order can free memory another thread is still reading.
- **Dual-slot / crossfade pattern for lock-free hot-swap:** `PartitionedConvolver` uses two `ConvSlot`s (`slots[2]`) with an `activeSlot` index and a `State` enum (`Idle`/`Warmup`/`Crossfading`) to swap IRs without locking the audio thread — new IR data is loaded into the inactive slot, then the state machine crossfades over several blocks (`src/Binaural/PartitionedConvolver.cpp:84-118`).
- **Deferred updates during transitions:** if a new IR arrives while already mid-transition, it is queued into a pre-allocated `pendingIR` buffer instead of allocating or racing the in-flight crossfade (`src/Binaural/PartitionedConvolver.cpp:100-109`).
- **Atomic/dual-buffer layout swap:** per `ARCHITECTURE.md`/`CLAUDE.md` design principles, output-format/layout changes use dual-buffered state with atomic swap so the audio thread never blocks on a layout change (see `RenderEngine::setOutputFormat`, exercised by `tests/Engine/RenderEngineTests.cpp:314`).
- **Stateless, pure-function algorithms:** `SpatializationAlgorithm::computeGains` implementations take all state via `SourcePosition`/`LayoutContext` parameters and write into caller-owned `outputGains` buffers — no internal mutable state, no allocation (`include/SpatialCore/Algorithms/SpatializationAlgorithm.h`).

## JUCE Patterns In Use

- **JUCE modules used:** `juce_core`, `juce_audio_basics`, `juce_audio_formats` (FLAC enabled), `juce_dsp` (for `juce::dsp::FFT`), `juce_osc`, `juce_events` — see `tests/CMakeLists.txt` link graph and `CMakeLists.txt` target sources.
- **`juce::String` for names/identifiers:** `SpatializationAlgorithm::getName()` returns `juce::String`, not `std::string`.
- **`juce::SpinLock`** (not `std::mutex`) guards the shared FFT cache — appropriate for the very short critical section on a realtime-adjacent path.
- **UI layer isolated as a separate CMake target (`SpatialCoreUI`)**, deliberately excluded from the DSP-only `SpatialCore` static library so consumer plugins can link the engine without pulling in GUI code — see the `NOTE:` comment above the `add_library(SpatialCore STATIC ...)` block in `CMakeLists.txt`. UI components (`SpatialMapComponent`, `SMLLookAndFeel`, `ReverseSlider`, `IndicatorToggle`, `StyledButton`, `GlobalTapDrawer`, `IOSectionComponent`, `OSCSectionComponent`, `ObjectPanel`, `PresetBrowser`) live under `src/UI/` and `include/SpatialCore/UI/`.
- **`JUCE_USE_CURL=0`** compile definition applied to both `SpatialCore` and `SpatialCoreUI` targets to avoid a Linux libcurl link failure — a deliberate, documented workaround rather than a default.
- **`JUCE_UNIT_TESTS=1`** compile definition is set only on the `SpatialCoreTests` target, gating a synchronous test-only entry point (`ADMOSCReceiver::testProcessOSCMessage()`) that bypasses the normal async OSC dispatch for deterministic testing (`tests/CMakeLists.txt`).
- **degrees/radians conversion via `juce::degreesToRadians`** used consistently at test/API boundaries rather than hand-rolled math (`tests/Binaural/WoodworthFallbackTests.cpp`).

## Comments

**When to Comment:**
- Section-banner comments (`//===...===`) divide files into logical regions (e.g. `// --- Tests ---` in `CMakeLists.txt`, `//=== Modular 3D Audio Core -- 6th-Order Ambisonics (ACN/SN3D) ===` in `src/Core/SpatialMath.cpp`).
- Non-obvious platform/threading rationale is documented inline at the point of the workaround, with issue numbers where applicable (see `SharedFFTCache` comment above).
- Provenance/extraction comments are common given the OpenSpatialDelay origin: `FROZEN (D-02) — moved verbatim from Source/PluginProcessor.h:104-132. Do not add, remove, or change any virtual method signature.` on `SpatializationAlgorithm` (`include/SpatialCore/Algorithms/SpatializationAlgorithm.h:11-12`) — this is a hard constraint enforced by comment, not compiler.

**JSDoc/TSDoc equivalent:**
- Public API methods on interfaces get a one-line `/** ... */` doc comment describing behavior and calling contract (e.g. each virtual method on `SpatializationAlgorithm`).

## Function Design

**Size:** Small, single-purpose private helpers (`loadIRIntoSlot`, `resetSlot`) factor out repeated logic from larger public methods (`prepare`, `setIR`).

**Parameters:** Raw pointer + length pairs are used for audio buffers on hot paths (`const float* ir, int length`, `float* outputGains, int numSpeakers`) rather than `std::vector`/`juce::AudioBuffer` — consistent with realtime, allocation-free design.

**Return Values:** Algorithm interface methods return small value types (`bool`, `juce::String`, `BinauralGains` struct) or write through output parameters for bulk data (gain arrays).

## Module Design

**Exports:** Each `.h` in `include/SpatialCore/` is a self-contained public header guarded with `#pragma once`; there is no barrel/index file per category (no `Algorithms/index.h`), except the umbrella `AllAlgorithms.h` which aggregates all 8 algorithm headers for convenience, and the top-level `SpatialCore.h`.

**Interface Freezing:** Some interfaces are marked `FROZEN` in comments with an explicit versioning rule (see `CLAUDE.md`: "NEVER modify the SpatializationAlgorithm interface without bumping the major version") — treat any change to `SpatializationAlgorithm`'s virtual methods as a breaking, major-version change.

---

*Convention analysis: 2026-08-10*
