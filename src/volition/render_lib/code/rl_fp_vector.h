// Original: volition/render_lib/code/rl_fp_vector.h
// Implementation: volition/render_lib/code/rl_fp_vector.cpp
//
// rl_fp_vector helpers on SIMD vectors (rl_fp_vector8): two vector4s packed as signed fixed point shorts. The "high"
// vector is the first four shorts, the "low" one the last four. Each value is scaled by 2^BITS and truncated toward
// zero (the clone mesh elements use 13 fraction bits for directions and colours, 10 for positions).
#pragma once

#include "volition/vlib/code/math/vector4.h"

// size 0x10
class rl_fp_vector8
{
public: // access inferred: the fp vector templates below read and write the shorts
    short x0; // 0x00
    short y0; // 0x02
    short z0; // 0x04
    short w0; // 0x06
    short x1; // 0x08
    short y1; // 0x0A
    short z1; // 0x0C
    short w1; // 0x0E
};

// <13,13> 0x0091EDC0 (.h:81), <13,10> 0x0091EE80 (.h:81)
// The products are made in double (the float times 2^BITS) and truncated, then stored as shorts.
template<int HIGH_BITS, int LOW_BITS>
inline void _compress_fp_vector8(rl_fp_vector8& out_vec, const vector4& in_vec0, const vector4& in_vec1)
{
    const double high_scale = (double)(1 << HIGH_BITS);
    const double low_scale = (double)(1 << LOW_BITS);
    out_vec.x0 = (short)(int)((double)in_vec0.x * high_scale);
    out_vec.y0 = (short)(int)((double)in_vec0.y * high_scale);
    out_vec.z0 = (short)(int)((double)in_vec0.z * high_scale);
    out_vec.w0 = (short)(int)((double)in_vec0.w * high_scale);
    out_vec.x1 = (short)(int)((double)in_vec1.x * low_scale);
    out_vec.y1 = (short)(int)((double)in_vec1.y * low_scale);
    out_vec.z1 = (short)(int)((double)in_vec1.z * low_scale);
    out_vec.w1 = (short)(int)((double)in_vec1.w * low_scale);
}

// <13> 0x004D8910 (.h:93)
template<int BITS>
inline void _decompress_fp_vector8_high(vector4& out_vec, const rl_fp_vector8& in_vec)
{
    const double scale = 1.0 / (double)(1 << BITS);
    out_vec.x = (float)((double)(float)in_vec.x0 * scale);
    out_vec.y = (float)((double)(float)in_vec.y0 * scale);
    out_vec.z = (float)((double)(float)in_vec.z0 * scale);
    out_vec.w = (float)((double)(float)in_vec.w0 * scale);
}

// <13> 0x004D8990 (.h:101), <10> 0x004D8A10 (.h:101)
template<int BITS>
inline void _decompress_fp_vector8_low(vector4& out_vec, const rl_fp_vector8& in_vec)
{
    const double scale = 1.0 / (double)(1 << BITS);
    out_vec.x = (float)((double)(float)in_vec.x1 * scale);
    out_vec.y = (float)((double)(float)in_vec.y1 * scale);
    out_vec.z = (float)((double)(float)in_vec.z1 * scale);
    out_vec.w = (float)((double)(float)in_vec.w1 * scale);
}
