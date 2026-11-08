// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/internal/geometryprocessing/convexhull/hkgpconvexhull.cpp
//
// Havok 2014.2.5 (havok_impl.md HK3): hkgpConvexHull and the hkgpConvexHullImpl functions compiled here in rfg.exe
// (reset, the domain, plane indexing, mass properties), only what rfg.exe reaches (hkpAccurateInertiaTensorComputer):
// building, expanding by a planar margin, the mass properties and their getters. The code follows the SSE operation
// order of the original.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/internal/geometryprocessing/convexhull/hkgpconvexhull.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/algorithm/sort/hksort.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/pointermap/hkmap.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/system/error/hkerror.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/geometry/hkstridedvertices.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/internal/geometryprocessing/convexhull/hkgpconvexhullinternals.h"

#include <intrin.h>
#include <string.h>

using namespace hkgpConvexHullSse;

namespace
{
typedef hkgpConvexHullImpl::Vertex ImplVertex;
typedef hkgpConvexHullImpl::Triangle ImplTriangle;
typedef hkgpConvexHullImpl::Edge ImplEdge;

inline float first(__m128 v) { return _mm_cvtss_f32(v); }

inline __m128 signMask() { return _mm_castsi128_ps(_mm_set1_epi32((int)0x80000000)); }

inline ImplVertex* impl(const hkgpConvexHull::Vertex* v) { return (ImplVertex*)v; }
inline ImplTriangle* impl(const hkgpConvexHull::Triangle* t) { return (ImplTriangle*)t; }

const char* const s_internalsFile = "Y:\\NightlyJobs\\20150629-01\\Source\\Common/Internal/GeometryProcessing/ConvexHull/hkgpConvexHullInternals.h";

// hkgpConvexHullImpl::mustHaveMassProperties 0x00FDD1E0: HK_ERROR(0x79f9d887, "No mass properties available (" <<
// uid << ") hkgpConvexHull::buildMassProperties() must be called first.") (the message text is not formatted here)
void mustHaveMassProperties(const hkgpConvexHullImpl* data)
{
    if (!data->m_hasMassProperties)
    {
        if (hkErrorFwd::messageError(0x79F9D887, "No mass properties available", s_internalsFile, 0x16E) != 0)
        {
            __debugbreak();
        }
    }
}
}

// 0x03BAC908
long hkgpConvexHullImpl::g_uidGenerator;

// ---- hkgpConvexHullImpl

// 0x00FE59E0
hkgpConvexHullImpl::hkgpConvexHullImpl()
{
    m_uid = (unsigned int)_InterlockedExchangeAdd(&g_uidGenerator, 1);
    reset(false);
}

// 0x00FE5A50 (the arrays, then the mesh: triangles, vertices)
hkgpConvexHullImpl::~hkgpConvexHullImpl()
{
    reset(false);
}

// 0x00FE0AB0
void hkgpConvexHullImpl::reset(bool keepPlane)
{
    m_mesh.reset();
    m_planes.clear();
    m_pendings.clear();
    m_supportCache.clear();
    m_centroids[0].m_quad = setW(_mm_setzero_ps(), constant(HK_QUADREAL_MAX));
    m_centroids[1].m_quad = setW(_mm_setzero_ps(), constant(HK_QUADREAL_MAX));
    for (int i = 0; i < 2; ++i)
    {
        m_boundingBoxes[i].m_min.m_quad = constant(HK_QUADREAL_MAX);
        m_boundingBoxes[i].m_max.m_quad = _mm_xor_ps(constant(HK_QUADREAL_MAX), signMask());
    }
    m_hasMassProperties = false;
    m_degeneratedMassProperties = false;
    m_hasIndexing = false;
    m_pid = 0;
    m_localTransform.m_rotation.m_col0.m_quad = constant(HK_QUADREAL_1000);
    m_localTransform.m_rotation.m_col1.m_quad = constant(HK_QUADREAL_0100);
    m_localTransform.m_rotation.m_col2.m_quad = constant(HK_QUADREAL_0010);
    m_localTransform.m_translation.m_quad = _mm_setzero_ps();
    m_inertia.m_quad = _mm_setzero_ps();
    m_inertiaMatrix.m_col0.m_quad = constant(HK_QUADREAL_1000);
    m_inertiaMatrix.m_col1.m_quad = constant(HK_QUADREAL_0100);
    m_inertiaMatrix.m_col2.m_quad = constant(HK_QUADREAL_0010);
    m_projectionAxis.m_quad = _mm_setzero_ps();
    m_volume.m_real = _mm_setzero_ps();
    m_surface.m_real = _mm_setzero_ps();
    m_dimensions = -1;
    m_scale.m_quad = _mm_setzero_ps();
    m_scaleInv.m_quad = _mm_setzero_ps();
    m_origin.m_quad = _mm_setzero_ps();
    if (!keepPlane)
    {
        m_projectionPlane.m_quad = _mm_setzero_ps();
    }
}

