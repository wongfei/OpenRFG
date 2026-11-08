// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/geometryutilities/inertia/hkinertiatensorcomputer.cpp
//
// Havok 2014.2.5 (havok_impl.md HK3): mass properties (volume, mass, centre of mass, inertia tensor) of primitives,
// triangle meshes (Mirtich's polyhedral mass properties, SSE version) and compounds, from rfg.exe. The code follows
// the SSE operation order of the original (each __m128 step is one instruction there).

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/geometryutilities/inertia/hkinertiatensorcomputer.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/geometry/hkgeometry.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/geometry/hkstridedvertices.h"

#include <emmintrin.h>

// 0x03A380D4: set to hkpAccurateInertiaTensorComputer::computeConvexHullMassProperties when the optional component
// hkpAccurateInertiaTensorComputer is requested (hkProductFeatures::initialize)
hkInertiaTensorComputer::ConvexHullMassPropertiesFunction hkInertiaTensorComputer::s_computeConvexHullMassPropertiesFunction;

namespace
{
inline __m128 splatX(__m128 v) { return _mm_shuffle_ps(v, v, 0x00); }
inline __m128 splatY(__m128 v) { return _mm_shuffle_ps(v, v, 0x55); }
inline __m128 splatZ(__m128 v) { return _mm_shuffle_ps(v, v, 0xAA); }
inline __m128 constant(hkVectorConstant c) { return g_vectorfConstants[c]; }
inline __m128 broadcast(float f) { return _mm_set1_ps(f); }

// hkVector4fComparison_maskToComparison[m]: lane k set when bit k of m is
inline __m128 laneMask(int m)
{
    return _mm_castsi128_ps(_mm_set_epi32((m & 8) ? -1 : 0, (m & 4) ? -1 : 0, (m & 2) ? -1 : 0, (m & 1) ? -1 : 0));
}

// mask ? a : b
inline __m128 select(__m128 mask, __m128 a, __m128 b)
{
    return _mm_or_ps(_mm_andnot_ps(mask, b), _mm_and_ps(mask, a));
}

// w = 0 (byte shifts)
inline __m128 clearW(__m128 v)
{
    return _mm_castsi128_ps(_mm_srli_si128(_mm_slli_si128(_mm_castps_si128(v), 4), 4));
}

inline __m128 absolute(__m128 v)
{
    return _mm_castsi128_ps(_mm_srli_epi32(_mm_slli_epi32(_mm_castps_si128(v), 1), 1));
}

// (y + x) + z in all lanes
inline __m128 horizontalAdd3(__m128 v)
{
    return _mm_add_ps(_mm_add_ps(splatY(v), splatX(v)), splatZ(v));
}

// a x b
inline __m128 cross(__m128 a, __m128 b)
{
    const __m128 c = _mm_sub_ps(_mm_mul_ps(_mm_shuffle_ps(b, b, 0xC9), a), _mm_mul_ps(_mm_shuffle_ps(a, a, 0xC9), b));
    return _mm_shuffle_ps(c, c, 0xC9);
}

// normalize<3> with the 23-bit reciprocal square root (one Newton step), zero for a zero vector
inline __m128 normalize3(__m128 v)
{
    const __m128 len2 = horizontalAdd3(_mm_mul_ps(v, v));
    const __m128 r = _mm_rsqrt_ps(len2);
    const __m128 inv = _mm_mul_ps(_mm_sub_ps(_mm_set1_ps(3.0f), _mm_mul_ps(_mm_mul_ps(r, len2), r)), _mm_mul_ps(r, _mm_set1_ps(0.5f)));
    return _mm_mul_ps(_mm_andnot_ps(_mm_cmple_ps(len2, _mm_setzero_ps()), inv), v);
}

// length<3>, zero for a zero vector
inline __m128 length3(__m128 v)
{
    const __m128 len2 = horizontalAdd3(_mm_mul_ps(v, v));
    return _mm_andnot_ps(_mm_cmple_ps(len2, _mm_setzero_ps()), _mm_sqrt_ps(len2));
}

// The inertia contribution of a mass at a point (or the parallel axis term): the diagonal (y^2 + z^2, x^2 + z^2,
// x^2 + y^2) times diagScale and the products xy, xz, yz times offScale, as three columns.
void pointInertia(__m128 diagTerms, __m128 offTerms, __m128* cols)
{
    const __m128 diag = clearW(diagTerms);
    cols[0] = select(laneMask(9), diag, _mm_shuffle_ps(offTerms, offTerms, 0xA0));
    cols[1] = select(laneMask(10), diag, _mm_unpacklo_ps(offTerms, offTerms));
    cols[2] = _mm_shuffle_ps(_mm_shuffle_ps(offTerms, offTerms, 0xE6), diag, 0xE4);
}

// (y^2 + z^2, x^2 + z^2, x^2 + y^2, y^2 + y^2) of v (v squared)
inline __m128 crossSums(__m128 sq)
{
    return _mm_add_ps(_mm_shuffle_ps(sq, sq, 0x5A), _mm_shuffle_ps(sq, sq, 0x41));
}

// (y x, z y, x z, w w) of v
inline __m128 crossProducts(__m128 v)
{
    return _mm_mul_ps(_mm_shuffle_ps(v, v, 0xC9), v);
}

// The cylinder's (and the capsule's) frame: the rotation of the z axis onto the axis direction (identity when the
// axis is nearly along z), about cross(z, direction) by acos(direction.z).
void axisRotation(__m128 axis, hkRotationf& rotation)
{
    const __m128 dir = normalize3(axis);
    const __m128 cosAngle = splatZ(dir);
    if (!(_mm_cvtss_f32(absolute(cosAngle)) < 0.99999f)) // 0x3F7FFF58
    {
        rotation.setIdentity();
        return;
    }
    hkVector4f rotationAxis;
    rotationAxis.m_quad = normalize3(cross(constant(HK_QUADREAL_0010), dir));
    hkSimdFloat32 angle;
    angle.m_real = hkMath::quadAcos(cosAngle);
    hkQuaternionf q;
    q.setAxisAngle(rotationAxis, angle);
    rotation.set(q);
}

// The solid cylinder's inertia about its centre (axis along z): ((len^2 / 3 + r^2) / 4, the same, r^2 / 2).
void cylinderInertia(__m128 length, __m128 radius, __m128* cols)
{
    const __m128 r2 = _mm_mul_ps(radius, radius);
    __m128 a = _mm_add_ps(_mm_mul_ps(_mm_mul_ps(length, length), constant(HK_QUADREAL_INV_3)), r2);
    const __m128 b = _mm_unpacklo_ps(_mm_mul_ps(r2, constant(HK_QUADREAL_INV_2)), constant(HK_QUADREAL_INV_4));
    a = _mm_mul_ps(a, constant(HK_QUADREAL_INV_4));
    a = _mm_unpacklo_ps(a, a);
    const __m128 diag = _mm_movelh_ps(a, b);
    cols[0] = _mm_mul_ps(diag, constant(HK_QUADREAL_1000));
    cols[1] = _mm_mul_ps(diag, constant(HK_QUADREAL_0100));
    cols[2] = _mm_mul_ps(diag, constant(HK_QUADREAL_0010));
}
}

