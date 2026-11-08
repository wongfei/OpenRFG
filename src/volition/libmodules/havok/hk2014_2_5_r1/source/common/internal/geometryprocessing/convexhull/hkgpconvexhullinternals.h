// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/internal/geometryprocessing/convexhull/hkgpconvexhullinternals.h
//
// Hand-written (havok_impl.md HK3): hkgpConvexHullImpl, the private implementation of hkgpConvexHull (layout from
// rfg.pdb). Only the parts rfg.exe reaches are declared: building a hull from points (an exact incremental hull on
// integer coordinates quantised to [-8192, 8191], 64-bit plane distances), plane indexing and mass properties.
// build, computeProjectionPlane, flushPending and clearHorizon are in hkgpconvexhullinternals.cpp, the rest in
// hkgpconvexhull.cpp (where rfg.exe has its copies).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/hkmath.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkfourtransposedpointsf.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/geometry/aabb/hkaabb.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/internal/geometryprocessing/abstractmesh/hkgpabstractmesh.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/internal/geometryprocessing/convexhull/hkgpconvexhull.h"

#include <emmintrin.h>

// SSE steps shared by the hull code (each is one instruction sequence of the original).
namespace hkgpConvexHullSse
{
inline __m128 splatX(__m128 v) { return _mm_shuffle_ps(v, v, 0x00); }
inline __m128 splatY(__m128 v) { return _mm_shuffle_ps(v, v, 0x55); }
inline __m128 splatZ(__m128 v) { return _mm_shuffle_ps(v, v, 0xAA); }
inline __m128 splatW(__m128 v) { return _mm_shuffle_ps(v, v, 0xFF); }
inline __m128 constant(hkVectorConstant c) { return g_vectorfConstants[c]; }

// mask ? a : b
inline __m128 select(__m128 mask, __m128 a, __m128 b) { return _mm_or_ps(_mm_and_ps(mask, a), _mm_andnot_ps(mask, b)); }

// hkVector4fComparison_maskToComparison[m]: lane k set when bit k of m is
inline __m128 laneMask(int m)
{
    return _mm_castsi128_ps(_mm_set_epi32((m & 8) ? -1 : 0, (m & 4) ? -1 : 0, (m & 2) ? -1 : 0, (m & 1) ? -1 : 0));
}

// w = 0 (byte shifts)
inline __m128 clearW(__m128 v) { return _mm_castsi128_ps(_mm_srli_si128(_mm_slli_si128(_mm_castps_si128(v), 4), 4)); }

// (v.xyz, w.x) (unpckhps + shufps 0xC4)
inline __m128 setW(__m128 v, __m128 w) { return _mm_shuffle_ps(v, _mm_unpackhi_ps(v, w), 0xC4); }

inline __m128 absolute(__m128 v) { return _mm_castsi128_ps(_mm_srli_epi32(_mm_slli_epi32(_mm_castps_si128(v), 1), 1)); }

// (y + x) + z in all lanes
inline __m128 horizontalAdd3(__m128 v) { return _mm_add_ps(_mm_add_ps(splatY(v), splatX(v)), splatZ(v)); }

inline __m128 dot3(__m128 a, __m128 b) { return horizontalAdd3(_mm_mul_ps(a, b)); }

// a x b
inline __m128 cross(__m128 a, __m128 b)
{
    const __m128 c = _mm_sub_ps(_mm_mul_ps(_mm_shuffle_ps(b, b, 0xC9), a), _mm_mul_ps(_mm_shuffle_ps(a, a, 0xC9), b));
    return _mm_shuffle_ps(c, c, 0xC9);
}

// 1 / sqrt(len2) with one Newton step, zero for len2 <= 0
inline __m128 invSqrt(__m128 len2)
{
    const __m128 r = _mm_rsqrt_ps(len2);
    const __m128 inv = _mm_mul_ps(_mm_sub_ps(_mm_set1_ps(3.0f), _mm_mul_ps(_mm_mul_ps(r, len2), r)), _mm_mul_ps(r, _mm_set1_ps(0.5f)));
    return _mm_andnot_ps(_mm_cmple_ps(len2, _mm_setzero_ps()), inv);
}

// normalize<3> (a zero vector stays zero)
inline __m128 normalize3(__m128 v) { return _mm_mul_ps(invSqrt(dot3(v, v)), v); }

// normalizeIfNotZero<3>: v unchanged unless its length is positive
inline __m128 normalize3IfNotZero(__m128 v)
{
    const __m128 len2 = dot3(v, v);
    const __m128 positive = _mm_cmplt_ps(_mm_sub_ps(_mm_setzero_ps(), len2), _mm_setzero_ps());
    return select(positive, _mm_mul_ps(invSqrt(len2), v), v);
}

// length<3> as len2 * invSqrt(len2)
inline __m128 length3(__m128 v)
{
    const __m128 len2 = dot3(v, v);
    const __m128 r = _mm_rsqrt_ps(len2);
    const __m128 len = _mm_mul_ps(_mm_mul_ps(_mm_sub_ps(_mm_set1_ps(3.0f), _mm_mul_ps(_mm_mul_ps(r, len2), r)), _mm_mul_ps(r, _mm_set1_ps(0.5f))), len2);
    return _mm_andnot_ps(_mm_cmple_ps(len2, _mm_setzero_ps()), len);
}

// The plane through a, b, c: normalized (b - a) x (c - a) (unchanged when zero) and w = -n.a.
inline __m128 planeFromTriangle(__m128 a, __m128 b, __m128 c)
{
    const __m128 n = normalize3IfNotZero(cross(_mm_sub_ps(b, a), _mm_sub_ps(c, a)));
    return setW(n, _mm_sub_ps(_mm_setzero_ps(), dot3(n, a)));
}
}

