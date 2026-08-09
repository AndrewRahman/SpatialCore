# Coding Conventions

**Analysis Date:** 2026-08-09

## Naming Patterns

**Files:**
- Header files: PascalCase with `.h` extension (e.g., `VBAPAlgorithm.h`, `SpeakerLayout.h`)
- Implementation files: PascalCase with `.cpp` extension (e.g., `VBAPAlgorithm.cpp`, `SpeakerLayout.cpp`)
- Test files: Descriptive names ending with `Tests.cpp` (e.g., `SpatializationAlgorithmTests.cpp`, `UtilitiesTests.cpp`)

**Classes and Structs:**
- PascalCase (e.g., `VBAPAlgorithm`, `ObjectState`, `BinauralProfile`, `OutputFormatRegistry`)
- Structs used for simple data containers; classes for functional types
- Base classes prefixed when representing a protocol (e.g., `SpatializationAlgorithm` as abstract base)

**Functions:**
- camelCase starting with lowercase (e.g., `computeGains()`, `layoutHasHeight()`, `cartesianToPolar()`)
- Private/static helpers use same camelCase convention
- Getter functions: no `get` prefix for simple accessors; use `getInfo()`, `getName()` for compound results

**Variables:**
- Local variables: camelCase (e.g., `azimuthRad`, `objectIndex`, `bestDot`)
- Member variables: camelCase with lowercase start (e.g., `headRadius`, `numSpeakers`, `connected`)
- Loop counters: single letters allowed (e.g., `i`, `s`, `t`) only in tight loops under 10 lines

**Constants:**
- Global/static constants: ALL_CAPS with underscores (e.g., `MAX_SOURCES`, `MAX_SPEAKERS`)
- Inline constants: use `constexpr` with camelCase (e.g., `constexpr float kRadToDeg = 180.0f / pi`)

**Namespaces:**
- Single namespace: `spatialcore` containing all public types
- No nested namespaces for public APIs

## Code Style

**Formatting:**
- 4-space indentation (not tabs)
- Opening braces on same line for classes/functions (`class Foo {` not `class Foo\n{`)
- Spacing: `if (condition)` with space after keyword
- Continuation: Multi-line function signatures indent parameters 4 spaces
- Line length: Aim for 100 chars; algorithm comments may exceed

**Linting:**
- No external linter configured (clang-format, eslint, etc.)
- Consistency checked through code review; follow patterns in existing files

**Section Separators:**
- Use 78-char comment separators for major sections:
  ```cpp
  //==============================================================================
  // Section Title Here
  //==============================================================================
  ```
- Applies to namespace intro, class definition, major function groups

## Import Organization

**Order:**
1. `#pragma once` (headers only)
2. SpatialCore includes: `#include <SpatialCore/...>` - project headers
3. JUCE includes: `#include <juce_*.h>` or full module includes
4. Standard library: `#include <vector>`, `<cmath>`, `<string>`, etc.

**Path Aliases:**
- No CMake aliases used; always relative to `include/` for headers
- Consumers include as: `#include <SpatialCore/Algorithms/VBAPAlgorithm.h>`

**Example from `BinauralRenderer.h`:**
```cpp
#pragma once

#include <SpatialCore/Core/Types.h>
#include <SpatialCore/Binaural/PartitionedConvolver.h>
#include <SpatialCore/Binaural/HRTFDatabase.h>
#include <juce_dsp/juce_dsp.h>
#include <vector>
```

## Error Handling

**Patterns:**
- Guard clauses with early return (no exceptions)
  ```cpp
  if (! ctx.triplets.empty())
      computeVBAPGains3D(...);
  else if (layoutHasHeight(ctx.layout))
  {
      jassertfalse;  // Alert developer of logic error
      nearestSpeaker3DFallback(...);
  }
  ```
- Bounds checking with return or clamp:
  ```cpp
  if (objectIndex >= MAX_SOURCES) return;
  if (dist > 1.0f) dist = 1.0f;
  ```
- Type validation before use:
  ```cpp
  if (message.size() >= 1 && message[0].isFloat32())
      azDeg = message[0].getFloat32();
  ```
- No C++ exceptions used in audio-critical code
- Fatal errors: Use `jassertfalse` from JUCE for developer discovery (will abort in debug builds)

## Logging

**Framework:** console-only; no logging macros in production

**Patterns:**
- No logging in functions called from `processBlock()` (audio thread)
- No logging in signal-processing algorithms
- UI components may use `DBG()` JUCE macro for development only
- All logging statements removed before release builds

