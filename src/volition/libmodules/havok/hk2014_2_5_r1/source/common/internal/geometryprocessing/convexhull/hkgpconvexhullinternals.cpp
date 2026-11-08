// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/internal/geometryprocessing/convexhull/hkgpconvexhullinternals.cpp
//
// Havok 2014.2.5 (havok_impl.md HK3): building the convex hull of a point set, from rfg.exe. The points are quantised
// to integers in [-8192, 8191] over their bounding box; the 3D hull is incremental (first tetrahedron, then each
// pending point removes the triangles it sees and is joined to their horizon) with exact 64-bit plane tests. The
// planar hull is Andrew's monotone chain on the two coordinates that remain after dropping the dominant axis of the
// projection plane.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/internal/geometryprocessing/convexhull/hkgpconvexhullinternals.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/algorithm/sort/hksort.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/system/error/hkerror.h"

#include <string.h>

using namespace hkgpConvexHullSse;

namespace
{
typedef hkgpConvexHullImpl::Vertex Vertex;
typedef hkgpConvexHullImpl::Triangle Triangle;
typedef hkgpConvexHullImpl::Edge Edge;

inline unsigned int ip1mod3(unsigned int i) { return Edge::ip1mod3(i); }
inline unsigned int ip2mod3(unsigned int i) { return Edge::ip2mod3(i); }
inline float first(__m128 v) { return _mm_cvtss_f32(v); }
inline hkgpTopology::Uid uid(Triangle* t, unsigned int i) { return hkgpTopology::packUid(t, i); }

// The index (hkVector4Comparison_maskToLastIndex) of the largest absolute component of the plane normal; the last one
// wins a tie.
int majorAxis(__m128 plane)
{
    const __m128 a = absolute(plane);
    const __m128 m = _mm_max_ps(splatZ(a), _mm_max_ps(splatY(a), splatX(a)));
    const int mask = _mm_movemask_ps(_mm_and_ps(_mm_cmple_ps(m, a), laneMask(7)));
    return hkVector4Comparison_maskToLastIndex[mask];
}

// Moves the coordinate of the major axis to m_x[2] (and back: the swap is its own inverse).
void swapMajorAxis(hkArray<Vertex, hkContainerHeapAllocator>& vertices, int count, int axis)
{
    if (axis == 0)
    {
        for (int i = 0; i < count; ++i)
        {
            const int t = vertices[i].m_x[0];
            vertices[i].m_x[0] = vertices[i].m_x[2];
            vertices[i].m_x[2] = t;
        }
    }
    else if (axis == 1)
    {
        for (int i = 0; i < count; ++i)
        {
            const int t = vertices[i].m_x[1];
            vertices[i].m_x[1] = vertices[i].m_x[2];
            vertices[i].m_x[2] = t;
        }
    }
}

// The 2D orientation of p relative to a -> b (32-bit integer products).
inline int orientation(const Vertex& a, const Vertex& b, const Vertex& p)
{
    return (b.m_x[0] - a.m_x[0]) * (p.m_x[1] - a.m_x[1]) - (b.m_x[1] - a.m_x[1]) * (p.m_x[0] - a.m_x[0]);
}

// Grows a vertex array to n, the new elements default constructed (hkArray of a non POD type).
void setVertexArraySize(hkArray<Vertex, hkContainerHeapAllocator>& a, int n)
{
    a.reserve(n);
    for (int i = a.getSize(); i < n; ++i)
    {
        new (&a[i]) Vertex();
    }
    a.setSizeUnchecked(n);
}

inline hkInt64 distanceToPlane(const Vertex& v, const hkgpConvexHullImpl::Plane& p)
{
    return (hkInt64)p.m_c[0] * v.m_x[0] + (hkInt64)p.m_c[1] * v.m_x[1] + (hkInt64)p.m_c[2] * v.m_x[2] + p.m_d;
}
}

