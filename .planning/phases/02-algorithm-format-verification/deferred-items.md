## Deferred Items

- 02-04: JUCE prints "Leaked objects detected: 1 instance(s) of class FFT" at test-binary exit once any test loads a SOFA profile through BinauralRenderer::setProfile
  status: open
  **What:** `SharedFFTCache::getInstance()` (`src/Binaural/PartitionedConvolver.cpp:27`) is a function-local `static` that holds `std::shared_ptr<juce::dsp::FFT>` until static destruction, which runs after JUCE's `LeakedObjectDetector` check, so the detector reports the cached FFT as leaked. It is a static-destruction-order false positive, not a runtime leak, and the exit code stays 0. Plan 02-04's `[sanitize]` HRTF test is the first suite test to call `setProfile` on a loaded profile, which is why the line now appears in a full run.
  **Found during:** Plan 02-04 Task 2.
  **Why deferred:** out of scope for 02-04 (no SharedFFTCache change is planned in Phase 2); the singleton's lifetime is Binaural infrastructure, closest to Phase 3 (HRTF) or Phase 5 (audio-thread hygiene).
- 02-07: spatial-audio-dsp skill §4 "Virtual Speaker Deployment" describes a 16-speaker virtual array (and "30 pre-computed triangulations") for binaural rendering that SpatialCore's code does not contain
  status: open
  **What:** `struct VirtualSpeaker` in `include/SpatialCore/IO/SpeakerLayout.h` is unused, and `RenderEngine::activateLayout` gives the Binaural format a 0-speaker layout; binaural output is per-source HRTF or Woodworth. `README.md`'s "16-speaker virtual array for binaural rendering" bullet makes the same claim. Plan 02-07 replaced only that section's below-horizon paragraph, as instructed.
  **Found during:** Plan 02-07 Task 2.
  **Why deferred:** the plan said "change nothing else in the skill"; whether the section is removed or rewritten is a docs decision (Phase 6 docs, or whichever phase owns README accuracy).
- 02-07: spatial-audio-dsp skill §1.3 VBAP says triplets come from a "Delaunay triangulation"
  status: open
  **What:** `buildVBAPTripletsForLayout` keeps every non-degenerate (|det| >= 0.01) triple rather than a Delaunay triangulation, which is why coplanar quads carry both triangulations (02-05 Deviation 2, AndrewRahman/SpatialCore#22).
  **Found during:** Plan 02-07 Task 2.
  **Why deferred:** outside the sites the plan listed; one-line wording fix for the next docs pass.
- 02-07: `ADMOSCReceiver.h` raises five `-Wunused-parameter` warnings (lines 45, 46, 51) in every OpenSpatialDelay translation unit that includes it
  status: open
  **What:** the default-empty virtual bodies of `admObjectParamReceived` and `admGlobalParamReceived` name their parameters. Seen in the DR-3 OSD build; unchanged since d43cb15, so not a Phase 2 regression.
  **Found during:** Plan 02-07 Task 3.
  **Why deferred:** pre-existing and outside Phase 2; fix is commenting out the parameter names.
- 02-10: other tight test-local-reference tolerances could fail on another compiler or build type (none fails today)
  status: open
  **What:** candidates from the G-02-10 diagnosis, line numbers as of 1ac244a. `tests/Core/VBAPTripletSelectionTests.cpp:787` (one-part-per-million comparison of the test-local largest-min-gain reference with `computeVBAPGains3D`, the closest analog); `:86`, `:152`, `:861`, `:1054` (containment predicates re-implemented in the test, where a sign flip picks a different triplet); `:1268` (unit-power check over about 600k directions); and the five binaural golden FNV hashes (exact bits). Low risk: `SpatialMathTests.cpp` softClip / outputLimiter / distanceAttenuation, `PanningLawTests.cpp:718-721` and `:765-766`.
  **Found during:** Plan 02-10 (diagnosis `.planning/debug/ambi-pin-release-tolerance.md`).
  **Why deferred:** none fails in Debug or Release arm64; G-02-10 was scoped to `[ambi-pin]`.
- 02-10: CI has no arm64 / FMA leg
  status: open
  **What:** `.github/workflows/ci.yml` builds Release on ubuntu-latest (x86-64 without FMA), where this class very likely does not show (clang x86-64 proxy clean; GCC not run). A Release leg on a macOS arm64 runner would show it. This branch has also not been pushed, so CI has not run Phase 2 at all.
  **Found during:** Plan 02-10 (diagnosis).
  **Why deferred:** out of G-02-10's scope; Phase 6 success criterion 3 (green macOS CI, SpatialCore#9) is where it lands.
- 02-10: HUTUBS PP2 golden checksum fails in Debug only
  status: open
  **What:** it passes in Release arm64 because the golden (2d6dc08, plan 08-04) was captured from an optimised build; same floating-point class as G-02-10.
  **Found during:** Plan 02-10 (diagnosis).
  **Why deferred:** Phase 3 owns the binaural goldens.
- 02-10: CI test step runs zero tests (ctest from the build root finds none)
  status: open
  **What:** `include(CTest)` and `catch_discover_tests` sit in `tests/CMakeLists.txt`, with no `enable_testing()` at the top level, so the build root's `CTestTestfile.cmake` has no tests. `ctest --test-dir build-release` prints "No tests were found!!!" and exits 0, in both `build/` and `build-release/`. `.github/workflows/ci.yml` runs `ctest --output-on-failure` with `working-directory: build`, so the CI test step is a vacuous pass. Running ctest in `<build>/tests` registers all 193 cases. The phase gate commands in 02-VALIDATION.md and TESTING.md use `build-release/tests` for that reason.
  **Found during:** Plan 02-10 Task 2 Step 2.
  **Why deferred:** fixing it means editing `.github/workflows/ci.yml` or a CMake file, both out of G-02-10's scope and prohibited in this plan; closest owner is Phase 6 (CI).
