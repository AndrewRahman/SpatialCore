---
phase: 03-binaural-defects-hrtf-packaging
reviewed: 2026-10-04T23:30:00Z
depth: standard
files_reviewed: 36
files_reviewed_list:
  - CMakeLists.txt
  - include/SpatialCore/Binaural/BinauralRenderer.h
  - include/SpatialCore/Binaural/HRTFDatabase.h
  - include/SpatialCore/Binaural/HRTFProfile.h
  - include/SpatialCore/Binaural/HRTFProfileResolver.h
  - include/SpatialCore/Binaural/PartitionedConvolver.h
  - include/SpatialCore/Core/SimpleBinauralCues.h
  - include/SpatialCore/Engine/RenderEngine.h
  - include/SpatialCore/SpatialCore.h
  - src/Binaural/BinauralRenderer.cpp
  - src/Binaural/EmbeddedHRTF.cpp
  - src/Binaural/HRTFDatabase.cpp
  - src/Binaural/HRTFProfileResolver.cpp
  - src/Binaural/PartitionedConvolver.cpp
  - src/Engine/RenderEngine.cpp
  - tests/CMakeLists.txt
  - tests/Algorithms/PanningLawTests.cpp
  - tests/Binaural/BinauralMetrics.h
  - tests/Binaural/BinauralTestUtilities.h
  - tests/Binaural/BinauralCueTests.cpp
  - tests/Binaural/BinauralRendererTests.cpp
  - tests/Binaural/PartitionedConvolverTests.cpp
  - tests/Binaural/ProfileLoudnessTests.cpp
  - tests/Binaural/HRTFEmbeddedTests.cpp
  - tests/Binaural/BernschuetzKU100Tests.cpp
  - tests/Binaural/CipicSubject003Tests.cpp
  - tests/Binaural/HutubsPP2Tests.cpp
  - tests/Binaural/MitKemarLargePinnaTests.cpp
  - tests/Binaural/SadieD2KU100Tests.cpp
  - tests/Engine/ProfileSwitchTests.cpp
  - CLAUDE.md
  - README.md
  - docs/development-roadmap.md
  - docs/integration-guide.md
  - .claude/skills/spatial-audio-dsp/SKILL.md
  - .claude/skills/spatialcore-architecture/spatialcore-architecture.md
findings:
  critical: 0
  warning: 0
  info: 0
  total: 0
status: clean
---

# Phase 3: Code Review Report (iteration 4)

**Reviewed:** 2026-10-04T23:30:00Z
**Depth:** standard
**Files Reviewed:** 36
**Status:** clean

## Summary

Reviewed fix commits 0636937 (custom-file limit numbers in the integration guide and the
`HRTFDatabase.h` bounds comment) and 5d2a92e (`O_NOCTTY` on the shared SOFA file open) on top of
the iteration-3 state. Both iteration-3 notes are resolved and no new defect was found. All reviewed
files meet quality standards. No issues found.

Verified in the source:

- **0636937, numbers.** Every figure now stated in `docs/integration-guide.md:336-350` and
  `HRTFDatabase.h:66-81` was recomputed against the shipped data. `kMaxDecodedSamples` is 2^27
  floats, which is 512 MiB of floats and "134 million". 2^27 / (65536 x 2) is 1024 samples, and
  2^27 / (16384 x 2) is 4096 positions, matching the guide. KEMAR is 512 samples at 44.1 kHz, which is
  ceil (512 x 48000 / 44100) = 558 at 48 kHz (also the value the tests expect: `kExpectedIRLength`,
  `getIRLength() == 558`), and 2229 (about 2.2k) at 192 kHz. The largest decoded size at 192 kHz is
  SADIE II, 8802 positions x 2 x 1024 = 18.0 million floats, "about 18 million". Bernschuetz is
  16020 x 2 x 512 = 16.4 million, "about 16 million". The old wrong figures (2.4k, 80 million) are
  gone from the tree; a grep over `include`, `src`, `docs`, `README.md`, `CLAUDE.md` and
  `.claude/skills` finds no stale copy. The 36 MB (largest built-in file is 36,591,729 bytes), 256 MB
  and "2 x 58 MB" (60.9 MB of files, 58 MiB) figures in `CLAUDE.md`, `RenderEngine.h` and
  `HRTFProfileResolver.h` are consistent with the files in `HRTF/`.
- **5d2a92e, `O_NOCTTY`.** `readFileCapped` (`HRTFDatabase.cpp:74-130`) opens with
  `O_RDONLY | O_NONBLOCK | O_NOCTTY | O_CLOEXEC`, so a symlink to a terminal device cannot become the
  controlling terminal, and a FIFO opens at once; `fstat` on the same descriptor then refuses
  anything that is not a regular file. All three flags are in `<fcntl.h>`, which is included. `close`
  runs on every path after a successful open. Boundary traces of the read loop: a zero-byte file
  (capacity 1, read returns 0, parsed and refused downstream), `st_size == cap` (capacity cap + 1,
  total reaches the limit, `total <= cap` is false, refused), `st_size < cap` (exits on `read == 0`),
  and a file that grows during the read (capacity doubles up to `cap + 1`, then refused).
- **Pre-validation path (`loadFromBytes`, `declaredShapeWithinBounds`), re-confirmed.** The parse
  handle is freed on every path, the `R*M*N` equality is done in uint64, the range test is written so
  NaN fails, and the float budget bounds the allocation before `mysofa_open_data` runs. The retained
  post-open check covers the one-ulp ceil difference.
- **Resolver chain.** `resolveHRTFProfile` takes the file name only from `kHRTFProfiles`, skips an
  empty or non-directory folder, checks the size before reading, and a failed shared load leaves the
  database unloaded before the embedded fallback runs. Status text is built from the static table and
  integers only.
- **Build and tests.** `build/` is up to date for 5d2a92e. The HRTF, shared-folder and FIFO test
  subset ran: all tests passed (1315 assertions in 32 test cases). The only extra output is the
  known Debug-only `SharedFFTCache` LeakedObjectDetector message at exit, already tracked in STATE.md.

Not re-raised, per the accepted-residual list: the WR-05 64-sample ITD line (issue #25), libmysofa's
own HDF5 parse allocating before validation (bounded by the 256 MB file cap), the public
test-only loader-throw hook (documented TEST-ONLY), and the Debug-only leak message.

---

_Reviewed: 2026-10-04T23:30:00Z_
_Reviewer: Claude (gsd-code-reviewer)_
_Depth: standard_