// 0x00FDEBC0
bool hkgpConvexHullImpl::makePlane(const Vertex& a, const Vertex& b, const Vertex& c, Plane& p)
{
    const int e1[3] = { b.m_x[0] - a.m_x[0], b.m_x[1] - a.m_x[1], b.m_x[2] - a.m_x[2] };
    const int e2[3] = { c.m_x[0] - a.m_x[0], c.m_x[1] - a.m_x[1], c.m_x[2] - a.m_x[2] };
    p.m_c[0] = e1[1] * e2[2] - e1[2] * e2[1];
    p.m_c[1] = e1[2] * e2[0] - e1[0] * e2[2];
    p.m_c[2] = e1[0] * e2[1] - e1[1] * e2[0];
    p.m_d = 0;
    if (p.m_c[0] != 0 || p.m_c[1] != 0 || p.m_c[2] != 0)
    {
        p.m_d = -((hkInt64)p.m_c[0] * a.m_x[0] + (hkInt64)p.m_c[1] * a.m_x[1] + (hkInt64)p.m_c[2] * a.m_x[2]);
        return true;
    }
    return false;
}

// 0x00FFBDF0: the plane of the first point triple with a cross product length squared above 1e-5 (else the longest
// one), normalized ((1, 0, 0) for a zero normal), through the first point
void hkgpConvexHullImpl::computeProjectionPlane(const hkVector4f* points, int numPoints, hkVector4f& planeOut)
{
    const __m128 threshold = _mm_set1_ps(1e-5f); // 0x3727C5AC
    __m128 best = _mm_setzero_ps();
    planeOut.m_quad = _mm_setzero_ps();
    for (int i = 0; i < numPoints; ++i)
    {
        const __m128 o = points[i].m_quad;
        for (int j = i + 1; j < numPoints; ++j)
        {
            const __m128 e1 = _mm_sub_ps(points[j].m_quad, o);
            for (int k = j + 1; k < numPoints; ++k)
            {
                const __m128 n = cross(e1, _mm_sub_ps(points[k].m_quad, o));
                const __m128 len2 = dot3(n, n);
                if (first(len2) > first(best))
                {
                    planeOut.m_quad = n;
                    best = len2;
                    if (first(len2) > first(threshold))
                    {
                        i = j = k = numPoints;
                    }
                }
            }
        }
    }
    const __m128 n = planeOut.m_quad;
    const __m128 len2 = dot3(n, n);
    const __m128 positive = _mm_cmplt_ps(_mm_sub_ps(_mm_setzero_ps(), len2), _mm_setzero_ps());
    planeOut.m_quad = select(positive, _mm_mul_ps(invSqrt(len2), n), n);
    if (_mm_movemask_ps(positive) == 0)
    {
        planeOut.m_quad = constant(HK_QUADREAL_1000);
    }
    planeOut.m_quad = setW(planeOut.m_quad, _mm_sub_ps(_mm_setzero_ps(), dot3(planeOut.m_quad, points[0].m_quad)));
}

