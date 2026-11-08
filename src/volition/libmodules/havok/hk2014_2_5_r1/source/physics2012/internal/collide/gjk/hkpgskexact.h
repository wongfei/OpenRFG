// New code (OpenRFG, havok_impl.md HK6), not an SDK header: SSE helpers for the narrowphase code ported from rfg.exe
// (GSK, manifolds, penetration depth, agents). Each helper is the exact instruction sequence the shipped binary uses
// (the inlined hkVector4f code of the 2014.2.5 SDK), so the ported functions keep their results bit for bit:
// - dot3: mul, then (y + x) + z broadcast (hkVector4f::dot<3>);
// - cross: shuf(v0 * shuf(v1) - v1 * shuf(v0)) (hkVector4f::setCross);
// - recip23: rcp refined by one Newton step, rcp * (2 - x * rcp) (hkMath::quadReciprocal, HK_ACC_23_BIT);
// - invSqrt23SetZero: rsqrt refined by one Newton step, 0 where x <= 0 (HK_ACC_23_BIT, HK_SQRT_SET_ZERO);
// - comparisons follow the ucomiss branches: lessOrNaN is the carry branch (JC), etc.
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/hkmath.h"

#include <emmintrin.h>

namespace hkGskExact
{
    typedef __m128 Q;

    inline Q splat(float f) { return _mm_set1_ps(f); }
    inline Q bits(unsigned int u) { return _mm_castsi128_ps(_mm_set1_epi32((int)u)); }
    inline float lane0(Q a) { return _mm_cvtss_f32(a); }

    inline Q add(Q a, Q b) { return _mm_add_ps(a, b); }
    inline Q sub(Q a, Q b) { return _mm_sub_ps(a, b); }
    inline Q mul(Q a, Q b) { return _mm_mul_ps(a, b); }

    template<int I> inline Q bcast(Q a) { return _mm_shuffle_ps(a, a, _MM_SHUFFLE(I, I, I, I)); }

    // (y + x) + z of a * b, in all lanes
    inline Q dot3(Q a, Q b)
    {
        const Q m = _mm_mul_ps(a, b);
        return _mm_add_ps(_mm_add_ps(bcast<1>(m), bcast<0>(m)), bcast<2>(m));
    }

    // (y + x) + z of a, in all lanes
    inline Q hadd3(Q m)
    {
        return _mm_add_ps(_mm_add_ps(bcast<1>(m), bcast<0>(m)), bcast<2>(m));
    }

    inline Q cross(Q v0, Q v1)
    {
        const Q c0 = _mm_mul_ps(v0, _mm_shuffle_ps(v1, v1, _MM_SHUFFLE(3, 0, 2, 1)));
        const Q c1 = _mm_mul_ps(v1, _mm_shuffle_ps(v0, v0, _MM_SHUFFLE(3, 0, 2, 1)));
        const Q d = _mm_sub_ps(c0, c1);
        return _mm_shuffle_ps(d, d, _MM_SHUFFLE(3, 0, 2, 1));
    }

    // the sign bits of a (pslld/psrld by 31)
    inline Q signBits(Q a) { return _mm_castsi128_ps(_mm_slli_epi32(_mm_srli_epi32(_mm_castps_si128(a), 31), 31)); }
    // |a| (pslld/psrld by 1)
    inline Q abs(Q a) { return _mm_castsi128_ps(_mm_srli_epi32(_mm_slli_epi32(_mm_castps_si128(a), 1), 1)); }
    // v with its sign flipped where s is negative
    inline Q flipSign(Q v, Q s) { return _mm_xor_ps(v, signBits(s)); }
    // -v (xor with 0x80000000 in all lanes)
    inline Q neg(Q v) { return _mm_xor_ps(v, bits(0x80000000u)); }
    // |d| * d
    inline Q signedSquare(Q d) { return _mm_mul_ps(abs(d), d); }

    inline Q recip23(Q x)
    {
        const Q r = _mm_rcp_ps(x);
        return _mm_mul_ps(_mm_sub_ps(splat(2.0f), _mm_mul_ps(r, x)), r);
    }

    inline Q invSqrt23SetZero(Q len2)
    {
        const Q le0 = _mm_cmple_ps(len2, _mm_setzero_ps());
        const Q r = _mm_rsqrt_ps(len2);
        const Q t = _mm_mul_ps(_mm_sub_ps(splat(3.0f), _mm_mul_ps(_mm_mul_ps(r, len2), r)), _mm_mul_ps(r, splat(0.5f)));
        return _mm_andnot_ps(le0, t);
    }

    // rsqrt refined by one Newton step, unmasked (HK_ACC_23_BIT): (3 - (r * x) * r) * (r * 0.5)
    inline Q invSqrt23(Q x)
    {
        const Q r = _mm_rsqrt_ps(x);
        return _mm_mul_ps(_mm_sub_ps(splat(3.0f), _mm_mul_ps(_mm_mul_ps(r, x), r)), _mm_mul_ps(r, splat(0.5f)));
    }

