#pragma once

#include <juce_core/juce_core.h>
#include <juce_dsp/juce_dsp.h>
#include <map>
#include <memory>

namespace spatialcore
{

//==============================================================================
// Process-global FFT cache (D-08).
//
// Contract: one juce::dsp::FFT per order, created on the first request and kept for the
// life of the process. The permanent allocation is deliberate, not a leak. The function-local
// static that owns the cache is destroyed at process exit, after JUCE's leak detector has run,
// which is why a Debug build prints "Leaked objects detected: 1 FFT" (one line per order used) at exit; that
// line is this cache, not a defect.
//
// Threads: getOrCreate() takes `lock` (a SpinLock), so it is only ever called at prepare time
// (PartitionedConvolver::prepare, HRTFDatabase::convertToMinPhase), on a message or loader thread,
// never from the audio thread. The audio thread of every engine in the process then shares the
// FFT objects and calls their perform*() functions. On Apple vDSP those only read set-up data,
// so no lock is taken; SharedFFTCacheTests.cpp and MultiInstanceTests.cpp check the sharing
// under ThreadSanitizer.
//
// Provenance: OpenSpatialDelay PR #142 added it for Spatial-Media-Lab/OpenSpatialDelay#131 (eight
// instances crashed in CoreGraphics), on the hypothesis that vDSP shares twiddle-factor memory
// across set-ups of one order and frees it under a reader when the last set-up goes. It sits
// beside the per-renderer HRTFDatabase that fixed Spatial-Media-Lab/OpenSpatialDelay#96 (shared
// HRTF state between instances). The vDSP hypothesis did not reproduce on macOS 26 / Apple
// Silicon (Phase 5 research: 8 threads x 4,000 create-use-destroy cycles, 0 mismatches, 3 runs), so the tests pin the
// invariants (one object per order, retained, safe concurrent first use), not that crash.
//
// Platform limit: on Windows and Linux without FFTW, MKL or IPP, JUCE selects its fallback FFT,
// whose perform() takes a SpinLock. With one shared FFT per order every instance's audio thread
// then contends on that one lock. That is a lock on the audio path that the macOS proof cannot
// see. Phase 6 (Plan 06-06) read it in JUCE 9.0.0 (modules/juce_dsp/frequency/juce_FFT.cpp):
// FFTFallback (line 89, priority -1, the lowest) takes `SpinLock::ScopedLockType sl (processLock)`
// in perform() at line 115, and the real-only transforms PartitionedConvolver calls reach it
// (lines 175-193). The engines ranked above it are Apple vDSP (macOS and iOS, no lock), FFTW,
// Intel MKL and Intel IPP; this repository enables none of the last three, and neither the Linux
// nor the Windows configure and build logs of CI mention them, so those builds use FFTFallback.
// On macOS vDSP is used and takes no lock. The contention itself was not measured, and the
// Windows tests are not run (D-08). OpenSpatialDelay v1.0.0 already ships this same cache design
// (a SpinLock-guarded map, one FFT per order) in its Windows build, so the migration adds no new
// exposure. It is not fixed in v1.
//==============================================================================
struct SharedFFTCache
{
    juce::SpinLock lock;
    std::map<int, std::shared_ptr<juce::dsp::FFT>> cache;
    std::shared_ptr<juce::dsp::FFT> getOrCreate (int fftOrder);
};

SharedFFTCache& getSharedFFTCache();

} // namespace spatialcore