// 0x00FF9B40: returns the hull dimension (0 point, 1 segment, 2 polygon, 3 polyhedron, -1 failed)
int hkgpConvexHullImpl::build(const hkVector4f* points, const int numPoints, bool forcePlanar, const hkgpConvexHullImpl* domain)
{
    if (numPoints != 0 && ((hkUlong)points & 0xF) != 0)
    {
        hkArray<hkVector4f, hkContainerHeapAllocator> aligned;
        if (numPoints > 0)
        {
            aligned.reserveExactly(numPoints);
        }
        aligned.setSizeUnchecked(numPoints);
        for (int i = 0; i < numPoints; ++i)
        {
            aligned[i].m_quad = _mm_loadu_ps((const float*)&points[i]);
        }
        return build(aligned.begin(), numPoints, forcePlanar, domain);
    }

    reset(forcePlanar);
    if (numPoints >= 1)
    {
        if (domain != nullptr)
        {
            m_origin = domain->m_origin;
            m_scale = domain->m_scale;
            m_scaleInv = domain->m_scaleInv;
        }
        else if (!m_config.m_internalInputs.m_bool)
        {
            rebuildDomainParameters(points, numPoints);
        }
        else
        {
            m_origin.m_quad = _mm_setzero_ps();
            m_scale.m_quad = clearW(constant(HK_QUADREAL_1));
            m_scaleInv.m_quad = clearW(constant(HK_QUADREAL_1));
        }

        m_dimensions = numPoints - 1 > 2 ? 3 : numPoints - 1;
        m_projectionAxis.m_quad = _mm_setzero_ps();
        bool planeComputed = false;
        if (forcePlanar)
        {
            m_dimensions = 2;
            planeComputed = true;
        }
        else
        {
            m_projectionPlane.m_quad = _mm_setzero_ps();
            if (numPoints > 1)
            {
                m_projectionAxis.m_quad = _mm_sub_ps(points[1].m_quad, points[0].m_quad);
            }
            if (numPoints > 2)
            {
                m_projectionPlane.m_quad = setW(constant(HK_QUADREAL_1000), _mm_sub_ps(_mm_setzero_ps(), splatX(points[0].m_quad)));
                if (m_config.m_alwaysComputeProjectionPlane.m_bool)
                {
                    computeProjectionPlane(points, numPoints, m_projectionPlane);
                    planeComputed = true;
                }
            }
        }

        // the pending vertices: the points mapped to the integer domain (rounded half away from zero)
        m_pendings.setSizeUnchecked(0);
        setVertexArraySize(m_pendings, numPoints);
        const __m128 maxBound = _mm_set1_ps(8191.0f); // 0x014C0670 (DomainMaxBound)
        const __m128 minBound = _mm_set1_ps(-8192.0f); // 0x014C0660 (DomainMinBound)
        const __m128 two31 = _mm_set1_ps(2147483648.0f); // hkIntVector::setConvertF32toS32
        for (int i = 0; i < numPoints; ++i)
        {
            const __m128 p = points[i].m_quad;
            const __m128 v = _mm_mul_ps(_mm_sub_ps(p, m_origin.m_quad), m_scale.m_quad);
            __m128 c = select(_mm_cmplt_ps(v, maxBound), v, maxBound);
            c = _mm_max_ps(minBound, c);
            const __m128 r = select(_mm_cmplt_ps(c, _mm_setzero_ps()), _mm_sub_ps(c, constant(HK_QUADREAL_INV_2)), _mm_add_ps(c, constant(HK_QUADREAL_INV_2)));
            const __m128i xi = _mm_xor_si128(_mm_cvttps_epi32(r), _mm_castps_si128(_mm_cmple_ps(two31, r)));
            Vertex& pending = m_pendings[i];
            pending.m_source.m_quad = p;
            pending.m_x[0] = _mm_cvtsi128_si32(xi);
            pending.m_x[1] = _mm_cvtsi128_si32(_mm_shuffle_epi32(xi, 0x55));
            pending.m_x[2] = _mm_cvtsi128_si32(_mm_shuffle_epi32(xi, 0xAA));
            pending.m_scratch = 0.0f;
            pending.m_refs = 0; // rfg.exe copies an uninitialized stack word (every use resets it)
            pending.m_index = -1;
            if (m_config.m_setSourceIndices.m_bool)
            {
                // setInt24W: the index in the mantissa of w
                ((int*)&pending.m_source)[3] = i | 0x3F000000;
            }
        }
        if (!forcePlanar && m_config.m_sortInputs.m_bool && m_pendings.getSize() > 1)
        {
            hkAlgorithm::quickSortRecursive(m_pendings.begin(), 0, m_pendings.getSize() - 1, hkAlgorithm::less<Vertex>());
        }

        bool again;
        do
        {
            again = false;
            switch (m_dimensions)
            {
            case 0:
            {
                Vertex* v = m_mesh.m_vertices.allocate(m_pendings[0]);
                v->m_refs = 0;
                break;
            }

            case 1:
            {
                // the extreme points along the projection axis
                int minIndex = 0;
                int maxIndex = 0;
                const __m128 axis = m_projectionAxis.m_quad;
                for (int i = 1; i < numPoints; ++i)
                {
                    const __m128 d = dot3(m_pendings[i].m_source.m_quad, axis);
                    if (!(first(d) >= first(dot3(m_pendings[minIndex].m_source.m_quad, axis)))) // ucomiss/jnc: NaN too
                    {
                        minIndex = i;
                    }
                    if (first(d) > first(dot3(m_pendings[maxIndex].m_source.m_quad, axis)))
                    {
                        maxIndex = i;
                    }
                }
                const Vertex vmin = m_pendings[minIndex];
                const Vertex vmax = m_pendings[maxIndex];
                // original quirk: only x and y are compared
                if (vmin.m_x[0] == vmax.m_x[0] && vmin.m_x[1] == vmax.m_x[1])
                {
                    m_dimensions = 0;
                    again = true;
                    break;
                }
                Vertex* a = m_mesh.m_vertices.allocate();
                Vertex* b = m_mesh.m_vertices.allocate();
                *a = vmin;
                a->m_refs = 0;
                *b = vmax;
                b->m_refs = 0;
                break;
            }

            case 2:
            {
                if (!planeComputed)
                {
                    computeProjectionPlane(points, numPoints, m_projectionPlane);
                    planeComputed = true;
                }
                const int axis = majorAxis(m_projectionPlane.m_quad);
                const int n = m_pendings.getSize();
                swapMajorAxis(m_pendings, n, axis);
                if (n > 1)
                {
                    hkAlgorithm::quickSortRecursive(m_pendings.begin(), 0, n - 1, hkAlgorithm::less<Vertex>());
                }

                // monotone chain: lower then upper hull, the first point repeated at the end
                hkArray<Vertex, hkContainerHeapAllocator> hull;
                setVertexArraySize(hull, numPoints * 2);
                int k = 0;
                for (int i = 0; i < numPoints; ++i)
                {
                    while (k > 1 && orientation(hull[k - 2], hull[k - 1], m_pendings[i]) <= 0)
                    {
                        k--;
                    }
                    hull[k++] = m_pendings[i];
                }
                for (int i = numPoints - 2, t = k + 1; i >= 0; --i)
                {
                    while (k >= t && orientation(hull[k - 2], hull[k - 1], m_pendings[i]) <= 0)
                    {
                        k--;
                    }
                    hull[k++] = m_pendings[i];
                }

                if (k < 4)
                {
                    swapMajorAxis(m_pendings, m_pendings.getSize(), axis);
                    m_dimensions = 1;
                    again = true;
                    break;
                }

                m_pendings.swap(hull);
                setVertexArraySize(m_pendings, k - 1);
                swapMajorAxis(m_pendings, k - 1, axis);

                // orientation: the polygon normal (largest cross product about the centroid) against the plane
                const int count = m_pendings.getSize();
                const __m128 countv = _mm_set1_ps((float)count);
                const __m128 r = _mm_rcp_ps(countv);
                const __m128 inv = _mm_mul_ps(_mm_sub_ps(_mm_set1_ps(2.0f), _mm_mul_ps(r, countv)), r);
                __m128 sum = _mm_setzero_ps();
                for (int i = 0; i < count; ++i)
                {
                    sum = _mm_add_ps(sum, m_pendings[i].m_source.m_quad);
                }
                const __m128 centroid = _mm_mul_ps(sum, inv);
                __m128 normal = _mm_setzero_ps();
                for (int i = 0, j = count - 1; i < count; j = i++)
                {
                    const __m128 a = _mm_sub_ps(m_pendings[i].m_source.m_quad, centroid);
                    const __m128 b = _mm_sub_ps(m_pendings[j].m_source.m_quad, centroid);
                    const __m128 c = cross(b, a);
                    if (first(dot3(c, c)) > first(dot3(normal, normal)))
                    {
                        normal = c;
                    }
                }
                if (_mm_movemask_ps(dot3(m_projectionPlane.m_quad, normal)) != 0)
                {
                    // the copy reuses the old pending storage
                    hull.reserve(count);
                    for (int i = 0; i < count; ++i)
                    {
                        if (i < hull.getSize())
                        {
                            hull[i] = m_pendings[i];
                        }
                        else
                        {
                            new (&hull[i]) Vertex(m_pendings[i]);
                        }
                    }
                    hull.setSizeUnchecked(count);
                    for (int i = 0; i < count; ++i)
                    {
                        m_pendings[i] = hull[count - 1 - i];
                    }
                }

                for (int i = 0; i < m_pendings.getSize(); ++i)
                {
                    Vertex* v = m_mesh.m_vertices.allocate(m_pendings[i]);
                    v->m_refs = 0;
                }
                m_pendings.setSizeUnchecked(0);
                break;
            }

            case 3:
                if (!flushPending())
                {
                    if (!m_config.m_allowLowerDimensions.m_bool)
                    {
                        m_dimensions = -1;
                    }
                    else
                    {
                        m_dimensions = 2;
                        again = true;
                    }
                }
                break;

            default:
                break;
            }
        } while (again);
    }

    if (m_dimensions != -1)
    {
        if (m_config.m_buildIndices.m_bool)
        {
            buildIndices();
        }
        if (m_config.m_buildMassProperties.m_bool)
        {
            buildMassProperties();
        }
    }
    return m_dimensions;
}