// ---- Mirtich's polyhedral mass properties (the projection, face and volume integrals), C the dominant normal axis
// and A, B the two others ((C + 1) % 3, (C + 2) % 3). Each lane block holds several integrals.
class InternalInertiaTensorComputer
{
public:
    void compProjectionIntegrals(const hkVector4f* vertices);
    void compFaceIntegrals(const hkVector4f* vertices, const hkVector4f& normal);
    void compVolumeIntegrals(const hkGeometry* geom, const hkVector4f& offset);
    void computeInertialTensorInternal(const hkSimdFloat32& mass, const hkSimdFloat32& density, hkVector4f& r, hkMatrix3f& J);
    static void compGeometryAabb(const float* vertices, int numVertices, int striding, hkVector4f& centerOut, hkVector4f& aabbExtentsOut);

    // (a, b, c) of v for the current C
    __m128 permute(__m128 v) const
    {
        if (m_C == 0)
        {
            return _mm_shuffle_ps(v, v, 0xC9);
        }
        if (m_C == 1)
        {
            return _mm_shuffle_ps(v, v, 0xD2);
        }
        return v;
    }
    // the inverse permutation
    __m128 unpermute(__m128 v) const
    {
        if (m_C == 0)
        {
            return _mm_shuffle_ps(v, v, 0xD2);
        }
        if (m_C == 1)
        {
            return _mm_shuffle_ps(v, v, 0xC9);
        }
        return v;
    }

    int m_C; // 0x00
    __m128 m_P1; // 0x10
    __m128 m_Pab; // 0x20
    __m128 m_Paab; // 0x30
    __m128 m_Pabb; // 0x40
    __m128 m_P_a; // 0x50 (Pa, Paa, Paaa)
    __m128 m_P_b; // 0x60 (Pb, Pbb, Pbbb)
    __m128 m_Faab; // 0x70
    __m128 m_Fbbc; // 0x80
    __m128 m_Fcca; // 0x90
    __m128 m_F_a; // 0xA0
    __m128 m_F_b; // 0xB0
    __m128 m_F_c; // 0xC0
    __m128 m_T0; // 0xD0
    __m128 m_T1; // 0xE0
    __m128 m_T2; // 0xF0
    __m128 m_TP; // 0x100
};

namespace
{
const int InternalInertiaTensorComputer_mod3table[4] = { 1, 2, 0, 1 }; // 0x01371530
const float InternalInertiaTensorComputer_scaleConst[4] = { 1.0f / 6.0f, 1.0f / 12.0f, 1.0f / 20.0f, 2.0f }; // 0x01371540
}

// 0x00F38AA0: the integrals over the face's projection onto the A-B plane (the edges of the triangle)
void InternalInertiaTensorComputer::compProjectionIntegrals(const hkVector4f* vertices)
{
    const __m128 zero = _mm_setzero_ps();
    m_P1 = zero;
    m_Pab = zero;
    m_Paab = zero;
    m_Pabb = zero;
    m_P_a = zero;
    m_P_b = zero;
    const __m128 two = constant(HK_QUADREAL_2);
    const __m128 three = constant(HK_QUADREAL_3);
    const __m128 four = constant(HK_QUADREAL_4);
    for (int i = 0; i < 3; i++)
    {
        const __m128 v0 = permute(vertices[i].m_quad);
        const __m128 v1 = permute(vertices[InternalInertiaTensorComputer_mod3table[i]].m_quad);
        const __m128 a0 = splatX(v0);
        const __m128 b0 = splatY(v0);
        const __m128 a1 = splatX(v1);
        const __m128 b1 = splatY(v1);

        const __m128 a0_2 = _mm_mul_ps(a0, a0);
        const __m128 da = _mm_sub_ps(a1, a0);
        const __m128 db = _mm_sub_ps(b1, b0);
        const __m128 a0_3 = _mm_mul_ps(a0_2, a0);
        const __m128 b0_2 = _mm_mul_ps(b0, b0);
        const __m128 b0_3 = _mm_mul_ps(b0_2, b0);
        const __m128 b1_2 = _mm_mul_ps(b1, b1);
        const __m128 b1_3 = _mm_mul_ps(b1_2, b1);
        const __m128 C1 = _mm_add_ps(a1, a0);
        const __m128 twoA1A0 = _mm_mul_ps(_mm_mul_ps(a1, two), a0);
        const __m128 a1_2 = _mm_mul_ps(a1, a1);
        const __m128 Cab = _mm_add_ps(_mm_add_ps(_mm_mul_ps(a1_2, three), twoA1A0), a0_2);
        const __m128 a1_2PlusTwoA1A0 = _mm_add_ps(a1_2, twoA1A0);
        m_P1 = _mm_add_ps(_mm_mul_ps(C1, db), m_P1);
        const __m128 Ca = _mm_add_ps(_mm_mul_ps(C1, a1), a0_2);
        const __m128 Kab = _mm_add_ps(_mm_mul_ps(a0_2, three), a1_2PlusTwoA1A0);
        const __m128 Caa = _mm_add_ps(_mm_mul_ps(Ca, a1), a0_3);
        const __m128 a0_4 = _mm_mul_ps(a0_2, a0_2);
        const __m128 Caaa = _mm_add_ps(_mm_mul_ps(Caa, a1), a0_4);
        const __m128 Ca3 = _mm_movelh_ps(_mm_unpacklo_ps(Ca, Caa), _mm_unpacklo_ps(Caaa, two));
        m_P_a = _mm_add_ps(_mm_mul_ps(Ca3, db), m_P_a);

        const __m128 Cb = _mm_add_ps(_mm_mul_ps(_mm_add_ps(b1, b0), b1), b0_2);
        const __m128 Cbb = _mm_add_ps(_mm_mul_ps(Cb, b1), b0_3);
        const __m128 b0_4 = _mm_mul_ps(b0_2, b0_2);
        const __m128 Cbbb = _mm_add_ps(b0_4, _mm_mul_ps(Cbb, b1));
        const __m128 Cb3 = _mm_movelh_ps(_mm_unpacklo_ps(Cb, Cbb), _mm_unpacklo_ps(Cbbb, two));
        m_P_b = _mm_add_ps(_mm_mul_ps(Cb3, da), m_P_b);

        m_Pab = _mm_add_ps(_mm_mul_ps(_mm_add_ps(_mm_mul_ps(Cab, b1), _mm_mul_ps(Kab, b0)), db), m_Pab);
        const __m128 Caab = _mm_add_ps(_mm_mul_ps(_mm_mul_ps(a1_2, a1), four), _mm_mul_ps(Cab, a0));
        const __m128 Kaab = _mm_add_ps(_mm_mul_ps(Kab, a1), _mm_mul_ps(a0_3, four));
        m_Paab = _mm_add_ps(_mm_mul_ps(_mm_add_ps(_mm_mul_ps(Caab, b1), _mm_mul_ps(Kaab, b0)), db), m_Paab);

        const __m128 threeB1_2B0 = _mm_mul_ps(_mm_mul_ps(b1_2, three), b0);
        const __m128 twoB1B0_2 = _mm_mul_ps(_mm_mul_ps(b1, two), b0_2);
        const __m128 threeB1B0_2 = _mm_mul_ps(_mm_mul_ps(b1, three), b0_2);
        const __m128 Cabb = _mm_add_ps(_mm_add_ps(_mm_add_ps(threeB1_2B0, _mm_mul_ps(b1_3, four)), twoB1B0_2), b0_3);
        const __m128 Kabb = _mm_add_ps(_mm_add_ps(_mm_add_ps(_mm_mul_ps(_mm_mul_ps(b1_2, two), b0), b1_3), threeB1B0_2), _mm_mul_ps(b0_3, four));
        m_Pabb = _mm_add_ps(_mm_mul_ps(_mm_add_ps(_mm_mul_ps(Cabb, a1), _mm_mul_ps(Kabb, a0)), da), m_Pabb);
    }
    m_P1 = _mm_mul_ps(m_P1, constant(HK_QUADREAL_INV_2));
    const __m128 scale = _mm_loadu_ps(InternalInertiaTensorComputer_scaleConst);
    m_P_a = _mm_mul_ps(scale, m_P_a);
    m_P_b = _mm_mul_ps(_mm_xor_ps(_mm_set1_ps(-0.0f), scale), m_P_b);
    m_Pab = _mm_mul_ps(broadcast(1.0f / 24.0f), m_Pab); // 0x3D2AAAAB
    m_Paab = _mm_mul_ps(broadcast(1.0f / 60.0f), m_Paab); // 0x3C888889
    m_Pabb = _mm_mul_ps(_mm_sub_ps(zero, broadcast(1.0f / 60.0f)), m_Pabb);
}

