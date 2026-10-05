#include <SpatialCore/Engine/RenderEngine.h>
#include "../Core/FloatSemanticsGuard.h"   // WR-04: no fast-math in this TU
#include <SpatialCore/Core/SpatialMath.h>
#include <SpatialCore/Core/SimpleBinauralCues.h>
#include <SpatialCore/Binaural/HRTFProfile.h>
#include <SpatialCore/Binaural/HRTFProfileResolver.h>

#include <cstdlib>
#include <new>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <type_traits>

namespace spatialcore
{

//==============================================================================
// Profile-status word (DATA-01, D-06). One 32-bit atomic carries {subject profile,
// state, source, problem} so a reader on any thread gets a consistent value without a
// lock. The worker is the only writer. The subject is the profile number clamped into
// 16 bits; the requested profile itself is reported from its own atomic.
//==============================================================================
namespace
{
    uint32_t packHRTFStatus (int subjectProfile, HRTFLoadState state,
                             HRTFProfileSource source, HRTFProfileProblem problem) noexcept
    {
        const int clamped = std::max (-32768, std::min (32767, subjectProfile));
        return (static_cast<uint32_t> (static_cast<uint16_t> (static_cast<int16_t> (clamped))))
             | (static_cast<uint32_t> (state)   << 16)
             | (static_cast<uint32_t> (source)  << 20)
             | (static_cast<uint32_t> (problem) << 24);
    }

    struct UnpackedHRTFStatus
    {
        int subjectProfile;
        HRTFLoadState state;
        HRTFProfileSource source;
        HRTFProfileProblem problem;
    };

    UnpackedHRTFStatus unpackHRTFStatus (uint32_t word) noexcept
    {
        return { static_cast<int> (static_cast<int16_t> (static_cast<uint16_t> (word & 0xFFFFu))),
                 static_cast<HRTFLoadState> ((word >> 16) & 0xFu),
                 static_cast<HRTFProfileSource> ((word >> 20) & 0xFu),
                 static_cast<HRTFProfileProblem> ((word >> 24) & 0xFu) };
    }
}

//==============================================================================
// HRTFProfileLoader -- the one worker thread behind setHRTFProfile (D-05).
//
// Protocol (T-03-14, T-03-15): the worker writes only a renderer that is not active and
// not the source of a running crossfade -- a free renderer (it clears the free flag to
// claim it) or its own unclaimed result (taken back from the mailbox with an exchange).
// It hands a finished renderer to the audio thread with a release store into the one-slot
// mailbox. It never signals the audio thread; when it needs a renderer back it polls.
//==============================================================================
class RenderEngine::HRTFProfileLoader : public juce::Thread
{
public:
    explicit HRTFProfileLoader (RenderEngine& engine)
        : juce::Thread ("SpatialCore HRTF loader"), owner (engine)
    {
        // What is playing right now, read from the active renderer's own record. Nothing is
        // in flight here: prepare() stops the worker and clears the mailbox before a new one starts.
        const auto active = unpackHRTFStatus (
            owner.rendererMeta_[owner.activeRendererIndex.load (std::memory_order_acquire)]
                .load (std::memory_order_acquire));
        activeProfile = active.subjectProfile;
        activeSource = active.source;
        activeProblem = active.problem;
        lastHandled = owner.handledSerial_.load (std::memory_order_acquire);
    }

    void run() override
    {
        while (! threadShouldExit())
        {
            const uint32_t serial = owner.requestSerial_.load (std::memory_order_acquire);
            if (serial == lastHandled)
            {
                wait (200);
                continue;
            }

            owner.loaderBusy_.store (true, std::memory_order_release);

            // A load can throw (std::bad_alloc from the decoded SOFA data or the convolver
            // buffers). Without a handler the exception would end this thread: loaderBusy_ and
            // the held renderer would never be released and the status would read Loading
            // forever. Settle the request as failed instead; the current profile keeps
            // playing and a later request retries (D-06).
            bool settled = false;
            try
            {
                settled = serveRequest (serial);
            }
            catch (...)
            {
                releaseHeld();
                publishStatus (owner.requestedProfile_.load (std::memory_order_acquire),
                               HRTFLoadState::Failed, HRTFProfileSource::None,
                               HRTFProfileProblem::LoadFailed);
                settle (serial);
                settled = true;
            }

            owner.loaderBusy_.store (false, std::memory_order_release);
            if (settled)
                lastHandled = serial;
        }

        releaseHeld();
    }

private:
    RenderEngine& owner;

    uint32_t lastHandled = 0;

    // The renderer this thread currently owns (loaded or not), -1 when none.
    int heldIdx = -1;
    int heldProfile = -1;                         // profile loaded into it, -1 when none
    HRTFProfileSource heldSource = HRTFProfileSource::None;
    HRTFProfileProblem heldProblem = HRTFProfileProblem::None;

    // The result most recently put into the mailbox and not yet known to be claimed.
    bool mailboxPending = false;
    int publishedProfile = 0;
    HRTFProfileSource publishedSource = HRTFProfileSource::None;
    HRTFProfileProblem publishedProblem = HRTFProfileProblem::None;

    // This thread's belief about what the audio thread is playing.
    int activeProfile = 0;
    HRTFProfileSource activeSource = HRTFProfileSource::Simple;
    HRTFProfileProblem activeProblem = HRTFProfileProblem::None;

    void releaseHeld()
    {
        if (heldIdx >= 0)
            owner.rendererFree_[static_cast<size_t> (heldIdx)].store (true, std::memory_order_release);
        heldIdx = -1;
        heldProfile = -1;
    }

    /** Settles what happened to the last published result: either the audio thread claimed it
        (it is now the active profile) or it is still in the mailbox and comes back to us. */
    void reclaimMailbox()
    {
        const int back = owner.readyRenderer_.exchange (-1, std::memory_order_acq_rel);

        if (back >= 0)
        {
            releaseHeld();
            heldIdx = back;
            heldProfile = publishedProfile;
            heldSource = publishedSource;
            heldProblem = publishedProblem;
            mailboxPending = false;
        }
        else if (mailboxPending)
        {
            activeProfile = publishedProfile;
            activeSource = publishedSource;
            activeProblem = publishedProblem;
            mailboxPending = false;
        }
    }

    void publishStatus (int subject, HRTFLoadState state, HRTFProfileSource source, HRTFProfileProblem problem)
    {
        owner.loaderStatusWord_.store (packHRTFStatus (subject, state, source, problem), std::memory_order_release);
    }

    void settle (uint32_t serial)
    {
        owner.handledSerial_.store (serial, std::memory_order_release);
    }