// 0x00FFCCA0: the 3D hull of the pending vertices; false (with nothing changed) when they are coplanar
bool hkgpConvexHullImpl::flushPending()
{
    const int numPendings = m_pendings.getSize();
    if (numPendings < 4)
    {
        // HK_ERROR(0x642cf968, "Internal error"): unreachable (build only comes here with 4 points or more); rfg.exe
        // returns whatever AL holds
        if (hkErrorFwd::messageError(0x642CF968, "Internal error", "Y:\\NightlyJobs\\20150629-01\\Source\\Common/Internal/GeometryProcessing/ConvexHull/hkgpConvexHullInternals.h", 0x287) != 0)
        {
            __debugbreak();
        }
        return false;
    }

    // the first tetrahedron: the first non degenerate triangle (i, j, k) and the first point l off its plane
    bool found = false;
    m_projectionPlane.m_quad = _mm_setzero_ps();
    for (int i = 0; i < numPendings; ++i)
    {
        const Vertex& pi = m_pendings[i];
        for (int j = i + 1; j < numPendings; ++j)
        {
            for (int k = j + 1; k < numPendings; ++k)
            {
                Plane plane;
                if (!makePlane(pi, m_pendings[j], m_pendings[k], plane))
                {
                    continue;
                }
                if (m_config.m_allowLowerDimensions.m_bool)
                {
                    // the projection plane of the planar fallback: the largest normalized normal (the first one)
                    const __m128 a = transform(pi);
                    const __m128 n = normalize3(cross(_mm_sub_ps(transform(m_pendings[j]), a), _mm_sub_ps(transform(m_pendings[k]), a)));
                    if (first(dot3(n, n)) > first(dot3(m_projectionPlane.m_quad, m_projectionPlane.m_quad)))
                    {
                        m_projectionPlane.m_quad = setW(n, _mm_sub_ps(_mm_setzero_ps(), dot3(n, a)));
                    }
                }
                for (int l = k + 1; l < numPendings; ++l)
                {
                    const hkInt64 volume = distanceToPlane(m_pendings[l], plane);
                    if (volume == 0)
                    {
                        continue;
                    }
                    Vertex* v0 = m_mesh.m_vertices.allocate();
                    Vertex* v1 = m_mesh.m_vertices.allocate();
                    Vertex* v2 = m_mesh.m_vertices.allocate();
                    Vertex* v3 = m_mesh.m_vertices.allocate();
                    *v0 = m_pendings[i];
                    v0->m_refs = 0;
                    *v1 = m_pendings[j];
                    v1->m_refs = 0;
                    *v2 = m_pendings[k];
                    v2->m_refs = 0;
                    *v3 = m_pendings[l];
                    v3->m_refs = 0;
                    Vertex* a = v0;
                    Vertex* b = v1;
                    if (volume > 0)
                    {
                        a = v1;
                        b = v0;
                    }
                    Triangle* t[4];
                    for (int m = 0; m < 4; ++m)
                    {
                        t[m] = m_mesh.m_triangles.allocate();
                    }
                    Vertex* const faces[4][3] = { { a, b, v2 }, { a, v3, b }, { a, v2, v3 }, { b, v3, v2 } };
                    for (int m = 0; m < 4; ++m)
                    {
                        t[m]->setVertices(faces[m][0], faces[m][1], faces[m][2]);
                        t[m]->m_pid = 0;
                        t[m]->m_index = -1;
                        faces[m][0]->m_refs++;
                        faces[m][1]->m_refs++;
                        faces[m][2]->m_refs++;
                        makePlane(*faces[m][0], *faces[m][1], *faces[m][2], t[m]->m_plane);
                    }
                    t[0]->m_links[0] = uid(t[1], 2);
                    t[1]->m_links[2] = uid(t[0], 0);
                    t[0]->m_links[1] = uid(t[3], 2);
                    t[3]->m_links[2] = uid(t[0], 1);
                    t[0]->m_links[2] = uid(t[2], 0);
                    t[2]->m_links[0] = uid(t[0], 2);
                    t[1]->m_links[0] = uid(t[2], 2);
                    t[2]->m_links[2] = uid(t[1], 0);
                    t[2]->m_links[1] = uid(t[3], 1);
                    t[3]->m_links[1] = uid(t[2], 1);
                    t[3]->m_links[0] = uid(t[1], 1);
                    t[1]->m_links[1] = uid(t[3], 0);

                    // removeAt (the whole 0x40 bytes of the last element, links included)
                    const int removed[4] = { l, k, j, i };
                    for (int m = 0; m < 4; ++m)
                    {
                        const int last = m_pendings.getSize() - 1;
                        m_pendings.setSizeUnchecked(last);
                        if (last != removed[m])
                        {
                            memcpy(&m_pendings[removed[m]], &m_pendings[last], sizeof(Vertex));
                        }
                    }
                    found = true;
                    i = j = k = l = numPendings;
                }
                if (!found)
                {
                    return false;
                }
            }
        }
    }
    if (!found)
    {
        return false;
    }

    // the other points, last first
    while (m_pendings.getSize() > 0)
    {
        const Vertex& pending = m_pendings[m_pendings.getSize() - 1];
        for (Triangle* visible = m_mesh.m_triangles.getFirst(); visible; visible = visible->next())
        {
            if (distanceToPlane(pending, visible->m_plane) <= 0)
            {
                continue;
            }
            Vertex* v = m_mesh.m_vertices.allocate(pending);
            v->m_refs = 0;
            m_pid++;
            Edge horizon(nullptr, 0);
            clearHorizon(*v, Edge(visible, 0), horizon);

            // the cone from v over the horizon (naked edges), the starting edge last
            Edge e = horizon;
            while (e.m_triangle->m_links[e.m_index] & ~(hkgpTopology::Uid)3)
            {
                e.m_index = ip1mod3(e.m_index);
            }
            const hkgpTopology::Uid start = e.uid();
            Triangle* first = nullptr;
            Triangle* prev = nullptr;
            for (;;)
            {
                e.m_index = ip1mod3(e.m_index);
                while (e.m_triangle->m_links[e.m_index] & ~(hkgpTopology::Uid)3)
                {
                    const hkgpTopology::Uid link = e.m_triangle->m_links[e.m_index];
                    e.m_triangle = (Triangle*)(link & ~(hkgpTopology::Uid)3);
                    e.m_index = ip1mod3((unsigned int)(link & 3));
                }
                const bool last = e.uid() == start;
                Triangle* t = m_mesh.m_triangles.allocate();
                Vertex* s = e.m_triangle->m_vertices[e.m_index];
                Vertex* f = e.m_triangle->m_vertices[ip1mod3(e.m_index)];
                t->setVertices(v, f, s);
                t->m_pid = 0;
                t->m_index = -1;
                v->m_refs++;
                f->m_refs++;
                s->m_refs++;
                makePlane(*v, *f, *s, t->m_plane);
                e.m_triangle->m_links[e.m_index] = uid(t, 1);
                t->m_links[1] = e.uid();
                if (last)
                {
                    prev->m_links[0] = uid(t, 2);
                    t->m_links[2] = uid(prev, 0);
                    t->m_links[0] = uid(first, 2);
                    if (first)
                    {
                        first->m_links[2] = uid(t, 0);
                    }
                    break;
                }
                if (prev)
                {
                    prev->m_links[0] = uid(t, 2);
                    t->m_links[2] = uid(prev, 0);
                }
                else
                {
                    first = t;
                }
                prev = t;
            }
            break;
        }
        m_pendings.setSizeUnchecked(m_pendings.getSize() - 1);
    }
    m_pendings.clearAndDeallocate();
    return true;
}