// 0x00FE0C70: maps the bounding box of the points to [-8191, 8191] (origin at its centre)
void hkgpConvexHullImpl::rebuildDomainParameters(const hkVector4f* points, const int numPoints)
{
    if (numPoints == 0)
    {
        return;
    }
    __m128 minimum = points[0].m_quad;
    __m128 maximum = points[0].m_quad;
    for (int i = 1; i < numPoints; ++i)
    {
        minimum = _mm_min_ps(minimum, points[i].m_quad);
        maximum = _mm_max_ps(maximum, points[i].m_quad);
    }
    const __m128 maxValue = constant(HK_QUADREAL_MAX);
    const int outside = _mm_movemask_ps(_mm_cmplt_ps(minimum, _mm_xor_ps(maxValue, signMask()))) | _mm_movemask_ps(_mm_cmplt_ps(maxValue, maximum));
    if (outside & 7)
    {
        // HK_ERROR(0x405a2174, "Input domain out of range")
        if (hkErrorFwd::messageError(0x405A2174, "Input domain out of range", s_internalsFile, 0x23C) != 0)
        {
            __debugbreak();
        }
    }
    const __m128 halfExtents = _mm_mul_ps(_mm_max_ps(_mm_sub_ps(maximum, minimum), _mm_set1_ps(2e-5f)), constant(HK_QUADREAL_INV_2)); // 0x014C0000
    const __m128 r = _mm_rcp_ps(halfExtents);
    const __m128 scale = setW(_mm_mul_ps(_mm_sub_ps(_mm_set1_ps(2.0f), _mm_mul_ps(r, halfExtents)), r), constant(HK_QUADREAL_1));
    m_origin.m_quad = clearW(_mm_add_ps(halfExtents, minimum));
    m_scale.m_quad = _mm_mul_ps(_mm_set1_ps(8191.0f), clearW(scale)); // DomainMaxBound
    const __m128 r2 = _mm_rcp_ps(m_scale.m_quad);
    m_scaleInv.m_quad = setW(_mm_mul_ps(_mm_sub_ps(_mm_set1_ps(2.0f), _mm_mul_ps(m_scale.m_quad, r2)), r2), constant(HK_QUADREAL_1));
}

