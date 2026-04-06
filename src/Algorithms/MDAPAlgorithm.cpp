#include <SpatialCore/Algorithms/MDAPAlgorithm.h>
#include <SpatialCore/Core/SpatialMath.h>

namespace spatialcore
{

//==============================================================================
// MDAPAlgorithm -- Multiple-Direction Amplitude Panning (Pulkki 2000)
//==============================================================================
void MDAPAlgorithm::computeGains (const SourcePosition& source, const LayoutContext& ctx,
                                   float* outputGains, int numSpeakers) const
{
    constexpr int NUM_AUX = 8;

    for (int s = 0; s < numSpeakers; ++s)
        outputGains[s] = 0.0f;

    if (numSpeakers == 0) return;

    float alphaDegs = 0.9f * (180.0f / static_cast<float> (std::max (1, numSpeakers)));
    alphaDegs = juce::jlimit (5.0f, 30.0f, alphaDegs);
    float alphaRad = juce::degreesToRadians (alphaDegs);

    float px = std::cos (source.elevationRad) * std::sin (source.azimuthRad);
    float py = std::cos (source.elevationRad) * std::cos (source.azimuthRad);
    float pz = std::sin (source.elevationRad);

    float ax, ay, az;
    if (std::abs (pz) < 0.9f)
    {
        ax = py;  ay = -px;  az = 0.0f;
    }
    else
    {
        ax = 0.0f;  ay = pz;  az = -py;
    }
    float aNorm = std::sqrt (ax * ax + ay * ay + az * az);
    if (aNorm > 1e-6f) { ax /= aNorm;  ay /= aNorm;  az /= aNorm; }

    float tempGains[MAX_SPEAKERS] = {};

    // Main source VBAP contribution
    if (! ctx.triplets.empty())
        computeVBAPGains3D (ctx.layout, ctx.triplets, source.azimuthRad, source.elevationRad, tempGains);
    else if (layoutHasHeight (ctx.layout))
    {
        jassertfalse;
        nearestSpeaker3DFallback (ctx.layout, source.azimuthRad, source.elevationRad,
                                  tempGains, numSpeakers);
    }
    else
        computeVBAPGains2D (ctx.layout, source.azimuthRad, tempGains);

    for (int s = 0; s < numSpeakers; ++s)
        outputGains[s] += tempGains[s];

    // Auxiliary sources on a ring at angle alpha around the main direction
    for (int i = 0; i < NUM_AUX; ++i)
    {
        float phi = 2.0f * juce::MathConstants<float>::pi * static_cast<float> (i) / static_cast<float> (NUM_AUX);

        // Rodrigues' rotation: rotate the orthogonal vector around p by phi
        float cphi = std::cos (phi);
        float sphi = std::sin (phi);
        float dot_pa = px * ax + py * ay + pz * az;
        float cross_x = py * az - pz * ay;
        float cross_y = pz * ax - px * az;
        float cross_z = px * ay - py * ax;

        float rot_ax = ax * cphi + cross_x * sphi + px * dot_pa * (1.0f - cphi);
        float rot_ay = ay * cphi + cross_y * sphi + py * dot_pa * (1.0f - cphi);
        float rot_az = az * cphi + cross_z * sphi + pz * dot_pa * (1.0f - cphi);

        // Rotate p around rot_a by alpha to get auxiliary source direction
        float ca = std::cos (alphaRad);
        float sa = std::sin (alphaRad);
        float dot_rp = rot_ax * px + rot_ay * py + rot_az * pz;
        float cross2_x = rot_ay * pz - rot_az * py;
        float cross2_y = rot_az * px - rot_ax * pz;
        float cross2_z = rot_ax * py - rot_ay * px;

        float qx = px * ca + cross2_x * sa + rot_ax * dot_rp * (1.0f - ca);
        float qy = py * ca + cross2_y * sa + rot_ay * dot_rp * (1.0f - ca);
        float qz = pz * ca + cross2_z * sa + rot_az * dot_rp * (1.0f - ca);

        // Convert back to spherical
        float qNorm = std::sqrt (qx * qx + qy * qy + qz * qz);
        if (qNorm > 1e-6f) { qx /= qNorm;  qy /= qNorm;  qz /= qNorm; }

        float auxEl = std::asin (juce::jlimit (-1.0f, 1.0f, qz));
        float auxAz = std::atan2 (qx, qy);

        for (int s = 0; s < numSpeakers && s < MAX_SPEAKERS; ++s)
            tempGains[s] = 0.0f;

        if (! ctx.triplets.empty())
            computeVBAPGains3D (ctx.layout, ctx.triplets, auxAz, auxEl, tempGains);
        else if (layoutHasHeight (ctx.layout))
        {
            jassertfalse;
            nearestSpeaker3DFallback (ctx.layout, auxAz, auxEl, tempGains, numSpeakers);
        }
        else
            computeVBAPGains2D (ctx.layout, auxAz, tempGains);

        for (int s = 0; s < numSpeakers; ++s)
            outputGains[s] += tempGains[s];
    }

    // Energy normalization
    float totalPower = 0.0f;
    for (int s = 0; s < numSpeakers; ++s)
        totalPower += outputGains[s] * outputGains[s];

    if (totalPower > 1e-12f)
    {
        float scale = 1.0f / std::sqrt (totalPower);
        for (int s = 0; s < numSpeakers; ++s)
            outputGains[s] *= scale;
    }
}

} // namespace spatialcore
