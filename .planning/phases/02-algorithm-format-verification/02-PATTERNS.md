# Phase 2: Algorithm & Format Verification - Pattern Map

**Mapped:** 2026-09-30
**Files analyzed:** 27 (9 src/include, 6 existing test files extended, 1 new test optional, 5 reference files, 6 doc sites)
**Analogs found:** 24 / 27 (all analogs are git-tracked; verified `git ls-files` on the src/CMake files, all others are under tracked `src/`, `include/`, `tests/`)

Test binary: `build/tests/SpatialCoreTests`. Known pre-existing failure: HUTUBS PP2 checksum (F8).

## File Classification

| New/Modified File | Role | Data Flow | Closest Analog | Match |
|---|---|---|---|---|
| `src/Core/SpatialMath.cpp` (`evalSH` SN3D fix, `computeVBAPGains2D/3D` guards + lower pass + best-triplet) | utility | transform (audio thread) | itself (existing 3D loop, `:223-291`) | self |
| `include/SpatialCore/Core/SpatialMath.h` (docblock, deprecation comment) | utility/header | - | itself | self |
| `src/IO/AmbisonicsCodec.cpp` (forwarder, `numSpeakers > MAX_SPEAKERS` early return) | utility | transform | itself | self |
| `src/IO/SpeakerLayout.cpp` + `include/.../SpeakerLayout.h` (shared height constant, `appendLowerHemisphereTriplets`, `VBAPTriplet` flag fields) | model/utility | batch (message thread) | `buildVBAPTripletsForLayout` (`SpeakerLayout.cpp:106-167`) | exact |
| `src/Engine/RenderEngine.cpp` + `RenderEngine.h` (guard, D-09 adapter, sanitiser members, delete `computeAmbiDecodeForLayout`) | service | request-response | `activateLayout` (`:575-644`), `gainScratch_ = blockCtx;` (`:75`) | self |
| `src/Algorithms/{VBAP,VBIP,MDAP}Algorithm.cpp` (delete fallback branch, VBIP sqrt, MDAP cite) | service | transform | `VBIPAlgorithm.cpp:10-40` | self |
| `tests/Algorithms/SpatializationAlgorithmTests.cpp` (panning-law, textbook, robust, ear) | test | transform | same file `:23-53` helpers + `:186-219` goldens | exact |
| `tests/Core/SpatialMathTests.cpp` (SN3D invariant, 49 literals, sanitiser) | test | transform | same file `:1-25` | exact |
| `tests/IO/AmbisonicsCodecTests.cpp` (forwarder, round trip) | test | transform | same file `:1-30` | exact |
| `tests/IO/SpeakerLayoutTests.cpp` (static_assert, D-03, lower builder) | test | batch | same file `:133-150`, `:221-235` | exact |
| `tests/Engine/RenderEngineTests.cpp` (23-format resolve, `[ambi-pin]`, `[sanitize]`, ear via engine) | test | request-response | same file `:314-332` | exact |
| `tests/Core/CountsTests.cpp` (AllAlgorithmTypes enumeration reuse) | test | - | same file `:15-28`, `:62` | exact |
| `tests/CMakeLists.txt` (only if a new .cpp) | config | - | same file `:9-27` | exact |
| `tests/reference/*.py`, `*.h`, `README.md` | utility (offline) | batch | RESEARCH Appendix (no in-repo analog) | none |
| Docs: `README.md`, `docs/integration-guide.md`, `.claude/skills/spatial-audio-dsp/SKILL.md`, `*Algorithm.h` | docs | - | RESEARCH Pattern 6 site list | n/a |

## Pattern Assignments

### Test files: Catch2 conventions (applies to every `tests/**` edit)

**Includes / namespace** (`tests/Algorithms/SpatializationAlgorithmTests.cpp:1-9`):
```cpp
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <SpatialCore/Algorithms/AllAlgorithms.h>
#include <SpatialCore/IO/SpeakerLayout.h>
#include <memory>
#include <cmath>

using namespace spatialcore;
using Catch::Matchers::WithinAbs;
```
**Style:** `TEST_CASE ("Area: sentence", "[area][subtag]")`, `CHECK_THAT (x, WithinAbs (v, 1e-5f))`, `CHECK`/`REQUIRE`, `SECTION(exp.name)` inside table loops, helpers in an anonymous `namespace {}`. Tags used today: `[algorithms]`, `[vbap]`, `[golden]`, `[io][layout]`, `[ambisonics]`, `[spatialmath]`, `[counts]`, `[engine][renderblock]`. New tags (registered by use): `[ear] [sn3d] [roundtrip] [sanitize] [robust] [panning-law] [ambi-pin] [d03] [consumer-surface]`.