// buildIndices<PositionAsTransform> 0x00FE6620: the plane equations (m_planes). In 3D the triangles are visited by
// decreasing area and each unassigned one starts a plane, flooded over the neighbours whose normal is within
// m_minCosAngle; a polygon gets one plane per edge (outward), a segment one per end.
void hkgpConvexHullImpl::buildIndices()
{
    if (m_hasIndexing)
    {
        return;
    }
    hkArray<SortedTriangle, hkContainerHeapAllocator> sorted;
    hkMap<unsigned long, unsigned long> planeOfTriangle;
    hkArray<hkVector4f, hkContainerHeapAllocator> trianglePlanes;
    const int numTriangles = m_mesh.m_triangles.getSize();
    if (numTriangles != 0)
    {
        if (numTriangles > 0)
        {
            sorted.reserveExactly(numTriangles);
        }
        for (Triangle* t = m_mesh.m_triangles.getFirst(); t; t = t->next())
        {
            SortedTriangle& s = sorted[sorted.getSize()];
            sorted.setSizeUnchecked(sorted.getSize() + 1);
            s.m_data = t;
            const __m128 a = transform(*t->m_vertices[0]);
            s.m_value = first(length3(cross(_mm_sub_ps(transform(*t->m_vertices[1]), a), _mm_sub_ps(transform(*t->m_vertices[2]), a))));
        }
        if (sorted.getSize() > 1)
        {
            hkAlgorithm::quickSortRecursive(sorted.begin(), 0, sorted.getSize() - 1, hkAlgorithm::less<SortedTriangle>());
        }
        planeOfTriangle.reserve(m_mesh.m_triangles.getSize());
        trianglePlanes.setSize(m_mesh.m_triangles.getSize());
        int index = 0;
        for (Triangle* t = m_mesh.m_triangles.getFirst(); t; t = t->next(), ++index)
        {
            planeOfTriangle.insert((unsigned long)t, (unsigned long)index);
            trianglePlanes[index].m_quad = planeFromTriangle(transform(*t->m_vertices[0]), transform(*t->m_vertices[1]), transform(*t->m_vertices[2]));
            t->m_index = index;
            t->m_plane.m_index = -1;
        }
    }

    int vertexIndex = 0;
    for (Vertex* v = m_mesh.m_vertices.getFirst(); v; v = v->next())
    {
        v->m_index = vertexIndex++;
    }

    m_planes.clear();
    if (m_dimensions == 3)
    {
        hkArray<Edge, hkContainerHeapAllocator> stack;
        for (int k = 0; k < sorted.getSize(); ++k)
        {
            Triangle* seed = sorted[k].m_data;
            if (seed->m_plane.m_index != -1)
            {
                continue;
            }
            const __m128 reference = trianglePlanes[(int)planeOfTriangle.getWithDefault((unsigned long)seed, (unsigned long)-1)].m_quad;
            seed->m_plane.m_index = m_planes.getSize();
            hkVector4f plane;
            plane.m_quad = reference;
            m_planes.pushBack(plane);
            stack.clear();
            stack.pushBack(Edge(seed->m_links[0]));
            stack.pushBack(Edge(seed->m_links[1]));
            stack.pushBack(Edge(seed->m_links[2]));
            while (stack.getSize() != 0)
            {
                const Edge e = stack.back();
                stack.popBack();
                Triangle* t = e.m_triangle;
                if (t->m_plane.m_index != -1)
                {
                    continue;
                }
                const __m128 p = trianglePlanes[(int)planeOfTriangle.getWithDefault((unsigned long)t, (unsigned long)-1)].m_quad;
                // a degenerate (invalid) plane always merges
                bool merge = true;
                if ((_mm_movemask_ps(_mm_cmpord_ps(p, p)) & 7) == 7 && !(first(absolute(_mm_sub_ps(dot3(p, p), constant(HK_QUADREAL_1)))) >= 1e-4f)) // 0x38D1B717
                {
                    const float d = first(dot3(reference, p));
                    merge = d >= m_config.m_minCosAngle; // x87 fcomip: NaN does not merge
                }
                if (merge)
                {
                    t->m_plane.m_index = seed->m_plane.m_index;
                    stack.pushBack(Edge(t->m_links[Edge::ip1mod3(e.m_index)]));
                    stack.pushBack(Edge(t->m_links[Edge::ip2mod3(e.m_index)]));
                }
            }
        }
    }
    else if (m_dimensions == 2)
    {
        const __m128 centroid = getCentroid<PositionAsTransform>().m_quad;
        Vertex* prev = m_mesh.m_vertices.getFirst();
        while (prev && prev->next())
        {
            prev = prev->next();
        }
        for (Vertex* v = m_mesh.m_vertices.getFirst(); v; prev = v, v = v->next())
        {
            const __m128 a = transform(*prev);
            const __m128 n = normalize3IfNotZero(cross(m_projectionPlane.m_quad, _mm_sub_ps(transform(*v), a)));
            const __m128 plane = setW(n, _mm_sub_ps(_mm_setzero_ps(), dot3(n, a)));
            // flip it unless the centroid is behind: (w + y) + (z + x) of the plane times (centroid, 1)
            const __m128 s = setW(_mm_mul_ps(plane, centroid), plane);
            const __m128 t = _mm_add_ps(_mm_shuffle_ps(s, s, 0x4E), s);
            const __m128 distance = _mm_add_ps(_mm_shuffle_ps(t, t, 0xB1), t);
            const __m128i positive = _mm_cmpeq_epi32(_mm_srai_epi32(_mm_castps_si128(distance), 31), _mm_setzero_si128());
            hkVector4f out;
            out.m_quad = _mm_xor_ps(_mm_castsi128_ps(_mm_slli_epi32(_mm_srli_epi32(positive, 31), 31)), plane);
            m_planes.pushBack(out);
        }
    }
    else if (m_dimensions == 1)
    {
        Vertex* last = m_mesh.m_vertices.getFirst();
        const __m128 a = transform(*last);
        while (last && last->next())
        {
            last = last->next();
        }
        const __m128 b = transform(*last);
        const __m128 n = normalize3(_mm_sub_ps(b, a));
        hkVector4f plane;
        plane.m_quad = setW(n, _mm_sub_ps(_mm_setzero_ps(), dot3(b, n)));
        m_planes.pushBack(plane);
        const __m128 n2 = _mm_xor_ps(plane.m_quad, signMask());
        plane.m_quad = setW(n2, _mm_sub_ps(_mm_setzero_ps(), dot3(a, n2)));
        m_planes.pushBack(plane);
    }

    if (m_config.m_ensurePlaneEnclosing.m_bool)
    {
        for (Vertex* v = m_mesh.m_vertices.getFirst(); v; v = v->next())
        {
            for (int i = 0; i < m_planes.getSize(); ++i)
            {
                const __m128 p = m_planes[i].m_quad;
                const __m128 d = _mm_add_ps(dot3(v->m_source.m_quad, p), splatW(p));
                if (first(d) > 0.0f)
                {
                    m_planes[i].m_quad = setW(p, _mm_sub_ps(splatW(p), d));
                }
            }
        }
    }
    if (m_planes.getSize() != 0 && m_planes.getSize() < m_planes.getCapacity())
    {
        hkArrayUtil::_reduce(hkContainerHeapAllocator::s_alloc, &m_planes, sizeof(hkVector4f), nullptr, m_planes.getSize());
    }
    m_hasIndexing = true;
}