// 0x00F38E50: the integrals over the face (the plane n . x + w = 0 through the first vertex)
void InternalInertiaTensorComputer::compFaceIntegrals(const hkVector4f* vertices, const hkVector4f& normal)
{
    compProjectionIntegrals(vertices);
    const __m128 zero = _mm_setzero_ps();
    const __m128 w = _mm_sub_ps(zero, horizontalAdd3(_mm_mul_ps(vertices[0].m_quad, normal.m_quad)));
    const __m128 n = permute(normal.m_quad);
    const __m128 two = constant(HK_QUADREAL_2);
    const __m128 three = constant(HK_QUADREAL_3);
    const __m128 sq = _mm_mul_ps(n, n);
    const __m128 na2 = splatX(sq);
    const __m128 nb2 = splatY(sq);
    const __m128 k1 = _mm_div_ps(constant(HK_QUADREAL_1), splatZ(n));
    const __m128 k2 = _mm_mul_ps(k1, k1);
    const __m128 k3 = _mm_mul_ps(k2, k1);
    m_F_a = _mm_mul_ps(k1, m_P_a);
    const __m128 na = splatX(n);
    const __m128 nb = splatY(n);
    m_F_b = _mm_mul_ps(k1, m_P_b);
    const __m128 naP_a = _mm_mul_ps(na, m_P_a);
    const __m128 nbP_b = _mm_mul_ps(nb, m_P_b);
    const __m128 d = _mm_add_ps(splatX(nbP_b), splatX(naP_a)); // na Pa + nb Pb
    const __m128 wP1 = _mm_mul_ps(m_P1, w);
    const __m128 minusK2 = _mm_sub_ps(zero, k2);
    const __m128 twoNaNb = _mm_mul_ps(_mm_mul_ps(two, na), nb);
    const __m128 naPaa = splatY(naP_a);
    const __m128 sqTerm = _mm_add_ps(_mm_add_ps(_mm_mul_ps(na, naPaa), _mm_mul_ps(m_Pab, twoNaNb)), _mm_mul_ps(splatY(nbP_b), nb));
    const __m128 nbPbbb = splatZ(nbP_b);
    const __m128 Paab = m_Paab;
    const __m128 Fc = _mm_mul_ps(_mm_add_ps(wP1, d), minusK2);
    const __m128 naPaaa = splatZ(naP_a);
    const __m128 Fcc = _mm_mul_ps(_mm_add_ps(_mm_mul_ps(_mm_add_ps(_mm_mul_ps(two, d), wP1), w), sqTerm), k3);
    __m128 cubic = _mm_add_ps(_mm_mul_ps(_mm_mul_ps(_mm_mul_ps(three, na2), nb), Paab), _mm_mul_ps(na2, naPaaa));
    cubic = _mm_add_ps(cubic, _mm_mul_ps(_mm_mul_ps(_mm_mul_ps(three, na), nb2), m_Pabb));
    cubic = _mm_add_ps(cubic, _mm_mul_ps(nb2, nbPbbb));
    cubic = _mm_add_ps(cubic, _mm_mul_ps(_mm_mul_ps(three, w), sqTerm));
    const __m128 nbPabPlusNaPaa = _mm_add_ps(_mm_mul_ps(nb, m_Pab), naPaa);
    const __m128 w2 = _mm_mul_ps(w, w);
    const __m128 k4 = _mm_mul_ps(k2, k2);
    const __m128 Fccc = _mm_mul_ps(_mm_add_ps(cubic, _mm_mul_ps(_mm_add_ps(_mm_mul_ps(three, d), wP1), w2)), _mm_sub_ps(zero, k4));
    m_Faab = _mm_mul_ps(k1, Paab);
    const __m128 twoNaNbPaab = _mm_mul_ps(Paab, twoNaNb);
    m_F_c = _mm_movelh_ps(_mm_unpacklo_ps(Fc, Fcc), _mm_unpacklo_ps(Fccc, two));
    const __m128 Pbb = splatY(m_P_b);
    m_Fbbc = _mm_mul_ps(_mm_add_ps(_mm_mul_ps(Pbb, w), _mm_add_ps(_mm_mul_ps(na, m_Pabb), nbPbbb)), minusK2);
    const __m128 Pa = splatX(m_P_a);
    __m128 fcca = _mm_mul_ps(_mm_add_ps(_mm_mul_ps(nbPabPlusNaPaa, two), _mm_mul_ps(Pa, w)), w);
    fcca = _mm_add_ps(fcca, _mm_add_ps(_mm_add_ps(_mm_mul_ps(na, naPaaa), twoNaNbPaab), _mm_mul_ps(nb2, m_Pabb)));
    m_Fcca = _mm_mul_ps(fcca, k3);
}