**Hand-built layout + context helpers to reuse** (`SpatializationAlgorithmTests.cpp:25-52`):
```cpp
SpeakerLayout makeQuadLayout() { SpeakerLayout layout {}; layout.numSpeakers = 4; layout.lfeChannelIndex = -1;
    layout.totalChannels = 4; const float az[] = { 45.0f, -45.0f, 135.0f, -135.0f };
    for (int i = 0; i < 4; ++i) { layout.speakers[i].azimuthRad = juce::degreesToRadians (az[i]);
        layout.speakers[i].elevationRad = 0.0f; layout.speakers[i].channelIndex = i; } return layout; }
SourcePosition makeSource (float azDeg, float elDeg, float distance = 0.5f)
    { return { juce::degreesToRadians (azDeg), juce::degreesToRadians (elDeg), distance }; }
LayoutContext makeCtx (const SpeakerLayout& layout, const std::vector<VBAPTriplet>& triplets,
                       const float (*ambiMatrix)[MAX_SPEAKERS], int ambiNumSpeakers)
    { return { layout, triplets, ambiMatrix, ambiNumSpeakers }; }   // 4-initialiser aggregate: do NOT add members
```
D-03 regression: copy `makeQuadLayout` shape, build a 5-speaker layout with one speaker at 0.8 degrees, assert `layoutHasHeight(l) == ! triplets.empty()` after `buildVBAPTripletsForLayout`.

**Golden-vector style** (`:186-201`, VBIP golden at `:203-219` must be regenerated to `0.8880738 / 0.4597008`; edit the test title "squared VBAP" too):
```cpp
VBAPAlgorithm algo; SpeakerLayout layout = makeQuadLayout();
std::vector<VBAPTriplet> triplets; float ambiMatrix[1][MAX_SPEAKERS] = {};
LayoutContext ctx = makeCtx (layout, triplets, ambiMatrix, 0);
float gains[4] = {}; algo.computeGains (makeSource (30.0f, 0.0f), ctx, gains, 4);
CHECK_THAT (gains[0], WithinAbs (0.9659258f, 1e-5f));
```
Textbook vs regression: keep the "pre-move OSD" comment for regression goldens; add new textbook cases beside them citing the formula. DBAP existing golden at dist 0.5 already equals Lossius at R=12.04 dB (F2): update only its comment. Quad code order is `45,-45,135,-135`; DBAP az30 = `0.9984304, 0.0459007, 0.0270259, 0.0173052`.

**Enumerating all algorithms** (`tests/Core/CountsTests.cpp:15-28`, use at `:62`) - copy verbatim for D-13 per-algorithm property loops; never hand-list the 8:
```cpp
template <typename... Algorithms>
std::vector<std::unique_ptr<SpatializationAlgorithm>>
instantiateAll (AlgorithmTypeList<Algorithms...>)
{
    std::vector<std::unique_ptr<SpatializationAlgorithm>> out;
    out.reserve (sizeof...(Algorithms));
    (out.emplace_back (std::make_unique<Algorithms>()), ...);
    return out;
}
// use: const auto algorithms = instantiateAll (AllAlgorithmTypes{});
```
Put this helper in an anonymous namespace in `SpatializationAlgorithmTests.cpp` (copy; do not cross-include test .cpp files). Filter by `supportsSurround()`/`supportsSHDomain()`/`supportsBinauralDirect()` for which algorithms get gain-sum/mirror checks (Ambisonics is SH-domain; DirectBinaural is not surround).

**Height-layout tests get real layouts + triplets** (pattern from `SpeakerLayoutTests.cpp:221-235`):
```cpp
const auto& layout = getLayoutDef(exp.id);
std::vector<VBAPTriplet> triplets;
buildVBAPTripletsForLayout(layout, triplets);
```
Then call `appendLowerHemisphereTriplets (layout, triplets)` in `[ear]` tests to mimic `activateLayout`. Use fixed seeds (`std::mt19937_64 rng (42)`, <= 200k points), self-filtering tie helper for mirror/continuity on height layouts (RESEARCH Validation Architecture scoping table).