    /** True when the request is settled (a result or a failure was published), false when the
        thread should look again (a newer request arrived, or the thread is exiting). */
    bool serveRequest (uint32_t serial)
    {
        const int target = owner.requestedProfile_.load (std::memory_order_acquire);

        // Whatever the answer is, an older unclaimed result is stale now (latest request wins).
        reclaimMailbox();

        if (! isValidHRTFProfileIndex (target))
        {
            releaseHeld();
            publishStatus (target, HRTFLoadState::Failed, HRTFProfileSource::None, HRTFProfileProblem::InvalidIndex);
            settle (serial);
            return true;
        }

        const bool force = owner.forceReload_.load (std::memory_order_acquire);

        // Already playing it: nothing to load.
        if (target == activeProfile && ! force)
        {
            releaseHeld();
            publishStatus (target, HRTFLoadState::Ready, activeSource, activeProblem);
            settle (serial);
            return true;
        }

        // Get a renderer to write: our own result (taken back above) or a free one. Only the
        // audio thread frees one (and it must never signal this thread), so poll, backing off
        // from 2 ms to 50 ms: a host that has stopped calling the audio callback no longer
        // costs about 500 wakeups a second. wait() also returns at once on notify(), so a newer
        // request or shutdown is not delayed by the backoff.
        int pollMs = 2;
        while (heldIdx < 0)
        {
            if (threadShouldExit() || owner.requestSerial_.load (std::memory_order_acquire) != serial)
                return false;

            for (int i = 0; i < 2 && heldIdx < 0; ++i)
            {
                auto& flag = owner.rendererFree_[static_cast<size_t> (i)];
                if (flag.load (std::memory_order_relaxed) && flag.exchange (false, std::memory_order_acquire))
                {
                    heldIdx = i;
                    heldProfile = -1;
                }
            }

            if (heldIdx < 0)
            {
                wait (pollMs);
                pollMs = std::min (pollMs * 2, 50);
            }
        }

        if (heldProfile != target)
        {
            auto& renderer = owner.binauralRenderers[static_cast<size_t> (heldIdx)];
            renderer.prepare (owner.preparedSampleRate_, owner.preparedMaxBlock_);

            if (owner.loaderThrowForTesting_.load (std::memory_order_acquire))
                throw std::bad_alloc();

            const HRTFResolveResult result = resolveHRTFProfile (target, renderer.hrtfDatabase,
                                                                  static_cast<float> (owner.preparedSampleRate_),
                                                                  owner.getSharedFolderForLoader());
            if (! result.loaded)
            {
                // The current profile keeps playing; only this renderer was touched (D-06).
                releaseHeld();
                publishStatus (target, HRTFLoadState::Failed, HRTFProfileSource::None, result.problem);
                settle (serial);
                return true;
            }

            renderer.invalidateSources();
            renderer.setProfile (target);
            heldProfile = target;
            heldSource = result.source;
            heldProblem = result.problem;
            owner.forceReload_.store (false, std::memory_order_release);
        }

        // A newer request arrived while loading: keep the renderer and look again.
        if (threadShouldExit() || owner.requestSerial_.load (std::memory_order_acquire) != serial)
            return false;

        // Hand it over. The meta record is written first so the release store below carries it.
        owner.rendererMeta_[static_cast<size_t> (heldIdx)].store (
            packHRTFStatus (target, HRTFLoadState::Ready, heldSource, heldProblem), std::memory_order_relaxed);
        owner.readyRenderer_.store (heldIdx, std::memory_order_release);

        mailboxPending = true;
        publishedProfile = target;
        publishedSource = heldSource;
        publishedProblem = heldProblem;
        heldIdx = -1;
        heldProfile = -1;

        publishStatus (target, HRTFLoadState::Ready, publishedSource, publishedProblem);
        settle (serial);
        return true;
    }
};

RenderEngine::RenderEngine()
{
    resetLastGoodPositions();

    // SC-18: D-09 index order 0..6 (see kAlgorithmIndex* in RenderEngine.h).
    speakerAlgorithms_[kAlgorithmIndexAmbisonics] = &ambisonicsAlgorithm_;
    speakerAlgorithms_[kAlgorithmIndexConstantPower] = &constantPowerAlgorithm_;
    speakerAlgorithms_[kAlgorithmIndexDBAP] = &dbapAlgorithm_;
    speakerAlgorithms_[kAlgorithmIndexKNN] = &knnAlgorithm_;
    speakerAlgorithms_[kAlgorithmIndexMDAP] = &mdapAlgorithm_;
    speakerAlgorithms_[kAlgorithmIndexVBAP] = &vbapAlgorithm_;
    speakerAlgorithms_[kAlgorithmIndexVBIP] = &vbipAlgorithm_;

    // A fresh engine is playing profile 0 (Simple); both renderers start as that.
    const uint32_t simple = packHRTFStatus (0, HRTFLoadState::Ready, HRTFProfileSource::Simple, HRTFProfileProblem::None);
    rendererMeta_[0].store (simple, std::memory_order_relaxed);
    rendererMeta_[1].store (simple, std::memory_order_relaxed);
    loaderStatusWord_.store (packHRTFStatus (0, HRTFLoadState::Idle, HRTFProfileSource::Simple, HRTFProfileProblem::None),
                             std::memory_order_relaxed);
}

void RenderEngine::setAlgorithmIndex (int index)
{
    // SC-18: clamp on store; the audio thread additionally routes any index
    // outside the speaker table to VBAP (speakerAlgorithmFor).
    algorithmIndex_.store (juce::jlimit (0, kNumAlgorithmIndices - 1, index), std::memory_order_relaxed);
}

int RenderEngine::getAlgorithmIndex() const
{
    return algorithmIndex_.load (std::memory_order_relaxed);
}

const SpatializationAlgorithm& RenderEngine::speakerAlgorithmFor (int index) const
{
    // Stereo indices (7..11) and anything outside the table render as VBAP on a
    // speaker layout. An entry that cannot render to speakers also falls back
    // to VBAP (mirrors OpenSpatialDelay's supportsSurround() fallback).
    if (index < 0 || index >= kNumSpeakerAlgorithmIndices)
        return vbapAlgorithm_;

    const SpatializationAlgorithm* chosen = speakerAlgorithms_[index];
    return (chosen != nullptr && chosen->supportsSurround()) ? *chosen : vbapAlgorithm_;
}

RenderEngine::~RenderEngine()
{
    if (hrtfLoader_ != nullptr)
    {
        hrtfLoader_->signalThreadShouldExit();
        hrtfLoader_->notify();
        hrtfLoader_->stopThread (15000);
    }
}

//==============================================================================
// Engine-owned profile switching: message-thread side (D-04, D-05, D-06)
//==============================================================================
juce::File RenderEngine::getSharedFolderForLoader() const
{
    const juce::ScopedLock lock (sharedFolderLock_);
    return hasSharedFolderOverride_ ? sharedFolderOverride_ : getSharedHRTFFolder();
}

void RenderEngine::setSharedHRTFFolderForTesting (const juce::File& folder)
{
    const juce::ScopedLock lock (sharedFolderLock_);
    sharedFolderOverride_ = folder;
    hasSharedFolderOverride_ = true;
}

void RenderEngine::setLoaderFailureForTesting (bool enabled)
{
    loaderThrowForTesting_.store (enabled, std::memory_order_release);
}

void RenderEngine::setHRTFProfile (int profileIndex)
{
    hrtfEverRequested_ = true;

    // Settled on this very profile and playing it: nothing to do (no reload, no crossfade,
    // status unchanged). A request after a failure always goes through (retry).
    if (profileIndex == requestedProfile_.load (std::memory_order_acquire)
        && profileIndex == activeHRTFProfile_.load (std::memory_order_acquire)
        && requestSerial_.load (std::memory_order_acquire) == handledSerial_.load (std::memory_order_acquire)
        && unpackHRTFStatus (loaderStatusWord_.load (std::memory_order_acquire)).state == HRTFLoadState::Ready)
        return;

    requestedProfile_.store (profileIndex, std::memory_order_release);
    requestSerial_.fetch_add (1, std::memory_order_acq_rel);

    // Before the first prepare() the request is held; prepare() serves it.
    if (preparedSampleRate_ > 0.0)
    {
        if (hrtfLoader_ == nullptr)
        {
            hrtfLoader_ = std::make_unique<HRTFProfileLoader> (*this);
            hrtfLoader_->startThread (juce::Thread::Priority::low);
        }
        hrtfLoader_->notify();
    }
}

HRTFProfileStatus RenderEngine::getHRTFProfileStatus() const noexcept
{
    // handled before serial: if they are equal at the second read, nothing was pending then.
    const uint32_t handled = handledSerial_.load (std::memory_order_acquire);
    const uint32_t serial = requestSerial_.load (std::memory_order_acquire);
    const bool busy = loaderBusy_.load (std::memory_order_acquire);
    const auto word = unpackHRTFStatus (loaderStatusWord_.load (std::memory_order_acquire));

    HRTFProfileStatus status;
    status.requestedProfile = requestedProfile_.load (std::memory_order_acquire);
    status.activeProfile = activeHRTFProfile_.load (std::memory_order_acquire);

    if (serial != handled || busy)
    {
        status.state = HRTFLoadState::Loading;
        status.source = HRTFProfileSource::None;
        status.problem = HRTFProfileProblem::None;
    }
    else
    {
        status.state = word.state;
        status.source = word.source;
        status.problem = word.problem;
    }

    return status;
}

bool RenderEngine::waitForHRTFProfileIdle (int timeoutMs)
{
    const juce::uint32 start = juce::Time::getMillisecondCounter();

    for (;;)
    {
        if (handledSerial_.load (std::memory_order_acquire) == requestSerial_.load (std::memory_order_acquire)
            && ! loaderBusy_.load (std::memory_order_acquire))
            return true;

        if (static_cast<int> (juce::Time::getMillisecondCounter() - start) >= timeoutMs)
            return false;

        juce::Thread::sleep (2);
    }
}

//==============================================================================
// Audio-thread side: claim a ready renderer from the mailbox (atomics only, DR-1)
//==============================================================================
void RenderEngine::claimReadyRenderer (bool hrtfPathThisBlock)
{
    // A path change mid-fade: the crossfade cannot finish on a path that does not run it, and
    // its source renderer would never come back free. End it now. Only once engine-owned
    // switching is in use, so a legacy consumer keeps its crossfade state exactly as before.
    if (rendererXfading_ && ! hrtfPathThisBlock
        && requestSerial_.load (std::memory_order_relaxed) != 0)
        endRendererFade();

    if (rendererXfading_)
        return;

    if (readyRenderer_.load (std::memory_order_relaxed) < 0)
        return;

    const int idx = readyRenderer_.exchange (-1, std::memory_order_acq_rel);
    if (idx < 0)
        return;

    const int oldIdx = activeRendererIndex.load (std::memory_order_acquire);
    if (idx == oldIdx)
        return;

    activeRendererIndex.store (idx, std::memory_order_release);
    activeHRTFProfile_.store (binauralRenderers[static_cast<size_t> (idx)].getActiveProfile(),
                              std::memory_order_release);

    if (! hrtfPathThisBlock)
    {
        // No HRTF crossfade on this path: switch at once and give the old renderer back.
        rendererFree_[static_cast<size_t> (oldIdx)].store (true, std::memory_order_release);
        prevActiveRendererIdx_ = idx;
    }
    // On the HRTF path the swap detection in renderDirectBinauralHRTF starts the crossfade
    // from the previous index and frees it where the crossfade ends.
}

//==============================================================================
// prepare() — engine-owned DSP setup slice of the pre-move
// OpenSpatialDelayProcessor::prepareToPlay (verbatim behavior, transplanted
// scope only: LFE filter, NFC-HOA filters, binaural renderers, crossfade
// buffers, direct-binaural accumulation buffers).
//==============================================================================
void RenderEngine::prepare (double sampleRate, int maxBlockSize)
{
    // Engine-owned switching: stop the worker before anything it could be writing is touched.
    // A load in flight finishes (it cannot be interrupted); its unclaimed result is discarded
    // below. The worker is started again at the end when a request was ever made.
    if (hrtfLoader_ != nullptr)
    {
        hrtfLoader_->signalThreadShouldExit();
        hrtfLoader_->notify();
        hrtfLoader_->stopThread (15000);
        hrtfLoader_.reset();
    }

    juce::dsp::ProcessSpec spec { sampleRate, static_cast<juce::uint32> (maxBlockSize), 1 };

    // v0.2: Configure LFE low-pass filter (120 Hz, 2nd order Butterworth).
    // Double precision state/coefficients: at 120 Hz / 48 kHz the poles sit
    // very close to z=1 and a float32 biquad amplifies rounding noise to ~1e-6,
    // which under block-periodic input falls into a rounding limit cycle whose
    // shape is compiler/platform dependent (macOS vs MSVC differ). Double keeps
    // the LFE deterministic across platforms; output is still rounded to float.
    //
    // Order matters: a default-constructed Filter holds order-1 coefficients,
    // makeLowPass is order 2. Filter::check() (run on every processSample)
    // calls reset() -> memory.malloc() whenever the coefficient order differs
    // from the state order. Assigning the order-2 coefficients FIRST means
    // prepare()/reset() size the state for order 2 here, so the audio thread
    // never reallocates on its first block.
    *lfeFilter.coefficients = *juce::dsp::IIR::Coefficients<double>::makeLowPass (sampleRate, 120.0);
    lfeFilter.prepare (spec);
    lfeFilter.reset();

    // v0.5: Prepare NFC-HOA filters (per-object, per-SH-order, Ambisonics output only)
    for (int obj = 0; obj < MAX_SOURCES; ++obj)
    {
        for (int n = 0; n < kMaxAmbiOrder; ++n)
        {
            nfcFilters[obj][n].prepare (spec);
            nfcFilters[obj][n].reset();
            *nfcFilters[obj][n].coefficients =
                *juce::dsp::IIR::Coefficients<float>::makeAllPass (sampleRate, 1000.0f);
        }
        prevNfcDistance[obj] = -1.0f;  // Force coefficient update on first block
        smoothedNfcDistance[obj] = 0.0f;
    }

    // HRTF convolution: prepare both renderers (double-buffered)
    binauralRenderers[0].prepare (sampleRate, maxBlockSize);
    binauralRenderers[1].prepare (sampleRate, maxBlockSize);

    // Pre-allocate renderer crossfade buffers (issue Spatial-Media-Lab/OpenSpatialDelay#131: avoid audio-thread allocation)
    xfadeWetL_.resize (static_cast<size_t> (maxBlockSize), 0.0f);
    xfadeWetR_.resize (static_cast<size_t> (maxBlockSize), 0.0f);

    // D-15: scratch for the Woodworth path while it is blended with an HRTF renderer.
    simpleWetL_.resize (static_cast<size_t> (maxBlockSize), 0.0f);
    simpleWetR_.resize (static_cast<size_t> (maxBlockSize), 0.0f);

    // v0.5: Contiguous per-source accumulation buffers for direct binaural
    sourceAccumStorage_.resize (static_cast<size_t> (MAX_SOURCES * maxBlockSize), 0.0f);
    for (int src = 0; src < MAX_SOURCES; ++src)
        sourceAccumBufPtrs[src] = sourceAccumStorage_.data() + src * maxBlockSize;
    wetBufL_.resize (static_cast<size_t> (maxBlockSize), 0.0f);
    wetBufR_.resize (static_cast<size_t> (maxBlockSize), 0.0f);

    // BUG-01: Simple-path cue bank for the actual sample rate.
    designSimpleCueBank (sampleRate);
    resetSimpleCueState();
    simplePathRanLastBlock_ = false;

    // D-06(a): a fresh prepare forgets every held position.
    resetLastGoodPositions();

    // Engine-owned switching bookkeeping (the worker is stopped, so this is single-threaded).
    const bool firstPrepare = preparedSampleRate_ <= 0.0;
    const bool settingsChanged = sampleRate != preparedSampleRate_ || maxBlockSize != preparedMaxBlock_;
    preparedSampleRate_ = sampleRate;
    preparedMaxBlock_ = maxBlockSize;

    // Discard an unclaimed result and recompute who is free: the active renderer is not, the other is.
    readyRenderer_.store (-1, std::memory_order_release);
    const int activeIdx = activeRendererIndex.load (std::memory_order_acquire);
    rendererFree_[static_cast<size_t> (activeIdx)].store (false, std::memory_order_release);
    rendererFree_[static_cast<size_t> (1 - activeIdx)].store (true, std::memory_order_release);

    if (hrtfEverRequested_)
    {
        rendererXfading_ = false;
        rendererXfadeSamplesDone_ = 0;
        rendererXfadeLengthSamples_ = 0;
        rendererXfadeActive_.store (false, std::memory_order_release);
        prevActiveRendererIdx_ = activeIdx;

        if (settingsChanged && ! firstPrepare)
        {
            // The loaded profile is at the old rate: reload it at the new settings. Until the new
            // copy is claimed, keep the active renderer's convolvers sized for the new block size.
            forceReload_.store (true, std::memory_order_release);
            auto& active = binauralRenderers[static_cast<size_t> (activeIdx)];
            if (! active.isSimpleMode() && active.hrtfDatabase.isLoaded())
                active.setProfile (active.getActiveProfile());
        }

        // Re-issue the current request so the restarted worker serves it.
        requestSerial_.fetch_add (1, std::memory_order_acq_rel);
        hrtfLoader_ = std::make_unique<HRTFProfileLoader> (*this);
        hrtfLoader_->startThread (juce::Thread::Priority::low);
        hrtfLoader_->notify();
    }
}

//==============================================================================
// Simple-path cue bank design (BUG-01, SpatialCore#15, D-01). Runs in prepare()
// only; the audio thread reads the resulting fixed-size coefficient members.
//==============================================================================
namespace
{
    template <size_t N, typename Biquad>
    void designCueBranch (const SimpleCueStage (&stages)[N], Biquad (&out)[N], double sampleRate)
    {
        for (size_t i = 0; i < N; ++i)
        {
            const auto& st = stages[i];
            // Design frequencies are capped at 0.45 x the sample rate so a low
            // host rate (e.g. 22.05 kHz) never asks for a shelf above Nyquist.
            const float freq = static_cast<float> (std::min (static_cast<double> (st.frequencyHz),
                                                              0.45 * sampleRate));
            const float gain = juce::Decibels::decibelsToGain (st.gainDb);
            const auto coeffs = (st.kind == SimpleCueStage::Kind::HighShelf)
                ? juce::dsp::IIR::Coefficients<float>::makeHighShelf (sampleRate, freq, st.q, gain)
                : juce::dsp::IIR::Coefficients<float>::makePeakFilter (sampleRate, freq, st.q, gain);

            // JUCE stores [b0, b1, b2, a1, a2] already divided by a0.
            out[i].b0 = coeffs->coefficients[0];
            out[i].b1 = coeffs->coefficients[1];
            out[i].b2 = coeffs->coefficients[2];
            out[i].a1 = coeffs->coefficients[3];
            out[i].a2 = coeffs->coefficients[4];
        }
    }

