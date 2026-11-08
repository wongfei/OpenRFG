// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/convex/capsule/hkpcapsuleshape.cpp
//
// Havok 2014.2.5 (havok_impl.md HK3): a capsule (a segment with the convex radius around it), with the out-of-line
// copies of its inline members (hkpCapsuleShape.inl in Havok 2014.1). The vertices keep the radius in w. Vertex ids
// are the byte offsets of the vertices from m_vertexA (0 and 16).

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/convex/capsule/hkpcapsuleshape.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/algorithm/collide/linesegment/hklinesegmentutil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/matrix/hktransformf.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/geometry/aabb/hkaabb.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/geometry/sphere/hksphere.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/geometry/internal/algorithms/raycast/hkcdraycastsphere.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/geometry/internal/types/hkcdvertex.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/query/hkpshaperaycastinput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/query/hkpshaperaycastoutput.h"

// 0x00D81260
hkpCapsuleShape::hkpCapsuleShape(const hkVector4f& vertexA, const hkVector4f& vertexB, float radius)
    : hkpConvexShape(hkcdShapeType::CAPSULE, radius)
{
    const hkSimdReal r = hkSimdReal::fromFloat(radius);
    m_vertexA.setXYZ_W(vertexA, r);
    m_vertexB.setXYZ_W(vertexB, r);
}

// 0x00D812D0
hkpCapsuleShape::hkpCapsuleShape(hkFinishLoadedObjectFlag flag)
    : hkpConvexShape(flag)
{
    m_type.m_storage = hkcdShapeType::CAPSULE;
}

hkpCapsuleShape::hkpCapsuleShape()
{
}

// 0x00D81D60
hkpCapsuleShape::~hkpCapsuleShape()
{
}

// 0x0080D5B0 (hkpcapsuleshape.inl:66)
const hkVector4f* hkpCapsuleShape::getVertices() const
{
    return &m_vertexA;
}

// 0x0080D5C0 (hkpcapsuleshape.inl:71)
const hkVector4f& hkpCapsuleShape::getVertex(int i) const
{
    return (&m_vertexA)[i];
}

// 0x0080D580 (hkpcapsuleshape.inl:40): the radius stays in w
void hkpCapsuleShape::setVertex(int i, const hkVector4f& position)
{
    (&m_vertexA)[i].setXYZ_W(position, hkSimdReal::fromFloat(m_radius));
}

// 0x00D81D20
int hkpCapsuleShape::getSize() const
{
    return sizeof(*this);
}

// 0x00D81D30
int hkpCapsuleShape::calcSizeForSpu(const hkpShape::CalcSizeForSpuInput& input, int spuBufferSizeLeft) const
{
    return sizeof(*this);
}

// 0x00D81410
void hkpCapsuleShape::getAabb(const hkTransformf& localToWorld, float tolerance, hkAabb& out) const
{
    hkVector4 tol4; tol4.setAll(tolerance + m_radius);
    tol4.zeroComponent<3>();

    hkVector4 obj[2];
    for (int i = 1; i >= 0; i--)
    {
        obj[i]._setTransformedPos(localToWorld, getVertex(i));
    }
    out.m_min.setMin(obj[0], obj[1]);
    out.m_max.setMax(obj[0], obj[1]);
    out.m_min.sub(tol4);
    out.m_max.add(tol4);
}

// 0x00D81E60 (hkpCapsuleShape.inl)
void hkpCapsuleShape::getSupportingVertex(const hkVector4f& direction, hkcdVertex& supportingVertexOut) const
{
    hkVector4 v0 = m_vertexA;
    v0.setInt24W(0);
    hkVector4 v1 = m_vertexB;
    v1.setInt24W(sizeof(hkVector4)); // the offset of m_vertexB
    hkVector4 axis; axis.setSub(m_vertexB, m_vertexA);
    const hkSimdReal dot = axis.dot<3>(direction);
    hkVector4 v; v.setSelect(dot.lessZero(), v0, v1);
    supportingVertexOut.assign(v);
}

// 0x00D81D90 (hkpCapsuleShape.inl)
void hkpCapsuleShape::convertVertexIdsToVertices(const unsigned short* ids, int numIds, hkcdVertex* verticesOut) const
{
    for (int i = numIds - 1; i >= 0; i--)
    {
        const int id = *ids++;
        hkVector4 v = *reinterpret_cast<const hkVector4*>(reinterpret_cast<const char*>(&m_vertexA) + id);
        v.setInt24W(id);
        (verticesOut++)->assign(v);
    }
}