### `tests/IO/SpeakerLayoutTests.cpp`

**Table tie-in** - table is at `:133` (not `:87`, F14). Add right after the array:
```cpp
static const LayoutExpectation kLayoutExpectations[] = { { "Quad", LayoutID::Quad, 4, 4, -1, false }, /* ...15 rows */ };
static_assert (sizeof (kLayoutExpectations) / sizeof (kLayoutExpectations[0]) == NUM_LAYOUT_DEFS,
               "kLayoutExpectations must have one row per layout def");
```
**D-02(c) strengthening** (`:221-235`): this test calls `buildVBAPTripletsForLayout` (regular only, F11). Keep it regular-only; do not merge lower-hemisphere triplets into it. Add a separate `[ear]` case calling `appendLowerHemisphereTriplets` and asserting `> 0` extras per height layout, `lowerHemisphere == true` on those, and `== false` on regular ones.

### `src/IO/SpeakerLayout.cpp` (builder analog for `appendLowerHemisphereTriplets`, D-03)

**Thresholds to unify** (`:89-98`, `:108-117`):
```cpp
bool layoutHasHeight (const SpeakerLayout& layout)
{   for (int s = 0; s < layout.numSpeakers; ++s)
        if (std::abs (layout.speakers[s].elevationRad) > 0.0175f)  // ~1 degree
            return true;
    return false; }
void buildVBAPTripletsForLayout (const SpeakerLayout& layout, std::vector<VBAPTriplet>& triplets)
{   triplets.clear(); const int N = layout.numSpeakers;
    bool hasHeight = false;
    for (int s = 0; s < N; ++s)
        if (std::abs (layout.speakers[s].elevationRad) > 0.01f) { hasHeight = true; break; }
    if (! hasHeight) return;
    for (int a = 0; a < N - 2; ++a) for (int b = a + 1; b < N - 1; ++b) for (int cc = b + 1; cc < N; ++cc)
    { auto toCart = [](float az, float el) -> std::array<float, 3> { /* cos(el)*sin(az), cos(el)*cos(az), sin(el) */ };
```
Replace both literals with one `constexpr float kHeightThresholdRad` used by both (declare in `SpeakerLayout.h` or an anonymous namespace shared by both functions; `builder` should simply call `layoutHasHeight`). New builder copies the `a<b<cc` triple loop, `toCart` lambda, `push_back` of `VBAPTriplet` with `inv` from the 3x3 inverse; add double precision and the supporting-plane test per RESEARCH Pattern 1 (lines 386-409). Message-thread allocation via `push_back` is allowed.

### `src/Engine/RenderEngine.cpp` `activateLayout` (D-02a, D-04, D-09)

**Existing tail to modify** (`:633-644`):
```cpp
    computeAmbiDecodeForLayout (buf.layout, buf.ambiDecodeMatrix, buf.ambiNumSpeakers);   // D-09: replace with getDecodeMatrix adapter (Pattern 5)
    buildVBAPTripletsForLayout (buf.layout, buf.vbapTriplets);
    jassert (buf.vbapTriplets.empty() == ! layoutHasHeight (buf.layout));                  // D-02a: Debug+Release crash
    // Atomic swap: audio thread now reads the fully-populated buffer
    activeLayoutIndex.store (prepareLayoutIndex, std::memory_order_release);
    prepareLayoutIndex = 1 - prepareLayoutIndex;
```
Insert `appendLowerHemisphereTriplets (buf.layout, buf.vbapTriplets);` AFTER the guard and BEFORE the `store`. The guard body: `jassertfalse; std::abort();` (RESEARCH Code Examples `:623`). Non-height and Ambisonics/Binaural early-return branches (`:600-627`) already `clear()` then `store`+flip; leave them alone.

**Publish pattern (do not invent new sync):** write the inactive `layoutBuffers[prepareLayoutIndex]`, then `activeLayoutIndex.store (prepareLayoutIndex, std::memory_order_release); prepareLayoutIndex = 1 - prepareLayoutIndex;`. `LayoutState` (`RenderEngine.h:241-248`):
```cpp
struct LayoutState { OutputFormat format = OutputFormat::Binaural; SpeakerLayout layout {};
    float ambiDecodeMatrix[MAX_SPEAKERS][MAX_SPEAKERS] = {}; int ambiNumSpeakers = 0;
    std::vector<VBAPTriplet> vbapTriplets; };
```
Extras live in `vbapTriplets` (flagged); no new `LayoutState`/`LayoutContext` member.

