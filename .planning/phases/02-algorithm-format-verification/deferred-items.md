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
