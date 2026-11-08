// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/quaternion/hkquaternionf.cpp
//
// Havok 2014.2.5 (havok_impl.md HK2): the out-of-line hkQuaternionf functions of the implementation closure, from
// rfg.exe.
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/quaternion/hkquaternionf.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkvector4util.h"

// 0x00CEDF10
void hkQuaternionf::set(const hkRotationf& r)
{
    _set(r);
}

// 0x00CECED0
// (axis * sin(angle / 2), cos(angle / 2)) with the SSE sine/cosine (hkMath::quadSinCos).
void hkQuaternionf::setAxisAngle(hkVector4fParameter axis, hkSimdFloat32Parameter angle)
{
    const hkSimdFloat32 halfAngle = angle * hkSimdFloat32_Inv2;
    hkSimdFloat32 s, c;
    hkVector4UtilImpl<hkFloat32>::sinCos(halfAngle, s, c);
    hkVector4f imag;
    imag.setMul(s, axis);
    m_vec.setXYZ_W(imag, c);
}

// 0x00CED040
void hkQuaternionf::setAxisAngle(hkVector4fParameter axis, hkFloat32 angle)
{
    setAxisAngle(axis, hkSimdFloat32::fromFloat(angle));
}

// 0x00CED810
// 2 * acos(|w|) (hkMath::quadAcos).
hkSimdFloat32 hkQuaternionf::getAngleSr() const
{
    hkSimdFloat32 absReal;
    absReal.setAbs(m_vec.getComponent<3>());
    const hkSimdFloat32 halfAngle = hkSimdFloat32::convert(hkMath::quadAcos(absReal.m_real));
    return halfAngle + halfAngle;
}

// The half-angle of setSlerp wrapped into [-pi, pi]: x -= 2pi * floor((x + pi) / 2pi) until the clamp to [-pi, pi]
// no longer changes lane 0 (the 2^23 floor of hkMath::quadFloor; MP7, from rfg.exe).
static __m128 slerpWrapAngle(__m128 x, const __m128& invTwoPi)
{
    const __m128 pi = g_vectorfConstants[HK_QUADREAL_PI];
    const __m128 minusPi = _mm_sub_ps(_mm_setzero_ps(), pi);
    const __m128 two23 = _mm_set1_ps(8388608.0f); // 0x4B000000, 2^23: floats at least this large are integers
    for (;;)
    {
        const __m128 v = _mm_mul_ps(_mm_add_ps(pi, x), invTwoPi);
        const __m128 r = _mm_sub_ps(_mm_add_ps(_mm_add_ps(_mm_sub_ps(v, two23), two23), two23), two23);
        const __m128 absV = _mm_castsi128_ps(_mm_srli_epi32(_mm_slli_epi32(_mm_castps_si128(v), 1), 1));
        const __m128 big = _mm_cmplt_ps(two23, absV);
        const __m128 rounded = _mm_add_ps(_mm_cvtepi32_ps(_mm_castps_si128(_mm_cmplt_ps(v, r))), r); // -1 when it rounded up
        const __m128 k = _mm_or_ps(_mm_and_ps(big, v), _mm_andnot_ps(big, rounded));
        x = _mm_sub_ps(x, _mm_mul_ps(g_vectorfConstants[HK_QUADREAL_TWO_PI], k));
        const __m128 lt = _mm_cmplt_ps(x, pi);
        const __m128 clamped = _mm_max_ps(minusPi, _mm_or_ps(_mm_and_ps(lt, x), _mm_andnot_ps(lt, pi)));
        if (_mm_cvtss_f32(clamped) == _mm_cvtss_f32(x) || _mm_cvtss_f32(clamped) != _mm_cvtss_f32(clamped))
        {
            return x; // UCOMISS + JNZ: equal or unordered ends the loop
        }
    }
}