// 0x00D81D70 (hkpCapsuleShape.inl)
void hkpCapsuleShape::getCentre(hkVector4f& centreOut) const
{
    centreOut.setAdd(m_vertexB, m_vertexA);
    centreOut.mul(hkSimdReal_Inv2);
}

// 0x00D81230
void hkpCapsuleShape::getFirstVertex(hkVector4f& v) const
{
    v = m_vertexB;
}

// 0x00D81D40 (hkpCapsuleShape.inl)
int hkpCapsuleShape::getNumCollisionSpheres() const
{
    return 2;
}

// 0x00D81240: the two vertices (the radius is in their w)
const hkSphere* hkpCapsuleShape::getCollisionSpheres(hkSphere* sphereBuffer) const
{
    sphereBuffer[0].setPositionAndRadius(m_vertexA);
    sphereBuffer[1].setPositionAndRadius(m_vertexB);
    return sphereBuffer;
}

// 0x00D81360
// The point of the segment [B, B2] closest to A.
void hkpCapsuleShape::closestPointLineSeg(const hkVector4f& A, const hkVector4f& B, const hkVector4f& B2, hkVector4f& pt)
{
    hkVector4 d12; d12.setSub(B2, B);
    hkVector4 dA; dA.setSub(B, A);
    const hkSimdReal d12Len2 = d12.lengthSquared<3>();
    const hkSimdReal t = hkSimdReal_0 - dA.dot<3>(d12);
    if (!(t.getReal() > 0.0f))
    {
        pt = B;
        return;
    }
    if (t.getReal() >= d12Len2.getReal()) // a NaN length interpolates, as in the original
    {
        pt = B2;
        return;
    }
    hkSimdReal ratio; ratio.setDiv<HK_ACC_23_BIT, HK_DIV_IGNORE>(t, d12Len2);
    pt.setAddMul(B, d12, ratio);
}

// 0x00D812F0
void hkpCapsuleShape::closestInfLineSegInfLineSeg(const hkVector4f& A, const hkVector4f& dA, const hkVector4f& B, const hkVector4f& dB, float& distSquared, float& t, float& u, hkVector4f& p, hkVector4f& q)
{
    hkLineSegmentUtil::ClosestPointInfLineInfLineResult result;
    hkLineSegmentUtil::closestPointInfLineInfLine(A, dA, B, dB, result);
    p = result.m_closestPointA;
    q = result.m_closestPointB;
    t = result.m_fractionA;
    u = result.m_fractionB;
    distSquared = result.m_distanceSquared;
}