// 0x00F391A0: the volume integrals of the closed mesh (vertices moved by offset): T0 the volume, T1 the first
// moments, T2 the second moments and TP the products of inertia
void InternalInertiaTensorComputer::compVolumeIntegrals(const hkGeometry* geom, const hkVector4f& offset)
{
    const __m128 zero = _mm_setzero_ps();
    m_T0 = zero;
    m_T1 = zero;
    m_T2 = zero;
    m_TP = zero;
    for (int t = 0; t < geom->m_triangles.getSize(); t++)
    {
        const hkGeometry::Triangle& tri = geom->m_triangles[t];
        alignas(16) hkVector4f v[3];
        v[0].m_quad = _mm_add_ps(geom->m_vertices[tri.m_a].m_quad, offset.m_quad);
        v[1].m_quad = _mm_add_ps(geom->m_vertices[tri.m_b].m_quad, offset.m_quad);
        v[2].m_quad = _mm_add_ps(geom->m_vertices[tri.m_c].m_quad, offset.m_quad);
        const __m128 n = cross(_mm_sub_ps(v[1].m_quad, v[0].m_quad), _mm_sub_ps(v[2].m_quad, v[0].m_quad));
        const __m128 len2 = horizontalAdd3(_mm_mul_ps(n, n));
        if (!(_mm_cvtss_f32(len2) > 0.0f))
        {
            continue;
        }
        hkVector4f normal;
        normal.m_quad = _mm_mul_ps(_mm_div_ps(constant(HK_QUADREAL_1), _mm_sqrt_ps(len2)), n);

        // C: the (last) axis of the largest normal component
        const __m128 absN = absolute(normal.m_quad);
        const __m128 maxN = _mm_max_ps(splatZ(absN), _mm_max_ps(splatY(absN), splatX(absN)));
        const int largest = _mm_movemask_ps(_mm_and_ps(_mm_cmple_ps(maxN, absN), laneMask(7)));
        m_C = hkVector4Comparison_maskToLastIndex[largest];
        compFaceIntegrals(v, normal);

        // (Fa, Fb, Fc), (Faa, Fbb, Fcc), (Faaa, Fbbb, Fccc): transposed to rows of (a, b, c)
        const __m128 lo = _mm_unpacklo_ps(m_F_a, m_F_b);
        const __m128 hi = _mm_unpackhi_ps(m_F_a, m_F_b);
        const __m128 Fc = m_F_c;
        const __m128 row0 = _mm_movelh_ps(lo, Fc);
        m_F_a = row0;
        m_F_b = _mm_shuffle_ps(_mm_movehl_ps(row0, lo), Fc, 0xD4);
        m_F_c = _mm_shuffle_ps(hi, Fc, 0xE4);

        // T0 += n.x * (the x lane among a, b, c)
        __m128 sel = _mm_and_ps(laneMask(1 << (2 - m_C)), m_F_a);
        sel = _mm_or_ps(_mm_shuffle_ps(sel, sel, 0x4E), sel);
        sel = _mm_or_ps(_mm_shuffle_ps(sel, sel, 0xB1), sel);
        m_T0 = _mm_add_ps(_mm_mul_ps(sel, splatX(normal.m_quad)), m_T0);
        m_T1 = _mm_add_ps(_mm_mul_ps(unpermute(m_F_b), normal.m_quad), m_T1);
        m_T2 = _mm_add_ps(_mm_mul_ps(unpermute(m_F_c), normal.m_quad), m_T2);
        __m128 px;
        __m128 py;
        __m128 pz;
        if (m_C == 0)
        {
            pz = m_Fbbc;
            px = m_Fcca;
            py = m_Faab;
        }
        else if (m_C == 1)
        {
            pz = m_Faab;
            px = m_Fbbc;
            py = m_Fcca;
        }
        else
        {
            pz = m_Fcca;
            px = m_Faab;
            py = m_Fbbc;
        }
        const __m128 products = _mm_movelh_ps(_mm_unpacklo_ps(px, py), _mm_unpacklo_ps(pz, pz));
        m_TP = _mm_add_ps(_mm_mul_ps(products, normal.m_quad), m_TP);
    }
    m_T1 = _mm_mul_ps(m_T1, constant(HK_QUADREAL_INV_2));
    m_T2 = _mm_mul_ps(constant(HK_QUADREAL_INV_3), m_T2);
    m_TP = _mm_mul_ps(m_TP, constant(HK_QUADREAL_INV_2));
}

// 0x00F38080: the centre of mass r (T1 / T0) and the inertia tensor J about it from the volume integrals
void InternalInertiaTensorComputer::computeInertialTensorInternal(const hkSimdFloat32& mass, const hkSimdFloat32& density, hkVector4f& r, hkMatrix3f& J)
{
    const __m128 com = _mm_mul_ps(m_T1, _mm_div_ps(constant(HK_QUADREAL_1), m_T0));
    r.m_quad = com;
    const __m128 shiftDiag = _mm_mul_ps(crossSums(_mm_mul_ps(com, com)), mass.m_real);
    const __m128 shiftOff = _mm_mul_ps(crossProducts(com), mass.m_real);
    const __m128 diag = _mm_mul_ps(_mm_add_ps(_mm_shuffle_ps(m_T2, m_T2, 0xD2), _mm_shuffle_ps(m_T2, m_T2, 0xC9)), density.m_real);
    const __m128 off = _mm_add_ps(shiftOff, _mm_mul_ps(_mm_sub_ps(_mm_setzero_ps(), density.m_real), m_TP));
    __m128 cols[3];
    pointInertia(_mm_sub_ps(diag, shiftDiag), off, cols);
    J.m_col0.m_quad = cols[0];
    J.m_col1.m_quad = cols[1];
    J.m_col2.m_quad = cols[2];
}

// 0x00F39480: the centre and extents of the vertices' box (both zero without vertices)
void InternalInertiaTensorComputer::compGeometryAabb(const float* vertices, int numVertices, int striding, hkVector4f& centerOut, hkVector4f& aabbExtentsOut)
{
    centerOut.m_quad = _mm_setzero_ps();
    aabbExtentsOut.m_quad = _mm_setzero_ps();
    __m128 minV = constant(HK_QUADREAL_MAX);
    __m128 maxV = _mm_xor_ps(_mm_set1_ps(-0.0f), minV);
    const char* p = reinterpret_cast<const char*>(vertices);
    for (int i = 0; i < numVertices; i++, p += striding)
    {
        const __m128 v = _mm_load_ps(reinterpret_cast<const float*>(p));
        minV = _mm_min_ps(v, minV);
        maxV = _mm_max_ps(v, maxV);
    }
    if (numVertices != 0)
    {
        const __m128 extents = _mm_sub_ps(maxV, minV);
        centerOut.m_quad = _mm_add_ps(_mm_mul_ps(extents, constant(HK_QUADREAL_INV_2)), minV);
        aabbExtentsOut.m_quad = extents;
    }
}

// ---- hkInertiaTensorComputer