// buildMassProperties<PositionAsTransform> 0x00FE8000: volume, surface, centre of mass and inertia (principal axes in
// m_localTransform) of the solid hull; a polygon gets its area and centroid; lower dimensions get zeros.
hkResult hkgpConvexHullImpl::buildMassProperties()
{
    if (m_hasMassProperties)
    {
        return hkResult(HK_SUCCESS);
    }
    m_degeneratedMassProperties = false;
    if (m_dimensions == 3)
    {
        const __m128 centroid = getCentroid<PositionAsTransform>().m_quad;
        __m128 boxMin = constant(HK_QUADREAL_MAX);
        __m128 boxMax = _mm_xor_ps(constant(HK_QUADREAL_MAX), signMask());
        __m128 volume6 = _mm_setzero_ps();
        __m128 comSum = _mm_setzero_ps();
        __m128 surface2 = _mm_setzero_ps();
        float diag[4] = { 0.0f, 0.0f, 0.0f, 0.0f }; // the x^2, y^2, z^2 integrals
        float products[4] = { 0.0f, 0.0f, 0.0f, 0.0f }; // the yz, zx, xy integrals
        // value * v + acc[i] in one lane
        auto accumulate = [](float* acc, int i, __m128 value, __m128 v) { acc[i] = first(_mm_add_ps(_mm_mul_ps(value, v), _mm_set1_ps(acc[i]))); };
        for (Triangle* t = m_mesh.m_triangles.getFirst(); t; t = t->next())
        {
            const __m128 a = _mm_sub_ps(transform(*t->m_vertices[0]), centroid);
            const __m128 b = _mm_sub_ps(transform(*t->m_vertices[1]), centroid);
            const __m128 d = _mm_sub_ps(transform(*t->m_vertices[2]), centroid);
            boxMin = _mm_min_ps(_mm_min_ps(_mm_min_ps(boxMin, a), b), d);
            boxMax = _mm_max_ps(_mm_max_ps(_mm_max_ps(boxMax, a), b), d);
            const __m128 n = cross(b, d);
            const __m128 v = dot3(n, a);
            volume6 = _mm_add_ps(v, volume6);
            comSum = _mm_add_ps(_mm_mul_ps(_mm_add_ps(_mm_add_ps(b, a), d), v), comSum);
            surface2 = _mm_add_ps(length3(_mm_add_ps(cross(a, _mm_sub_ps(b, d)), n)), surface2);

            // transposed: X = (a.x, b.x, d.x, d.x), ...
            const __m128 ab01 = _mm_shuffle_ps(a, b, 0x44);
            const __m128 ab23 = _mm_shuffle_ps(a, b, 0xEE);
            const __m128 dd01 = _mm_shuffle_ps(d, d, 0x44);
            const __m128 dd23 = _mm_shuffle_ps(d, d, 0xEE);
            const __m128 X = _mm_shuffle_ps(ab01, dd01, 0x88);
            const __m128 Y = _mm_shuffle_ps(ab01, dd01, 0xDD);
            const __m128 Z = _mm_shuffle_ps(ab23, dd23, 0x88);
            const __m128 sumX = horizontalAdd3(X);
            const __m128 sumY = horizontalAdd3(Y);
            const __m128 sumZ = horizontalAdd3(Z);
            accumulate(diag, 0, horizontalAdd3(_mm_mul_ps(_mm_add_ps(_mm_shuffle_ps(X, X, 0xC9), X), X)), v);
            accumulate(products, 0, _mm_add_ps(horizontalAdd3(_mm_mul_ps(Z, Y)), _mm_mul_ps(sumZ, sumY)), v);
            accumulate(diag, 1, horizontalAdd3(_mm_mul_ps(_mm_add_ps(_mm_shuffle_ps(Y, Y, 0xC9), Y), Y)), v);
            accumulate(products, 1, _mm_add_ps(horizontalAdd3(_mm_mul_ps(Z, X)), _mm_mul_ps(sumX, sumZ)), v);
            accumulate(diag, 2, horizontalAdd3(_mm_mul_ps(_mm_add_ps(_mm_shuffle_ps(Z, Z, 0xC9), Z), Z)), v);
            accumulate(products, 2, _mm_add_ps(horizontalAdd3(_mm_mul_ps(Y, X)), _mm_mul_ps(sumX, sumY)), v);
        }
        const __m128 inv = _mm_div_ps(constant(HK_QUADREAL_1), volume6);
        const __m128 comOffset = _mm_mul_ps(_mm_mul_ps(constant(HK_QUADREAL_INV_4), inv), comSum);
        const __m128 com = _mm_add_ps(centroid, comOffset);
        m_localTransform.m_translation.m_quad = com;

        if (m_config.m_checkForDegeneratedMassProperties.m_bool)
        {
            __m128 maxDistance = _mm_setzero_ps();
            for (Triangle* t = m_mesh.m_triangles.getFirst(); t; t = t->next())
            {
                const __m128 plane = planeFromTriangle(transform(*t->m_vertices[0]), transform(*t->m_vertices[1]), transform(*t->m_vertices[2]));
                maxDistance = _mm_max_ps(maxDistance, _mm_add_ps(dot3(com, plane), splatW(plane)));
            }
            if (first(maxDistance) > 0.0f)
            {
                m_degeneratedMassProperties = true;
            }
            if (m_hasIndexing && m_planes.getSize() > 0)
            {
                maxDistance = _mm_setzero_ps();
                for (int i = 0; i < m_planes.getSize(); ++i)
                {
                    const __m128 plane = m_planes[i].m_quad;
                    maxDistance = _mm_max_ps(maxDistance, _mm_add_ps(dot3(com, plane), splatW(plane)));
                }
            }
            if (first(maxDistance) > 0.0f)
            {
                m_degeneratedMassProperties = true;
            }
        }

        m_surface.m_real = _mm_mul_ps(surface2, constant(HK_QUADREAL_INV_2));
        const __m128 extents = _mm_sub_ps(boxMax, boxMin);
        const __m128 maxExtent = _mm_max_ps(splatZ(extents), _mm_max_ps(splatY(extents), splatX(extents)));
        const __m128 maxExtent2 = _mm_mul_ps(maxExtent, maxExtent);
        const __m128 maxExtent3 = _mm_mul_ps(maxExtent2, maxExtent);
        const __m128 volume = _mm_mul_ps(volume6, constant(HK_QUADREAL_INV_6));
        m_volume.m_real = volume;
        if (!(first(volume) > first(_mm_mul_ps(maxExtent3, constant(HK_QUADREAL_EPS)))))
        {
            // flat
            m_localTransform.m_rotation.m_col0.m_quad = constant(HK_QUADREAL_1000);
            m_localTransform.m_rotation.m_col1.m_quad = constant(HK_QUADREAL_0100);
            m_localTransform.m_rotation.m_col2.m_quad = constant(HK_QUADREAL_0010);
            m_degeneratedMassProperties = true;
            m_volume.m_real = _mm_setzero_ps();
            m_inertiaMatrix.m_col0.m_quad = _mm_setzero_ps();
            m_inertiaMatrix.m_col1.m_quad = _mm_setzero_ps();
            m_inertiaMatrix.m_col2.m_quad = _mm_setzero_ps();
            m_inertia.m_quad = _mm_setzero_ps();
            m_hasMassProperties = true;
            return hkResult(HK_SUCCESS);
        }

        // the inertia about the centroid (density 1 / volume), then moved to the centre of mass
        const __m128 s = _mm_mul_ps(_mm_mul_ps(_mm_set1_ps(0.1f), inv), _mm_loadu_ps(diag)); // 0x3DCCCCCD
        const __m128 diagonal = clearW(_mm_add_ps(_mm_shuffle_ps(s, s, 0x5A), _mm_shuffle_ps(s, s, 0x41)));
        const __m128 p = _mm_mul_ps(_mm_mul_ps(_mm_set1_ps(-0.05f), inv), _mm_loadu_ps(products)); // 0xBD4CCCCD
        hkMatrix3f inertia;
        inertia.m_col0.m_quad = select(laneMask(9), diagonal, _mm_shuffle_ps(p, p, 0x5A));
        inertia.m_col1.m_quad = select(laneMask(10), diagonal, _mm_shuffle_ps(p, p, 0x4E));
        inertia.m_col2.m_quad = _mm_shuffle_ps(_mm_shuffle_ps(p, p, 0xE1), diagonal, 0xE4);
        hkVector4f r;
        r.m_quad = comOffset;
        hkMatrix3f skewR;
        skewR.setCrossSkewSymmetric(r);
        hkMatrix3f skewZero;
        skewZero.setCrossSkewSymmetric(*(const hkVector4f*)&g_vectorfConstants[HK_QUADREAL_0]);
        hkMatrix3f shift;
        shift.setMul(skewR, skewR);
        hkMatrix3f zero2;
        zero2.setMul(skewZero, skewZero);
        zero2.transpose();
        shift.sub(zero2);
        hkSimdFloat32 one;
        one.m_real = constant(HK_QUADREAL_1);
        inertia.addMul(one, shift);

        hkRotationf eigenVectors;
        hkVector4f eigenValues;
        if (inertia.diagonalizeSymmetric(eigenVectors, eigenValues, 128, 1.1920929e-07f).m_enum == HK_FAILURE) // 0x34000000
        {
            m_degeneratedMassProperties = true;
            eigenVectors.m_col0.m_quad = constant(HK_QUADREAL_1000);
            eigenVectors.m_col1.m_quad = constant(HK_QUADREAL_0100);
            eigenVectors.m_col2.m_quad = constant(HK_QUADREAL_0010);
            eigenValues.m_quad = constant(HK_QUADREAL_1);
            // HK_WARN(0x45465, "Failed to diagonalize inertia matrix (" << m_uid << ")")
            hkErrorFwd::messageWarning(0x45465, "Failed to diagonalize inertia matrix", s_internalsFile, 0x7CB);
        }
        eigenVectors.transpose();
        eigenVectors.renormalize();
        m_inertiaMatrix = inertia;
        inertia.changeBasis(eigenVectors);
        __m128 principal = select(laneMask(2), inertia.m_col1.m_quad, inertia.m_col0.m_quad);
        principal = select(laneMask(4), inertia.m_col2.m_quad, principal);
        m_inertia.m_quad = principal;
        eigenVectors.transpose();
        m_localTransform.m_rotation = eigenVectors;

        const __m128 absDiagonal = absolute(diagonal);
        const __m128 minDiagonal = _mm_min_ps(splatZ(absDiagonal), _mm_min_ps(splatY(absDiagonal), splatX(absDiagonal)));
        const __m128 threshold = _mm_mul_ps(_mm_mul_ps(maxExtent2, constant(HK_QUADREAL_EPS)), _mm_mul_ps(maxExtent3, constant(HK_QUADREAL_INV_6)));
        if (!(first(_mm_mul_ps(minDiagonal, volume)) > first(threshold)))
        {
            m_degeneratedMassProperties = true;
        }
        m_hasMassProperties = true;
    }
    else if (m_dimensions == 2)
    {
        hkArray<hkVector4f, hkContainerHeapAllocator> positions;
        const int numVertices = m_mesh.m_vertices.getSize();
        if (numVertices > 0)
        {
            positions.reserveExactly(numVertices);
        }
        positions.setSizeUnchecked(numVertices);
        int i = 0;
        for (Vertex* v = m_mesh.m_vertices.getFirst(); v; v = v->next())
        {
            positions[i++].m_quad = transform(*v);
        }
        __m128 area2 = _mm_setzero_ps();
        __m128 comSum = _mm_setzero_ps();
        const int count = positions.getSize() - 1;
        if (count > 1)
        {
            const __m128 plane = m_projectionPlane.m_quad;
            const __m128 o = positions[0].m_quad;
            for (int k = 1; k < count; ++k)
            {
                const __m128 a = _mm_sub_ps(positions[k].m_quad, o);
                const __m128 b = _mm_sub_ps(positions[k + 1].m_quad, o);
                const __m128 area = absolute(dot3(cross(a, b), plane));
                comSum = _mm_add_ps(comSum, _mm_mul_ps(_mm_mul_ps(area, constant(HK_QUADREAL_INV_3)), _mm_add_ps(b, a)));
                area2 = _mm_add_ps(area2, area);
            }
        }
        m_hasMassProperties = true;
        m_surface.m_real = _mm_mul_ps(area2, constant(HK_QUADREAL_INV_2));
        m_inertiaMatrix.m_col0.m_quad = _mm_move_ss(m_inertiaMatrix.m_col0.m_quad, constant(HK_QUADREAL_1));
        m_inertia.m_quad = constant(HK_QUADREAL_1000);
        m_localTransform.m_translation.m_quad = _mm_add_ps(_mm_mul_ps(_mm_div_ps(constant(HK_QUADREAL_1), area2), comSum), positions[0].m_quad);
    }
    else
    {
        m_hasMassProperties = true;
        m_surface.m_real = _mm_setzero_ps();
        m_volume.m_real = _mm_setzero_ps();
        m_inertiaMatrix.m_col0.m_quad = constant(HK_QUADREAL_1000);
        m_inertiaMatrix.m_col1.m_quad = constant(HK_QUADREAL_0100);
        m_inertiaMatrix.m_col2.m_quad = constant(HK_QUADREAL_0010);
        m_inertia.m_quad = _mm_setzero_ps();
        m_localTransform.m_rotation.m_col0.m_quad = constant(HK_QUADREAL_1000);
        m_localTransform.m_rotation.m_col1.m_quad = constant(HK_QUADREAL_0100);
        m_localTransform.m_rotation.m_col2.m_quad = constant(HK_QUADREAL_0010);
        m_localTransform.m_translation.m_quad = _mm_setzero_ps();
    }
    return hkResult(m_hasMassProperties ? HK_SUCCESS : HK_FAILURE);
}

