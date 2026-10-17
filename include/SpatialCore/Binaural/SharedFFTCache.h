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
// Provenance: Spatial-Media-Lab/OpenSpatialDelay#142 (PR) added it for Spatial-Media-Lab/OpenSpatialDelay#131 (eight
// instances crashed in CoreGraphics), on the hypothesis that vDSP shares twiddle-factor memory
// across set-ups of one order and frees it under a reader when the last set-up goes. It sits
// beside the per-renderer HRTFDatabase that fixed Spatial-Media-Lab/OpenSpatialDelay#96 (shared
// HRTF state between instances). The vDSP hypothesis did not reproduce on macOS 26 / Apple
// Silicon (Phase 5 research: 8 threads x 4,000 create-use-destroy cycles, 0 mismatches, 3 runs), so the tests pin the
// invariants (one object per order, retained, safe concurrent first use), not that crash.
//
// Platform limit: on Windows and Linux without FFTW, MKL or IPP, JUCE selects its fallback FFT,
// whose perform() takes a SpinLock. With one shared FFT per order every instance's audio thread
// then contends on that one lock. That is a lock on the audio path that this macOS proof cannot
// see; it is checked in Phase 6 (D-13), not claimed here.
//==============================================================================
struct SharedFFTCache
{
    juce::SpinLock lock;
    std::map<int, std::shared_ptr<juce::dsp::FFT>> cache;
    std::shared_ptr<juce::dsp::FFT> getOrCreate (int fftOrder);
};

SharedFFTCache& getSharedFFTCache();

} // namespace spatialcore
