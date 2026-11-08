// OpenRFG (havok_impl.md HK8): the SSE building blocks of the constraint solver in the operation order rfg.exe uses
// (the Havok solver sources are private; these mirror its inlined hkVector4f/hkSimdFloat32 code). Products and sums of
// two values are commutative in IEEE arithmetic, so only the grouping of three or more terms is fixed here.
// The helpers here and in the solver files take const references: an out-of-line copy (MSVC ignored __forceinline)
// taking more than three __m128 by value got mismatched argument passing from MSVC x86 (the caller put some on the
// stack by value, the callee read pointers).
#pragma once

#include <xmmintrin.h>
#include <emmintrin.h>

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/hkmath.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraintsolver/accumulator/hkpvelocityaccumulator.h"

namespace hkSolverSse
{
    typedef __m128 Q;

    __forceinline Q splat(float f) { return _mm_set1_ps(f); }
    __forceinline Q xxxx(const Q& v) { return _mm_shuffle_ps(v, v, 0x00); }
    __forceinline Q yyyy(const Q& v) { return _mm_shuffle_ps(v, v, 0x55); }
    __forceinline Q zzzz(const Q& v) { return _mm_shuffle_ps(v, v, 0xAA); }
    __forceinline Q wwww(const Q& v) { return _mm_shuffle_ps(v, v, 0xFF); }
    __forceinline Q yzxw(const Q& v) { return _mm_shuffle_ps(v, v, 0xC9); }

    // hkVector4f::setCross(a, b): ((a * b.yzx) - (b * a.yzx)).yzx
    __forceinline Q cross(const Q& a, const Q& b) { return yzxw(_mm_sub_ps(_mm_mul_ps(a, yzxw(b)), _mm_mul_ps(b, yzxw(a)))); }

    // the 3x3 rotation of v by the columns c0, c1, c2: (x * c0 + y * c1) + z * c2 (all four lanes)
    __forceinline Q rotate(const Q& c0, const Q& c1, const Q& c2, const Q& v) { return _mm_add_ps(_mm_add_ps(_mm_mul_ps(xxxx(v), c0), _mm_mul_ps(yyyy(v), c1)), _mm_mul_ps(zzzz(v), c2)); }

    // dot<3>/horizontalAdd<3>: (y + x) + z in all lanes
    __forceinline Q hadd3(const Q& v) { return _mm_add_ps(_mm_add_ps(yyyy(v), xxxx(v)), zzzz(v)); }

    // horizontalAdd<4>: (v + v.zwxy) + its yxwz swap, equal in all lanes
    __forceinline Q hadd4(const Q& v)
    {
        const Q s = _mm_add_ps(_mm_shuffle_ps(v, v, 0x4E), v);
        return _mm_add_ps(_mm_shuffle_ps(s, s, 0xB1), s);
    }

    // v with its w replaced by w's w (UNPCKHPS + SHUFPS 0xC4)
    __forceinline Q setW(const Q& v, const Q& w) { return _mm_shuffle_ps(v, _mm_unpackhi_ps(v, w), 0xC4); }

    // the 23-bit reciprocal: one Newton step, (2 - r * x) * r
    __forceinline Q rcp23(const Q& x)
    {
        const Q r = _mm_rcp_ps(x);
        return _mm_mul_ps(_mm_sub_ps(g_vectorfConstants[HK_QUADREAL_2], _mm_mul_ps(r, x)), r);
    }

    // the 23-bit inverse square root, 0 for x <= 0: (3 - (r * x) * r) * (r * 0.5)
    __forceinline Q invSqrt23(const Q& x)
    {
        const Q r = _mm_rsqrt_ps(x);
        const Q inv = _mm_mul_ps(_mm_sub_ps(g_vectorfConstants[HK_QUADREAL_3], _mm_mul_ps(_mm_mul_ps(r, x), r)), _mm_mul_ps(r, _mm_set1_ps(0.5f)));
        return _mm_andnot_ps(_mm_cmple_ps(x, _mm_setzero_ps()), inv);
    }

    // the square root by the inverse, 0 for x <= 0: ((3 - (r * x) * r) * (r * 0.5)) * x
    __forceinline Q sqrt23(const Q& x)
    {
        const Q r = _mm_rsqrt_ps(x);
        const Q inv = _mm_mul_ps(_mm_sub_ps(g_vectorfConstants[HK_QUADREAL_3], _mm_mul_ps(_mm_mul_ps(r, x), r)), _mm_mul_ps(r, _mm_set1_ps(0.5f)));
        return _mm_andnot_ps(_mm_cmple_ps(x, _mm_setzero_ps()), _mm_mul_ps(inv, x));
    }

    __forceinline float lane0(const Q& v) { return _mm_cvtss_f32(v); }

    // the accumulator's core-from-world rotation (scratch0..2, columns) as a quaternion in scratch2; the sum velocities
    // (scratch0/1) start at zero (hkpVelocityAccumulator::convertToSolverType 0x00E0D7F0, inlined by the solver and
    // the TOI solver setup; 0.5 / s by a real division, s * 0.5 on the x87 in some copies, which is exact either way)
    __forceinline void convertToSolverType(hkpVelocityAccumulator* acc)
    {
        acc->m_context.m_storage = hkpVelocityAccumulator::ACCUMULATOR_CONTEXT_SOLVER;
        const float* m = acc->m_scratch0.m_quad.m128_f32; // column c, row r at m[c * 4 + r]
        HK_ALIGN16(float q[4]);
        const float trace = (m[0] + m[5]) + m[10];
        if (trace > 0.0f)
        {
            const float s = lane0(_mm_sqrt_ss(_mm_set_ss(trace - -1.0f)));
            const float f = 0.5f / s;
            q[0] = (m[6] - m[9]) * f;
            q[1] = (m[8] - m[2]) * f;
            q[2] = (m[1] - m[4]) * f;
            q[3] = s * 0.5f;
        }
        else
        {
            static const int next[3] = { 1, 2, 0 };
            int i = 0;
            if (m[5] > m[0])
            {
                i = 1;
            }
            if (m[10] > m[i * 5])
            {
                i = 2;
            }
            const int j = next[i];
            const int k = next[j];
            const float s = lane0(_mm_sqrt_ss(_mm_set_ss((m[i * 5] - (m[j * 5] + m[k * 5])) - -1.0f)));
            const float f = 0.5f / s;
            q[i] = s * 0.5f;
            q[3] = (m[j * 4 + k] - m[k * 4 + j]) * f;
            q[j] = (m[j * 4 + i] + m[i * 4 + j]) * f;
            q[k] = (m[k * 4 + i] + m[i * 4 + k]) * f;
        }
        acc->m_scratch2.m_quad = _mm_load_ps(q);
        acc->m_scratch0.setZero();
        acc->m_scratch1.setZero();
    }

    // writes the low byte of a schema's first float after adding FLT_EPSILON to it (hkpJacobianSchema::setType)
    __forceinline void setSchemaType(void* schema, unsigned char type)
    {
        float* f = static_cast<float*>(schema);
        *f = lane0(_mm_add_ss(_mm_load_ss(f), _mm_set_ss(1.1920929e-07f))); // 0x34000000
        *static_cast<unsigned char*>(schema) = type;
    }
}