// ---- hkgpConvexHull

// 0x00FD1170
hkgpConvexHull::BuildConfig::BuildConfig()
{
    m_allowLowerDimensions = false;
    m_alwaysComputeProjectionPlane = false;
    m_minCosAngle = 0.99999f; // 0x3F7FFF58 (HKGPCONVEXHULL_DEFAULT_MIN_COS_ANGLE)
    m_setSourceIndices = false;
    m_buildIndices = true;
    m_buildMassProperties = false;
    m_sortInputs = true;
    m_internalInputs = false;
    m_checkForDegeneratedMassProperties = false;
    m_ensurePlaneEnclosing = false;
}

// 0x00FD4520
hkgpConvexHull::hkgpConvexHull()
{
    m_data = new hkgpConvexHullImpl();
    m_data->m_hull = this;
    m_userData = nullptr;
    m_userObject = nullptr;
}

// 0x00FD5A40
hkgpConvexHull::~hkgpConvexHull()
{
    if (m_data)
    {
        delete m_data;
    }
    deleteUserObject();
}

// 0x00FD1200
void hkgpConvexHull::deleteUserObject()
{
    if (m_userObject)
    {
        delete m_userObject;
    }
    m_userObject = nullptr;
}

// 0x00FD1250
int hkgpConvexHull::build(const hkVector4f* points, int numPoints, const BuildConfig& config)
{
    memcpy(&m_data->m_config, &config, sizeof(BuildConfig)); // two movq (the padding too)
    return m_data->build(points, numPoints, false, nullptr);
}