    // a where mask, else b
    inline Q selectQ(Q mask, Q a, Q b) { return _mm_or_ps(_mm_and_ps(a, mask), _mm_andnot_ps(mask, b)); }

    // hkVector4Util::calculatePerpendicularVector (inlined): (-y, x, 0, 0), or (-z, 0, x, 0) when x^2 + y^2 is the smaller
    inline Q perpendicular(Q vIn)
    {
        const Q v = _mm_castsi128_ps(_mm_srli_si128(_mm_slli_si128(_mm_castps_si128(vIn), 4), 4));
        const Q zw0x = _mm_shuffle_ps(v, v, 0xCE);
        const Q sq = _mm_mul_ps(zw0x, zw0x);
        const Q s1 = _mm_add_ps(_mm_shuffle_ps(sq, sq, 0x4E), sq);
        const Q xzLen = _mm_add_ps(_mm_shuffle_ps(s1, s1, 0xB1), s1);
        const Q a2 = _mm_mul_ps(vIn, vIn);
        const Q xyLen = _mm_add_ps(bcast<1>(a2), bcast<0>(a2));
        const Q useXz = _mm_cmplt_ps(xyLen, xzLen);
        const Q signX = _mm_castsi128_ps(_mm_insert_epi16(_mm_setzero_si128(), 0x8000, 1));
        const Q perpXy = _mm_xor_ps(_mm_shuffle_ps(v, v, 0xF1), signX);
        const Q perpXz = _mm_xor_ps(signX, zw0x);
        return _mm_or_ps(_mm_andnot_ps(useXz, perpXy), _mm_and_ps(perpXz, useXz));
    }

    // xyz of a, w of b
    inline Q setXYZ_W(Q xyz, Q w) { return _mm_shuffle_ps(xyz, _mm_unpackhi_ps(xyz, w), _MM_SHUFFLE(3, 0, 1, 0)); }

    // t * v as point: ((y * c1 + x * c0) + z * c2) + translation
    inline Q transformPos(const hkTransformf& t, Q v)
    {
        const Q x = _mm_mul_ps(bcast<0>(v), t.getRotation().getColumn<0>().m_quad);
        const Q y = _mm_mul_ps(bcast<1>(v), t.getRotation().getColumn<1>().m_quad);
        const Q z = _mm_mul_ps(bcast<2>(v), t.getRotation().getColumn<2>().m_quad);
        return _mm_add_ps(_mm_add_ps(_mm_add_ps(y, x), z), t.getTranslation().m_quad);
    }

    // r * v as direction: (y * c1 + x * c0) + z * c2
    inline Q rotate(const hkMatrix3f& r, Q v)
    {
        const Q x = _mm_mul_ps(bcast<0>(v), r.getColumn<0>().m_quad);
        const Q y = _mm_mul_ps(bcast<1>(v), r.getColumn<1>().m_quad);
        const Q z = _mm_mul_ps(bcast<2>(v), r.getColumn<2>().m_quad);
        return _mm_add_ps(_mm_add_ps(y, x), z);
    }

    // transpose(r) * v: the HK_TRANSPOSE3 rows, (y * r1 + x * r0) + z * r2
    inline Q rotateInverse(const hkMatrix3f& r, Q v)
    {
        hkVector4f c0 = r.getColumn<0>();
        hkVector4f c1 = r.getColumn<1>();
        hkVector4f c2 = r.getColumn<2>();
        HK_TRANSPOSE3f(c0, c1, c2);
        const Q x = _mm_mul_ps(bcast<0>(v), c0.m_quad);
        const Q y = _mm_mul_ps(bcast<1>(v), c1.m_quad);
        const Q z = _mm_mul_ps(bcast<2>(v), c2.m_quad);
        return _mm_add_ps(_mm_add_ps(y, x), z);
    }

    // ucomiss a, b branches (lane 0)
    inline bool less(Q a, Q b) { return lane0(a) < lane0(b); } // jc not taken on NaN: use lessOrNaN for jc
    inline bool lessOrNaN(Q a, Q b) { return !(lane0(a) >= lane0(b)); } // jc / setc
    inline bool greaterEqual(Q a, Q b) { return lane0(a) >= lane0(b); } // jnc
    inline bool greater(Q a, Q b) { return lane0(a) > lane0(b); } // ja (jbe not taken)
    inline bool lessEqualOrNaN(Q a, Q b) { return !(lane0(a) > lane0(b)); } // jbe

    // the lane mask of bit mask m (hkVector4fComparison_maskToComparison)
    inline Q laneMask(int m) { return _mm_load_ps((const float*)&hkVector4fComparison_maskToComparison[m * 4]); }

    inline Q load(const hkVector4f& v) { return v.m_quad; }
    inline void store(hkVector4f& v, Q q) { v.m_quad = q; }
}
