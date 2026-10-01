## Deferred Items

- 02-04: JUCE prints "Leaked objects detected: 1 instance(s) of class FFT" at test-binary exit once any test loads a SOFA profile through BinauralRenderer::setProfile
  status: open
  **What:** `SharedFFTCache::getInstance()` (`src/Binaural/PartitionedConvolver.cpp:27`) is a function-local `static` that holds `std::shared_ptr<juce::dsp::FFT>` until static destruction, which runs after JUCE's `LeakedObjectDetector` check, so the detector reports the cached FFT as leaked. It is a static-destruction-order false positive, not a runtime leak, and the exit code stays 0. Plan 02-04's `[sanitize]` HRTF test is the first suite test to call `setProfile` on a loaded profile, which is why the line now appears in a full run.
  **Found during:** Plan 02-04 Task 2.
  **Why deferred:** out of scope for 02-04 (no SharedFFTCache change is planned in Phase 2); the singleton's lifetime is Binaural infrastructure, closest to Phase 3 (HRTF) or Phase 5 (audio-thread hygiene).
