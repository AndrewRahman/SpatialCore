#include <SpatialCore/Engine/RenderEngine.h>
#include <SpatialCore/Core/SpatialMath.h>

#include <cstring>
#include <cmath>
#include <algorithm>

namespace spatialcore
{

RenderEngine::RenderEngine() = default;
RenderEngine::~RenderEngine() = default;

//==============================================================================
// prepare() — engine-owned DSP setup slice of the pre-move
// OpenSpatialDelayProcessor::prepareToPlay (verbatim behavior, transplanted
// scope only: LFE filter, NFC-HOA filters, binaural renderers, crossfade
// buffers, direct-binaural accumulation buffers).
//==============================================================================
void RenderEngine::prepare (double sampleRate, int maxBlockSize)
{
    juce::dsp::ProcessSpec spec { sampleRate, static_cast<juce::uint32> (maxBlockSize), 1 };

    // v0.2: Configure LFE low-pass filter (120 Hz, 2nd order Butterworth)
    lfeFilter.prepare (spec);
    lfeFilter.reset();
    *lfeFilter.coefficients = *juce::dsp::IIR::Coefficients<float>::makeLowPass (sampleRate, 120.0f);

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

    // Pre-allocate renderer crossfade buffers (issue #131: avoid audio-thread allocation)
    xfadeWetL_.resize (static_cast<size_t> (maxBlockSize), 0.0f);
    xfadeWetR_.resize (static_cast<size_t> (maxBlockSize), 0.0f);

    // v0.5: Contiguous per-source accumulation buffers for direct binaural
    sourceAccumStorage_.resize (static_cast<size_t> (MAX_SOURCES * maxBlockSize), 0.0f);
    for (int src = 0; src < MAX_SOURCES; ++src)
        sourceAccumBufPtrs[src] = sourceAccumStorage_.data() + src * maxBlockSize;
    wetBufL_.resize (static_cast<size_t> (maxBlockSize), 0.0f);
    wetBufR_.resize (static_cast<size_t> (maxBlockSize), 0.0f);
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
    float* outL = (numOutCh > 0) ? outChannels[0] : nullptr;
    float* outR = (numOutCh > 1) ? outChannels[1] : nullptr;

    if (blockCtx.isStereoVariant)
    {
        renderStereoVariant (sources, blockCtx, outL, outR, numOutCh);
    }
    else if (blockCtx.isBinaural && blockCtx.useHRTF)
    {
        renderDirectBinauralHRTF (sources, outL, outR, numOutCh);
    }
    else if (blockCtx.isBinaural)
    {
        renderSimpleBinauralWoodworth (sources, blockCtx, outL, outR, numOutCh);
    }
    else if (blockCtx.isAmbiOutput)
    {
        renderAmbisonicsOutput (sources, blockCtx, outChannels, numOutCh);
    }
    else
    {
        renderDiscreteSurround (sources, blockCtx, outChannels, numOutCh);
    }
}

//==============================================================================
// renderDirectBinauralHRTF — verbatim-transplanted from
// OpenSpatialDelayProcessor::renderDirectBinauralHRTF. Engine-boundary change
// only: reads pre-delayed/pre-fed-back/pre-Doppler-pitched mono buffers from
// RenderSources instead of calling readObjectSample()/writeDelayLine()/
// processFeedbackSample() itself (those stay OSD-side, D-02).
//==============================================================================
void RenderEngine::renderDirectBinauralHRTF (const RenderSources& sources, float* outL, float* outR, int numOutCh)
{
    const int numSamples = sources.numSamples;

    int currentActiveIdx = activeRendererIndex.load (std::memory_order_acquire);
    auto& activeRenderer = binauralRenderers[static_cast<size_t> (currentActiveIdx)];

    // Detect renderer swap -> start crossfade.
    if (currentActiveIdx != prevActiveRendererIdx_ && ! rendererXfading_)
    {
        rendererXfading_ = true;
        rendererXfadeActive_.store (true, std::memory_order_release);
        rendererXfadeBlockCount_ = 0;
        rendererXfadeFromIdx_ = prevActiveRendererIdx_;
        prevRxFadeOut_ = 1.0f;
        prevRxFadeIn_ = 0.0f;
        prevActiveRendererIdx_ = currentActiveIdx;
    }

    // Update per-source HRIRs at block boundary for any taps that moved
    // (new renderer only — old renderer keeps its existing HRIRs during crossfade)
    for (int t = 0; t < MAX_SOURCES; ++t)
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

    activeRenderer.renderSourceBuffers (srcBufPtrs, sourceEnabled, MAX_SOURCES,
                                        numSamples, wetBufL_.data(), wetBufR_.data());

    // Renderer-level crossfade.
    if (rendererXfading_)
    {
        auto ns = static_cast<size_t> (numSamples);
        if (xfadeWetL_.size() < ns) { xfadeWetL_.resize (ns, 0.0f); xfadeWetR_.resize (ns, 0.0f); }

        auto& oldRenderer = binauralRenderers[static_cast<size_t> (rendererXfadeFromIdx_)];
        oldRenderer.renderSourceBuffers (srcBufPtrs, sourceEnabled, MAX_SOURCES,
                                          numSamples, xfadeWetL_.data(), xfadeWetR_.data());

        ++rendererXfadeBlockCount_;
        float progress = static_cast<float> (rendererXfadeBlockCount_)
                       / static_cast<float> (kRendererXfadeBlocks);
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
            wetBufL_[static_cast<size_t> (i)] = xfadeWetL_[static_cast<size_t> (i)] * gOut
                                              + wetBufL_[static_cast<size_t> (i)]     * gIn;
            wetBufR_[static_cast<size_t> (i)] = xfadeWetR_[static_cast<size_t> (i)] * gOut
                                              + wetBufR_[static_cast<size_t> (i)]     * gIn;
        }

        prevRxFadeOut_ = fadeOutGain;
        prevRxFadeIn_  = fadeInGain;

        if (rendererXfadeBlockCount_ >= kRendererXfadeBlocks)
        {
            rendererXfading_ = false;
            rendererXfadeActive_.store (false, std::memory_order_release);
        }
    }

