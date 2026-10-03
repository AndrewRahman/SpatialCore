#include <SpatialCore/IO/AmbisonicsCodec.h>
#include <SpatialCore/Core/SpatialMath.h>
#include <algorithm>
#include <cmath>
#include <cstring>

namespace spatialcore
{

//==============================================================================
// evaluateSH -- forwards to spatialcore::evalSH, the single SH implementation
// in SpatialCore (D-08). Both public names are kept because consumers compile
// against both (DR-3). Convention (ACN, SN3D, no Condon-Shortley phase) is
// documented on evalSH in SpatialMath.h. encode() and getDecodeMatrix() call
// this, so they reach the same corrected evaluator.
//==============================================================================
float AmbisonicsCodec::evaluateSH(int acn, float az, float el)
{
    return spatialcore::evalSH (acn, az, el);
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
bool AmbisonicsCodec::getDecodeMatrix(int order, int numSpeakers,
                                      const float* speakerAzimuths,
                                      const float* speakerElevations,
                                      float* decodeMatrix)
{
    // D-20: E below is sized MAX_SPEAKERS columns, so a larger count would
    // write out of bounds. Write nothing and report it instead. Message-thread
    // code, so the check is free (RESEARCH F6).
    if (numSpeakers < 0 || numSpeakers > MAX_SPEAKERS) return false;

    // IN-03: a negative order used to give a positive (order + 1)^2 and decode
    // anyway; reject it with the over-range orders.
    if (order < 0 || order > MAX_AMBI_ORDER) return false;

    const int M = (order + 1) * (order + 1);

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
    return true;
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