**Sanitiser scratch idiom** (copy from `RenderEngine.cpp:75`, `gainScratch_ = blockCtx;`): trivially-copyable member copied at top of `renderBlock`, preallocated; lastGood arrays sized `MAX_SOURCES`, defaults az 0, el 0, distance 0.5f (`Types.h:45-51`). `.cpp` only (F9).

**Engine-test pattern** (`tests/Engine/RenderEngineTests.cpp:314-332`, 81-110):
```cpp
RenderEngine engine; engine.prepare (kSampleRate, kBlockSize);
engine.setOutputFormat (OutputFormat::Surround5_1);
CHECK (engine.getActiveLayout().format == OutputFormat::Surround5_1);
CHECK (engine.getActiveLayout().layout.numSpeakers > 0);
// render: SourceFixture fixture; RenderSources sources = fixture.makeSources();
// RenderBlockContext ctx; ctx.sampleRate = kSampleRate; ... engine.renderBlock (sources, ctx, outPtrs, n);
```
Helpers available: `SourceFixture`, `allFinite`, `anyNonzero`, `allZero` (anonymous namespace, `:22-75`). `[ambi-pin]`: loop `OutputFormat` values that map to layouts, `setOutputFormat(f)`, read `getActiveLayout().ambiDecodeMatrix`, compare to test-local `referenceAmbiDecode` copied from `git show bf10fac:src/Engine/RenderEngine.cpp` (lines 653-734). Tolerance 1e-6.
Criterion-3 23-format loop: iterate `for (int i = 0; i < NUM_OUTPUT_FORMATS; ++i)` over `OutputFormatRegistry::table` (idiom in `CountsTests.cpp:41-44`).

### `src/Algorithms/VBIPAlgorithm.cpp` (D-01, D-14)

**Current code to change** (`:10-40`, shown above in classification; fallback branch `else if (layoutHasHeight ...) { jassertfalse; nearestSpeaker3DFallback ... }` goes away in all 4 sites). Target shape (RESEARCH Pattern 2 + 3):
```cpp
if (! ctx.triplets.empty()) computeVBAPGains3D (ctx.layout, ctx.triplets, source.azimuthRad, source.elevationRad, outputGains);
else                        computeVBAPGains2D (ctx.layout, source.azimuthRad, outputGains);
jassert (ctx.triplets.empty() == ! layoutHasHeight (ctx.layout));      // Debug-only
for (int s = 0; s < numSpeakers; ++s) { outputGains[s] = std::sqrt (std::max (0.0f, outputGains[s])); sum += outputGains[s] * outputGains[s]; }
if (sum > 1e-12f) { const float scale = 1.0f / std::sqrt (sum); for (...) outputGains[s] *= scale; }
```
MDAP (`:48`, `:101`) has two call sites; same deletion. Fix citation `Pulkki 2000` -> `Pulkki, WASPAA 1999` in the 5 sites (F12) - comment-only.

### `src/Core/SpatialMath.cpp`

**3D path**: loop + `-1e-6f` tolerance + `sum < bestGainSum` min-sum rule must stay byte-for-byte in pass 1 (D-07). Structure in RESEARCH Pattern 2 (`:411-444`). Replace unbounded `while (azimuthRad > pi) azimuthRad -= 2pi;` loops in `computeVBAPGains2D` with `std::remainder` after an `isfinite` check; non-finite returns zeroed output (`for s<N: out[s]=0`). Fallback when no triplet: largest-min-gain candidate, clamp negatives, renormalise; `jassert (best >= 0)` is Debug-only.

**`evalSH`**: edit the 22 case lines listed in RESEARCH Reference Data B (`:519-553`) - constants only, shape/sign unchanged. `AmbisonicsCodec::evaluateSH` (`src/IO/AmbisonicsCodec.cpp:13`) becomes `return spatialcore::evalSH (acn, az, el);` (keep declaration at `AmbisonicsCodec.h:23`).

**D-20** at top of `getDecodeMatrix` (`AmbisonicsCodec.cpp:139`): `if (numSpeakers > MAX_SPEAKERS) return;` (the `E[MAX_AMBI_CHANNELS][MAX_SPEAKERS]` array is at `:148`).

### `tests/Core/SpatialMathTests.cpp` and `tests/IO/AmbisonicsCodecTests.cpp`

