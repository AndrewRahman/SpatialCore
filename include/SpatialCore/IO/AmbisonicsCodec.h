#pragma once

#include <SpatialCore/Core/Types.h>
#include <SpatialCore/Core/SourcePosition.h>

namespace spatialcore
{

/** Ambisonics encode / decode helpers, orders 0-6 (up to 49 channels).

    Convention (D-10; AndrewRahman/SpatialCore#11): real spherical harmonics, ACN channel
    order (acn = l*l + l + m; m > 0 uses cos(m*az), m < 0 uses sin(|m|*az)), SN3D
    normalisation (the sum over m of Y_lm^2 is 1 at every order), no Condon-Shortley phase,
    angles in radians, azimuth 0 = front, positive azimuth toward +Y (left), elevation 0 =
    horizon, positive up. This is the AmbiX convention, the same layout the
    OutputFormatRegistry advertises as "AmbiX ACN/SN3D".
*/
class AmbisonicsCodec
{
public:
    static constexpr int MAX_AMBI_ORDER = 6;
    static constexpr int MAX_AMBI_CHANNELS = (MAX_AMBI_ORDER + 1) * (MAX_AMBI_ORDER + 1);

    static void encode(const SourcePosition& source, int order,
                       float* shCoeffs, int numCoeffs);

    /** Tikhonov-regularised mode-matching decode, D = E^T (E E^T + 0.01 I)^-1, written
        row-major as decodeMatrix[s * M + c], M = (order + 1)^2, rows in speaker index
        order, columns in ACN order. Writes nothing when numSpeakers > MAX_SPEAKERS
        (D-20) or numSpeakers == 0. Message thread only (about 32 KB of stack arrays). */
    static void getDecodeMatrix(int order, int numSpeakers,
                                const float* speakerAzimuths,
                                const float* speakerElevations,
                                float* decodeMatrix);

    /** Forwards to spatialcore::evalSH, the single SH implementation (D-08); kept as a
        public name for existing consumers. */
    static float evaluateSH(int acn, float azimuthRad, float elevationRad);

    static void applyMaxREWeights(float* shCoeffs, int order);
};

} // namespace spatialcore