// 0x00F37DC0
hkResult hkInertiaTensorComputer::computeSphereVolumeMassProperties(float radius, float mass, hkMassProperties& result)
{
    const __m128 m = broadcast(mass);
    if (!(_mm_cvtss_f32(m) > 0.0f))
    {
        return HK_FAILURE;
    }
    const __m128 r = broadcast(radius);
    if (!(_mm_cvtss_f32(r) > 0.0f))
    {
        return HK_FAILURE;
    }
    const __m128 k = _mm_mul_ps(_mm_mul_ps(_mm_mul_ps(r, m), r), broadcast(0.4f)); // 2/5 m r^2 (0x3ECCCCCD)
    result.m_inertiaTensor.m_col0.m_quad = _mm_mul_ps(k, constant(HK_QUADREAL_1000));
    result.m_inertiaTensor.m_col1.m_quad = _mm_mul_ps(k, constant(HK_QUADREAL_0100));
    result.m_inertiaTensor.m_col2.m_quad = _mm_mul_ps(k, constant(HK_QUADREAL_0010));
    result.m_centerOfMass.m_quad = _mm_setzero_ps();
    result.m_volume = _mm_cvtss_f32(_mm_mul_ps(_mm_mul_ps(_mm_mul_ps(constant(HK_QUADREAL_FOUR_PI_THIRD), r), r), r));
    result.m_mass = mass;
    return HK_SUCCESS;
}

// 0x00F37F10: m / 3 (y^2 + z^2, x^2 + z^2, x^2 + y^2) of the half extents (w 1) and the volume 8 x y z
hkResult hkInertiaTensorComputer::computeBoxVolumeMassPropertiesDiagonalized(const hkVector4f& halfExtents, float mass, hkVector4f& inertiaDiagonal, float& volume)
{
    const __m128 m = broadcast(mass);
    if (!(_mm_cvtss_f32(m) > 0.0f))
    {
        return HK_FAILURE;
    }
    const __m128 he = halfExtents.m_quad;
    const __m128 diag = _mm_mul_ps(crossSums(_mm_mul_ps(he, he)), _mm_mul_ps(constant(HK_QUADREAL_INV_3), m));
    inertiaDiagonal.m_quad = _mm_shuffle_ps(diag, _mm_unpackhi_ps(diag, constant(HK_QUADREAL_1)), 0xC4);
    volume = _mm_cvtss_f32(_mm_mul_ps(_mm_mul_ps(_mm_mul_ps(splatY(he), splatX(he)), splatZ(he)), constant(HK_QUADREAL_8)));
    return HK_SUCCESS;
}

// 0x00F385D0
hkResult hkInertiaTensorComputer::computeBoxVolumeMassProperties(const hkVector4f& halfExtents, float mass, hkMassProperties& result)
{
    result.m_centerOfMass.m_quad = _mm_setzero_ps();
    result.m_mass = mass;
    hkVector4f diag;
    diag.m_quad = _mm_setzero_ps();
    const hkResult res = computeBoxVolumeMassPropertiesDiagonalized(halfExtents, mass, diag, result.m_volume);
    result.m_inertiaTensor.m_col0.m_quad = _mm_mul_ps(constant(HK_QUADREAL_1000), diag.m_quad);
    result.m_inertiaTensor.m_col1.m_quad = _mm_mul_ps(constant(HK_QUADREAL_0100), diag.m_quad);
    result.m_inertiaTensor.m_col2.m_quad = _mm_mul_ps(constant(HK_QUADREAL_0010), diag.m_quad);
    return res;
}

// 0x00F37FB0: the parallel axis theorem towards the centre of mass (minus m (|s|^2 I - s s^T))
void hkInertiaTensorComputer::shiftInertiaToCom(const hkVector4f& shift, const hkSimdFloat32& mass, hkMatrix3f& inertia)
{
    const __m128 s = shift.m_quad;
    const __m128 diag = _mm_mul_ps(crossSums(_mm_mul_ps(s, s)), _mm_sub_ps(_mm_setzero_ps(), mass.m_real));
    const __m128 off = _mm_mul_ps(crossProducts(s), mass.m_real);
    __m128 cols[3];
    pointInertia(diag, off, cols);
    inertia.m_col0.m_quad = _mm_add_ps(cols[0], inertia.m_col0.m_quad);
    inertia.m_col1.m_quad = _mm_add_ps(cols[1], inertia.m_col1.m_quad);
    inertia.m_col2.m_quad = _mm_add_ps(cols[2], inertia.m_col2.m_quad);
}

// 0x00F38050
void hkInertiaTensorComputer::shiftInertiaFromCom(const hkVector4f& shift, const hkSimdFloat32& mass, hkMatrix3f& inertia)
{
    hkSimdFloat32 negMass;
    negMass.m_real = _mm_sub_ps(_mm_setzero_ps(), mass.m_real);
    shiftInertiaToCom(shift, negMass, inertia);
}

// 0x00F38650
// The masses and volumes add up, the centre of mass is the mass weighted one (of the elements' transformed centres)
// and each element's inertia is rotated into the common frame and moved to the common centre.
hkResult hkInertiaTensorComputer::combineMassProperties(const hkArrayBase<hkMassElement>& elements, hkMassProperties& result)
{
    __m128 totalMass = _mm_setzero_ps();
    __m128 weightedCom = _mm_setzero_ps();
    __m128 totalVolume = _mm_setzero_ps();
    for (int i = 0; i < elements.getSize(); i++)
    {
        const hkMassElement& e = elements[i];
        hkVector4f com;
        com._setTransformedPos(e.m_transform, e.m_properties.m_centerOfMass);
        const __m128 m = broadcast(e.m_properties.m_mass);
        totalMass = _mm_add_ps(totalMass, m);
        weightedCom = _mm_add_ps(weightedCom, _mm_mul_ps(com.m_quad, m));
        totalVolume = _mm_add_ps(totalVolume, broadcast(e.m_properties.m_volume));
    }
    if (!(_mm_cvtss_f32(totalMass) > 0.0f))
    {
        return HK_FAILURE;
    }
    const __m128 com = _mm_mul_ps(_mm_div_ps(constant(HK_QUADREAL_1), totalMass), weightedCom);
    result.m_centerOfMass.m_quad = com;
    result.m_mass = _mm_cvtss_f32(totalMass);
    result.m_volume = _mm_cvtss_f32(totalVolume);
    result.m_inertiaTensor.setZero();
    for (int i = 0; i < elements.getSize(); i++)
    {
        const hkMassElement& e = elements[i];
        hkMatrix3f inertia = e.m_properties.m_inertiaTensor;
        inertia.changeBasis(e.m_transform.getRotation());
        hkVector4f shift;
        shift._setTransformedPos(e.m_transform, e.m_properties.m_centerOfMass);
        shift.m_quad = _mm_sub_ps(shift.m_quad, com);
        hkSimdFloat32 m;
        m.m_real = broadcast(e.m_properties.m_mass);
        shiftInertiaFromCom(shift, m, inertia);
        result.m_inertiaTensor.add(inertia);
    }
    return HK_SUCCESS;
}

