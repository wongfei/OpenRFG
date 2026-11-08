// Original: volition/render_lib/code/rl_sampling_offsets.h
//
// Sampling offset/weight tables for blur and downscale shaders; rl_downscale_sampling_offsets<N> is written by hand
// (addresses of <16>).
#pragma once

#include <math.h>

// size 0x10
struct rl_sampling_vector
{
    rl_sampling_vector(float _x, float _y, float _z, float _w); // 0x004B1DB0 (.h:24)
    rl_sampling_vector(); // 0x004B1DA0 (.h:23)

    float x; // 0x00
    float y; // 0x04
    float z; // 0x08
    float w; // 0x0C
};

// size 0x8
class rl_sampling_offsets
{
public:
    rl_sampling_offsets(); // 0x004B1DE0 (.h:36)

    rl_sampling_vector* m_values_p; // 0x00
    int m_num_samples; // 0x04
};

// size 0x118
class rl_gaussian5x5_blur_kernel : public rl_sampling_offsets
{
public:
    rl_gaussian5x5_blur_kernel(); // 0x004DAFE0 (.h:79)
    float lerp(float a, float b, float t); // 0x004B1DF0 (.h:84)
    void init_bilinear(int width, int height, bool average_weights, float* gaussian_weights_alt, float weight_multiplier, float rho); // 0x00508820 (.h:89)
    void init(int width, int height, bool average_weights, float* gaussian_weights_alt, float weight_multiplier, float rho); // 0x004DB000 (.h:156)
    void init_simple(int width, int height); // 0x004DB2A0 (.h:202)
private:
    float get_gaussian_distribution(float x, float y, float rho); // 0x004B1E20 (.h:238)

public:
    static const int SAMPLE_COUNT; // 0x012BECA0
    rl_sampling_vector m_values[17]; // 0x08
};

// size 0x108 (<16>), 0xD0 (<9>)
template<int N>
class rl_downscale_sampling_offsets : public rl_sampling_offsets
{
public:
    rl_downscale_sampling_offsets(); // <16> 0x004D5260 (.h:43)
    void init(int width, int height); // <16> 0x004D5270 (.h:50)

    rl_sampling_vector m_values[N]; // 0x08
};

// ---- inline members

// 0x004B1DA0 (.h:23)
// Uninitialized.
inline rl_sampling_vector::rl_sampling_vector()
{
}

// 0x004B1DB0 (.h:24)
inline rl_sampling_vector::rl_sampling_vector(float _x, float _y, float _z, float _w)
    : x(_x)
    , y(_y)
    , z(_z)
    , w(_w)
{
}

// 0x004B1DE0 (.h:36)
// Uninitialized.
inline rl_sampling_offsets::rl_sampling_offsets()
{
}

// <16> 0x004D5260 (.h:43)
template<int N>
inline rl_downscale_sampling_offsets<N>::rl_downscale_sampling_offsets()
{
    m_num_samples = N;
    m_values_p = m_values;
}

// <16> 0x004D5270 (.h:50)
// The 4x4 box of a 4:1 downscale: sample (x, y) at ((x - 1.5) / width, (y - 1.5) / height), stored at x + 4 * y.
template<int N>
inline void rl_downscale_sampling_offsets<N>::init(int width, int height)
{
    for (int x = 0; x < 4; x++)
    {
        for (int y = 0; y < 4; y++)
        {
            // Magic number: 1.5, the offset of the box center from the first texel.
            m_values[x + 4 * y].x = (float)(((float)x - 1.5) * (1.0 / (float)width));
            m_values[x + 4 * y].y = (float)(((float)y - 1.5) * (1.0 / (float)height));
        }
    }
}

// 0x004DAFE0 (.h:79)
// Uninitialized (init* set the values pointer and count).
inline rl_gaussian5x5_blur_kernel::rl_gaussian5x5_blur_kernel()
{
}

