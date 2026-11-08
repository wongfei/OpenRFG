// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/convex/convexvertices/hkpconvexverticesshape.cpp
//
// Havok 2014.2.5 (havok_impl.md HK3): a convex hull given by its vertices (stored transposed, four per
// hkFourTransposedPointsf, the last batch padded with the last vertex) and optionally its plane equations, from
// rfg.exe. The vertex ids are the vertex indices. The shape-building constructor (BuildConfig, hkGeometryUtility) is
// not in rfg.exe.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/convex/convexvertices/hkpconvexverticesshape.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/localarray/hklocalarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/matrix/hktransformf.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkvector4util.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/geometry/aabb/hkaabb.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/geometry/aabb/hkaabbutil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/geometry/hkstridedvertices.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/geometry/sphere/hksphere.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/geometry/internal/algorithms/gsk/hkcdgsk.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/geometry/internal/algorithms/raycast/hkcdraycastconvex.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/geometry/internal/algorithms/supportingvertex/hkcdsupportingvertex.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/geometry/internal/types/hkcdvertex.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/convex/convexvertices/hkpconvexverticesconnectivity.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/query/hkpshaperaycastinput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/query/hkpshaperaycastoutput.h"

// 0x00D983C0
hkpConvexVerticesShape::hkpConvexVerticesShape(float radius)
    : hkpConvexShape(hkcdShapeType::CONVEX_VERTICES, radius)
{
    m_useSpuBuffer = false;
    m_connectivity = nullptr;
}

// 0x00D98410
hkpConvexVerticesShape::hkpConvexVerticesShape(const hkStridedVertices& vertsIn, const hkArray<hkVector4f, hkContainerHeapAllocator>& planeEquations, float radius)
    : hkpConvexShape(hkcdShapeType::CONVEX_VERTICES, radius)
{
    m_useSpuBuffer = false;
    m_connectivity = nullptr;
    m_planeEquations = planeEquations;
    copyVertexData(vertsIn.m_vertices, vertsIn.m_striding, vertsIn.m_numVertices);
}

// 0x00D98500: over external (already transposed) vertices and planes, which the shape does not own
hkpConvexVerticesShape::hkpConvexVerticesShape(hkFourTransposedPointsf* rotatedVertices, int numVertices, hkVector4f* planes, int numPlanes, const hkAabb& aabb, float radius)
    : hkpConvexShape(hkcdShapeType::CONVEX_VERTICES, radius)
{
    const int numBatches = (numVertices + 3) / 4;
    m_rotatedVertices.m_data = rotatedVertices;
    m_rotatedVertices.m_size = numBatches;
    m_rotatedVertices.m_capacityAndFlags = numBatches | hkArrayBase<hkFourTransposedPointsf>::DONT_DEALLOCATE_FLAG;
    m_numVertices = numVertices;
    m_useSpuBuffer = false;
    m_planeEquations.m_size = numPlanes;
    m_planeEquations.m_data = planes;
    m_planeEquations.m_capacityAndFlags = numPlanes | hkArrayBase<hkVector4f>::DONT_DEALLOCATE_FLAG;
    m_connectivity = nullptr;

    const hkSimdReal half = hkSimdReal_Inv2;
    m_aabbHalfExtents.setSub(aabb.m_max, aabb.m_min);
    m_aabbHalfExtents.mul(half);
    m_aabbCenter.setAdd(aabb.m_max, aabb.m_min);
    m_aabbCenter.mul(half);
}

// 0x00D985B0
hkpConvexVerticesShape::hkpConvexVerticesShape(hkFinishLoadedObjectFlag flag)
    : hkpConvexShape(flag)
    , m_rotatedVertices(flag)
    , m_planeEquations(flag)
{
    if (flag.m_finishing)
    {
        afterReflectNew();
    }
}

// 0x00D985E0
hkpConvexVerticesShape::~hkpConvexVerticesShape()
{
    if (m_connectivity)
    {
        m_connectivity->removeReference();
    }
}

// 0x00D97860
void hkpConvexVerticesShape::afterReflectNew()
{
    m_type.m_storage = hkcdShapeType::CONVEX_VERTICES;
}