// 0x00FD4620: strided or unaligned points are copied (xyz, w = 0)
int hkgpConvexHull::build(const hkStridedVertices& points, const BuildConfig& config)
{
    if (points.m_striding == 0x10 && ((hkUlong)points.m_vertices & 0xF) == 0)
    {
        return build((const hkVector4f*)points.m_vertices, points.m_numVertices, config);
    }
    hkArray<hkVector4f, hkContainerHeapAllocator> copy;
    const int n = points.m_numVertices;
    if (n > 0)
    {
        copy.reserveExactly(n);
    }
    copy.setSizeUnchecked(n);
    const char* src = (const char*)points.m_vertices;
    for (int i = 0; i < points.m_numVertices; ++i, src += points.m_striding)
    {
        const float* f = (const float*)src;
        copy[i].m_quad = clearW(_mm_movelh_ps(_mm_castpd_ps(_mm_load_sd((const double*)f)), _mm_load_ss(f + 2)));
    }
    return build(n != 0 ? copy.begin() : nullptr, n, config);
}

// 0x00FD5900
void hkgpConvexHull::fetchPositions(Inputs inputs, hkArray<hkVector4f, hkContainerHeapAllocator>& positions) const
{
    const hkgpConvexHullImpl* data = m_data;
    const int n = data->m_mesh.m_vertices.getSize();
    const int start = positions.getSize();
    positions.reserve(start + n);
    positions.setSizeUnchecked(start + n);
    hkVector4f* out = positions.begin() + start;
    for (const ImplVertex* v = data->m_mesh.m_vertices.getFirst(); v; v = v->next(), ++out)
    {
        out->m_quad = inputs == SOURCE_VERTICES ? v->m_source.m_quad : data->transform(*v);
    }
}

// 0x00FD5A90
void hkgpConvexHull::buildIndices(float minCosAngle, hkBool forceRebuild)
{
    if (forceRebuild.m_bool)
    {
        m_data->m_hasIndexing = false;
    }
    m_data->m_config.m_minCosAngle = minCosAngle;
    m_data->buildIndices();
}

