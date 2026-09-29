# Deferred Items — Phase 01

Out-of-scope discoveries logged during plan execution. Not fixed per the
executor's SCOPE BOUNDARY rule (only auto-fix issues directly caused by the
current task's changes).

## 01-01 — Pre-existing HUTUBS PP2 golden checksum failure

- **Found during:** Task 2 verification (`./build/tests/SpatialCoreTests`, full suite)
- **File:** `tests/Binaural/HutubsPP2Tests.cpp:47`
- **Test case:** "HUTUBS PP2 — golden HRIR checksum at az=90deg (own control)"
- **Symptom:** `CHECK( checksum == kGoldenChecksum )` fails —
  `8806157918509638672` vs expected `11402032843575911607`. A JUCE assertion
  in `RenderEngine.cpp:197` also fires during the run.
- **Not caused by this plan:** the test was added in commit `2d6dc08`
  ("test(08-04): add per-profile binaural test suite"), a prior phase. This
  plan's Task 2 touches only `include/SpatialCore/IO/SpeakerLayout.h`,
  `include/SpatialCore/Algorithms/AllAlgorithms.h`, and
  `tests/Core/CountsTests.cpp` — none of which are on the include path of
  `RenderEngine.cpp` or the HRTF/binaural convolution path.
- **Reproduction:** `./build/tests/SpatialCoreTests "HUTUBS PP2*"` fails
  identically in isolation, confirming it is unrelated to the `[counts]`
  additions.
- **Status:** deferred — out of scope for phase 01 (documentation truth
  contract freeze). Needs its own investigation/fix in a future phase.