**When to Log:**
- Do NOT log in real-time audio code paths
- Do log connection/initialization errors in setup methods
- Do log validation failures in non-critical functions

## Comments

**When to Comment:**
- Algorithm descriptions: Always explain the math or reference paper
- Non-obvious calculations: Especially for spatial audio and DSP
- Version tags: Mark changes with `v0.2:`, `v1.0:` prefixes
- Fallback logic: Explain why fallback was chosen

**JSDoc/TSDoc:**
- Use C++ style docstrings: `/** ... */` for public APIs
- Parameters: Document azimuth units (degrees vs. radians)
- Return values: Document gain normalization, expected ranges

**Example from `SpatializationAlgorithm.h`:**
```cpp
/** Compute speaker gains for a source position in the given layout. */
virtual void computeGains (const SourcePosition& source,
                           const LayoutContext& ctx,
                           float* outputGains,
                           int numSpeakers) const = 0;

/** Whether this algorithm can produce direct binaural output (bypassing speakers). */
virtual bool supportsBinauralDirect() const { return false; }
```

## Function Design

**Size:** Keep under 50 lines for most functions; complex algorithms up to 100 lines acceptable

**Parameters:**
- Const references for read-only parameters: `const SourcePosition& source`
- Output via pointer parameters: `float* outputGains` (for performance-critical code)
- Context structs group related parameters: `const LayoutContext& ctx`
- Unused parameters: Comment out in function signature: `const SourcePosition& /*source*/`

**Return Values:**
- Prefer output parameters for performance-critical functions
- Use return values for queries or stateless transformations
- Floating-point results: Return raw float or struct with gains/phases

**Const Correctness:**
- Member functions: `const` if they don't modify state
- Parameters: `const` for all read-only inputs
- Methods on temporary objects: mark `const` to enable rvalue binding

## Module Design

**Exports:**
- One main class per module (e.g., `VBAPAlgorithm` in `Algorithms/VBAPAlgorithm.h`)
- Helper functions in implementation only (`.cpp` file, not `.h`)
- Static factories or builder functions as part of the main class

**Barrel Files:**
- No barrel/index files (e.g., no `Algorithms/index.h`)
- Consumers include specific headers: `#include <SpatialCore/Algorithms/VBAPAlgorithm.h>`

**Stateless Design:**
- Algorithms are pure functions: all state passed via context structs
- No global mutable state except thread-safe singletons (e.g., `SharedFFTCache`)
- Prefer composition over inheritance for algorithm variants

**Example: Stateless Algorithm from `VBAPAlgorithm.cpp`:**
```cpp
void VBAPAlgorithm::computeGains (const SourcePosition& source, const LayoutContext& ctx,
                                   float* outputGains, int /*numSpeakers*/) const
{
    if (! ctx.triplets.empty())
        computeVBAPGains3D (ctx.layout, ctx.triplets, source.azimuthRad, source.elevationRad, outputGains);
    else
        computeVBAPGains2D (ctx.layout, source.azimuthRad, outputGains);
}
```

## Memory Management

**Allocations:**
- No `new`/`delete` in audio-critical code; use stack allocation
- Use `std::unique_ptr` for ownership and RAII
- Use `std::array<T, N>` for fixed-size buffers instead of `std::vector`

**Lifetime:**
- Member objects initialized in constructor
- Use JUCE macro `JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR` for UI components
- No global allocations in real-time paths

## Macros

**JUCE Macros (commonly used):**
- `jassertfalse` - Assert in debug builds, log warning in release
- `JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ClassName)` - Prevent copies, detect leaks
- `DBG(message)` - Debug output to console

**Custom Macros:**
- Minimal; prefer inline functions over macros
- If used, wrap in parentheses: `#define MACRO(x) ((x) * 2)`

## Platform-Specific Code

**Apple/macOS:**
- Compiler flag: `-ffast-math` (enables unsafe float optimizations)
- Minimum deployment: macOS 12.0
- Architecture: arm64 (Apple Silicon)

**Windows/MSVC:**
- Compiler flag: `/fp:fast` (fast floating-point mode)
- Preprocessor: `_CRT_SECURE_NO_WARNINGS` enabled

**Cross-Platform Pattern:**
```cpp
if(APPLE)
    target_compile_options(SpatialCore PRIVATE -ffast-math)
elseif(MSVC)
    target_compile_options(SpatialCore PRIVATE /fp:fast)
endif()
```

---

*Convention analysis: 2026-08-09*
