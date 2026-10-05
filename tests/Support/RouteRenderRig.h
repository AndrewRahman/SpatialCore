#pragma once

// RouteRenderRig -- the consumer-side wiring a plugin writes to drive RenderEngine (Phase 4,
// D-11 / D-12 / D-18).
//
// A control route (ADM-OSC, a trajectory, the spatial map) ends in an object position. These
// tests prove the position moves the SOUND, not just a number, so the rig renders a short mono
// tone through RenderEngine and reports the RMS of every output channel. Nothing here is library
// code: the glue between a control source and RenderSources lives with the consumer (D-12), and
// the rig is where this test suite keeps its copy of it.
//
// Both opt-in flags are set on every block (D-18, the INTG-02 opt-out path):
//   - engineComputesGains: RenderEngine fills objGains / objChannelGains from the positions.
//   - engineDerivesDispatch: RenderEngine fills activeFormat, isStereoVariant, isBinaural,
//     isAmbiOutput and ambiOrder from its own layout, so no dispatch field is hand-set here.
// Stereo-variant gains (objGainL / objGainR) stay consumer-side in the library, so tests render
// Binaural (the Simple path, no profile requested) and discrete surround formats, never Stereo.

#include <SpatialCore/Engine/RenderEngine.h>
#include <SpatialCore/Core/SpatialMath.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace spatialcore::test
{

struct RouteRenderRig
{
    static constexpr int kBlockSize = 64;
    static constexpr double kSampleRate = 48000.0;

    RouteRenderRig (OutputFormat format, int numOutChannels)
        : numOutCh (numOutChannels)
    {
        engine.prepare (kSampleRate, kBlockSize);
        engine.setOutputFormat (format);

        mono.resize ((size_t) kBlockSize);
        for (int n = 0; n < kBlockSize; ++n)
            mono[(size_t) n] = 0.5f * std::sin (0.2f * (float) n);

        tapFade.assign ((size_t) kBlockSize, 1.0f);

        for (auto& buf : distGain)
            buf.assign ((size_t) kBlockSize, 1.0f);
    }

    // Overwrites one object slot. A fresh slot is disabled until setObject enables it.
    void setObject (int index, float azDeg, float elDeg, float dist, bool enabled = true)
    {
        auto& o = objects[(size_t) index];
        o.azimuthDeg = azDeg;
        o.elevationDeg = elDeg;
        o.distance = dist;
        o.enabled = enabled;
    }

    // The consumer rule documented on ADMOSCReceiver::Listener::admPositionReceived: an axis
    // that arrives as NaN was not sent in this message, so the stored value is kept.
    void mergeObject (int index, float azDeg, float elDeg, float dist)
    {
        auto& o = objects[(size_t) index];
        if (! std::isnan (azDeg)) o.azimuthDeg = azDeg;
        if (! std::isnan (elDeg)) o.elevationDeg = elDeg;
        if (! std::isnan (dist))  o.distance = dist;
    }

    ObjectState object (int index) const { return objects[(size_t) index]; }

    // Renders `blocks` blocks (so the engine's per-block gain ramps settle) and returns the RMS
    // of each output channel over the last block.
    std::array<float, MAX_SPEAKERS> renderRms (int blocks = 4)
    {
        RenderSources sources;
        sources.numSamples = kBlockSize;
        for (int i = 0; i < MAX_SOURCES; ++i)
        {
            if (! objects[(size_t) i].enabled)
                continue;

            // What a consumer supplies for the surround, HRTF and Ambisonics paths. The Simple
            // binaural path ignores it and folds distance into its own gains.
            const float g = distanceAttenuation (objects[(size_t) i].distance);
            std::fill (distGain[(size_t) i].begin(), distGain[(size_t) i].end(), g);

            sources.monoBuffers[i] = mono.data();
            sources.tapFadeGainPerSample[i] = tapFade.data();
            sources.distGainPerSample[i] = distGain[(size_t) i].data();
            sources.objectLive[i] = true;
            sources.objects[i] = objects[(size_t) i];
        }

        std::array<std::vector<float>, MAX_SPEAKERS> channels;
        float* ptrs[MAX_SPEAKERS] = {};
        for (int c = 0; c < MAX_SPEAKERS; ++c)
            channels[(size_t) c].assign ((size_t) kBlockSize, 0.0f), ptrs[c] = channels[(size_t) c].data();

        for (int b = 0; b < blocks; ++b)
        {
            for (auto& ch : channels)
                std::fill (ch.begin(), ch.end(), 0.0f);

            RenderBlockContext ctx;
            ctx.sampleRate = kSampleRate;
            ctx.engineComputesGains = true;
            ctx.engineDerivesDispatch = true;
            engine.renderBlock (sources, ctx, ptrs, numOutCh);
        }

        std::array<float, MAX_SPEAKERS> rms {};
        for (int c = 0; c < MAX_SPEAKERS; ++c)
        {
            double sum = 0.0;
            for (float s : channels[(size_t) c])
                sum += (double) s * (double) s;
            rms[(size_t) c] = (float) std::sqrt (sum / (double) kBlockSize);
        }
        return rms;
    }

    float leftRightRatio (int blocks = 4)
    {
        const auto rms = renderRms (blocks);
        return rms[0] / std::max (rms[1], 1e-9f);
    }

    static int loudestChannel (const std::array<float, MAX_SPEAKERS>& rms, int numChannels)
    {
        int best = 0;
        for (int c = 1; c < numChannels; ++c)
            if (rms[(size_t) c] > rms[(size_t) best])
                best = c;
        return best;
    }

    RenderEngine engine;
    int numOutCh;

private:
    std::vector<float> mono;
    std::vector<float> tapFade;
    std::array<std::vector<float>, MAX_SOURCES> distGain;
    std::array<ObjectState, MAX_SOURCES> objects {};
};

} // namespace spatialcore::test