// 0x00ED8FF0: the shape (when there is memory) at memPtr, which moves past it either way
hkpConvexVerticesShape* hkpConvexVerticesShape::createInPlace(unsigned char*& memPtr, float convexRadius)
{
    hkpConvexVerticesShape* shape = memPtr ? new (memPtr) hkpConvexVerticesShape(convexRadius) : nullptr;
    memPtr += sizeof(hkpConvexVerticesShape);
    return shape;
}

// 0x00D99EA0
int hkpConvexVerticesShape::getSize() const
{
    return sizeof(*this);
}

// 0x00D99EB0
int hkpConvexVerticesShape::calcSizeForSpu(const hkpShape::CalcSizeForSpuInput& input, int spuBufferSizeLeft) const
{
    return sizeof(*this);
}

// 0x00D99EC0: one sphere per vertex
int hkpConvexVerticesShape::getNumCollisionSpheres() const
{
    return m_numVertices;
}

// 0x00D97690
const hkArray<hkVector4f, hkContainerHeapAllocator>& hkpConvexVerticesShape::getPlaneEquations() const
{
    return m_planeEquations;
}

// 0x00D98D50: the planes are put in the connectivity's face order when there is one
void hkpConvexVerticesShape::setPlaneEquations(const hkArray<hkVector4f, hkContainerHeapAllocator>& planes)
{
    m_planeEquations = planes;
    if (m_connectivity)
    {
        sortPlanes();
    }
}

// 0x00D98D10
void hkpConvexVerticesShape::setConnectivity(const hkpConvexVerticesConnectivity* connect, bool sort)
{
    if (connect)
    {
        connect->addReference();
    }
    if (m_connectivity)
    {
        m_connectivity->removeReference();
    }
    m_connectivity = connect;
    if (sort && connect)
    {
        sortPlanes();
    }
}

// 0x00D981C0
// The vertices (x, y, z at byteStriding apart) transposed four at a time; the last batch is padded with the last
// vertex. The AABB of the vertices gives the extents and the centre.
void hkpConvexVerticesShape::copyVertexData(const float* vertexIn, int byteStriding, int numVertices)
{
    m_numVertices = numVertices;
    m_rotatedVertices.setSize((numVertices + 3) / 4);

    const int numFullBatches = numVertices >> 2;
    const char* v = reinterpret_cast<const char*>(vertexIn);
    for (int i = 0; i < numFullBatches; i++)
    {
        const float* p0 = reinterpret_cast<const float*>(v);
        const float* p1 = reinterpret_cast<const float*>(v + byteStriding);
        const float* p2 = reinterpret_cast<const float*>(v + byteStriding * 2);
        const float* p3 = reinterpret_cast<const float*>(v + byteStriding * 3);
        hkFourTransposedPointsf& batch = m_rotatedVertices[i];
        batch.m_vertices[0].set(p0[0], p1[0], p2[0], p3[0]);
        batch.m_vertices[1].set(p0[1], p1[1], p2[1], p3[1]);
        batch.m_vertices[2].set(p0[2], p1[2], p2[2], p3[2]);
        v += byteStriding * 4;
    }

    const int numLeft = numVertices - numFullBatches * 4;
    if (numLeft != 0)
    {
        hkVector4 left[4];
        for (int i = 0; i < numLeft; i++)
        {
            const float* p = reinterpret_cast<const float*>(v);
            left[i].set(p[0], p[1], p[2], 0.0f);
            v += byteStriding;
        }
        for (int i = numLeft; i < 4; i++)
        {
            left[i] = left[numLeft - 1];
        }
        hkFourTransposedPointsf& batch = m_rotatedVertices[numFullBatches];
        batch.m_vertices[0].set(left[0](0), left[1](0), left[2](0), left[3](0));
        batch.m_vertices[1].set(left[0](1), left[1](1), left[2](1), left[3](1));
        batch.m_vertices[2].set(left[0](2), left[1](2), left[2](2), left[3](2));
    }

    hkAabb aabb;
    hkAabbUtil::calcAabb(vertexIn, numVertices, byteStriding, aabb);
    const hkSimdReal half = hkSimdReal_Inv2;
    m_aabbHalfExtents.setSub(aabb.m_max, aabb.m_min);
    m_aabbHalfExtents.mul(half);
    m_aabbCenter.setAdd(aabb.m_min, aabb.m_max);
    m_aabbCenter.mul(half);
}

