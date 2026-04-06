#include <SpatialCore/IO/AmbisonicsCodec.h>
#include <algorithm>
#include <cmath>
#include <cstring>

namespace spatialcore
{

//==============================================================================
// Real spherical harmonics, ACN ordering, SN3D normalization
// 6th-Order Ambisonics (49 channels)
//==============================================================================
float AmbisonicsCodec::evaluateSH(int acn, float az, float el)
{
    float cosAz  = std::cos(az);
    float sinAz  = std::sin(az);
    float cos2Az = std::cos(2.0f * az);
    float sin2Az = std::sin(2.0f * az);
    float cos3Az = std::cos(3.0f * az);
    float sin3Az = std::sin(3.0f * az);
    float sinEl  = std::sin(el);
    float cosEl  = std::cos(el);
    float sinEl2 = sinEl * sinEl;
    float cosEl2 = cosEl * cosEl;

    // Higher-order trig (computed only when needed)
    float cos4Az = 0.0f, sin4Az = 0.0f, cos5Az = 0.0f, sin5Az = 0.0f, cos6Az = 0.0f, sin6Az = 0.0f;
    float cosEl3 = 0.0f, cosEl4 = 0.0f, cosEl5 = 0.0f, cosEl6 = 0.0f;
    float sinEl4 = 0.0f;

    if (acn >= 16)
    {
        cos4Az = std::cos(4.0f * az);  sin4Az = std::sin(4.0f * az);
        cosEl3 = cosEl2 * cosEl;       cosEl4 = cosEl2 * cosEl2;
        sinEl4 = sinEl2 * sinEl2;
        if (acn >= 25)
        {
            cos5Az = std::cos(5.0f * az);  sin5Az = std::sin(5.0f * az);
            cosEl5 = cosEl4 * cosEl;
        }
        if (acn >= 36)
        {
            cos6Az = std::cos(6.0f * az);  sin6Az = std::sin(6.0f * az);
            cosEl6 = cosEl4 * cosEl2;
        }
    }

    switch (acn)
    {
        // Order 0
        case 0: return 1.0f;

        // Order 1
        case 1: return sinAz * cosEl;
        case 2: return sinEl;
        case 3: return cosAz * cosEl;

        // Order 2
        case 4: return std::sqrt(3.0f) * 0.5f * sin2Az * cosEl2;
        case 5: return std::sqrt(3.0f) * sinAz * sinEl * cosEl;
        case 6: return 0.5f * (3.0f * sinEl2 - 1.0f);
        case 7: return std::sqrt(3.0f) * cosAz * sinEl * cosEl;
        case 8: return std::sqrt(3.0f) * 0.5f * cos2Az * cosEl2;

        // Order 3
        case  9: return std::sqrt(5.0f / 8.0f) * sin3Az * cosEl * cosEl2;
        case 10: return std::sqrt(15.0f) * 0.5f * sin2Az * sinEl * cosEl2;
        case 11: return std::sqrt(3.0f / 8.0f) * sinAz * cosEl * (5.0f * sinEl2 - 1.0f);
        case 12: return 0.5f * sinEl * (5.0f * sinEl2 - 3.0f);
        case 13: return std::sqrt(3.0f / 8.0f) * cosAz * cosEl * (5.0f * sinEl2 - 1.0f);
        case 14: return std::sqrt(15.0f) * 0.5f * cos2Az * sinEl * cosEl2;
        case 15: return std::sqrt(5.0f / 8.0f) * cos3Az * cosEl * cosEl2;

        // Order 4
        case 16: return std::sqrt(35.0f) * 0.375f * sin4Az * cosEl4;
        case 17: return std::sqrt(35.0f / 8.0f) * sin3Az * sinEl * cosEl3;
        case 18: return std::sqrt(5.0f) * 0.25f * sin2Az * cosEl2 * (7.0f * sinEl2 - 1.0f);
        case 19: return std::sqrt(5.0f / 8.0f) * sinAz * sinEl * cosEl * (7.0f * sinEl2 - 3.0f);
        case 20: return 0.125f * (35.0f * sinEl4 - 30.0f * sinEl2 + 3.0f);
        case 21: return std::sqrt(5.0f / 8.0f) * cosAz * sinEl * cosEl * (7.0f * sinEl2 - 3.0f);
        case 22: return std::sqrt(5.0f) * 0.25f * cos2Az * cosEl2 * (7.0f * sinEl2 - 1.0f);
        case 23: return std::sqrt(35.0f / 8.0f) * cos3Az * sinEl * cosEl3;
        case 24: return std::sqrt(35.0f) * 0.375f * cos4Az * cosEl4;

        // Order 5
        case 25: return std::sqrt(63.0f / 8.0f) * sin5Az * cosEl5;
        case 26: return std::sqrt(315.0f) * 0.375f * sin4Az * sinEl * cosEl4;
        case 27: return std::sqrt(35.0f / 16.0f) * sin3Az * cosEl3 * (9.0f * sinEl2 - 1.0f);
        case 28: return std::sqrt(105.0f / 8.0f) * sin2Az * sinEl * cosEl2 * (3.0f * sinEl2 - 1.0f);
        case 29: return std::sqrt(15.0f) * 0.125f * sinAz * cosEl * (21.0f * sinEl4 - 14.0f * sinEl2 + 1.0f);
        case 30: return 0.125f * sinEl * (63.0f * sinEl4 - 70.0f * sinEl2 + 15.0f);
        case 31: return std::sqrt(15.0f) * 0.125f * cosAz * cosEl * (21.0f * sinEl4 - 14.0f * sinEl2 + 1.0f);
        case 32: return std::sqrt(105.0f / 8.0f) * cos2Az * sinEl * cosEl2 * (3.0f * sinEl2 - 1.0f);
        case 33: return std::sqrt(35.0f / 16.0f) * cos3Az * cosEl3 * (9.0f * sinEl2 - 1.0f);
        case 34: return std::sqrt(315.0f) * 0.375f * cos4Az * sinEl * cosEl4;
        case 35: return std::sqrt(63.0f / 8.0f) * cos5Az * cosEl5;

        // Order 6
        case 36: return std::sqrt(231.0f / 16.0f) * sin6Az * cosEl6;
        case 37: return std::sqrt(693.0f / 8.0f) * sin5Az * sinEl * cosEl5;
        case 38: return std::sqrt(63.0f / 16.0f) * sin4Az * cosEl4 * (11.0f * sinEl2 - 1.0f);
        case 39: return std::sqrt(315.0f / 16.0f) * sin3Az * sinEl * cosEl3 * (11.0f * sinEl2 - 3.0f);
        case 40: return std::sqrt(105.0f / 16.0f) * sin2Az * cosEl2 * (33.0f * sinEl4 - 18.0f * sinEl2 + 1.0f) * 0.25f;
        case 41: return std::sqrt(21.0f / 16.0f) * sinAz * sinEl * cosEl * (33.0f * sinEl4 - 30.0f * sinEl2 + 5.0f);
        case 42: return (231.0f * sinEl4 * sinEl2 - 315.0f * sinEl4 + 105.0f * sinEl2 - 5.0f) / 16.0f;
        case 43: return std::sqrt(21.0f / 16.0f) * cosAz * sinEl * cosEl * (33.0f * sinEl4 - 30.0f * sinEl2 + 5.0f);
        case 44: return std::sqrt(105.0f / 16.0f) * cos2Az * cosEl2 * (33.0f * sinEl4 - 18.0f * sinEl2 + 1.0f) * 0.25f;
        case 45: return std::sqrt(315.0f / 16.0f) * cos3Az * sinEl * cosEl3 * (11.0f * sinEl2 - 3.0f);
        case 46: return std::sqrt(63.0f / 16.0f) * cos4Az * cosEl4 * (11.0f * sinEl2 - 1.0f);
        case 47: return std::sqrt(693.0f / 8.0f) * cos5Az * sinEl * cosEl5;
        case 48: return std::sqrt(231.0f / 16.0f) * cos6Az * cosEl6;

        default: return 0.0f;
    }
}

//==============================================================================
// Encode a source position into SH coefficients
//==============================================================================
void AmbisonicsCodec::encode(const SourcePosition& source, int order,
                             float* shCoeffs, int numCoeffs)
{
    int maxACN = (order + 1) * (order + 1);
    if (maxACN > numCoeffs)
        maxACN = numCoeffs;

    for (int acn = 0; acn < maxACN; ++acn)
        shCoeffs[acn] = evaluateSH(acn, source.azimuthRad, source.elevationRad);

    // Zero remaining coefficients
    for (int i = maxACN; i < numCoeffs; ++i)
        shCoeffs[i] = 0.0f;
}

//==============================================================================
// Decode matrix: D = E^T (E E^T + epsilon I)^{-1}
// Tikhonov-regularized pseudo-inverse
//==============================================================================
void AmbisonicsCodec::getDecodeMatrix(int order, int numSpeakers,
                                      const float* speakerAzimuths,
                                      const float* speakerElevations,
                                      float* decodeMatrix)
{
    const int M = (order + 1) * (order + 1);
    if (M > MAX_AMBI_CHANNELS) return;

    // Build encoding matrix E[c][s] = evaluateSH(c, speaker_s_position)
    float E[MAX_AMBI_CHANNELS][MAX_SPEAKERS] = {};
    for (int s = 0; s < numSpeakers; ++s)
        for (int c = 0; c < M; ++c)
            E[c][s] = evaluateSH(c, speakerAzimuths[s], speakerElevations[s]);

    // Compute EET = E * E^T  (M x M)
    float EET[MAX_AMBI_CHANNELS][MAX_AMBI_CHANNELS] = {};
    for (int i = 0; i < M; ++i)
        for (int j = 0; j < M; ++j)
        {
            float sum = 0.0f;
            for (int s = 0; s < numSpeakers; ++s)
                sum += E[i][s] * E[j][s];
            EET[i][j] = sum;
        }

    // Tikhonov regularization: EET += epsilon * I
    float epsilon = 0.01f;
    for (int i = 0; i < M; ++i)
        EET[i][i] += epsilon;

    // Invert EET via Gauss-Jordan
    float inv[MAX_AMBI_CHANNELS][MAX_AMBI_CHANNELS] = {};
    for (int i = 0; i < M; ++i)
        inv[i][i] = 1.0f;

    float aug[MAX_AMBI_CHANNELS][MAX_AMBI_CHANNELS];
    for (int i = 0; i < M; ++i)
        for (int j = 0; j < M; ++j)
            aug[i][j] = EET[i][j];

    for (int col = 0; col < M; ++col)
    {
        int pivot = col;
        for (int row = col + 1; row < M; ++row)
            if (std::abs(aug[row][col]) > std::abs(aug[pivot][col]))
                pivot = row;

        if (pivot != col)
        {
            std::swap_ranges(aug[col], aug[col] + M, aug[pivot]);
            std::swap_ranges(inv[col], inv[col] + M, inv[pivot]);
        }

        float diagVal = aug[col][col];
        if (std::abs(diagVal) < 1e-10f) continue;

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
    for (int s = 0; s < numSpeakers; ++s)
        for (int c = 0; c < M; ++c)
        {
            float sum = 0.0f;
            for (int k = 0; k < M; ++k)
                sum += E[k][s] * inv[k][c];
            decodeMatrix[s * M + c] = sum;
        }
}

//==============================================================================
// Max-rE weighting for improved spatial resolution
// Weights per order: cos(pi / (2*N + 2))^l  where N = ambi order, l = SH order
//==============================================================================
void AmbisonicsCodec::applyMaxREWeights(float* shCoeffs, int order)
{
    static constexpr float kPi = 3.14159265358979323846f;

    for (int l = 0; l <= order; ++l)
    {
        float weight = std::cos(kPi * static_cast<float>(l) / (2.0f * static_cast<float>(order) + 2.0f));

        int startACN = l * l;
        int endACN   = (l + 1) * (l + 1);

        for (int acn = startACN; acn < endACN; ++acn)
            shCoeffs[acn] *= weight;
    }
}

} // namespace spatialcore
