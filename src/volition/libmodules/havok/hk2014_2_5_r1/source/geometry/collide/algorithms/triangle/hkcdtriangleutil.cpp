// Original: volition/libmodules/havok/hk2014_2_5_r1/source/geometry/collide/algorithms/triangle/hkcdtriangleutil.cpp
//
// Havok 2014.2.5 (havok_impl.md HK3): triangle helpers, from rfg.exe (Havok 2014.1 hkcdTriangleUtil.inl).

#include "volition/libmodules/havok/hk2014_2_5_r1/source/geometry/collide/algorithms/triangle/hkcdtriangleutil.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/hkmath.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/gjk/hkpgskexact.h"

// 0x00FD0380
// Degenerate: a negative tolerance never; otherwise when the normals from a and from b are both shorter (squared)
// than the tolerance in every lane, or when the edges from b are exactly parallel (zero Gram determinant).
unsigned int hkcdTriangleUtil::isDegenerate(const hkVector4f& a, const hkVector4f& b, const hkVector4f& c, const hkSimdFloat32& tolerance)
{
    if (tolerance.isLessZero())
    {
        return 0;
    }

    {
        hkVector4 ab; ab.setSub(a, b);
        hkVector4 ac; ac.setSub(a, c);
        hkVector4 normalA; normalA.setCross(ab, ac);
        hkVector4 ba; ba.setSub(b, a);
        hkVector4 bc; bc.setSub(b, c);
        hkVector4 normalB; normalB.setCross(ba, bc);

        hkVector4 tol; tol.setAll(tolerance);
        hkVector4 lenA; lenA.setAll(normalA.lengthSquared<3>());
        hkVector4 lenB; lenB.setAll(normalB.lengthSquared<3>());
        hkVector4Comparison small; small.setOr(lenA.less(tol), lenB.less(tol));
        if (small.allAreSet())
        {
            return 1;
        }
    }

    hkVector4 q; q.setSub(a, b);
    hkVector4 r; r.setSub(c, b);
    const hkSimdReal qq = q.lengthSquared<3>();
    const hkSimdReal rr = r.lengthSquared<3>();
    const hkSimdReal qr = r.dot<3>(q);
    const hkSimdReal det = rr * qq - qr * qr;
    return det.isEqualZero() ? 1 : 0;
}

// 0x00FD0520 (havok_impl.md HK6)
// The barycentric coordinates of P in triangle (A, B, C): the signed sub-triangle areas over their sum. A degenerate
// triangle (zero sum) gives the coordinates of P projected onto its longest edge (zero for a zero-length triangle).
void hkcdTriangleUtil::calcBarycentricCoordinates(const hkVector4f& vP, const hkVector4f& vA, const hkVector4f& vB, const hkVector4f& vC, hkVector4f& result)
{
    using namespace hkGskExact;
    const Q a = vA.m_quad;
    const Q b = vB.m_quad;
    const Q c = vC.m_quad;
    const Q p = vP.m_quad;
    const Q ab = sub(b, a);
    const Q n = cross(ab, sub(c, a));
    const Q nYzx = _mm_shuffle_ps(n, n, 0xC9);
    const Q nZxy = _mm_shuffle_ps(n, n, 0xD2);
    // x cross n, as x.yzx * n.zxy - x.zxy * n.yzx
    struct Local
    {
        static Q crossN(Q x, Q nYzx_, Q nZxy_)
        {
            return sub(mul(_mm_shuffle_ps(x, x, 0xC9), nZxy_), mul(_mm_shuffle_ps(x, x, 0xD2), nYzx_));
        }
    };
    const Q pc = sub(c, p);
    const Q pa = sub(a, p);
    const Q pb = sub(b, p);
    hkVector4f d0;
    d0.m_quad = mul(Local::crossN(pc, nYzx, nZxy), pb);
    hkVector4f d1;
    d1.m_quad = mul(Local::crossN(pa, nYzx, nZxy), pc);
    hkVector4f d2;
    d2.m_quad = mul(Local::crossN(pb, nYzx, nZxy), pa);
    HK_TRANSPOSE3f(d0, d1, d2);
    const Q dots = add(d2.m_quad, add(d0.m_quad, d1.m_quad));
    const Q sum = hadd3(dots);
    if (!(lane0(sum) == 0.0f) && lane0(sum) == lane0(sum))
    {
        result.m_quad = mul(recip23(sum), dots);
        return;
    }

    // degenerate (zero or NaN sum): the projection onto the longest edge
    const Q ca = sub(a, c);
    const Q bc = sub(c, b);
    hkVector4f t0;
    t0.m_quad = mul(sub(p, a), ab);
    hkVector4f t1;
    t1.m_quad = mul(sub(p, b), bc);
    hkVector4f t2;
    t2.m_quad = mul(sub(p, c), ca);
    const Q garbage = t2.m_quad; // the stack slot the original reads for an index of -1
    HK_TRANSPOSE3f(t0, t1, t2);
    const Q t = add(t2.m_quad, add(t0.m_quad, t1.m_quad));
    hkVector4f l0;
    l0.m_quad = mul(ab, ab);
    hkVector4f l1;
    l1.m_quad = mul(bc, bc);
    hkVector4f l2;
    l2.m_quad = mul(ca, ca);
    HK_TRANSPOSE3f(l0, l1, l2);
    const Q len = add(l2.m_quad, add(l0.m_quad, l1.m_quad));
    const Q maxLen = _mm_max_ps(bcast<2>(len), _mm_max_ps(bcast<1>(len), bcast<0>(len)));
    const int eqMask = _mm_movemask_ps(_mm_cmpeq_ps(maxLen, len));
    const int idx = (eqMask & 7) ? hkVector4Comparison_maskToFirstIndex[eqMask] : -1;
    const Q u = mul(recip23(_mm_max_ps(len, g_vectorfConstants[HK_QUADREAL_EPS])), t);
    const Q oneMinusU = sub(g_vectorfConstants[HK_QUADREAL_1], u);
    const Q zero = _mm_setzero_ps();
    const Q lo = _mm_unpacklo_ps(oneMinusU, u);
    const Q hi = _mm_unpackhi_ps(oneMinusU, u);
    Q candidates[4];
    candidates[0] = garbage;
    candidates[1] = _mm_movelh_ps(lo, zero); // edge AB: (1 - u0, u0, 0)
    {
        Q q = _mm_shuffle_ps(_mm_movehl_ps(candidates[1], lo), zero, _MM_SHUFFLE(3, 1, 1, 0));
        candidates[2] = _mm_shuffle_ps(q, q, 0x92); // edge BC: (0, 1 - u1, u1)
    }
    {
        Q q = _mm_shuffle_ps(hi, zero, _MM_SHUFFLE(3, 2, 1, 0));
        candidates[3] = _mm_shuffle_ps(q, q, 0x89); // edge CA: (u2, 0, 1 - u2)
    }
    const Q positive = _mm_cmplt_ps(zero, maxLen);
    result.m_quad = _mm_or_ps(_mm_and_ps(candidates[idx + 1], positive), _mm_andnot_ps(positive, zero));
}