// 0x00F39500
// The closed mesh's volume integrals about the centre of its box. A mesh without volume gets the box's mass
// properties (of its full extents as half extents) at the box centre. Diagonal inertia components below a tenth of
// the box's are raised to it (with the centre of mass moved back to the box centre on that axis and the products of
// that row clamped at zero).
void hkInertiaTensorComputer::computeGeometryVolumeMassProperties(const hkGeometry* geom, float mass, hkMassProperties& result)
{
    const __m128 m = broadcast(mass);
    const __m128 massV = select(_mm_cmple_ps(m, _mm_setzero_ps()), constant(HK_QUADREAL_1), m); // no mass: 1
    hkVector4f center;
    hkVector4f extents;
    InternalInertiaTensorComputer::compGeometryAabb(reinterpret_cast<const float*>(geom->m_vertices.begin()), geom->m_vertices.getSize(), sizeof(hkVector4f), center, extents);
    const float massF = _mm_cvtss_f32(massV);
    hkMassProperties box;
    computeBoxVolumeMassProperties(extents, massF, box);
    hkVector4f offset;
    offset.m_quad = _mm_xor_ps(_mm_set1_ps(-0.0f), center.m_quad);
    alignas(16) InternalInertiaTensorComputer computer;
    computer.compVolumeIntegrals(geom, offset);
    const __m128 volume = computer.m_T0;
    if (!(_mm_cvtss_f32(volume) > 0.0f))
    {
        result.m_volume = box.m_volume;
        result.m_mass = box.m_mass;
        result.m_centerOfMass = center;
        result.m_inertiaTensor = box.m_inertiaTensor;
        return;
    }
    hkSimdFloat32 density;
    density.m_real = _mm_div_ps(massV, volume);
    result.m_volume = _mm_cvtss_f32(volume);
    result.m_mass = massF;
    hkSimdFloat32 massS;
    massS.m_real = massV;
    computer.computeInertialTensorInternal(massS, density, result.m_centerOfMass, result.m_inertiaTensor);
    result.m_centerOfMass.m_quad = _mm_add_ps(result.m_centerOfMass.m_quad, center.m_quad);

    hkMatrix3f& J = result.m_inertiaTensor;
    const __m128 maskY = laneMask(2);
    const __m128 maskZ = laneMask(4);
    const __m128 diagJ = select(maskZ, J.m_col2.m_quad, select(maskY, J.m_col1.m_quad, J.m_col0.m_quad));
    const __m128 diagBox = select(maskZ, box.m_inertiaTensor.m_col2.m_quad, select(maskY, box.m_inertiaTensor.m_col1.m_quad, box.m_inertiaTensor.m_col0.m_quad));
    const __m128 minDiag = _mm_mul_ps(diagBox, broadcast(0.1f)); // 0x3DCCCCCD
    const __m128 tooSmall = _mm_cmplt_ps(diagJ, minDiag);
    const __m128 clamped = select(tooSmall, minDiag, diagJ);
    result.m_centerOfMass.m_quad = select(tooSmall, center.m_quad, result.m_centerOfMass.m_quad);
    J.m_col0.m_quad = _mm_move_ss(J.m_col0.m_quad, clamped);
    J.m_col1.m_quad = select(maskY, clamped, J.m_col1.m_quad);
    J.m_col2.m_quad = select(maskZ, clamped, J.m_col2.m_quad);
    alignas(16) float c0[4];
    alignas(16) float c1[4];
    alignas(16) float c2[4];
    _mm_store_ps(c0, _mm_max_ps(_mm_setzero_ps(), J.m_col0.m_quad));
    _mm_store_ps(c1, _mm_max_ps(_mm_setzero_ps(), J.m_col1.m_quad));
    _mm_store_ps(c2, _mm_max_ps(_mm_setzero_ps(), J.m_col2.m_quad));
    const int clampedAxes = _mm_movemask_ps(tooSmall);
    if (clampedAxes & 1)
    {
        J.m_col1(0) = c0[1];
        J.m_col0(1) = c0[1];
        J.m_col2(0) = c0[2];
        J.m_col0(2) = c0[2];
    }
    if (clampedAxes & 2)
    {
        J.m_col0(1) = c1[0];
        J.m_col1(0) = c1[0];
        J.m_col2(1) = c1[2];
        J.m_col1(2) = c1[2];
    }
    if (clampedAxes & 4)
    {
        J.m_col0(2) = c2[0];
        J.m_col2(0) = c2[0];
        J.m_col1(2) = c2[1];
        J.m_col2(1) = c2[1];
    }
}

// 0x00F39B00: the box around the vertices grown by the radius, of mass 1, centred on the box
hkResult hkInertiaTensorComputer::computeApproximateConvexHullMassProperties(const hkStridedVertices& vertices, float radius, hkMassProperties& result)
{
    hkVector4f center;
    hkVector4f extents;
    InternalInertiaTensorComputer::compGeometryAabb(vertices.m_vertices, vertices.m_numVertices, vertices.m_striding, center, extents);
    hkVector4f halfExtents;
    halfExtents.m_quad = _mm_add_ps(_mm_mul_ps(constant(HK_QUADREAL_INV_2), extents.m_quad), broadcast(radius));
    const hkResult res = computeBoxVolumeMassProperties(halfExtents, 1.0f, result);
    result.m_centerOfMass = center;
    result.m_mass = 1.0f;
    return res;
}

// 0x00F3A4E0
hkResult hkInertiaTensorComputer::computeConvexHullMassProperties(const hkStridedVertices& vertices, float radius, hkMassProperties& result)
{
    if (s_computeConvexHullMassPropertiesFunction)
    {
        return s_computeConvexHullMassPropertiesFunction(vertices, radius, result);
    }
    return computeApproximateConvexHullMassProperties(vertices, radius, result);
}

