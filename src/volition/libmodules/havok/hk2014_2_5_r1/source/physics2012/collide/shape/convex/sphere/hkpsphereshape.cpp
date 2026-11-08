// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/convex/sphere/hkpsphereshape.cpp
//
// Havok 2014.2.5 (havok_impl.md HK3): a sphere at the origin (the convex radius is the sphere's radius, the core is a
// point), with the out-of-line copies of its inline members (hkpSphereShape.inl in Havok 2014.1).

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/convex/sphere/hkpsphereshape.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/matrix/hktransformf.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/geometry/aabb/hkaabb.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/geometry/sphere/hksphere.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/geometry/internal/algorithms/raycast/hkcdraycastsphere.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/geometry/internal/types/hkcdvertex.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/query/hkpshaperaybundlecastinput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/query/hkpshaperaycastinput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/query/hkpshaperaycastoutput.h"

// 0x00D7B190
hkpSphereShape::hkpSphereShape(float radius)
    : hkpConvexShape(hkcdShapeType::SPHERE, radius)
{
}

// 0x00D7B1C0
hkpSphereShape::hkpSphereShape(hkFinishLoadedObjectFlag flag)
    : hkpConvexShape(flag)
{
    m_type.m_storage = hkcdShapeType::SPHERE;
}

hkpSphereShape::hkpSphereShape()
{
}

// 0x00743440
hkpSphereShape::~hkpSphereShape()
{
}

// 0x00D7BD60
int hkpSphereShape::getSize() const
{
    return sizeof(*this);
}

// 0x00D7BD70
int hkpSphereShape::calcSizeForSpu(const hkpShape::CalcSizeForSpuInput& input, int spuBufferSizeLeft) const
{
    return sizeof(*this);
}

// 0x00D7BDF0 (hkpSphereShape.inl)
void hkpSphereShape::getAabb(const hkTransformf& localToWorld, float tolerance, hkAabb& out) const
{
    hkVector4 radius; radius.setAll(m_radius + tolerance);
    out.m_min.setSub(localToWorld.getTranslation(), radius);
    out.m_max.setAdd(localToWorld.getTranslation(), radius);
}

// 0x00D7BDB0 (hkpSphereShape.inl): the core is the origin
void hkpSphereShape::getSupportingVertex(const hkVector4f& direction, hkcdVertex& supportingVertexOut) const
{
    supportingVertexOut.setZero();
}

// 0x00D7BDC0 (hkpSphereShape.inl)
void hkpSphereShape::convertVertexIdsToVertices(const unsigned short* ids, int numIds, hkcdVertex* verticesOut) const
{
    for (int i = numIds - 1; i >= 0; i--)
    {
        verticesOut[i].setZero();
    }
}

// 0x00D7BDE0 (hkpSphereShape.inl)
void hkpSphereShape::getCentre(hkVector4f& centreOut) const
{
    centreOut.setZero();
}

// 0x00D7B180
void hkpSphereShape::getFirstVertex(hkVector4f& v) const
{
    v.setZero();
}

// 0x00D7BD80 (hkpSphereShape.inl)
int hkpSphereShape::getNumCollisionSpheres() const
{
    return 1;
}

// 0x00D7BEC0 (hkpSphereShape.inl)
const hkSphere* hkpSphereShape::getCollisionSpheres(hkSphere* sphereBuffer) const
{
    sphereBuffer->getPositionAndRadius().setXYZ_W(hkVector4::getConstant<HK_QUADREAL_0>(), hkSimdReal::fromFloat(m_radius));
    return sphereBuffer;
}

// 0x00D7B1E0
// The shipped build records a "rcSphere" monitor timer around the query (TLS hkMonitorStream); it is not reproduced.
hkBool hkpSphereShape::castRay(const hkpShapeRayCastInput& input, hkpShapeRayCastOutput& results) const
{
    hkcdRay ray;
    ray.setEndPoints(input.m_from, input.m_to);
    hkVector4 sphere; sphere.setXYZ_W(hkVector4::getConstant<HK_QUADREAL_0>(), hkSimdReal::fromFloat(m_radius));
    hkSimdReal fraction = hkSimdReal::fromFloat(results.m_hitFraction);
    if (hkcdRayCastSphere(ray, sphere, &fraction, &results.m_normal, hkcdRayQueryFlags::NO_FLAGS).isHit())
    {
        fraction.store<1>(&results.m_hitFraction);
        results.setKey(0xFFFFFFFF); // HK_INVALID_SHAPE_KEY
        return true;
    }
    return false;
}

// 0x00D7B4D0
// The shipped build records a "rcSphereBundle" monitor timer around the query; it is not reproduced.
hkVector4fComparison hkpSphereShape::castRayBundle(const hkpShapeRayBundleCastInput& input, hkpShapeRayBundleCastOutput& results, const hkVector4fComparison& mask) const
{
    hkcdRayBundle rayBundle;
    rayBundle.m_start = input.m_from;
    rayBundle.m_end = input.m_to;
    rayBundle.m_activeRays = mask;

    hkVector4 fractions;
    fractions.set(results.m_outputs[0].m_hitFraction, results.m_outputs[1].m_hitFraction, results.m_outputs[2].m_hitFraction, results.m_outputs[3].m_hitFraction);
    hkFourTransposedPoints normals;
    const hkVector4Comparison hitMask = hkcdRayBundleSphereIntersect(rayBundle, hkSimdReal::fromFloat(m_radius), fractions, normals);

    hkVector4 vNormals[4];
    normals.extract(vNormals[0], vNormals[1], vNormals[2], vNormals[3]);
    const int hits = hitMask.getMask();
    for (int i = 0; i < 4; i++)
    {
        if (hits & (1 << i))
        {
            results.m_outputs[i].m_hitFraction = fractions(i);
            results.m_outputs[i].m_normal = vNormals[i];
            results.m_outputs[i].setKey(0xFFFFFFFF); // HK_INVALID_SHAPE_KEY
        }
    }
    return hitMask;
}