// 0x004DB000 (.h:156)
// 17 samples along a line from -8 to 8 texels (x / width, same / height; the blur pass picks the axis), weighted by a
// gaussian of rho (or gaussian_weights_alt) times weight_multiplier in w, optionally normalized to sum 1.
inline void rl_gaussian5x5_blur_kernel::init(int width, int height, bool average_weights, float* gaussian_weights_alt, float weight_multiplier, float rho)
{
    const float inv_width = 1.0f / (float)width;
    const float inv_height = 1.0f / (float)height;
    // Magic number: 17 samples.
    m_num_samples = 17;
    m_values_p = m_values;
    float weights[17];
    if (gaussian_weights_alt == nullptr)
    {
        // exp(-(d^2 + 0) / (2 rho^2)) / sqrt(2 pi rho^2); 2 pi rounded to float (6.2831854820251465).
        const double y_term = 0.0;
        const double rho_squared = (double)rho * (double)rho;
        const double two_rho_squared = rho_squared * 2.0;
        const float norm = 1.0f / sqrtf((float)(rho_squared * 6.2831854820251465));
        for (int i = 0; i < m_num_samples; i++)
        {
            const float d = (float)(i - 8);
            weights[i] = (float)exp((double)-((d * d + (float)y_term) / (float)two_rho_squared)) * norm;
        }
        // The center again, from 2 * y_term (the same value).
        weights[m_num_samples / 2] = (float)exp((double)(float)-((y_term + y_term) / two_rho_squared)) * norm;
        gaussian_weights_alt = weights;
    }
    float offset = (float)-((m_num_samples - 1) / 2);
    for (int i = 0; i < m_num_samples; i++)
    {
        m_values[i].x = offset * inv_width;
        m_values[i].y = offset * inv_height;
        m_values[i].z = 0.0f;
        m_values[i].w = gaussian_weights_alt[i];
        m_values[i].w = m_values[i].w * weight_multiplier;
        offset = (float)((double)offset + 1.0);
    }
    if (average_weights)
    {
        float total = 0.0f;
        for (int i = 0; i < m_num_samples; i++)
        {
            total = m_values[i].w + total;
        }
        for (int i = 0; i < m_num_samples; i++)
        {
            m_values[i].w = m_values[i].w / total;
        }
    }
}

// 0x00508820 (.h:89)
// init's 17 samples folded into 9 for bilinear filtering: each pair (-8, -7) ... (-2, -1) becomes one sample between
// the two texels at their weighted position with their summed weight, then the center, then the mirrored pairs. The
// unused samples 9-16 repeat the last one with weight 0.
inline void rl_gaussian5x5_blur_kernel::init_bilinear(int width, int height, bool average_weights, float* gaussian_weights_alt, float weight_multiplier, float rho)
{
    init(width, height, average_weights, gaussian_weights_alt, weight_multiplier, rho);
    // Magic number: 9 bilinear samples.
    m_num_samples = 9;
    rl_sampling_vector folded[9];
    for (int k = 0; k < 4; k++)
    {
        const int first = -8 + 2 * k;
        const float sum = m_values[2 * k + 1].w + m_values[2 * k].w;
        const float t = m_values[2 * k].w / sum;
        const float pos = (1.0f - t) * (float)(first + 1) + (float)first * t;
        folded[k] = rl_sampling_vector(pos / (float)width, pos / (float)height, 0.0f, sum);
    }
    folded[4] = m_values[8];
    for (int k = 0; k < m_num_samples - 5; k++)
    {
        folded[5 + k] = folded[3 - k];
        folded[5 + k].x = -folded[5 + k].x;
        folded[5 + k].y = -folded[5 + k].y;
    }
    for (int i = 0; i < 17; i++)
    {
        if (i < m_num_samples)
        {
            m_values[i] = folded[i];
        }
        else
        {
            m_values[i] = folded[m_num_samples - 1];
            m_values[i].w = 0.0f;
        }
    }
}

// 0x004DB2A0 (.h:202)
// The 13 samples of the 5x5 diamond (|x| + |y| <= 2) with unit gaussian weights in w, normalized to sum 1.
inline void rl_gaussian5x5_blur_kernel::init_simple(int width, int height)
{
    m_values_p = m_values;
    // Magic number: 13 samples in the diamond.
    m_num_samples = 13;
    const float inv_height = (float)(1.0 / (double)(float)height);
    int count = 0;
    for (int x = -2; x < 3; x++)
    {
        for (int y = -2; y < 3; y++)
        {
            if (fabsf((float)y) + fabsf((float)x) <= 2.0f)
            {
                const float fx = (float)x;
                const float fy = (float)y;
                m_values[count].x = fx * (float)(1.0 / (double)(float)width);
                m_values[count].y = fy * inv_height;
                // Magic numbers: exp(-(x^2 + y^2) / 2) / sqrt(2 pi), 2 pi rounded to float (6.2831854820251465).
                const float gauss = (float)exp((double)(float)((double)(fy * fy + fx * fx) * -0.5));
                const float norm = (float)sqrt(6.2831854820251465);
                m_values[count].w = (1.0f / norm) * gauss;
                count++;
            }
        }
    }
    float total = 0.0f;
    for (int i = 0; i < m_num_samples; i++)
    {
        total = m_values[i].w + total;
    }
    for (int i = 0; i < m_num_samples; i++)
    {
        m_values[i].w = m_values[i].w / total;
    }
}