// 0x00F39D30
// A thin triangle (thickness below 1e-5) is a surface: the vertices and the centroid weighted 1, 1, 1 and 9 over 12,
// moved to the centroid. A degenerate one (area below 1e-5) is a point mass at the centroid (not moved). Otherwise
// it is a prism of the thickness around the triangle (an hkGeometry through computeGeometryVolumeMassProperties).
// The volume is always the area times the thickness.
hkResult hkInertiaTensorComputer::computeTriangleSurfaceMassProperties(const hkVector4f& v0, const hkVector4f& v1, const hkVector4f& v2, float mass, float surfaceThickness, hkMassProperties& result)
{
    const __m128 massV = broadcast(mass);
    const __m128 zero = _mm_setzero_ps();
    if (!(_mm_cvtss_f32(massV) > 0.0f))
    {
        return HK_FAILURE;
    }
    const __m128 thickness = broadcast(surfaceThickness);
    if (!(_mm_cvtss_f32(thickness) >= 0.0f))
    {
        return HK_FAILURE;
    }
    const __m128 eps = broadcast(1e-5f); // 0x3727C5AC
    const __m128 a = v0.m_quad;
    const __m128 b = v1.m_quad;
    const __m128 c = v2.m_quad;
    const __m128 n = cross(_mm_sub_ps(c, b), _mm_sub_ps(a, b));
    const __m128 twiceArea = length3(n);

    __m128 com;
    __m128 cols[3];
    if (_mm_cvtss_f32(thickness) < _mm_cvtss_f32(eps))
    {
        const __m128 nine = broadcast(9.0f);
        com = _mm_mul_ps(_mm_add_ps(_mm_add_ps(a, b), c), constant(HK_QUADREAL_INV_3));
        __m128 sq = _mm_mul_ps(_mm_mul_ps(com, com), nine);
        sq = _mm_add_ps(sq, _mm_mul_ps(a, a));
        sq = _mm_add_ps(sq, _mm_mul_ps(b, b));
        const __m128 k = _mm_mul_ps(broadcast(1.0f / 12.0f), massV); // 0x3DAAAAAB
        sq = _mm_add_ps(sq, _mm_mul_ps(c, c));
        sq = _mm_mul_ps(sq, k);
        const __m128 minusK = _mm_sub_ps(zero, k);
        const __m128 diag = crossSums(sq);
        __m128 off = _mm_mul_ps(_mm_mul_ps(_mm_shuffle_ps(com, com, 0xE9), _mm_unpacklo_ps(com, com)), nine);
        off = _mm_add_ps(off, _mm_mul_ps(_mm_shuffle_ps(a, a, 0xE9), _mm_unpacklo_ps(a, a)));
        off = _mm_add_ps(off, _mm_mul_ps(_mm_shuffle_ps(b, b, 0xE9), _mm_unpacklo_ps(b, b)));
        off = _mm_add_ps(off, _mm_mul_ps(_mm_shuffle_ps(c, c, 0xE9), _mm_unpacklo_ps(c, c)));
        off = _mm_mul_ps(off, minusK);
        // the products (xy, xz, yz) in lanes 0-2
        hkMatrix3f inertia;
        const __m128 d = clearW(diag);
        inertia.m_col0.m_quad = select(laneMask(9), d, _mm_unpacklo_ps(off, off));
        inertia.m_col1.m_quad = select(laneMask(10), d, off);
        inertia.m_col2.m_quad = _mm_shuffle_ps(_mm_shuffle_ps(off, off, 0xE9), d, 0xE4);
        hkVector4f shift;
        shift.m_quad = com;
        hkSimdFloat32 m;
        m.m_real = massV;
        shiftInertiaToCom(shift, m, inertia);
        cols[0] = inertia.m_col0.m_quad;
        cols[1] = inertia.m_col1.m_quad;
        cols[2] = inertia.m_col2.m_quad;
    }
    else if (!(_mm_cvtss_f32(twiceArea) >= _mm_cvtss_f32(eps)))
    {
        const __m128 minusM = _mm_sub_ps(zero, massV);
        com = _mm_mul_ps(constant(HK_QUADREAL_INV_3), _mm_add_ps(_mm_add_ps(a, b), c));
        const __m128 diag = _mm_mul_ps(crossSums(_mm_mul_ps(com, com)), massV);
        const __m128 off = _mm_mul_ps(_mm_mul_ps(_mm_shuffle_ps(com, com, 0xE9), _mm_unpacklo_ps(com, com)), minusM);
        const __m128 d = clearW(diag);
        cols[0] = select(laneMask(9), d, _mm_unpacklo_ps(off, off));
        cols[1] = select(laneMask(10), d, off);
        cols[2] = _mm_shuffle_ps(_mm_shuffle_ps(off, off, 0xE9), d, 0xE4);
    }
    else
    {
        const __m128 unitNormal = normalize3(n);
        const __m128 halfThickness = _mm_mul_ps(thickness, constant(HK_QUADREAL_INV_2));
        hkGeometry prism;
        prism.m_vertices.setSize(6);
        const __m128 up = _mm_mul_ps(halfThickness, unitNormal);
        const __m128 down = _mm_mul_ps(_mm_sub_ps(zero, halfThickness), unitNormal);
        prism.m_vertices[0].m_quad = _mm_add_ps(a, up);
        prism.m_vertices[1].m_quad = _mm_add_ps(a, down);
        prism.m_vertices[2].m_quad = _mm_add_ps(b, up);
        prism.m_vertices[3].m_quad = _mm_add_ps(b, down);
        prism.m_vertices[4].m_quad = _mm_add_ps(up, c);
        prism.m_vertices[5].m_quad = _mm_add_ps(down, c);
        prism.m_triangles.setSize(8);
        static const int indices[8][3] = { { 0, 2, 4 }, { 1, 5, 3 }, { 0, 3, 2 }, { 0, 1, 3 }, { 1, 0, 4 }, { 1, 4, 5 }, { 2, 5, 4 }, { 2, 3, 5 } };
        for (int i = 0; i < 8; i++)
        {
            prism.m_triangles[i].m_a = indices[i][0];
            prism.m_triangles[i].m_b = indices[i][1];
            prism.m_triangles[i].m_c = indices[i][2];
            prism.m_triangles[i].m_material = -1;
        }
        hkMassProperties props;
        computeGeometryVolumeMassProperties(&prism, mass, props);
        com = props.m_centerOfMass.m_quad;
        cols[0] = props.m_inertiaTensor.m_col0.m_quad;
        cols[1] = props.m_inertiaTensor.m_col1.m_quad;
        cols[2] = props.m_inertiaTensor.m_col2.m_quad;
    }
    result.m_mass = mass;
    result.m_inertiaTensor.m_col0.m_quad = cols[0];
    result.m_inertiaTensor.m_col1.m_quad = cols[1];
    result.m_inertiaTensor.m_col2.m_quad = cols[2];
    result.m_centerOfMass.m_quad = com;
    result.m_volume = _mm_cvtss_f32(_mm_mul_ps(_mm_mul_ps(twiceArea, constant(HK_QUADREAL_INV_2)), thickness));
    return HK_SUCCESS;
}