// 0x00D98130: one vector per vertex, the vertex index in w (hkcdVertex)
void hkpConvexVerticesShape::getOriginalVertices(hkArray<hkVector4f, hkContainerHeapAllocator>& vertices) const
{
    vertices.setSize(m_rotatedVertices.getSize() * 4);
    hkFourTransposedPointsf::getOriginalVertices(m_rotatedVertices.begin(), m_numVertices, reinterpret_cast<hkcdVertex*>(vertices.begin()));
    vertices.setSize(m_numVertices);
}

// 0x00D98670
void hkpConvexVerticesShape::transformVerticesAndPlaneEquations(const hkTransformf& t)
{
    hkLocalArray<hkVector4> vertices(getNumCollisionSpheres());
    getOriginalVertices(vertices);
    hkVector4Util::transformPoints(t, vertices.begin(), vertices.getSize(), vertices.begin());
    copyVertexData(&vertices[0](0), sizeof(hkVector4), vertices.getSize());
    hkVector4Util::transformPlaneEquations(t, m_planeEquations.begin(), m_planeEquations.getSize(), m_planeEquations.begin());
}

// 0x00D98920
// Reorders the planes to the connectivity's faces: for each face (of at least 3 vertices), the remaining plane whose
// normal is closest to the face normal (from its first three vertices) is moved to the output; planes no face took
// go last.
void hkpConvexVerticesShape::sortPlanes()
{
    hkArray<hkVector4> planes;
    planes = m_planeEquations;
    m_planeEquations.setSizeUnchecked(0);

    hkLocalArray<hkVector4> vertices(m_numVertices);
    getOriginalVertices(vertices);

    int numPlanes = planes.getSize();
    int indexBase = 0;
    for (int f = 0; f < m_connectivity->m_numVerticesPerFace.getSize(); f++)
    {
        if (numPlanes == 0)
        {
            // every plane is placed; nothing is appended
            return;
        }
        const int numFaceVertices = m_connectivity->m_numVerticesPerFace[f];
        if (numFaceVertices < 3)
        {
            // original bug kept: the index base does not move past a degenerate face, so the faces after it read
            // its vertex indices
            continue;
        }
        const unsigned short* indices = &m_connectivity->m_vertexIndices[indexBase];
        indexBase += numFaceVertices;

        const hkVector4& v0 = vertices[indices[0]];
        hkVector4 e1; e1.setSub(vertices[indices[1]], v0);
        hkVector4 e2; e2.setSub(vertices[indices[2]], v0);
        hkVector4 normal; normal.setCross(e1, e2);
        if (!normal.normalizeIfNotZero<3>())
        {
            // original quirk kept: a face without a normal gets the first remaining plane, which also stays in the
            // remaining planes
            m_planeEquations.m_data[m_planeEquations.m_size++] = planes[0];
            continue;
        }

        int best = 0;
        hkSimdReal bestDot = normal.dot<3>(planes[0]);
        for (int i = 1; i < numPlanes; i++)
        {
            const hkSimdReal d = normal.dot<3>(planes[i]);
            if (bestDot < d)
            {
                best = i;
                bestDot = d;
            }
        }
        m_planeEquations.m_data[m_planeEquations.m_size++] = planes[best];
        planes.removeAt(best);
        numPlanes--;
    }

    if (numPlanes > 0)
    {
        m_planeEquations.insertAt(m_planeEquations.getSize(), planes.begin(), numPlanes);
    }
}

// 0x00D97870
void hkpConvexVerticesShape::getAabb(const hkTransformf& localToWorld, float tolerance, hkAabb& out) const
{
    hkAabbUtil::calcAabb(localToWorld, m_aabbHalfExtents, m_aabbCenter, hkSimdReal::fromFloat(m_radius + tolerance), out);
}