    // One transposed-direct-form-II biquad step.
    template <typename Biquad>
    inline float cueBiquadStep (const Biquad& c, float (&z)[2], float x)
    {
        const float y = c.b0 * x + z[0];
        z[0] = c.b1 * x - c.a1 * y + z[1];
        z[1] = c.b2 * x - c.a2 * y;
        return y;
    }
}

void RenderEngine::designSimpleCueBank (double sampleRate)
{
    designCueBranch (kSimpleCueRearStages, cueRear_, sampleRate);
    designCueBranch (kSimpleCueUpStages, cueUp_, sampleRate);
    designCueBranch (kSimpleCueDownStages, cueDown_, sampleRate);
}

void RenderEngine::resetSimpleCueState()
{
    std::memset (cueStateRear_, 0, sizeof (cueStateRear_));
    std::memset (cueStateUp_, 0, sizeof (cueStateUp_));
    std::memset (cueStateDown_, 0, sizeof (cueStateDown_));
}

//==============================================================================
// D-06(a) / D-19(ii) hold-last-good sanitiser. See the header declaration.
//==============================================================================
static_assert (std::is_trivially_copyable_v<RenderSources>,
               "sanitizeSources copies RenderSources into the preallocated "
               "sanitizedSources_ member once per block; that must stay a plain "
               "memberwise copy -- no allocation, lock or log on the audio "
               "thread (DR-1)");

void RenderEngine::resetLastGoodPositions()
{
    // The ObjectState defaults, so a first-ever non-finite value renders at
    // the default position.
    const ObjectState defaults {};
    for (int t = 0; t < MAX_SOURCES; ++t)
    {
        lastGoodAzimuthDeg_[t]   = defaults.azimuthDeg;
        lastGoodElevationDeg_[t] = defaults.elevationDeg;
        lastGoodDistance_[t]     = defaults.distance;
    }
}

const RenderSources& RenderEngine::sanitizeSources (const RenderSources& sources)
{
    sanitizedSources_ = sources;

    // Each field is held independently: a finite value becomes the slot's
    // last good value and passes through untouched (never wrapped or clamped,
    // so finite input stays bit-identical); a non-finite one is replaced.
    auto hold = [] (float& value, float& lastGood)
    {
        if (std::isfinite (value))
            lastGood = value;
        else
            value = lastGood;
    };

    for (int t = 0; t < MAX_SOURCES; ++t)
    {
        auto& obj = sanitizedSources_.objects[t];
        hold (obj.azimuthDeg,   lastGoodAzimuthDeg_[t]);
        hold (obj.elevationDeg, lastGoodElevationDeg_[t]);
        hold (obj.distance,     lastGoodDistance_[t]);
    }

    return sanitizedSources_;
}

//==============================================================================
// renderBlock() — reproduces the EXACT 5-branch dispatch from the pre-move
// OpenSpatialDelayProcessor::processBlock (D-09 — dispatch shape unchanged).
//==============================================================================
void RenderEngine::renderBlock (const RenderSources& sources,
                                 const RenderBlockContext& blockCtx,
                                 float* const* outChannels,
                                 int numOutCh)
{
    // D-06(a) / D-19(ii): every render path and the engine's own gain
    // computation read this sanitised copy, never the raw parameter. The
    // algorithm-layer guards remain the protection for consumers that call
    // computeGains directly (RESEARCH F7); this does not replace them.
    const RenderSources& src = sanitizeSources (sources);

    // SC-16: one layout snapshot per block. It feeds the dispatch derivation,
    // the SC-13 gain computation and the discrete-surround speaker routing
    // below, so a message-thread format switch landing mid-block cannot pair
    // one format's dispatch with another format's layout.
    const LayoutState& layout = acquireBlockLayout();

    // SC-13 / SC-16: when the consumer opts in to either, work on a scratch
    // copy of the context (an assignment into an existing member, no
    // allocation) and dispatch on that instead. The dispatch chain itself is
    // unchanged (selected once via pointer so it is never duplicated or
    // reordered, per D-09).
    const RenderBlockContext* dispatchCtx = &blockCtx;
    bool claimed = false;
    if (blockCtx.engineComputesGains || blockCtx.engineDerivesDispatch || blockCtx.engineSelectsHRTF)
    {
        gainScratch_ = blockCtx;
        if (blockCtx.engineDerivesDispatch)
            deriveDispatchFromLayout (layout, gainScratch_);
        if (blockCtx.engineComputesGains)
            computeObjectGains (src, layout, gainScratch_);
        else if (blockCtx.engineSelectsHRTF && gainScratch_.isBinaural && ! gainScratch_.isStereoVariant)
        {
            // WR-06: the Woodworth path (profile 0, and every Simple <-> HRTF blend) reads
            // objGains. The flag implies them, so a consumer that sets only engineSelectsHRTF
            // does not get silence on profile 0.
            computeObjectGains (src, layout, gainScratch_, true);
        }
        if (blockCtx.engineSelectsHRTF)
        {
            // D-15: claim any ready profile first, then read the path off the renderer that is
            // active for this block. A binaural block is renderer-crossfade-capable on both
            // paths (the fade blends Simple and HRTF too), so the claim must not switch at once.
            claimReadyRenderer (! gainScratch_.isStereoVariant && gainScratch_.isBinaural);
            gainScratch_.useHRTF = ! binauralRenderers[static_cast<size_t> (
                activeRendererIndex.load (std::memory_order_acquire))].isSimpleMode();
            claimed = true;
        }
        dispatchCtx = &gainScratch_;
    }
    const RenderBlockContext& ctx = *dispatchCtx;

    float* outL = (numOutCh > 0) ? outChannels[0] : nullptr;
    float* outR = (numOutCh > 1) ? outChannels[1] : nullptr;

    // Engine-owned profile switching: pick up a ready renderer from the mailbox before the
    // dispatch. hrtfPath mirrors the dispatch chain below exactly.
    if (! claimed)
        claimReadyRenderer (! ctx.isStereoVariant && ctx.isBinaural && ctx.useHRTF);

    // BUG-01: the Simple path's cue filters hold state, which is only valid
    // while that path runs every block. Every other branch clears the flag so
    // the Simple path cold-starts when it resumes.
    bool ranSimple = false;

    if (ctx.isStereoVariant)
    {
        renderStereoVariant (src, ctx, outL, outR, numOutCh);
    }
    else if (ctx.isBinaural && ctx.engineSelectsHRTF)
    {
        // D-15: the engine owns the path choice and the Simple <-> HRTF blend.
        ranSimple = renderBinauralWithProfileFade (src, ctx, outL, outR, numOutCh);
    }
    else if (ctx.isBinaural && ctx.useHRTF)
    {
        renderDirectBinauralHRTF (src, outL, outR, numOutCh, activeRendererIndex.load (std::memory_order_acquire));
    }
    else if (ctx.isBinaural)
    {
        renderSimpleBinauralWoodworth (src, ctx, outL, outR, numOutCh);
        ranSimple = true;
    }
    else if (ctx.isAmbiOutput)
    {
        renderAmbisonicsOutput (src, ctx, outChannels, numOutCh);
    }
    else
    {
        renderDiscreteSurround (src, ctx, layout, outChannels, numOutCh);
    }

    simplePathRanLastBlock_ = ranSimple;
}

//==============================================================================
// computeObjectGains — SC-13. Fills ctx.objChannelGains (surround/Ambisonics,
// via the algorithm chosen by algorithmIndex_) and ctx.objGains (simple binaural, via
// binauralAlgorithm_) for every object slot. Only called when the consumer
// sets RenderBlockContext::engineComputesGains, or (binauralOnly, objGains
// alone) engineSelectsHRTF on a binaural block (WR-06). Does not read or write
// objGainL/objGainR/stereoMode — those stay consumer-side (D-06).
//==============================================================================
void RenderEngine::computeObjectGains (const RenderSources& sources, const LayoutState& ls,
                                        RenderBlockContext& ctx, bool binauralOnly)
{
    LayoutContext layoutCtx { ls.layout, ls.vbapTriplets, ls.ambiDecodeMatrix, ls.ambiNumSpeakers };

    // SC-18: one relaxed load per block, resolved to one algorithm reference.
    const SpatializationAlgorithm& surroundAlgorithm = speakerAlgorithmFor (algorithmIndex_.load (std::memory_order_relaxed));

    for (int t = 0; t < MAX_SOURCES; ++t)
    {
        // Zero the full speaker-wide row first so a stale value from a wider
        // previous layout cannot survive into this block.
        if (! binauralOnly)
            for (int sp = 0; sp < MAX_SPEAKERS; ++sp)
                ctx.objChannelGains[t][sp] = 0.0f;

        if (! sources.objectLive[t])
        {
            ctx.objGains[t] = {};
            continue;
        }

        SourcePosition pos {
            juce::degreesToRadians (sources.objects[t].azimuthDeg),
            juce::degreesToRadians (sources.objects[t].elevationDeg),
            sources.objects[t].distance
        };

        if (! binauralOnly)
            surroundAlgorithm.computeGains (pos, layoutCtx, ctx.objChannelGains[t], ls.layout.numSpeakers);

        BinauralContext binCtx { binauralProfileIndex_, ctx.sampleRate, kDefaultBinauralProfiles };
        ctx.objGains[t] = binauralAlgorithm_.computeBinauralGains (pos, binCtx);
    }
}

//==============================================================================
// Renderer crossfade bookkeeping (shared by the legacy and the D-15 paths).
//==============================================================================
void RenderEngine::beginRendererFadeIfSwapped (int activeIdx, int numSamples)
{
    // Detect renderer swap -> start crossfade.
    if (activeIdx != prevActiveRendererIdx_ && ! rendererXfading_)
    {
        rendererXfading_ = true;
        rendererXfadeActive_.store (true, std::memory_order_release);
        // Sample-based, fixed now: the fade must outlast the HRIRs it blends at any block size.
        rendererXfadeSamplesDone_ = 0;
        rendererXfadeLengthSamples_ = std::max (kRendererXfadeBlocks * numSamples, kMinRendererXfadeSamples);
        rendererXfadeFromIdx_ = prevActiveRendererIdx_;
        prevRxFadeOut_ = 1.0f;
        prevRxFadeIn_ = 0.0f;
        prevActiveRendererIdx_ = activeIdx;
        activeHRTFProfile_.store (binauralRenderers[static_cast<size_t> (activeIdx)].getActiveProfile(),
                                  std::memory_order_release);
    }
}

void RenderEngine::endRendererFade()
{
    rendererXfading_ = false;
    rendererXfadeSamplesDone_ = 0;
    rendererXfadeLengthSamples_ = 0;
    rendererXfadeActive_.store (false, std::memory_order_release);
    // The renderer faded out is neither active nor fading any more: the worker may use it.
    rendererFree_[static_cast<size_t> (rendererXfadeFromIdx_)].store (true, std::memory_order_release);
}

//==============================================================================
// renderDirectBinauralHRTF — verbatim-transplanted from
// OpenSpatialDelayProcessor::renderDirectBinauralHRTF. Engine-boundary change
// only: reads pre-delayed/pre-fed-back/pre-Doppler-pitched mono buffers from
// RenderSources instead of calling readObjectSample()/writeDelayLine()/
// processFeedbackSample() itself (those stay OSD-side, D-02).
//
// D-15 addition: with simpleCtx set, a Simple-mode renderer on either side of a fade is the
// Woodworth path (see the header declaration); with it null nothing changes. A caller that sets
// simpleCtx must have a fade running (renderBinauralWithProfileFade guarantees it).
//==============================================================================
bool RenderEngine::renderDirectBinauralHRTF (const RenderSources& sources, float* outL, float* outR, int numOutCh,
                                              int activeIdx, const RenderBlockContext* simpleCtx)
{
    const int numSamples = sources.numSamples;

    auto& activeRenderer = binauralRenderers[static_cast<size_t> (activeIdx)];

    beginRendererFadeIfSwapped (activeIdx, numSamples);

    // D-15: with simpleCtx set, a renderer in Simple mode is the Woodworth path, not a (silent)
    // convolver. At most one side of a fade is Simple here (see renderBinauralWithProfileFade).
    const bool activeIsSimple = simpleCtx != nullptr && activeRenderer.isSimpleMode();
    const bool fromIsSimple = simpleCtx != nullptr && rendererXfading_
                              && binauralRenderers[static_cast<size_t> (rendererXfadeFromIdx_)].isSimpleMode();
    bool ranSimple = false;

    // Update per-source HRIRs at block boundary for any taps that moved
    // (new renderer only — old renderer keeps its existing HRIRs during crossfade).
    // A Simple-mode active renderer has no HRIRs to update.
    for (int t = 0; t < MAX_SOURCES && ! activeIsSimple; ++t)
    {
        if (sources.objectLive[t])
        {
            float azRad = juce::degreesToRadians (sources.objects[t].azimuthDeg);
            float elRad = juce::degreesToRadians (sources.objects[t].elevationDeg);
            activeRenderer.updateSourceHRIR (t, azRad, elRad);
        }
    }

    // Ensure per-source accumulation storage is sized for this block (issue
    // CR-03: numSamples may exceed the maxBlockSize this engine was prepared
    // with — e.g. a host requesting an oversized block. sourceAccumStorage_
    // is otherwise fixed at MAX_SOURCES * maxBlockSize from prepare(), so an
    // oversized block would overflow the per-source memset/write below.
    // This mirrors the wetBufL_/wetBufR_ defensive-resize pattern immediately
    // below. Regrowth can reallocate the backing vector, so every cached
    // sourceAccumBufPtrs[src] MUST be re-derived afterward — stale pointers
    // would alias freed memory.
    if (sourceAccumStorage_.size() < static_cast<size_t> (MAX_SOURCES) * static_cast<size_t> (numSamples))
    {
        // An oversized block is a prepare-contract violation (the engine
        // should have been prepared with the true maximum block size) —
        // flag it loudly in debug builds, consistent with BinauralRenderer's
        // own self-flagging elsewhere in this file.
        jassertfalse;
        sourceAccumStorage_.assign (static_cast<size_t> (MAX_SOURCES) * static_cast<size_t> (numSamples), 0.0f);
        for (int src = 0; src < MAX_SOURCES; ++src)
            sourceAccumBufPtrs[src] = sourceAccumStorage_.data() + static_cast<size_t> (src) * static_cast<size_t> (numSamples);
    }

    // Zero per-source accumulation buffers (contiguous allocation)
    for (int src = 0; src < MAX_SOURCES; ++src)
        std::memset (sourceAccumBufPtrs[src], 0, sizeof (float) * static_cast<size_t> (numSamples));

    // Ensure wet buffers are sized
    if (wetBufL_.size() < static_cast<size_t> (numSamples))
    {
        wetBufL_.resize (static_cast<size_t> (numSamples), 0.0f);
        wetBufR_.resize (static_cast<size_t> (numSamples), 0.0f);
    }

    // v1.0: Per-sample distGain interpolation to prevent clicks on rapid position changes
    for (int s = 0; s < numSamples; ++s)
    {
        for (int t = 0; t < MAX_SOURCES; ++t)
        {
            if (sources.tapFadeGainPerSample[t] == nullptr) continue;
            float tapFade = sources.tapFadeGainPerSample[t][s];
            if (tapFade <= 0.0f) continue;
            float objMono = (sources.monoBuffers[t] != nullptr) ? sources.monoBuffers[t][s] : 0.0f;
            float dist = (sources.distGainPerSample[t] != nullptr) ? sources.distGainPerSample[t][s] : 1.0f;
            sourceAccumBufPtrs[t][s] = objMono * dist * tapFade;
        }
    }

    // (distGain interpolation-state carry-forward is the consumer's
    // responsibility — see RenderSources::distGainPerSample note in the
    // header; nothing to store here.)

    // === PASS 2: Per-block per-source HRTF convolution -> wet L/R ===
    bool sourceEnabled[MAX_SOURCES];
    for (int t = 0; t < MAX_SOURCES; ++t)
        sourceEnabled[t] = (sources.tapFadeGainPerSample[t] != nullptr && numSamples > 0
                             && sources.tapFadeGainPerSample[t][numSamples - 1] > 0.0f)
                            || sources.objectLive[t];

    const float* srcBufPtrs[MAX_SOURCES];
    for (int t = 0; t < MAX_SOURCES; ++t)
        srcBufPtrs[t] = sourceAccumBufPtrs[t];

    // The Woodworth path's destination when a side is Simple. simpleWetL_/R_ cover every block up
    // to the prepared size; an oversized block is a prepare-contract violation (flagged above for
    // the wet buffers), and then the Woodworth output goes straight into the buffer of its own
    // side, which that violation path has already sized -- never an allocation here (DR-1).
    const size_t ns = static_cast<size_t> (numSamples);
    const bool simpleScratchFits = simpleWetL_.size() >= ns && simpleWetR_.size() >= ns;

    // Active (new) side.
    const float* newL = wetBufL_.data();
    const float* newR = wetBufR_.data();
    if (activeIsSimple)
    {
        float* dstL = simpleScratchFits ? simpleWetL_.data() : wetBufL_.data();
        float* dstR = simpleScratchFits ? simpleWetR_.data() : wetBufR_.data();
        renderSimpleBinauralWoodworth (sources, *simpleCtx, dstL, dstR, 2);
        ranSimple = true;
        newL = dstL;
        newR = dstR;
    }
    else
    {
        activeRenderer.renderSourceBuffers (srcBufPtrs, sourceEnabled, MAX_SOURCES,
                                            numSamples, wetBufL_.data(), wetBufR_.data());
    }

    // Renderer-level crossfade.
    if (rendererXfading_)
    {
        if (xfadeWetL_.size() < ns) { xfadeWetL_.resize (ns, 0.0f); xfadeWetR_.resize (ns, 0.0f); }

        const float* oldL = xfadeWetL_.data();
        const float* oldR = xfadeWetR_.data();
        if (fromIsSimple)
        {
            float* dstL = simpleScratchFits ? simpleWetL_.data() : xfadeWetL_.data();
            float* dstR = simpleScratchFits ? simpleWetR_.data() : xfadeWetR_.data();
            renderSimpleBinauralWoodworth (sources, *simpleCtx, dstL, dstR, 2);
            ranSimple = true;
            oldL = dstL;
            oldR = dstR;
        }
        else
        {
            auto& oldRenderer = binauralRenderers[static_cast<size_t> (rendererXfadeFromIdx_)];
            oldRenderer.renderSourceBuffers (srcBufPtrs, sourceEnabled, MAX_SOURCES,
                                              numSamples, xfadeWetL_.data(), xfadeWetR_.data());
        }

        // End-of-block progress from elapsed samples, so the fade length does not depend on how
        // the host splits the stream into blocks.
        rendererXfadeSamplesDone_ += numSamples;
        float progress = static_cast<float> (static_cast<double> (rendererXfadeSamplesDone_)
                                             / static_cast<double> (rendererXfadeLengthSamples_));
        if (progress > 1.0f) progress = 1.0f;

        constexpr float halfPi = juce::MathConstants<float>::halfPi;
        float fadeOutGain = std::cos (progress * halfPi);
        float fadeInGain  = std::sin (progress * halfPi);

        float fadeOutInc = (fadeOutGain - prevRxFadeOut_) / static_cast<float> (numSamples);
        float fadeInInc  = (fadeInGain  - prevRxFadeIn_)  / static_cast<float> (numSamples);
        float gOut = prevRxFadeOut_;
        float gIn  = prevRxFadeIn_;

        for (int i = 0; i < numSamples; ++i)
        {
            gOut += fadeOutInc;
            gIn  += fadeInInc;
            wetBufL_[static_cast<size_t> (i)] = oldL[i] * gOut + newL[i] * gIn;
            wetBufR_[static_cast<size_t> (i)] = oldR[i] * gOut + newR[i] * gIn;
        }

        prevRxFadeOut_ = fadeOutGain;
        prevRxFadeIn_  = fadeInGain;

        if (rendererXfadeSamplesDone_ >= rendererXfadeLengthSamples_)
            endRendererFade();
    }

    // === PASS 3: Write raw wet signal to output (dry/wet mix handled by consumer) ===
    if (outL != nullptr) std::memcpy (outL, wetBufL_.data(), sizeof (float) * static_cast<size_t> (numSamples));
    if (outR != nullptr) std::memcpy (outR, wetBufR_.data(), sizeof (float) * static_cast<size_t> (numSamples));

    // Remaining channels (ch >= 2) are left untouched here; the consumer clears
    // them exactly as the pre-move code did (`buffer.clear(ch, ...)` on a
    // juce::AudioBuffer is a consumer-side concern, not engine state).
    (void) numOutCh;
    return ranSimple;
}

//==============================================================================
// renderBinauralWithProfileFade -- D-15. Called for every binaural block of a consumer that
// set engineSelectsHRTF. The path is chosen from the engine's own active renderer, never from
// the consumer's useHRTF:
//   * Simple active, no fade       -> renderSimpleBinauralWoodworth, exactly the flag-off path
//   * HRTF active, no fade         -> renderDirectBinauralHRTF, exactly the flag-off path
//   * HRTF <-> HRTF fade           -> renderDirectBinauralHRTF, exactly the flag-off path
//   * Simple <-> HRTF fade         -> renderDirectBinauralHRTF in blend mode: the Woodworth path
//                                     runs once into simpleWetL_/R_ and is crossfaded with the
//                                     convolution output on the renderer crossfade's ramp
//==============================================================================
bool RenderEngine::renderBinauralWithProfileFade (const RenderSources& sources, const RenderBlockContext& ctx,
                                                   float* outL, float* outR, int numOutCh)
{
    const int activeIdx = activeRendererIndex.load (std::memory_order_acquire);
    beginRendererFadeIfSwapped (activeIdx, sources.numSamples);

    const bool activeSimple = binauralRenderers[static_cast<size_t> (activeIdx)].isSimpleMode();
    const bool fromSimple = rendererXfading_
                            && binauralRenderers[static_cast<size_t> (rendererXfadeFromIdx_)].isSimpleMode();

    if (activeSimple && (! rendererXfading_ || fromSimple))
    {
        // Two Simple renderers render the same signal, so there is nothing to blend: end the fade
        // and run the Woodworth path once.
        if (rendererXfading_)
            endRendererFade();
        renderSimpleBinauralWoodworth (sources, ctx, outL, outR, numOutCh);
        return true;
    }

    if (! rendererXfading_ || (! activeSimple && ! fromSimple))
    {
        renderDirectBinauralHRTF (sources, outL, outR, numOutCh, activeIdx);
        return false;
    }

    return renderDirectBinauralHRTF (sources, outL, outR, numOutCh, activeIdx, &ctx);
}

//==============================================================================
// renderSimpleBinauralWoodworth -- the Woodworth pan gains, preceded by the
// position-blended rear/up/down cue bank (BUG-01, SpatialCore#15, D-01):
//     y = x + wRear (R(x) - x) + wUp (U(x) - x) + wDown (D(x) - x)
// with the weights from the sanitised sources.objects position, interpolated
// per sample across the block like the pan gains. At ear level in the front
// half every weight is exactly 0, so y == x and the output is bit-identical
// to the pre-change engine.
//==============================================================================
void RenderEngine::renderSimpleBinauralWoodworth (const RenderSources& sources,
                                                    const RenderBlockContext& blockCtx,
                                                    float* outL, float* outR, int /*numOutCh*/)
{
    const int numSamples = sources.numSamples;
    float invN = (numSamples > 1) ? 1.0f / static_cast<float> (numSamples - 1) : 1.0f;

    // Block-rate target cue weights from the sanitised positions (finite by
    // construction: sanitizeSources replaced any non-finite value).
    SimpleCueWeights targetWeights[MAX_SOURCES];
    for (int t = 0; t < MAX_SOURCES; ++t)
    {
        targetWeights[t] = computeSimpleCueWeights (
            juce::degreesToRadians (sources.objects[t].azimuthDeg),
            juce::degreesToRadians (sources.objects[t].elevationDeg));
    }

    // Cold start: if the previous block ran another path, the cue state is
    // stale (possibly minutes-old audio). Zero it and start each source's
    // weights at its target so there is neither a decaying tail nor a glide
    // from a position the listener no longer hears.
    if (! simplePathRanLastBlock_)
    {
        resetSimpleCueState();
        for (int t = 0; t < MAX_SOURCES; ++t)
            prevCueWeights_[t] = targetWeights[t];
        simplePathRanLastBlock_ = true;
    }

    for (int s = 0; s < numSamples; ++s)
    {
        float frac = static_cast<float> (s) * invN;
        float wetL = 0.0f, wetR = 0.0f;

        for (int t = 0; t < MAX_SOURCES; ++t)
        {
            float tapFade = (sources.tapFadeGainPerSample[t] != nullptr) ? sources.tapFadeGainPerSample[t][s] : 0.0f;
            float objMono = (sources.monoBuffers[t] != nullptr) ? sources.monoBuffers[t][s] : 0.0f;

            // Run the cue filters for every slot that has a signal, before the
            // tapFade early-out, so the state never goes stale while a tap is
            // faded out. A non-finite input sample reaches the output exactly
            // as before but is fed to the filters as 0, so one bad sample can
            // never poison the recursive state for good.
            if (sources.monoBuffers[t] != nullptr)
            {
                const float xf = std::isfinite (objMono) ? objMono : 0.0f;
                float r = xf, u = xf, d = xf;
                for (int k = 0; k < 3; ++k)
                {
                    r = cueBiquadStep (cueRear_[k], cueStateRear_[t][k], r);
                    u = cueBiquadStep (cueUp_[k], cueStateUp_[t][k], u);
                }
                for (int k = 0; k < 2; ++k)
                    d = cueBiquadStep (cueDown_[k], cueStateDown_[t][k], d);

                const SimpleCueWeights& pw = prevCueWeights_[t];
                const SimpleCueWeights& tw = targetWeights[t];
                const float wRear = pw.rear + frac * (tw.rear - pw.rear);
                const float wUp   = pw.up   + frac * (tw.up   - pw.up);
                const float wDown = pw.down + frac * (tw.down - pw.down);

                // With all three weights exactly 0 this is x + 0 + 0 + 0 == x.
                objMono = objMono + wRear * (r - objMono)
                                  + wUp   * (u - objMono)
                                  + wDown * (d - objMono);
            }

            if (tapFade <= 0.0f) continue;

            // Interpolate between previous and current block gains
            float gL = prevBinauralGains[t].leftGain  + frac * (blockCtx.objGains[t].leftGain  - prevBinauralGains[t].leftGain);
            float gR = prevBinauralGains[t].rightGain + frac * (blockCtx.objGains[t].rightGain - prevBinauralGains[t].rightGain);
            wetL += objMono * gL * tapFade;
            wetR += objMono * gR * tapFade;
        }

        if (outL != nullptr) outL[s] = wetL;
        if (outR != nullptr) outR[s] = wetR;
    }

    // Store current gains as previous for next block
    for (int t = 0; t < MAX_SOURCES; ++t)
    {
        prevBinauralGains[t] = blockCtx.objGains[t];
        prevCueWeights_[t] = targetWeights[t];
    }
}

//==============================================================================
// renderStereoVariant — verbatim-transplanted (gain math for the 5 stereo
// modes stays where it was computed, in the consumer, and is handed in via
// RenderBlockContext.objGainL/objGainR — see the plan's D-09 note: this gain
// computation is not a SpatializationAlgorithm and was never touched by the
// Plan 08-03 algorithm extraction).
//==============================================================================
void RenderEngine::renderStereoVariant (const RenderSources& sources,
                                          const RenderBlockContext& blockCtx,
                                          float* outL, float* outR, int /*numOutCh*/)
{
    const int numSamples = sources.numSamples;
    float invN = (numSamples > 1) ? 1.0f / static_cast<float> (numSamples - 1) : 1.0f;

    for (int s = 0; s < numSamples; ++s)
    {
        float frac = static_cast<float> (s) * invN;
        float wetL = 0.0f, wetR = 0.0f;

        for (int t = 0; t < MAX_SOURCES; ++t)
        {
            float tapFade = (sources.tapFadeGainPerSample[t] != nullptr) ? sources.tapFadeGainPerSample[t][s] : 0.0f;
            float objMono = (sources.monoBuffers[t] != nullptr) ? sources.monoBuffers[t][s] : 0.0f;
            if (tapFade <= 0.0f) continue;

            // Interpolate between previous and current block gains
            float gL = prevStereoGainL[t] + frac * (blockCtx.objGainL[t] - prevStereoGainL[t]);
            float gR = prevStereoGainR[t] + frac * (blockCtx.objGainR[t] - prevStereoGainR[t]);
            wetL += objMono * gL * tapFade;
            wetR += objMono * gR * tapFade;
        }

        if (outL != nullptr) outL[s] = wetL;
        if (outR != nullptr) outR[s] = wetR;
    }

    // Store current gains as previous for next block
    for (int t = 0; t < MAX_SOURCES; ++t)
    {
        prevStereoGainL[t] = blockCtx.objGainL[t];
        prevStereoGainR[t] = blockCtx.objGainR[t];
    }
}

//==============================================================================
// renderAmbisonicsOutput — verbatim-transplanted (max-rE weighting +
// NFC-HOA per-order shelf filters, now engine-owned state).
//==============================================================================
void RenderEngine::renderAmbisonicsOutput (const RenderSources& sources,
                                             const RenderBlockContext& blockCtx,
                                             float* const* outChannels, int numOutCh)
{
    const int numSamples = sources.numSamples;
    const int ambiOrder = blockCtx.ambiOrder;
    const int numAmbiCh = std::min ((ambiOrder + 1) * (ambiOrder + 1), kMaxAmbiChannels);

    // Max-rE weights per SH order for perceptual quality (Zotter & Frank 2012)
    if (ambiOrder != cachedMaxrEOrder)
    {
        const float maxrEDenom = 2.0f * static_cast<float> (ambiOrder) + 2.0f;
        for (int n = 0; n <= ambiOrder; ++n)
            cachedMaxrE[n] = std::cos (static_cast<float> (n) * juce::MathConstants<float>::pi / maxrEDenom);
        cachedMaxrEOrder = ambiOrder;
    }

    // --- NFC-HOA: Update filter coefficients when distance changes (block-rate) ---
    constexpr float nfcSmoothAlpha = 0.15f;
    // Sample rate for the NFC pole/zero computation matches the pre-move
    // code's direct read of currentSampleRate — threaded through
    // RenderBlockContext per-block (D-02: renderBlock stays a pure function
    // of its inputs, no hidden dependency on prepare()'s cached rate).
    const double sr = blockCtx.sampleRate;

    for (int t = 0; t < MAX_SOURCES; ++t)
    {
        if (! sources.objectLive[t]) continue;
        float distMeters = sources.objects[t].distance * 10.0f;  // 0..1 -> 0..10m

        smoothedNfcDistance[t] += nfcSmoothAlpha * (distMeters - smoothedNfcDistance[t]);

        if (std::abs (smoothedNfcDistance[t] - prevNfcDistance[t]) > 0.01f)
        {
            for (int ord = 0; ord < ambiOrder && ord < kMaxAmbiOrder; ++ord)
            {
                int n = ord + 1;  // SH order (1-based)
                constexpr float c = 343.0f;  // speed of sound, m/s
                float r = std::max (0.05f, smoothedNfcDistance[t]);
                float fPole = static_cast<float> (n) * c / (2.0f * juce::MathConstants<float>::pi * r);
                float fZero = static_cast<float> (n) * c / (2.0f * juce::MathConstants<float>::pi * kNfcReferenceRadius);
                float maxFreq = static_cast<float> (sr) * 0.25f;
                fPole = std::min (fPole, maxFreq);
                fZero = std::min (fZero, maxFreq);
                float wPole = std::tan (juce::MathConstants<float>::pi * fPole / static_cast<float> (sr));
                float wZero = std::tan (juce::MathConstants<float>::pi * fZero / static_cast<float> (sr));
                float b0 = (1.0f + wZero);
                float b1 = (wZero - 1.0f);
                float a0 = (1.0f + wPole);
                float a1 = (wPole - 1.0f);
                *nfcFilters[t][ord].coefficients =
                    juce::dsp::IIR::Coefficients<float> (b0 / a0, b1 / a0, 1.0f, a1 / a0);
            }
            prevNfcDistance[t] = smoothedNfcDistance[t];
        }
    }

    // Pre-compute SH coefficients per enabled tap (block-rate)
    float objSHCoeffs[MAX_SOURCES][kMaxAmbiChannels] = {};
    for (int t = 0; t < MAX_SOURCES; ++t)
    {
        if (! sources.objectLive[t]) continue;
        float azRad = juce::degreesToRadians (sources.objects[t].azimuthDeg);
        float elRad = juce::degreesToRadians (sources.objects[t].elevationDeg);
        for (int c = 0; c < numAmbiCh; ++c)
            objSHCoeffs[t][c] = evalSH (c, azRad, elRad) * cachedMaxrE[acnToOrder (c)];
    }

    float invN = (numSamples > 1) ? 1.0f / static_cast<float> (numSamples - 1) : 1.0f;

    for (int s = 0; s < numSamples; ++s)
    {
        float frac = static_cast<float> (s) * invN;
        float ambiAccum[kMaxAmbiChannels] = {};

        for (int t = 0; t < MAX_SOURCES; ++t)
        {
            float tapFade = (sources.tapFadeGainPerSample[t] != nullptr) ? sources.tapFadeGainPerSample[t][s] : 0.0f;
            float objMono = (sources.monoBuffers[t] != nullptr) ? sources.monoBuffers[t][s] : 0.0f;
            if (tapFade <= 0.0f) continue;

            float dist = (sources.distGainPerSample[t] != nullptr) ? sources.distGainPerSample[t][s] : 1.0f;
            float scaledMono = objMono * dist * tapFade;

            // Order 0 (W channel): no NFC needed
            float sh0 = prevSHCoeffs[t][0] + frac * (objSHCoeffs[t][0] - prevSHCoeffs[t][0]);
            ambiAccum[0] += scaledMono * sh0;

            // Orders 1+: apply NFC-HOA per-order shelf filter
            for (int ord = 0; ord < ambiOrder && ord < kMaxAmbiOrder; ++ord)
            {
                float nfcMono = nfcFilters[t][ord].processSample (scaledMono);
                int startACN = (ord + 1) * (ord + 1);
                int endACN = (ord + 2) * (ord + 2);
                for (int c = startACN; c < endACN && c < numAmbiCh; ++c)
                {
                    float shc = prevSHCoeffs[t][c] + frac * (objSHCoeffs[t][c] - prevSHCoeffs[t][c]);
                    ambiAccum[c] += nfcMono * shc;
                }
            }
        }

        for (int c = 0; c < numAmbiCh && c < numOutCh; ++c)
            if (outChannels[c] != nullptr)
                outChannels[c][s] = ambiAccum[c];
    }

    // Store current SH coefficients as previous for next block (distGain
    // interpolation-state carry-forward is the consumer's responsibility).
    for (int t = 0; t < MAX_SOURCES; ++t)
        for (int c = 0; c < numAmbiCh; ++c)
            prevSHCoeffs[t][c] = objSHCoeffs[t][c];
}

//==============================================================================
// renderDiscreteSurround — verbatim-transplanted.
//==============================================================================
void RenderEngine::renderDiscreteSurround (const RenderSources& sources,
                                             const RenderBlockContext& blockCtx,
                                             const LayoutState& layout,
                                             float* const* outChannels, int numOutCh)
{
    const int numSamples = sources.numSamples;
    const auto& surLayout = layout.layout;
    const int numSpeakers = surLayout.numSpeakers;
    const int lfeIdx = surLayout.lfeChannelIndex;

    float invN = (numSamples > 1) ? 1.0f / static_cast<float> (numSamples - 1) : 1.0f;

    for (int s = 0; s < numSamples; ++s)
    {
        float frac = static_cast<float> (s) * invN;
        float channelAccum[MAX_SPEAKERS] = {};
        float wetMono = 0.0f;  // For LFE generation

        for (int t = 0; t < MAX_SOURCES; ++t)
        {
            float tapFade = (sources.tapFadeGainPerSample[t] != nullptr) ? sources.tapFadeGainPerSample[t][s] : 0.0f;
            float objMono = (sources.monoBuffers[t] != nullptr) ? sources.monoBuffers[t][s] : 0.0f;
            if (tapFade <= 0.0f) continue;

            float dist = (sources.distGainPerSample[t] != nullptr) ? sources.distGainPerSample[t][s] : 1.0f;

            for (int sp = 0; sp < numSpeakers; ++sp)
            {
                float g = prevChannelGains[t][sp] + frac * (blockCtx.objChannelGains[t][sp] - prevChannelGains[t][sp]);
                channelAccum[sp] += objMono * dist * g * tapFade;
            }

            wetMono += objMono * dist * tapFade;
        }

        for (int sp = 0; sp < numSpeakers; ++sp)
        {
            int ch = surLayout.speakers[sp].channelIndex;
            if (ch >= 0 && ch < numOutCh && outChannels[ch] != nullptr)
                outChannels[ch][s] = channelAccum[sp];
        }

        // LFE generation — low-pass filtered mono sum at -10 dB (raw, no dw/outGain)
        if (lfeIdx >= 0 && lfeIdx < numOutCh && outChannels[lfeIdx] != nullptr)
            outChannels[lfeIdx][s] = static_cast<float> (lfeFilter.processSample (static_cast<double> (wetMono))) * 0.316f;
    }

    // Store current channel gains as previous for next block (distGain
    // interpolation-state carry-forward is the consumer's responsibility).
    for (int t = 0; t < MAX_SOURCES; ++t)
        for (int sp = 0; sp < numSpeakers; ++sp)
            prevChannelGains[t][sp] = blockCtx.objChannelGains[t][sp];
}

//==============================================================================
// Output-format three-slot switching (glitch-free handoff via TripleBufferIndex,
// moved INTO the engine per the locked IO-ownership decision). Verbatim-transplanted from
// OpenSpatialDelayProcessor::activateLayout; the order-3 speaker decode now
// comes from AmbisonicsCodec::getDecodeMatrix, the library's one decoder
// (D-09), pinned to the transplanted original by the [ambi-pin] test.
//==============================================================================
void RenderEngine::setOutputFormat (OutputFormat format)
{
    // Single-writer detector (Debug jassert only; Release behaviour is
    // unchanged). The caller owns serialisation -- see RenderEngine.h.
    const bool overlapped = inSetOutputFormat_.exchange (true, std::memory_order_acquire);
    jassert (! overlapped);

    activateLayout (format);

    // On overlap the other writer's call is still in flight and owns the flag.
    if (! overlapped)
        inSetOutputFormat_.store (false, std::memory_order_release);
}

OutputFormat RenderEngine::getActiveOutputFormat() const
{
    return getActiveLayout().format;
}

const RenderEngine::LayoutState& RenderEngine::getActiveLayout() const
{
    // Writer-thread view: the slot most recently published. Never reached from
    // the audio thread.
    return layoutBuffers[static_cast<size_t> (layoutSlots_.lastPublishedSlot())];
}

const RenderEngine::LayoutState& RenderEngine::acquireBlockLayout()
{
    // The one audio-thread entry into the handoff: wait-free, at most one
    // atomic exchange per block.
    return layoutBuffers[static_cast<size_t> (layoutSlots_.acquireLatest())];
}

void RenderEngine::deriveDispatchFromLayout (const LayoutState& layout, RenderBlockContext& ctx)
{
    // T-02-23 / ASVS V5: bound the format before it indexes the registry.
    const int clamped = juce::jlimit (0, NUM_OUTPUT_FORMATS - 1, static_cast<int> (layout.format));
    const auto fmt = static_cast<OutputFormat> (clamped);
    const auto& info = OutputFormatRegistry::getInfo (fmt);

    ctx.activeFormat = fmt;
    ctx.ambiOrder = info.ambiOrder;
    ctx.isStereoVariant = info.isStereoVariant;
    ctx.isBinaural = (fmt == OutputFormat::Binaural);
    ctx.isAmbiOutput = info.isAmbisonicsOutput;
}

void RenderEngine::activateLayout (OutputFormat format)
{
    auto& buf = layoutBuffers[static_cast<size_t> (layoutSlots_.writeSlot())];
    buf.format = format;

    switch (format)
    {
        case OutputFormat::Quad:          buf.layout = getLayoutDef (LayoutID::Quad);       break;
        case OutputFormat::Surround5_0:   buf.layout = getLayoutDef (LayoutID::S5_0);       break;
        case OutputFormat::Surround5_1:   buf.layout = getLayoutDef (LayoutID::S5_1);       break;
        case OutputFormat::Surround7_0:   buf.layout = getLayoutDef (LayoutID::S7_0);       break;
        case OutputFormat::Surround7_1:   buf.layout = getLayoutDef (LayoutID::S7_1);       break;
        case OutputFormat::Surround9_1:   buf.layout = getLayoutDef (LayoutID::S9_1);       break;
        case OutputFormat::Surround5_1_2: buf.layout = getLayoutDef (LayoutID::S5_1_2);     break;
        case OutputFormat::Surround5_1_4: buf.layout = getLayoutDef (LayoutID::S5_1_4);     break;
        case OutputFormat::Surround7_1_2: buf.layout = getLayoutDef (LayoutID::S7_1_2);     break;
        case OutputFormat::Surround7_1_4: buf.layout = getLayoutDef (LayoutID::S7_1_4);     break;
        case OutputFormat::Surround7_1_6: buf.layout = getLayoutDef (LayoutID::S7_1_6);     break;
        case OutputFormat::Surround9_1_4: buf.layout = getLayoutDef (LayoutID::S9_1_4);     break;
        case OutputFormat::Surround9_1_6: buf.layout = getLayoutDef (LayoutID::S9_1_6);     break;
        case OutputFormat::SurroundSML13_1: buf.layout = getLayoutDef (LayoutID::SML13_1); break;
        case OutputFormat::Octaphonic:    buf.layout = getLayoutDef (LayoutID::Octaphonic); break;

        case OutputFormat::AmbisonicsFOA:
        case OutputFormat::AmbisonicsSOA:
        case OutputFormat::AmbisonicsHOA:
        case OutputFormat::Ambisonics4OA:
        case OutputFormat::Ambisonics5OA:
        case OutputFormat::Ambisonics6OA:
        {
            // Ambisonics output: no speaker layout, just channel count
            int fmtIdx = static_cast<int> (format);
            buf.layout = {};
            buf.layout.numSpeakers = 0;
            buf.layout.lfeChannelIndex = -1;
            buf.layout.totalChannels = OutputFormatRegistry::table[static_cast<size_t> (fmtIdx)].requiredChannels;
            buf.ambiNumSpeakers = 0;
            buf.vbapTriplets.clear();
            layoutSlots_.publish();
            return;
        }

        case OutputFormat::Binaural:
        case OutputFormat::Stereo:
        default:
            buf.layout = {};
            buf.layout.numSpeakers = 0;
            buf.layout.lfeChannelIndex = -1;
            buf.layout.totalChannels = 2;
            buf.ambiNumSpeakers = 0;
            buf.vbapTriplets.clear();
            layoutSlots_.publish();
            return;
    }

    // Order-3 Ambisonics speaker decode through the one shared decoder (D-09).
    // getDecodeMatrix writes decodeMatrix[s * 16 + c], which is exactly
    // ambiDecodeMatrix[s][c] because the row width equals the order-3 channel
    // count. Message/prepare thread: DR-1 does not apply.
    static_assert (MAX_SPEAKERS == (3 + 1) * (3 + 1),
                   "ambiDecodeMatrix rows must equal the order-3 channel count");
    float speakerAz[MAX_SPEAKERS] = {};
    float speakerEl[MAX_SPEAKERS] = {};
    for (int s = 0; s < buf.layout.numSpeakers; ++s)
    {
        speakerAz[s] = buf.layout.speakers[s].azimuthRad;
        speakerEl[s] = buf.layout.speakers[s].elevationRad;
    }
    // Rows at or beyond the speaker count are cleared, not left stale from the
    // layout this buffer held two switches ago.
    std::memset (buf.ambiDecodeMatrix, 0, sizeof (buf.ambiDecodeMatrix));
    if (! AmbisonicsCodec::getDecodeMatrix (3, buf.layout.numSpeakers, speakerAz, speakerEl,
                                            &buf.ambiDecodeMatrix[0][0]))
        jassertfalse;   // cannot happen for a shipped layout (<= 15 speakers, order 3);
                        // the memset above leaves a silent decode if it ever did
    buf.ambiNumSpeakers = buf.layout.numSpeakers;

    // Build 3D VBAP triplets using the SpatialCore IO helper
    buildVBAPTripletsForLayout (buf.layout, buf.vbapTriplets);

    // D-02a: a height layout must yield regular triplets and a flat layout must
    // yield none. This is the layout-build path (message/prepare thread, already
    // allocating), reachable only by a SpatialCore developer editing layoutDefs
    // or the triplet builder: OutputFormat is a closed enum, activateLayout is
    // private, and there is no custom-layout entry point. DR-1 does not apply
    // here, so a crash in every build type is the correct loud failure (D-02).
    if (buf.vbapTriplets.empty() == layoutHasHeight (buf.layout))
    {
        jassertfalse;   // Debug: stop at the cause
        std::abort();   // Release: layout table / builder mismatch
    }

    // Append the ITU-R BS.2127 lower-hemisphere triplets (D-04) after the
    // guard above, so the guard still sees the regular list alone (F11).
    // Message/prepare thread: allocation is fine here.
    appendLowerHemisphereTriplets (buf.layout, buf.vbapTriplets);

    // Publish: the audio thread picks up the fully-populated slot at its next
    // block; a publish superseded before then is skipped whole.
    layoutSlots_.publish();
}

} // namespace spatialcore