    // === PASS 3: Write raw wet signal to output (dry/wet mix handled by consumer) ===
    if (outL != nullptr) std::memcpy (outL, wetBufL_.data(), sizeof (float) * static_cast<size_t> (numSamples));
    if (outR != nullptr) std::memcpy (outR, wetBufR_.data(), sizeof (float) * static_cast<size_t> (numSamples));

    // Remaining channels (ch >= 2) are left untouched here; the consumer clears
    // them exactly as the pre-move code did (`buffer.clear(ch, ...)` on a
    // juce::AudioBuffer is a consumer-side concern, not engine state).
    (void) numOutCh;
}

//==============================================================================
// renderSimpleBinauralWoodworth — verbatim-transplanted.
//==============================================================================
void RenderEngine::renderSimpleBinauralWoodworth (const RenderSources& sources,
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
        prevBinauralGains[t] = blockCtx.objGains[t];
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
                                             float* const* outChannels, int numOutCh)
{
    const int numSamples = sources.numSamples;
    const auto& surLayout = getActiveLayout().layout;
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
            outChannels[lfeIdx][s] = lfeFilter.processSample (wetMono) * 0.316f;
    }

    // Store current channel gains as previous for next block (distGain
    // interpolation-state carry-forward is the consumer's responsibility).
    for (int t = 0; t < MAX_SOURCES; ++t)
        for (int sp = 0; sp < numSpeakers; ++sp)
            prevChannelGains[t][sp] = blockCtx.objChannelGains[t][sp];
}

//==============================================================================
// Output-format double-buffered switching (glitch-free swap, moved INTO the
// engine per the locked IO-ownership decision). Verbatim-transplanted from
// OpenSpatialDelayProcessor::activateLayout / computeAmbiDecodeForLayout.
//==============================================================================
void RenderEngine::setOutputFormat (OutputFormat format)
{
    activateLayout (format);
}

OutputFormat RenderEngine::getActiveOutputFormat() const
{
    return getActiveLayout().format;
}

const RenderEngine::LayoutState& RenderEngine::getActiveLayout() const
{
    return layoutBuffers[static_cast<size_t> (activeLayoutIndex.load (std::memory_order_acquire))];
}