// 0x00CEE0C0
// Spherical interpolation along the shorter arc (q1 negated when the dot product is negative), the result normalized.
// From |dot| >= 0.999 on it is a linear blend. The weights are sin((1 - t) theta) / sin(theta) and sin(t theta) /
// sin(theta) with theta = acos(|dot|) (hkMath::quadAcos), 1 / sin(theta) = 1 / sqrt(1 - dot^2) by a refined rsqrt and
// the sines (hkMath::quadSin) on lane 0 only, after wrapping the angles into [-pi, pi] (MP7, from rfg.exe).
void hkQuaternionf::setSlerp(hkQuaternionfParameter q0, hkQuaternionfParameter q1, hkSimdFloat32Parameter t)
{
    const __m128 a = q0.m_vec.m_quad;
    const __m128 b = q1.m_vec.m_quad;
    const __m128 one = g_vectorfConstants[HK_QUADREAL_1];
    const __m128 zero = _mm_setzero_ps();
    const __m128 prod = _mm_mul_ps(b, a);
    const __m128 s = _mm_add_ps(_mm_shuffle_ps(prod, prod, 0x4E), prod);
    const __m128 dot = _mm_add_ps(_mm_shuffle_ps(s, s, 0xB1), s);
    const __m128 sign = _mm_castsi128_ps(_mm_slli_epi32(_mm_srli_epi32(_mm_castps_si128(_mm_cmplt_ps(dot, zero)), 31), 31));
    const __m128 absDot = _mm_xor_ps(sign, dot);

    __m128 w0;
    __m128 w1;
    // Magic number: 0x3F7FBE77, 0.999f; UCOMISS + JNC, so a NaN takes the slerp branch.
    if (_mm_cvtss_f32(absDot) >= 0.999f)
    {
        w1 = t.m_real;
        w0 = _mm_sub_ps(one, w1);
    }
    else
    {
        const __m128 theta = hkMath::quadAcos(absDot);
        const __m128 x = _mm_sub_ps(one, _mm_mul_ps(absDot, absDot));
        const __m128 invTwoPi = _mm_div_ps(one, g_vectorfConstants[HK_QUADREAL_TWO_PI]);
        const __m128 r = _mm_rsqrt_ps(x);
        const __m128 invSin = _mm_mul_ps(_mm_sub_ps(_mm_set1_ps(3.0f), _mm_mul_ps(_mm_mul_ps(r, x), r)), _mm_mul_ps(r, _mm_set1_ps(0.5f)));
        const __m128 tTheta = _mm_mul_ps(t.m_real, theta);
        const __m128 angle0 = slerpWrapAngle(_mm_sub_ps(theta, tTheta), invTwoPi);
        const __m128 angle1 = slerpWrapAngle(tTheta, invTwoPi);
        // The angles pass through a float on the stack (an x87 round trip, exact): lane 0 only, the others 0.
        const float sin0 = _mm_cvtss_f32(hkMath::quadSin(_mm_set_ss(_mm_cvtss_f32(angle0))));
        const float sin1 = _mm_cvtss_f32(hkMath::quadSin(_mm_set_ss(_mm_cvtss_f32(angle1))));
        w0 = _mm_mul_ps(_mm_set1_ps(sin0), invSin);
        w1 = _mm_mul_ps(_mm_set1_ps(sin1), invSin);
    }

    const __m128 sum = _mm_add_ps(_mm_mul_ps(_mm_xor_ps(sign, w1), b), _mm_mul_ps(a, w0));
    const __m128 sq = _mm_mul_ps(sum, sum);
    const __m128 s2 = _mm_add_ps(_mm_shuffle_ps(sq, sq, 0x4E), sq);
    const __m128 len2 = _mm_add_ps(_mm_shuffle_ps(s2, s2, 0xB1), s2);
    const __m128 rl = _mm_rsqrt_ps(len2);
    const __m128 inv = _mm_mul_ps(_mm_sub_ps(_mm_set1_ps(3.0f), _mm_mul_ps(_mm_mul_ps(rl, len2), rl)), _mm_mul_ps(rl, _mm_set1_ps(0.5f)));
    m_vec.m_quad = _mm_mul_ps(_mm_andnot_ps(_mm_cmple_ps(len2, zero), inv), sum);
}
