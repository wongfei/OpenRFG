// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/matrix/hkrotationf.cpp
//
// Havok 2014.2.5 (havok_impl.md HK2): the out-of-line hkRotationf functions of the implementation closure, from rfg.exe.
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/matrix/hkrotationf.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkvector4util.h"

// 0x00CEFA00
void hkRotationf::set(hkQuaternionfParameter q)
{
    hkVector4UtilImpl<hkFloat32>::convertQuaternionToRotation(q, *this);
}

// 0x00CEFB00 (multiplayer.md MP7, the ragdoll joints' isValid): each column's squared length minus one at most
// epsilon (not its absolute value), and col0 x col1 within epsilon of col2 on x, y and z
bool hkRotationf::isOrthonormal(hkFloat32 epsilon) const
{
    const __m128 eps = _mm_set1_ps(epsilon);
    const __m128 one = g_vectorfConstants[HK_QUADREAL_1];
    const __m128* cols[3] = { &m_col0.m_quad, &m_col1.m_quad, &m_col2.m_quad };
    for (int i = 0; i < 3; i++)
    {
        const __m128 sq = _mm_mul_ps(*cols[i], *cols[i]);
        const __m128 len = _mm_add_ps(_mm_add_ps(_mm_shuffle_ps(sq, sq, 0x55), _mm_shuffle_ps(sq, sq, 0x00)), _mm_shuffle_ps(sq, sq, 0xAA));
        if (_mm_cvtss_f32(_mm_sub_ps(len, one)) > _mm_cvtss_f32(eps))
        {
            return false;
        }
    }
    const __m128 c0 = m_col0.m_quad;
    const __m128 c1 = m_col1.m_quad;
    const __m128 d = _mm_sub_ps(_mm_mul_ps(_mm_shuffle_ps(c1, c1, 0xC9), c0), _mm_mul_ps(_mm_shuffle_ps(c0, c0, 0xC9), c1));
    const __m128 diff = _mm_sub_ps(_mm_shuffle_ps(d, d, 0xC9), m_col2.m_quad);
    const __m128 absDiff = _mm_castsi128_ps(_mm_srli_epi32(_mm_slli_epi32(_mm_castps_si128(diff), 1), 1));
    return (_mm_movemask_ps(_mm_cmple_ps(absDiff, eps)) & 7) == 7;
}

// 0x00CEFC00
// Back and forth through a normalized quaternion (23-bit reciprocal square root).
void hkRotationf::renormalize()
{
    hkQuaternionf q;
    q.setAndNormalize(*this);
    set(q);
}