void RenderEngine::activateLayout (OutputFormat format)
{
    auto& buf = layoutBuffers[static_cast<size_t> (prepareLayoutIndex)];
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
            activeLayoutIndex.store (prepareLayoutIndex, std::memory_order_release);
            prepareLayoutIndex = 1 - prepareLayoutIndex;
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
            activeLayoutIndex.store (prepareLayoutIndex, std::memory_order_release);
            prepareLayoutIndex = 1 - prepareLayoutIndex;
            return;
    }

    // Compute Ambisonics decode matrix using shared helper
    computeAmbiDecodeForLayout (buf.layout, buf.ambiDecodeMatrix, buf.ambiNumSpeakers);

    // Build 3D VBAP triplets using the SpatialCore IO helper
    buildVBAPTripletsForLayout (buf.layout, buf.vbapTriplets);

    jassert (buf.vbapTriplets.empty() == ! layoutHasHeight (buf.layout));

    // Atomic swap: audio thread now reads the fully-populated buffer
    activeLayoutIndex.store (prepareLayoutIndex, std::memory_order_release);
    prepareLayoutIndex = 1 - prepareLayoutIndex;
}

//==============================================================================
// computeAmbiDecodeForLayout — verbatim-transplanted from
// OpenSpatialDelayProcessor::computeAmbiDecodeForLayout.
// D = E^T (E E^T + epsilon I)^{-1}  (Tikhonov-regularized pseudo-inverse)
//==============================================================================
void RenderEngine::computeAmbiDecodeForLayout (const SpeakerLayout& layout,
                                                float (*outMatrix)[MAX_SPEAKERS],
                                                int& outNumSpeakers)
{
    const int N = layout.numSpeakers;
    const int M = 16; // HOA_CHANNELS (surround decode cap, 3rd order)
    outNumSpeakers = N;

    // Build encoding matrix E[c][s] = evalSH(c, speaker_s_position)
    float E[16][16] = {};
    for (int s = 0; s < N; ++s)
        for (int c = 0; c < M; ++c)
            E[c][s] = evalSH (c, layout.speakers[s].azimuthRad,
                                 layout.speakers[s].elevationRad);

    // Compute EET = E * E^T  (M x M)
    float EET[16][16] = {};
    for (int i = 0; i < M; ++i)
        for (int j = 0; j < M; ++j)
        {
            float sum = 0.0f;
            for (int s = 0; s < N; ++s)
                sum += E[i][s] * E[j][s];
            EET[i][j] = sum;
        }

    // Tikhonov regularization: EET += epsilon * I
    float epsilon = 0.01f;
    for (int i = 0; i < M; ++i)
        EET[i][i] += epsilon;

    // Invert EET via Gauss-Jordan (M x M, small matrix)
    float inv[16][16] = {};
    for (int i = 0; i < M; ++i)
        inv[i][i] = 1.0f;

    float aug[16][16];
    for (int i = 0; i < M; ++i)
        for (int j = 0; j < M; ++j)
            aug[i][j] = EET[i][j];

    for (int col = 0; col < M; ++col)
    {
        int pivot = col;
        for (int row = col + 1; row < M; ++row)
            if (std::abs (aug[row][col]) > std::abs (aug[pivot][col]))
                pivot = row;

        if (pivot != col)
        {
            std::swap_ranges (aug[col], aug[col] + M, aug[pivot]);
            std::swap_ranges (inv[col], inv[col] + M, inv[pivot]);
        }

        float diagVal = aug[col][col];
        if (std::abs (diagVal) < 1e-10f) continue;

        for (int j = 0; j < M; ++j)
        {
            aug[col][j] /= diagVal;
            inv[col][j] /= diagVal;
        }

        for (int row = 0; row < M; ++row)
        {
            if (row == col) continue;
            float factor = aug[row][col];
            for (int j = 0; j < M; ++j)
            {
                aug[row][j] -= factor * aug[col][j];
                inv[row][j] -= factor * inv[col][j];
            }
        }
    }

    // D[s][c] = sum_k E^T[s][k] * inv[k][c] = sum_k E[k][s] * inv[k][c]
    for (int s = 0; s < N; ++s)
        for (int c = 0; c < M; ++c)
        {
            float sum = 0.0f;
            for (int k = 0; k < M; ++k)
                sum += E[k][s] * inv[k][c];
            outMatrix[s][c] = sum;
        }
}

} // namespace spatialcore