Existing style (`AmbisonicsCodecTests.cpp:9-30`):
```cpp
static constexpr float kPi = 3.14159265358979323846f;
static constexpr float kDegToRad = kPi / 180.0f;
TEST_CASE("AmbisonicsCodec: evaluateSH order 0 (W channel) is always 1", "[io][ambisonics]")
{   CHECK_THAT(AmbisonicsCodec::evaluateSH(0, 0.0f, 0.0f), WithinAbs(1.0f, 1e-5f)); }
```
Add: forwarder equality (`evalSH(c,az,el) == AmbisonicsCodec::evaluateSH(c,az,el)` exact, 49 channels, grid), `[sn3d]` per-order sum-of-squares == 1 (orders 1-6, tol 2e-5), `[sn3d][golden]` 49 literals from generated `tests/reference/ShReference.h` (`#include "../reference/ShReference.h"`; tol 1e-5), `[roundtrip]` with a test-local dense decoder using `AmbisonicsCodec::encode` (do not use shipped `getDecodeMatrix` beyond order 1, F6). `SpatialMathTests.cpp` non-finite style to copy for sanitiser/robust checks (`:22-27`): `CHECK (softClip (std::numeric_limits<float>::quiet_NaN()) == 0.0f);`.
Robust watchdog: sweep NaN/+-inf/1e9 for every algorithm on Quad and 7.1.4; hang detection per RESEARCH `:646`.

### `tests/CMakeLists.txt`

Explicit list, no glob (`:9-27`). Add a new `.cpp` by hand next to its sibling:
```cmake
add_executable(SpatialCoreTests
    Algorithms/SpatializationAlgorithmTests.cpp
    ...
    Core/CountsTests.cpp
    Engine/RenderEngineTests.cpp      # new e.g. Core/ConsumerSurfaceTests.cpp goes here
)
```
Generated headers in `tests/reference/` are include-only (no CMake change); if included relatively from `tests/<dir>/X.cpp` use `"../reference/ShReference.h"`. `catch_discover_tests(SpatialCoreTests)` picks tags automatically.

## Shared Patterns

### DR-1 (audio path)
**Apply to:** `SpatialMath.cpp`, `*Algorithm.cpp`, `renderBlock` sanitiser. No malloc/lock/log; `jassert`/`jassertfalse` only as Debug diagnostic; all storage preallocated (`LayoutState`, engine members). Layout-build functions (`SpeakerLayout.cpp`, `activateLayout`) may allocate.

### isfinite guards
**Apply to:** every guard. Place in a SpatialCore `.cpp` only, never header-inline (OSD uses `-ffast-math`, F9). No `[[deprecated]]` on `nearestSpeaker3DFallback`; comment-only deprecation at `SpatialMath.h:40-66`.

### Frozen surfaces (DR-2/3/7)
`SpatializationAlgorithm` virtuals, `LayoutContext` 4-member aggregate, `setOutputFormat` `void`, `evalSH` and `evaluateSH` both public. `VBAPTriplet` extra fields are default-initialised (OSD does `VBAPTriplet tri; tri.i = ...`).

### Oracle discipline
Reference values come from the checked-in generators (RESEARCH Appendix `:823-1020`: `gen_sh_reference.py`, `gen_ear_reference.py`, `gen_panning_reference.py`, `layouts_from_cpp.py`); regenerate headers, do not hand-transcribe. Not run in CI. Install recipe (RESEARCH `:305-312`) goes in `tests/reference/README.md`.

## No Analog Found

| File | Role | Data Flow | Reason |
|---|---|---|---|
| `tests/reference/*.py`, `*.h`, `README.md` | offline tooling | batch | No Python or generated-literal headers in repo; use RESEARCH Appendix verbatim |
| `appendLowerHemisphereTriplets` hull logic | utility | batch | Regular builder is the structural analog only; hull/supporting-plane math per RESEARCH Pattern 1 |
| Hold-last-good sanitiser | service | request-response | No input sanitiser exists; follow `gainScratch_` preallocated-copy idiom and RESEARCH Pattern 4 |

## Metadata

**Analog search scope:** `tests/**`, `src/Engine`, `src/IO`, `src/Core`, `src/Algorithms`, `include/SpatialCore/Algorithms`
**Files scanned:** ~14 read/partially read
**Pattern extraction date:** 2026-09-30