// 0x00FD5AC0
hkResult hkgpConvexHull::buildMassProperties()
{
    return m_data->buildMassProperties();
}

int hkgpConvexHull::getDimensions() const { return m_data->m_dimensions; } // 0x00FD1320
const hkVector4f& hkgpConvexHull::getProjectionPlane() const { return m_data->m_projectionPlane; } // 0x00FD1360
int hkgpConvexHull::getNumVertices() const { return m_data->m_mesh.m_vertices.getSize(); } // 0x00FD18E0
hkgpConvexHull::Vertex* hkgpConvexHull::getFirstVertex() const { return (Vertex*)m_data->m_mesh.m_vertices.getFirst(); } // 0x00FD18F0
hkgpConvexHull::Vertex* hkgpConvexHull::getNext(const Vertex* vertex) const { return (Vertex*)impl(vertex)->next(); } // 0x00FD1900
int hkgpConvexHull::getNumTriangles() const { return m_data->m_mesh.m_triangles.getSize(); } // 0x00FD1910
hkgpConvexHull::Triangle* hkgpConvexHull::getFirstTriangle() const { return (Triangle*)m_data->m_mesh.m_triangles.getFirst(); } // 0x00FD1920
hkgpConvexHull::Triangle* hkgpConvexHull::getNext(const Triangle* triangle) const { return (Triangle*)impl(triangle)->next(); } // 0x00FD1930
hkgpConvexHull::Vertex* hkgpConvexHull::getVertex(const Triangle* triangle, int index) const { return (Vertex*)impl(triangle)->m_vertices[index]; } // 0x00FD1940
const hkVector4f& hkgpConvexHull::getSourcePosition(const Vertex* vertex) const { return impl(vertex)->m_source; } // 0x00FD1410
const hkVector4f& hkgpConvexHull::getCentroid() const { return m_data->getCentroid<hkgpConvexHullImpl::PositionAsSource>(); } // 0x00FD2490

// 0x00FD1560
hkSimdFloat32 hkgpConvexHull::getVolume() const
{
    mustHaveMassProperties(m_data);
    return m_data->m_volume;
}

// 0x00FD1610
hkSimdFloat32 hkgpConvexHull::getSurfaceArea() const
{
    mustHaveMassProperties(m_data);
    return m_data->m_surface;
}

// 0x00FD16C0
const hkMatrix3f& hkgpConvexHull::getWorldInertia() const
{
    mustHaveMassProperties(m_data);
    return m_data->m_inertiaMatrix;
}

// 0x00FD1950
const hkVector4f& hkgpConvexHull::getCenterOfMass() const
{
    mustHaveMassProperties(m_data);
    return m_data->m_localTransform.m_translation;
}

// 0x00FD2B50: the plane of the source positions (normalize<3>, not the "if not zero" form)
hkVector4f hkgpConvexHull::getPlaneEquation(const Triangle* triangle) const
{
    const __m128 a = getSourcePosition(getVertex(triangle, 0)).m_quad;
    const __m128 e1 = _mm_sub_ps(getSourcePosition(getVertex(triangle, 1)).m_quad, a);
    const __m128 e2 = _mm_sub_ps(getSourcePosition(getVertex(triangle, 2)).m_quad, a);
    const __m128 n = normalize3(cross(e1, e2));
    hkVector4f plane;
    plane.m_quad = setW(n, _mm_sub_ps(_mm_setzero_ps(), dot3(a, n)));
    return plane;
}

