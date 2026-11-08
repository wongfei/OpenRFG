// Original: volition/libmodules/havok/hk2014_2_5_r1/source/geometry/internal/types/hkcdwingededge.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0); the inline members (hkcdwingededge.inl) are
// ported from their out-of-line copies in rfg.exe (HK6). Hand-maintained.
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/hkmath.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/geometry/internal/types/hkcdvertex.h"

// 0x01365430 / 0x01367A20 (one copy per translation unit): 1e-7
static const float HK_WINGED_EDGE_VISIBILITY_TOLERANCE_WIDTH = 1e-7f;

// size 0x2EB0
class alignas(16) hkcdWingedEdge
{
public:
    enum
    {
        HKCD_WINGED_EDGE_TRIANGLE_LIMIT = 100,
        HKCD_WINGED_EDGE_VERTICIES_LIMIT = 55,
    };

    enum WingedEdgeStatus
    {
        HKCD_WINGED_EDGE_OK = 0,
        HKCD_WINGED_EDGE_OUT_OF_MEMORY = 1,
        HKCD_WINGED_EDGE_FAIL = 2,
    };

    struct Triangle;
    struct Vertex;
    // size 0x10
    struct Edge
    {
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(Edge);

        Vertex* m_vertex; // 0x00
        Edge* m_next; // 0x04
        Edge* m_neighbour; // 0x08
        Triangle* m_triangle; // 0x0C
    };

    // size 0x50
    struct alignas(16) Triangle
    {
        enum TriangleState
        {
            HKCD_NOT_VISITED = 0,
            HKCD_VISIBLE = 1,
            HKCD_INVISIBLE = 2,
            HKCD_FIND_NEAREST_TRIANGLE_VISITED = 3,
            HKCD_UNKNOWN = 4,
        };

        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(Triangle);
        void calculateTriangleData(); // 0x00F0D130
        hkResult calculateTriangleDataWithCheck(const hkVector4f& v1, const hkVector4f& v2, const hkVector4f& v3); // 0x00E8A4F0
        // 0x00E89D80: the signed distance of p from the triangle's plane
        float calculateDistanceToTriangle(const hkVector4f& p) const
        {
            const __m128 d = _mm_mul_ps(_mm_sub_ps(p.m_quad, m_edge[0].m_vertex->m_position.m_quad), m_normal.m_quad);
            const __m128 h = _mm_add_ps(_mm_add_ps(_mm_shuffle_ps(d, d, 0x55), _mm_shuffle_ps(d, d, 0x00)), _mm_shuffle_ps(d, d, 0xAA));
            return _mm_cvtss_f32(h);
        }

        // 0x00E89DC0
        // Visible when the point of view is further in front than the tolerance border (the nearest such distance is
        // kept); otherwise invisible, the border growing to cover it. Unknown when the border reaches the nearest
        // visible distance.
        TriangleState calcStateAndUpdateTolerance(const hkVector4f& pointOfView, hkPadSpu<float>& toleranceBorder, hkPadSpu<float>& previousMinimumDistance) const
        {
            const float d = calculateDistanceToTriangle(pointOfView);
            TriangleState state;
            if (d > toleranceBorder.val())
            {
                state = HKCD_VISIBLE;
                if (previousMinimumDistance.val() > d)
                {
                    previousMinimumDistance = d;
                }
            }
            else
            {
                const float border = d + HK_WINGED_EDGE_VISIBILITY_TOLERANCE_WIDTH;
                if (border > toleranceBorder.val())
                {
                    toleranceBorder = border;
                }
                state = HKCD_INVISIBLE;
            }
            if (!(toleranceBorder.val() < previousMinimumDistance.val()))
            {
                state = HKCD_UNKNOWN;
            }
            return state;
        }
        void draw(unsigned int, hkBool);

        hkVector4f m_normal; // 0x00
        Edge m_edge[3]; // 0x10
        float m_distanceToOrigin; // 0x40
        TriangleState m_state; // 0x44
    };

    // size 0x40
    struct alignas(16) Vertex
    {
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(Vertex);
        Vertex() {} // 0x00E89CA0

        hkVector4f m_position; // 0x00
        hkcdVertex m_pointAinA; // 0x10
        hkcdVertex m_pointBinX; // 0x20
        hkPadSpu<Edge*> m_referee; // 0x30
    };

    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkcdWingedEdge);
    void init(const hkcdVertex* AinA, const hkcdVertex* BinA, const hkVector4f* simplex); // 0x00E89F40
    static void codeBegin(); // 0x00F0CDF0
    static void codeEnd(); // 0x00F0CE00
    // 0x00E89AD0
    void initialiseVibisilityTolerance()
    {
        m_toleranceBorder = HK_WINGED_EDGE_VISIBILITY_TOLERANCE_WIDTH;
        m_previousMinimumDistance = HK_REAL_MAX;
    }
    Triangle* findTriangleOnNearestPlane(); // 0x00E89AF0
    Triangle* findTriangleNearestOrigin(Triangle* triangleOnNearestPlane); // 0x00F0CFC0
    WingedEdgeStatus findVisibilityBoundaryEdges(Triangle* initialTriangle, const hkVector4f& pointOfView, hkPadSpu<int>& chainLength, hkPadSpu<Edge*>& edgeChain); // 0x00E89E50
    WingedEdgeStatus patchInBoundaryToPointOfViewExpansion(int chainLength, Edge* edgeChain, Vertex* pointOfView); // 0x00E8A7D0
    hkBool hasFreeVertex() { return m_numVertices < HKCD_WINGED_EDGE_VERTICIES_LIMIT; } // 0x00E89BF0
    int numFreeTriangles() { return m_numFreeTriangle + HKCD_WINGED_EDGE_TRIANGLE_LIMIT - m_numTriangles; } // 0x00E89C00
    void draw(unsigned int);
    // 0x00E89C10
    Vertex* allocateVertex()
    {
        Vertex* v = &m_vertex[m_numVertices];
        m_numVertices += 1;
        v->m_referee = nullptr;
        return v;
    }
    static void find2ClosestMultiTriangles(hkcdWingedEdge& we0, hkcdWingedEdge& we1, hkVector4f& directionOut, Triangle*& nearestTriangle0, Triangle*& nearestTriangle1); // 0x00F0D1E0
public:
    WingedEdgeStatus findVisibleTriangles(Triangle* initialTriangle, const hkVector4f& pointOfView, hkPadSpu<int>& chainLength, hkPadSpu<Edge*>& edgeChain); // 0x00F0CE70
    void allocateTriangles(int numRequired, Triangle** newTrianglesBuffer); // 0x00E89C30
    WingedEdgeStatus initializePolytopeExpansion(Triangle** newTriangles, int numTriangles, Edge* edgeChain, Vertex*& v); // 0x00E8A5E0
    void resetTriangleAndVertexStates(); // 0x00E89BB0
    void addTetrahedron(); // 0x00F0D550
    void validate(); // 0x00E87F90
public:
    // hkcdWingedEdge 0x00E89CB0: implicit (the vertices' and triangles' trivial constructors)

    hkPadSpu<float> m_toleranceBorder; // 0x00
    hkPadSpu<float> m_previousMinimumDistance; // 0x04
    hkPadSpu<int> m_numVertices; // 0x08
    hkPadSpu<int> m_numFreeTriangle; // 0x0C
    hkPadSpu<int> m_numTriangles; // 0x10
    Vertex m_vertex[55]; // 0x20
    Triangle* m_freeTriangle[100]; // 0xDE0
    Triangle m_triangles[100]; // 0xF70
};