// 0x00F3B330: a solid cylinder (the axis must have a length) about its centre, rotated onto the axis
hkResult hkInertiaTensorComputer::computeCylinderVolumeMassProperties(const hkVector4f& startAxis, const hkVector4f& endAxis, float radius, float mass, hkMassProperties& result)
{
    const __m128 massV = broadcast(mass);
    if (!(_mm_cvtss_f32(massV) > 0.0f))
    {
        return HK_FAILURE;
    }
    const __m128 r = broadcast(radius);
    if (!(_mm_cvtss_f32(r) > 0.0f))
    {
        return HK_FAILURE;
    }
    const __m128 axis = _mm_sub_ps(endAxis.m_quad, startAxis.m_quad);
    const __m128 length = length3(axis);
    if (!(_mm_cvtss_f32(length) > 0.0f))
    {
        return HK_FAILURE;
    }
    hkInplaceArray<hkMassElement, 1, hkContainerHeapAllocator> elements;
    hkMassElement& e = elements.expandOne();
    axisRotation(axis, e.m_transform.getRotation());
    e.m_transform.getTranslation().m_quad = _mm_mul_ps(_mm_add_ps(startAxis.m_quad, endAxis.m_quad), constant(HK_QUADREAL_INV_2));
    const __m128 volume = _mm_mul_ps(_mm_mul_ps(_mm_mul_ps(r, constant(HK_QUADREAL_PI)), r), length);
    __m128 cols[3];
    cylinderInertia(length, r, cols);
    e.m_properties.m_inertiaTensor.m_col0.m_quad = cols[0];
    e.m_properties.m_inertiaTensor.m_col1.m_quad = cols[1];
    e.m_properties.m_inertiaTensor.m_col2.m_quad = cols[2];
    hkSimdFloat32 m;
    m.m_real = massV;
    e.m_properties.m_inertiaTensor.mul(m);
    e.m_properties.m_centerOfMass.m_quad = _mm_setzero_ps();
    e.m_properties.m_volume = _mm_cvtss_f32(volume);
    e.m_properties.m_mass = mass;
    return combineMassProperties(elements, result);
}

// 0x00F3A940
// A cylinder between the end points and a hemisphere at each end (each a sphere's half, about its own centre of mass
// 3/8 r from the end), the mass shared by volume; a zero length axis has no cylinder volume.
hkResult hkInertiaTensorComputer::computeCapsuleVolumeMassProperties(const hkVector4f& startAxis, const hkVector4f& endAxis, float radius, float mass, hkMassProperties& result)
{
    const __m128 massV = broadcast(mass);
    if (!(_mm_cvtss_f32(massV) > 0.0f))
    {
        return HK_FAILURE;
    }
    const __m128 r = broadcast(radius);
    if (!(_mm_cvtss_f32(r) > 0.0f))
    {
        return HK_FAILURE;
    }
    const __m128 zero = _mm_setzero_ps();
    const __m128 axis = _mm_sub_ps(endAxis.m_quad, startAxis.m_quad);
    const __m128 length = length3(axis);
    hkRotationf rotation;
    if (_mm_cvtss_f32(length) > 0.0f)
    {
        axisRotation(axis, rotation);
    }
    else
    {
        rotation.setIdentity();
    }
    const __m128 half = constant(HK_QUADREAL_INV_2);
    const __m128 center = _mm_mul_ps(_mm_add_ps(startAxis.m_quad, endAxis.m_quad), half);
    const __m128 sphereVolume = _mm_mul_ps(_mm_mul_ps(_mm_mul_ps(constant(HK_QUADREAL_FOUR_PI_THIRD), r), r), r);
    const __m128 cylinderVolume = _mm_mul_ps(_mm_mul_ps(_mm_mul_ps(constant(HK_QUADREAL_PI), r), r), length);
    const __m128 totalVolume = _mm_add_ps(cylinderVolume, sphereVolume);
    const __m128 sphereMass = _mm_div_ps(_mm_mul_ps(sphereVolume, massV), totalVolume);
    hkSimdFloat32 cylinderMass;
    cylinderMass.m_real = _mm_div_ps(_mm_mul_ps(cylinderVolume, massV), totalVolume);

    hkInplaceArray<hkMassElement, 3, hkContainerHeapAllocator> elements;

    // the cylinder
    {
        hkMassElement e;
        e.m_properties.m_volume = 0.0f;
        e.m_properties.m_mass = 0.0f;
        e.m_transform.getRotation() = rotation;
        __m128 cols[3];
        cylinderInertia(length, r, cols);
        e.m_properties.m_inertiaTensor.m_col0.m_quad = cols[0];
        e.m_properties.m_inertiaTensor.m_col1.m_quad = cols[1];
        e.m_properties.m_inertiaTensor.m_col2.m_quad = cols[2];
        e.m_transform.getTranslation().m_quad = center;
        e.m_properties.m_inertiaTensor.mul(cylinderMass);
        e.m_properties.m_volume = _mm_cvtss_f32(cylinderVolume);
        e.m_properties.m_centerOfMass.m_quad = zero;
        e.m_properties.m_mass = _mm_cvtss_f32(cylinderMass.m_real);
        elements.pushBack(e);
    }

    // the hemispheres at +-length / 2 along the axis, their centres of mass 3/8 r further out
    const __m128 halfLength = _mm_mul_ps(half, length);
    for (int side = 0; side < 2; side++)
    {
        const __m128 along = side == 0 ? halfLength : _mm_sub_ps(zero, halfLength);
        const __m128 offset = _mm_movelh_ps(_mm_unpacklo_ps(zero, zero), _mm_unpacklo_ps(along, zero)); // (0, 0, along, 0)
        const hkMatrix3f& rot = rotation;
        __m128 end;
        if (side == 0)
        {
            end = _mm_add_ps(_mm_mul_ps(splatY(offset), rot.m_col1.m_quad), _mm_mul_ps(splatX(offset), rot.m_col0.m_quad));
        }
        else
        {
            end = _mm_add_ps(_mm_mul_ps(splatX(offset), rot.m_col0.m_quad), _mm_mul_ps(splatY(offset), rot.m_col1.m_quad));
        }
        end = _mm_add_ps(_mm_add_ps(end, _mm_mul_ps(splatZ(offset), rot.m_col2.m_quad)), center);
        const __m128 comOffset = _mm_mul_ps(broadcast(side == 0 ? 0.375f : -0.375f), r); // 0x3EC00000, 0xBEC00000
        hkMassElement e;
        e.m_properties.m_volume = 0.0f;
        e.m_properties.m_mass = 0.0f;
        e.m_properties.m_inertiaTensor.setZero();
        e.m_transform.getRotation() = rotation;
        e.m_transform.getTranslation().m_quad = end;
        e.m_properties.m_centerOfMass.m_quad = _mm_movelh_ps(zero, _mm_unpacklo_ps(comOffset, zero));
        hkMassProperties sphere;
        if (computeSphereVolumeMassProperties(radius, _mm_cvtss_f32(sphereMass), sphere).m_enum == HK_FAILURE)
        {
            return HK_FAILURE;
        }
        e.m_properties.m_inertiaTensor = sphere.m_inertiaTensor;
        hkSimdFloat32 halfS;
        halfS.m_real = half;
        e.m_properties.m_inertiaTensor.mul(halfS);
        hkSimdFloat32 halfMass;
        halfMass.m_real = _mm_mul_ps(sphereMass, half);
        shiftInertiaToCom(e.m_properties.m_centerOfMass, halfMass, e.m_properties.m_inertiaTensor);
        e.m_properties.m_volume = _mm_cvtss_f32(_mm_mul_ps(sphereVolume, half));
        e.m_properties.m_mass = _mm_cvtss_f32(_mm_mul_ps(sphereMass, half));
        elements.pushBack(e);
    }
    return combineMassProperties(elements, result);
}