// 0x00D814E0
// The ray against the infinite cylinder around the axis, then against the cap sphere on the side the intersection
// lies. Rays starting inside the capsule do not hit. m_extraInfo is the RayHitType (HIT_CAP0, HIT_CAP1, HIT_BODY).
// The shipped build records a "rcCapsule" monitor timer around the query; it is not reproduced.
hkBool hkpCapsuleShape::castRay(const hkpShapeRayCastInput& input, hkpShapeRayCastOutput& results) const
{
    {
        hkVector4 res;
        closestPointLineSeg(input.m_from, m_vertexA, m_vertexB, res);
        hkVector4 join; join.setSub(input.m_from, res);
        const float sToCylDistSq = join.lengthSquared<3>().getReal();
        if (m_radius * m_radius > sToCylDistSq)
        {
            return false; // the ray starts inside the capsule
        }
    }

    // the closest points of the infinite ray and the infinite axis
    float infInfDistSquared = HK_REAL_MAX;
    float t, u;
    hkVector4 p, q;
    hkVector4 dA; dA.setSub(input.m_to, input.m_from);
    hkVector4 dB; dB.setSub(m_vertexB, m_vertexA);
    closestInfLineSegInfLineSeg(input.m_from, dA, m_vertexA, dB, infInfDistSquared, t, u, p, q);
    if (infInfDistSquared > m_radius * m_radius)
    {
        return false; // the infinite ray misses the infinite cylinder
    }

    hkVector4 axisLength;
    hkVector4 axis;
    float ipT;
    {
        const hkSimdReal axisLengthSqrd = dB.lengthSquared<3>();
        if (axisLengthSqrd.getReal() > hkSimdReal_Eps.getReal())
        {
            hkVector4 lengthSquared; lengthSquared.setAll(axisLengthSqrd);
            hkVector4 invLength; invLength.setSqrtInverse<HK_ACC_23_BIT, HK_SQRT_SET_ZERO>(lengthSquared);
            axis.setMul(invLength, dB);
            axisLength.setMul(invLength, lengthSquared);
        }
        else
        {
            axis.setZero();
            axisLength.setZero();
        }

        // the ray direction in the plane perpendicular to the axis
        const hkSimdReal component = axis.dot<3>(dA);
        hkVector4 flatDir; flatDir.setAddMul(dA, axis, hkSimdReal_0 - component);
        const float flatDirLengthSquared3 = flatDir.lengthSquared<3>().getReal();
        if (!(flatDirLengthSquared3 < 0.0f || flatDirLengthSquared3 > 0.0f)) // zero or NaN (ucomiss + jz)
        {
            ipT = -1.0f; // parallel to the axis: only the caps can be hit
        }
        else
        {
            ipT = t - sqrtf((m_radius * m_radius - infInfDistSquared) / flatDirLengthSquared3);
        }
    }

    if (ipT >= results.m_hitFraction)
    {
        return false;
    }

    hkVector4 pointAProj; pointAProj.setAll(axis.dot<3>(m_vertexA));
    hkVector4 intersectPt; intersectPt.setInterpolate(input.m_from, input.m_to, hkSimdReal::fromFloat(ipT));
    hkVector4 ptHeight; ptHeight.setAll(intersectPt.dot<3>(axis));
    ptHeight.sub(pointAProj);

    if (ipT >= 0.0f && ptHeight(0) > 0.0f && !(ptHeight(0) >= axisLength(0)))
    {
        // the cylinder part
        hkVector4 invAxisLength; invAxisLength.setReciprocal<HK_ACC_23_BIT, HK_DIV_IGNORE>(axisLength);
        hkVector4 ratio; ratio.setMul(invAxisLength, ptHeight);
        hkVector4 projPtOnAxis; projPtOnAxis.setSub(m_vertexB, m_vertexA);
        projPtOnAxis.mul(ratio);
        projPtOnAxis.add(m_vertexA);
        hkVector4 normal; normal.setSub(intersectPt, projPtOnAxis);
        normal.normalize<3>();
        results.m_normal = normal;
        results.m_hitFraction = ipT;
        results.m_extraInfo = HIT_BODY;
        results.setKey(0xFFFFFFFF); // HK_INVALID_SHAPE_KEY
        return true;
    }

    // the caps
    {
        hkVector4 projParam; projParam.setAll(input.m_from.dot<3>(axis));
        projParam.sub(pointAProj);
        hkVector4 invAxisLength; invAxisLength.setReciprocal<HK_ACC_23_BIT, HK_DIV_IGNORE>(axisLength);
        hkVector4 ratio; ratio.setMul(invAxisLength, projParam);
        hkVector4 fromPtProjAxis; fromPtProjAxis.setSub(m_vertexB, m_vertexA);
        fromPtProjAxis.mul(ratio);
        fromPtProjAxis.add(m_vertexA);
        hkVector4 axisToRayStart; axisToRayStart.setSub(input.m_from, fromPtProjAxis);
        if (0.0f > ipT)
        {
            // The radius is squared on the x87 stack in rfg.exe (53-bit precision, exact): compared in double.
            const float axisToRayStartLenSq = axisToRayStart.lengthSquared<3>().getReal();
            if ((double)axisToRayStartLenSq > (double)m_radius * (double)m_radius)
            {
                return false; // the ray starts outside the infinite cylinder and points away
            }
        }

        // only one cap can be hit: the one on the intersection's side
        hkVector4 capEnd; capEnd.setSelect(ptHeight.lessEqualZero(), m_vertexA, m_vertexB);
        hkVector4 start; start.setSub(input.m_from, capEnd);
        hkVector4 end; end.setSub(input.m_to, capEnd);
        hkcdRay ray;
        ray.setEndPoints(start, end);
        hkVector4 sphere; sphere.setXYZ_W(hkVector4::getConstant<HK_QUADREAL_0>(), hkSimdReal::fromFloat(m_radius));
        hkSimdReal fraction = hkSimdReal::fromFloat(results.m_hitFraction);
        if (hkcdRayCastSphere(ray, sphere, &fraction, &results.m_normal, hkcdRayQueryFlags::NO_FLAGS).isHit())
        {
            fraction.store<1>(&results.m_hitFraction);
            results.setKey(0xFFFFFFFF); // HK_INVALID_SHAPE_KEY
            results.m_extraInfo = ptHeight(0) > 0.0f ? HIT_CAP1 : HIT_CAP0;
            return true;
        }
        return false;
    }
}