// 0x00FFC570: removes the triangles that see vertex (flood from root); horizon is the last edge reached on a triangle
// that does not see it. Coplanar triangles are removed.
void hkgpConvexHullImpl::clearHorizon(const Vertex& vertex, const Edge& root, Edge& horizon)
{
    Triangle* t = root.m_triangle;
    if (t == nullptr || t->m_pid == m_pid)
    {
        return;
    }
    t->m_pid = m_pid;
    if (distanceToPlane(vertex, t->m_plane) < 0)
    {
        horizon = root;
        return;
    }
    const unsigned int edges[3] = { root.m_index, ip1mod3(root.m_index), ip2mod3(root.m_index) };
    for (int i = 0; i < 3; ++i)
    {
        clearHorizon(vertex, Edge(t->m_links[edges[i]]), horizon);
    }
    for (int i = 0; i < 3; ++i)
    {
        const hkgpTopology::Uid link = t->m_links[edges[i]];
        if (link & ~(hkgpTopology::Uid)3)
        {
            ((Triangle*)(link & ~(hkgpTopology::Uid)3))->m_links[link & 3] = 0;
        }
        t->m_links[edges[i]] = 0;
    }
    for (int i = 0; i < 3; ++i)
    {
        Vertex* v = t->m_vertices[i];
        if (--v->m_refs == 0)
        {
            m_mesh.m_vertices.release(v);
        }
    }
    m_mesh.m_triangles.release(t);
}