// size 0x1C0
class hkgpConvexHullImpl
{
public:
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkgpConvexHullImpl); // new 0x00FDEAD0, delete 0x00FDEAF0

    enum
    {
        DomainRange = 14,
        DomainHalfRange = 13,
        DomainMinBound = -8192,
        DomainMaxBound = 8191,
        EOC = 8192,
    };

    typedef hkInt64 tDist;

    struct Vertex;
    struct Triangle;
    struct Edge;

    // size 0x20
    struct Plane
    {
        Plane() : m_index(-1) {} // 0x00FDD070

        int m_c[3]; // 0x00
        hkInt64 m_d; // 0x10
        int m_index; // 0x18
    };

    // size 0x40
    struct Vertex : public hkgpAbstractMeshDefinitions::Vertex<Vertex, hkgpConvexHull::Vertex>
    {
        Vertex() : m_scratch(0.0f), m_index(-1) {} // 0x00FE0A50 (m_refs is not set)

        // 0x00FDD090: lexicographic on the integer coordinates
        int compare(const Vertex& o) const
        {
            for (int i = 0; i < 3; ++i)
            {
                if (m_x[i] < o.m_x[i])
                {
                    return -1;
                }
                if (m_x[i] > o.m_x[i])
                {
                    return 1;
                }
            }
            return 0;
        }
        bool operator<(const Vertex& o) const { return compare(o) < 0; }

        hkVector4f m_source; // 0x10
        int m_x[3]; // 0x20
        float m_scratch; // 0x2C
        int m_refs; // 0x30
        int m_index; // 0x34
    };

    // size 0x48
    struct Triangle : public hkgpAbstractMeshDefinitions::Triangle<Triangle, hkgpConvexHull::Triangle, Vertex*>
    {
        Triangle() : m_index(-1) {} // 0x00FFB260

        Plane m_plane; // 0x20
        int m_pid; // 0x40
        int m_index; // 0x44
    };

    // size 0x8
    struct Edge : public hkgpTopology::Edge<Edge, Vertex, Triangle, Vertex*>
    {
        Edge() {} // 0x00FDEB20
        Edge(Triangle* t, int i) : hkgpTopology::Edge<Edge, Vertex, Triangle, Vertex*>(t, (unsigned int)i) {} // 0x00FDEB30
        Edge(unsigned long id) : hkgpTopology::Edge<Edge, Vertex, Triangle, Vertex*>(id) {} // 0x00FE3C30
    };

    // size 0x8: sorts by decreasing value
    struct SortedTriangle
    {
        bool operator<(const SortedTriangle& other) const { return 0.0f < m_value - other.m_value; } // 0x00FDE920

        Triangle* m_data; // 0x00
        float m_value; // 0x04
    };

    // The vertex positions of a template: the source point, or the integer coordinates mapped back.
    struct PositionAsSource
    {
        enum
        {
            INDEX = 0,
        };
        static __m128 get(const hkgpConvexHullImpl*, const Vertex* v) { return v->m_source.m_quad; } // 0x00FDD140
    };

    struct PositionAsTransform
    {
        enum
        {
            INDEX = 1,
        };
        static __m128 get(const hkgpConvexHullImpl* ch, const Vertex* v) { return ch->transform(*v); } // 0x00FE3C50
    };

    typedef hkgpAbstractMesh<Edge, Vertex, Triangle, hkContainerHeapAllocator> Mesh;

    hkgpConvexHullImpl(); // 0x00FE59E0
    ~hkgpConvexHullImpl(); // 0x00FE5A50
    void reset(bool keepPlane); // 0x00FE0AB0

    // (float)x * scaleInv + origin, w = 0
    __m128 transform(const Vertex& v) const
    {
        const __m128 x = _mm_cvtepi32_ps(_mm_set_epi32(0, v.m_x[2], v.m_x[1], v.m_x[0]));
        return _mm_add_ps(_mm_mul_ps(hkgpConvexHullSse::clearW(x), m_scaleInv.m_quad), m_origin.m_quad);
    }

    // 0x00FDEBC0: the integer plane of a, b, c ((b - a) x (c - a), d = -n.a; d = 0 for a zero normal)
    static bool makePlane(const Vertex& a, const Vertex& b, const Vertex& c, Plane& p);

    void rebuildDomainParameters(const hkVector4f* points, const int numPoints); // 0x00FE0C70
    void computeProjectionPlane(const hkVector4f* points, int numPoints, hkVector4f& planeOut); // 0x00FFBDF0
    int build(const hkVector4f* points, const int numPoints, bool forcePlanar, const hkgpConvexHullImpl* domain); // 0x00FF9B40
    bool flushPending(); // 0x00FFCCA0
    void clearHorizon(const Vertex& vertex, const Edge& root, Edge& horizon); // 0x00FFC570

    // getCentroid<PositionAsSource> 0x00FE1A30, <PositionAsTransform> 0x00FE58C0: the vertex average, cached in
    // m_centroids[INDEX] (w = MAX marks it invalid)
    template<typename POSITION>
    const hkVector4f& getCentroid()
    {
        hkVector4f& c = m_centroids[POSITION::INDEX];
        const float w = _mm_cvtss_f32(hkgpConvexHullSse::splatW(c.m_quad));
        const float maxValue = _mm_cvtss_f32(g_vectorfConstants[HK_QUADREAL_MAX]);
        if (w == maxValue || w != w) // ucomiss: NaN also recomputes
        {
            c.m_quad = _mm_setzero_ps();
            for (const Vertex* v = m_mesh.m_vertices.getFirst(); v; v = v->next())
            {
                c.m_quad = _mm_add_ps(POSITION::get(this, v), c.m_quad);
            }
            const int n = m_mesh.m_vertices.getSize();
            if (n)
            {
                const __m128 inv = _mm_div_ps(g_vectorfConstants[HK_QUADREAL_1], _mm_set1_ps((float)n));
                c.m_quad = _mm_mul_ps(c.m_quad, inv);
            }
            c.m_quad = hkgpConvexHullSse::clearW(c.m_quad);
        }
        return c;
    }

    void buildIndices(); // buildIndices<PositionAsTransform> 0x00FE6620
    hkResult buildMassProperties(); // buildMassProperties<PositionAsTransform> 0x00FE8000

    hkgpConvexHull::BuildConfig m_config; // 0x00
    hkgpConvexHull* m_hull; // 0x10
    Mesh m_mesh; // 0x14
    hkArray<Vertex, hkContainerHeapAllocator> m_pendings; // 0x3C
    hkArray<hkVector4f, hkContainerHeapAllocator> m_planes; // 0x48
    hkArray<hkFourTransposedPointsf, hkContainerHeapAllocator> m_supportCache; // 0x54
    hkVector4f m_centroids[2]; // 0x60
    hkAabb m_boundingBoxes[2]; // 0x80
    hkVector4f m_projectionAxis; // 0xC0
    hkVector4f m_projectionPlane; // 0xD0
    hkVector4f m_origin; // 0xE0
    hkVector4f m_scale; // 0xF0
    hkVector4f m_scaleInv; // 0x100
    hkTransformf m_localTransform; // 0x110
    hkVector4f m_inertia; // 0x150
    hkMatrix3f m_inertiaMatrix; // 0x160
    hkSimdFloat32 m_volume; // 0x190
    hkSimdFloat32 m_surface; // 0x1A0
    int m_dimensions; // 0x1B0
    int m_pid; // 0x1B4
    unsigned int m_uid; // 0x1B8
    bool m_hasMassProperties; // 0x1BC
    bool m_degeneratedMassProperties; // 0x1BD
    bool m_hasIndexing; // 0x1BE

    static long g_uidGenerator; // 0x03BAC908
};