// 0x00D97930
void hkpConvexVerticesShape::getSupportingVertex(const hkVector4f& direction, hkcdVertex& supportingVertexOut) const
{
    hkcdSupportingVertexPointsTranposed(m_rotatedVertices.begin(), m_rotatedVertices.getSize(), direction, &supportingVertexOut);
}

// 0x00D977A0
void hkpConvexVerticesShape::convertVertexIdsToVertices(const unsigned short* ids, int numIds, hkcdVertex* verticesOut) const
{
    for (int i = numIds - 1; i >= 0; i--)
    {
        const int id = *ids++;
        const hkFourTransposedPointsf& batch = m_rotatedVertices[id >> 2];
        const int a = id & 3;
        hkVector4 v; v.set(batch.m_vertices[0](a), batch.m_vertices[1](a), batch.m_vertices[2](a), 0.0f);
        v.setInt24W(id);
        (verticesOut++)->assign(v);
    }
}

// 0x00D97850
void hkpConvexVerticesShape::getCentre(hkVector4f& centreOut) const
{
    centreOut = m_aabbCenter;
}

// 0x00D976A0
void hkpConvexVerticesShape::getFirstVertex(hkVector4f& v) const
{
    const hkFourTransposedPointsf& batch = m_rotatedVertices[0];
    v.set(batch.m_vertices[0](0), batch.m_vertices[1](0), batch.m_vertices[2](0), 0.0f);
}

// 0x00D976D0: the vertices with the convex radius
const hkSphere* hkpConvexVerticesShape::getCollisionSpheres(hkSphere* sphereBuffer) const
{
    const float radius = m_radius;
    for (int i = 0; i < m_numVertices; i++)
    {
        const hkFourTransposedPointsf& batch = m_rotatedVertices[i >> 2];
        const int a = i & 3;
        sphereBuffer[i].getPositionAndRadius().set(batch.m_vertices[0](a), batch.m_vertices[1](a), batch.m_vertices[2](a), radius);
    }
    return sphereBuffer;
}

// 0x00D97A70
// Without plane equations, a GSK ray cast against the vertices with the convex radius (0.001 more for the
// termination); with them, the ray against the planes (the convex radius is not added). The shipped build records
// "convexVertCastRayGSK" / "convexVertCastRayPlaneEq" monitor timers (TLS hkMonitorStream); they are not reproduced.
hkBool hkpConvexVerticesShape::castRay(const hkpShapeRayCastInput& input, hkpShapeRayCastOutput& results) const
{
    if (m_planeEquations.getSize() == 0)
    {
        hkcdGsk::RayCastInput gskInput(m_radius, 0.001f); // 0.001: the termination tolerance on top of the radius
        gskInput.m_from = input.m_from;
        gskInput.m_direction.setSub(input.m_to, input.m_from);
        hkcdGsk::RayCastOutput gskOutput;
        gskOutput.m_normalOut.setZero();
        gskOutput.m_fractionInOut = hkSimdReal::fromFloat(results.m_hitFraction);
        const bool hit = hkcdGsk::rayCast(m_rotatedVertices.begin(), m_numVertices, gskInput, gskOutput);
        if (hit)
        {
            results.m_normal = gskOutput.m_normalOut;
            gskOutput.m_fractionInOut.store<1>(&results.m_hitFraction);
            results.setKey(0xFFFFFFFF); // HK_INVALID_SHAPE_KEY
        }
        return hit;
    }

    hkcdRay ray;
    ray.setEndPoints(input.m_from, input.m_to);
    hkSimdReal fraction = hkSimdReal::fromFloat(results.m_hitFraction);
    hkVector4 normal;
    if (hkcdRayCastConvex(ray, m_planeEquations.begin(), m_planeEquations.getSize(), hkSimdReal_0, &fraction, &normal, hkcdRayQueryFlags::NO_FLAGS).isHit())
    {
        fraction.store<1>(&results.m_hitFraction);
        results.m_normal = normal;
        results.setKey(0xFFFFFFFF); // HK_INVALID_SHAPE_KEY
        return true;
    }
    return false;
}