// 0x00FD5E90: rebuilds the hull from its source points pushed out by the margin (a point: an octahedron of half the
// margin; a segment: eight points around both ends at half the margin; a polygon: both sides of the projection plane,
// falling back to the centroid; a polyhedron: the triangle vertices moved along their plane normals). The indices
// and mass properties are rebuilt if they were there. True when the result is 3D.
hkBool hkgpConvexHull::expandByPlanarMargin(const float margin)
{
    const __m128 halfMargin = _mm_mul_ps(_mm_set1_ps(margin), constant(HK_QUADREAL_INV_2));
    const BuildConfig config = m_data->m_config;
    const bool hadIndexing = m_data->m_hasIndexing;
    const bool hadMassProperties = m_data->m_hasMassProperties;
    hkArray<hkVector4f, hkContainerHeapAllocator> points;
    bool rebuild = true;
    hkVector4f p;
    switch (getDimensions())
    {
    case 0:
    {
        const __m128 o = m_data->m_mesh.m_vertices.getFirst()->m_source.m_quad;
        points.reserveExactly(6);
        points.setSizeUnchecked(6);
        const __m128 minusHalf = _mm_sub_ps(_mm_setzero_ps(), halfMargin);
        points[0].m_quad = _mm_add_ps(_mm_mul_ps(halfMargin, constant(HK_QUADREAL_1000)), o);
        points[1].m_quad = _mm_add_ps(_mm_mul_ps(minusHalf, constant(HK_QUADREAL_1000)), o);
        points[2].m_quad = _mm_add_ps(_mm_mul_ps(halfMargin, constant(HK_QUADREAL_0100)), o);
        points[3].m_quad = _mm_add_ps(_mm_mul_ps(minusHalf, constant(HK_QUADREAL_0100)), o);
        points[4].m_quad = _mm_add_ps(_mm_mul_ps(halfMargin, constant(HK_QUADREAL_0010)), o);
        points[5].m_quad = _mm_add_ps(_mm_mul_ps(minusHalf, constant(HK_QUADREAL_0010)), o);
        break;
    }

    case 1:
    {
        points.reserveExactly(10);
        fetchPositions(SOURCE_VERTICES, points);
        const __m128 a = points[0].m_quad;
        const __m128 b = points[1].m_quad;
        const __m128 d = _mm_sub_ps(b, a);
        // a vector perpendicular to d: (-z, 0, x) or (-y, x, 0), whichever is longer
        const __m128 d0 = clearW(d);
        const __m128 zx = _mm_shuffle_ps(d0, d0, 0xCE);
        const __m128 sq = _mm_mul_ps(d, d);
        const __m128 zx2 = _mm_mul_ps(zx, zx);
        const __m128 s3 = _mm_add_ps(_mm_shuffle_ps(zx2, zx2, 0x4E), zx2);
        const __m128 lenXY = _mm_add_ps(splatY(sq), splatX(sq));
        const __m128 lenZX = _mm_add_ps(_mm_shuffle_ps(s3, s3, 0xB1), s3);
        const __m128 signX = _mm_castsi128_ps(_mm_set_epi32(0, 0, 0, (int)0x80000000));
        const __m128 perp = select(_mm_cmplt_ps(lenXY, lenZX), _mm_xor_ps(signX, zx), _mm_xor_ps(_mm_shuffle_ps(d0, d0, 0xF1), signX));
        const __m128 c2 = cross(d, perp);
        const __m128 u = _mm_mul_ps(_mm_mul_ps(invSqrt(dot3(perp, perp)), perp), halfMargin);
        const __m128 v = _mm_mul_ps(_mm_mul_ps(invSqrt(dot3(c2, c2)), c2), halfMargin);
        const __m128 pushed[8] = { _mm_add_ps(u, a), _mm_sub_ps(a, u), _mm_add_ps(v, a), _mm_sub_ps(a, v), _mm_add_ps(u, b), _mm_sub_ps(b, u), _mm_add_ps(v, b), _mm_sub_ps(b, v) };
        for (int i = 0; i < 8; ++i)
        {
            p.m_quad = pushed[i];
            points.pushBack(p);
        }
        break;
    }

    case 2:
    {
        const __m128 centroid = getCentroid().m_quad;
        const __m128 plane = getProjectionPlane().m_quad;
        points.reserve(getNumVertices() * 3);
        fetchPositions(SOURCE_VERTICES, points);
        const __m128 m = _mm_set1_ps(margin);
        const __m128 offset = _mm_mul_ps(plane, m);
        const __m128 minusM = _mm_sub_ps(_mm_setzero_ps(), m);
        for (Vertex* v = getFirstVertex(); v; v = getNext(v))
        {
            const __m128 s = getSourcePosition(v).m_quad;
            p.m_quad = _mm_add_ps(s, offset);
            points.pushBack(p);
            p.m_quad = _mm_add_ps(_mm_mul_ps(minusM, plane), s);
            points.pushBack(p);
        }
        hkStridedVertices strided;
        strided.m_vertices = (const float*)points.begin();
        strided.m_numVertices = points.getSize();
        strided.m_striding = 0x10;
        if (build(strided, config) != 3)
        {
            // flat: start again from the centroid
            points.clear();
            p.m_quad = centroid;
            points.pushBack(p);
            strided.m_vertices = (const float*)points.begin();
            strided.m_numVertices = points.getSize();
            build(strided, config);
            return expandByPlanarMargin(margin);
        }
        rebuild = false;
        break;
    }

    case 3:
    {
        const int n = getNumTriangles() * 6;
        if (n > 0)
        {
            points.reserveExactly(n);
        }
        fetchPositions(SOURCE_VERTICES, points);
        const __m128 m = _mm_set1_ps(margin);
        for (Triangle* t = getFirstTriangle(); t; t = getNext(t))
        {
            const __m128 plane = getPlaneEquation(t).m_quad;
            if ((_mm_movemask_ps(_mm_cmpord_ps(plane, plane)) & 7) == 7 && !(first(absolute(_mm_sub_ps(dot3(plane, plane), constant(HK_QUADREAL_1)))) >= 1e-4f)) // 0x38D1B717
            {
                const __m128 offset = _mm_mul_ps(plane, m);
                for (int i = 0; i < 3; ++i)
                {
                    p.m_quad = _mm_add_ps(getSourcePosition(getVertex(t, i)).m_quad, offset);
                    points.pushBack(p);
                }
            }
        }
        break;
    }

    default:
        // HK_ERROR(0x172cdf48, "Invalid dimemsions for that operation")
        if (hkErrorFwd::messageError(0x172CDF48, "Invalid dimemsions for that operation", "GeometryProcessing\\ConvexHull\\hkgpConvexHull.cpp", 0x544) != 0)
        {
            __debugbreak();
        }
        rebuild = false;
        break;
    }
    if (rebuild)
    {
        hkStridedVertices strided;
        strided.m_vertices = (const float*)points.begin();
        strided.m_numVertices = points.getSize();
        strided.m_striding = 0x10;
        build(strided, config);
    }
    points.clearAndDeallocate();

    if (getDimensions() == 3)
    {
        if (hadIndexing)
        {
            buildIndices(0.99999f, hkBool(false)); // 0x3F7FFF58
        }
        if (hadMassProperties)
        {
            buildMassProperties();
        }
        return hkBool(true);
    }
    return hkBool(false);
}
