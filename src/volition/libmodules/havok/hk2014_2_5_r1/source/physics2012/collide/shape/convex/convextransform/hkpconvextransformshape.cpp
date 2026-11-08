// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/convex/convextransform/hkpconvextransformshape.cpp
//
// Havok 2014.2.5 (havok_impl.md HK3): a convex child shape under a rotation, translation and (non-uniform) scale.
//
// The transform is an hkQsTransform. A non-uniform scale is applied to the child's core (the child shape without its
// convex radius), and the radius is kept as uniform as possible: m_extraScale moves vertices away from the core's
// centre so the scaled core plus the new radius covers the scaled child. The core centre is stored in the w components
// of m_transform.m_translation (x), m_transform.m_scale (y) and m_extraScale (z).

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/convex/convextransform/hkpconvextransformshape.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/matrix/hktransformf.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/geometry/aabb/hkaabb.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/geometry/aabb/hkaabbutil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/geometry/sphere/hksphere.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/geometry/internal/types/hkcdvertex.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcdbody.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/query/hkprayhitcollector.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/query/hkpshaperaycastinput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/query/hkpshaperaycastoutput.h"

// ---- hkpConvexTransformShapeBase

// 0x00D75210
hkpConvexTransformShapeBase::hkpConvexTransformShapeBase(hkFinishLoadedObjectFlag flag)
    : hkpConvexShape(flag)
    , m_childShape(flag)
{
}

hkpConvexTransformShapeBase::hkpConvexTransformShapeBase()
{
}

// 0x0071B5E0
hkpConvexTransformShapeBase::~hkpConvexTransformShapeBase()
{
}

// ---- hkpConvexTransformShape

// 0x00D752F0
hkpConvexTransformShape::hkpConvexTransformShape(const hkpConvexShape* childShape, const hkTransformf& transform, hkpShapeContainer::ReferencePolicy ref)
    : hkpConvexTransformShapeBase(hkcdShapeType::CONVEX_TRANSFORM, childShape->getRadius(), childShape, ref)
{
    m_transform.setFromTransformNoScale(transform);
    m_extraScale.setZero();
    m_userData = childShape->m_userData;
}

// 0x00D75FC0
hkpConvexTransformShape::hkpConvexTransformShape(const hkpConvexShape* childShape, const hkQsTransformf& transform, hkpShapeContainer::ReferencePolicy ref)
    : hkpConvexTransformShapeBase(hkcdShapeType::CONVEX_TRANSFORM, childShape->getRadius(), childShape, ref)
{
    setTransform(transform);
    m_userData = childShape->m_userData;
}

// 0x00D75340
hkpConvexTransformShape::hkpConvexTransformShape(hkFinishLoadedObjectFlag flag)
    : hkpConvexTransformShapeBase(flag)
{
    m_type.m_storage = hkcdShapeType::CONVEX_TRANSFORM;
}

hkpConvexTransformShape::hkpConvexTransformShape()
{
}

// 0x00D76350
hkpConvexTransformShape::~hkpConvexTransformShape()
{
}

// 0x00A12FD0 (hkpconvextransformshape.inl:10)
const hkpConvexShape* hkpConvexTransformShape::getChildShape() const
{
    return static_cast<const hkpConvexShape*>(m_childShape.getChild());
}

// 0x00E720D0
const hkQsTransformf& hkpConvexTransformShape::getQsTransform() const
{
    return m_transform;
}

// 0x00D760B0
void hkpConvexTransformShape::setTransform(const hkTransformf& transform)
{
    m_transform.setFromTransformNoScale(transform);
    m_extraScale.setZero();
}

// 0x00D75DC0
void hkpConvexTransformShape::setTransform(const hkQsTransformf& transform)
{
    m_transform = transform;
    m_transform.m_translation.zeroComponent<3>();
    m_transform.m_scale.zeroComponent<3>();

    const hkVector4& scale = transform.getScale();
    if (!scale.allEqual<3>(hkVector4::getConstant<HK_QUADREAL_1>(), hkSimdReal_Eps))
    {
        hkVector4 absScale; absScale.setAbs(scale);
        const hkpConvexShape* childShape = getChildShape();

        // spheres and capsules (fewer than 3 collision spheres) are scaled by x only
        if (childShape->getNumCollisionSpheres() < 3)
        {
            m_radius = childShape->getRadius() * absScale(0);
        }
        else
        {
            // the child's core AABB
            hkAabb aabb;
            childShape->getAabb(hkTransform::getIdentity(), 0.0f, aabb);
            const hkSimdReal childRadius = hkSimdReal::fromFloat(childShape->getRadius());
            aabb.expandBy(-childRadius);
            hkVector4 halfExtents; aabb.getHalfExtents(halfExtents);
            hkVector4 center; aabb.getCenter(center);

            // the new radius: the old one, or the smallest scaled extent if that is smaller
            const hkSimdReal radius = hkSimdReal::fromFloat(m_radius);
            hkVector4 scaledExtents; scaledExtents.setAdd(halfExtents, radius);
            scaledExtents.mul(absScale);
            hkVector4 radiusV; radiusV.setAll(radius);
            hkVector4 newRadius = radiusV;
            if (!radiusV.less(scaledExtents).allAreSet<hkVector4ComparisonMask::MASK_XYZ>())
            {
                newRadius.setAll(scaledExtents.horizontalMin<3>());
                m_radius = newRadius(0);
            }

            // the extra scale moves the core's vertices so its scaled size plus the new radius matches the scaled child
            hkVector4 radiusDiff; radiusDiff.setMul(radiusV, absScale);
            radiusDiff.sub(newRadius);
            m_extraScale.setDiv<HK_ACC_23_BIT, HK_DIV_SET_ZERO>(radiusDiff, halfExtents);
            m_extraScale.setFlipSign(m_extraScale, scale);

            m_transform.m_translation.setComponent<3>(center.getComponent<0>());
            m_transform.m_scale.setComponent<3>(center.getComponent<1>());
            m_extraScale.setComponent<3>(center.getComponent<2>());
            return;
        }
    }
    m_extraScale.setZero();
}

// 0x00D76240 (hkpConvexTransformShape.inl in Havok 2014.1): scale, extra scale around the core centre, rotate,
// translate
void hkpConvexTransformShape::transformVertex(const hkVector4f& localVertex, hkVector4f* vertexOut) const
{
    hkVector4 center;
    center.set(m_transform.m_translation.getW(), m_transform.m_scale.getW(), m_extraScale.getW(), hkSimdReal_0);
    vertexOut->setMul(localVertex, m_transform.m_scale);
    hkVector4 offset; offset.setSub(localVertex, center);
    vertexOut->addMul(offset, m_extraScale);
    vertexOut->_setRotatedDir(m_transform.m_rotation, *vertexOut);
    vertexOut->add(m_transform.m_translation);
}

// 0x00D752A0: the child's size, or the child placed right after this shape
int hkpConvexTransformShape::calcSizeForSpu(const hkpShape::CalcSizeForSpuInput& input, int spuBufferSizeLeft) const
{
    spuBufferSizeLeft -= sizeof(*this);
    const int childSize = getChildShape()->calcSizeForSpu(input, spuBufferSizeLeft);
    if (childSize < 0 || childSize > spuBufferSizeLeft)
    {
        return -1;
    }
    hkpConvexTransformShape* self = const_cast<hkpConvexTransformShape*>(this);
    if ((const void*)m_childShape.getChild() == (const void*)(this + 1))
    {
        self->m_childShapeSizeForSpu = 0;
        return sizeof(*this) + childSize;
    }
    self->m_childShapeSizeForSpu = childSize;
    return sizeof(*this);
}

// 0x00D75290
const hkpShapeContainer* hkpConvexTransformShape::getContainer() const
{
    return &m_childShape;
}

// 0x00D75370: the child's local AABB scaled, then transformed
void hkpConvexTransformShape::getAabb(const hkTransformf& localToWorld, float tolerance, hkAabb& out) const
{
    hkAabb childAabb;
    getChildShape()->getAabb(hkTransform::getIdentity(), 0.0f, childAabb);

    hkVector4 halfExtents; childAabb.getHalfExtents(halfExtents);
    halfExtents.mul(m_transform.m_scale);
    hkVector4 center; childAabb.getCenter(center);
    center.mul(m_transform.m_scale);

    hkTransform localToParent; m_transform.copyToTransformNoScale(localToParent);
    hkTransform childToWorld; childToWorld.setMul(localToWorld, localToParent);
    hkAabbUtil::calcAabb(childToWorld, halfExtents, center, hkSimdReal::fromFloat(tolerance), out);
}

// 0x00D754A0
// This shape's own castRay (from no earlier hit than 1); a hit is reported for the child body with its normal rotated
// to world space. The shipped build records a "rcCxTransform" monitor timer around the query; it is not reproduced.
void hkpConvexTransformShape::castRayWithCollector(const hkpShapeRayCastInput& input, const hkpCdBody& cdBody, hkpRayHitCollector& collector) const
{
    hkpShapeRayCastOutput results;
    if (castRay(input, results))
    {
        results.m_normal._setRotatedDir(cdBody.getTransform().getRotation(), results.m_normal);
        hkpCdBody childBody(&cdBody);
        childBody.setShape(getChildShape(), 0);
        collector.addRayHit(childBody, results);
    }
}

// 0x00D755B0
// The direction into the child's scaled space, the child's supporting vertex back out (its id in w kept).
void hkpConvexTransformShape::getSupportingVertex(const hkVector4f& direction, hkcdVertex& supportingVertexOut) const
{
    hkVector4 localDir; localDir._setRotatedInverseDir(m_transform.m_rotation, direction);
    localDir.mul(m_transform.m_scale);

    hkcdVertex childVertex;
    getChildShape()->getSupportingVertex(localDir, childVertex);
    transformVertex(childVertex, &supportingVertexOut);
    supportingVertexOut.setW(childVertex);
}

// 0x00D75710
void hkpConvexTransformShape::convertVertexIdsToVertices(const unsigned short* ids, int numIds, hkcdVertex* verticesOut) const
{
    getChildShape()->convertVertexIdsToVertices(ids, numIds, verticesOut);
    for (int i = 0; i < numIds; i++)
    {
        hkVector4 v;
        transformVertex(verticesOut[i], &v);
        verticesOut[i].setXYZ(v);
    }
}

// 0x00D75810
void hkpConvexTransformShape::getCentre(hkVector4f& centreOut) const
{
    hkVector4 childCentre;
    getChildShape()->getCentre(childCentre);
    transformVertex(childCentre, &centreOut);
}

// 0x00D758F0
void hkpConvexTransformShape::getFirstVertex(hkVector4f& v) const
{
    hkVector4 childVertex;
    getChildShape()->getFirstVertex(childVertex);
    transformVertex(childVertex, &v);
}

// 0x00D76300
int hkpConvexTransformShape::getNumCollisionSpheres() const
{
    return getChildShape()->getNumCollisionSpheres();
}

// 0x00D759D0: the child's sphere centres transformed (last first), each with this shape's radius
const hkSphere* hkpConvexTransformShape::getCollisionSpheres(hkSphere* sphereBuffer) const
{
    const hkSphere* spheres = getChildShape()->getCollisionSpheres(sphereBuffer);
    const int numSpheres = getChildShape()->getNumCollisionSpheres();
    const hkSimdReal radius = hkSimdReal::fromFloat(m_radius);
    for (int i = numSpheres - 1; i >= 0; i--)
    {
        hkVector4 v;
        transformVertex(spheres[i].getPositionAndRadius(), &v);
        sphereBuffer[i].getPositionAndRadius().setXYZ_W(v, radius);
    }
    return sphereBuffer;
}

// 0x00D75B00
// The ray in the child's unscaled space (the extra scale is ignored); a hit's normal is scaled back, rotated and
// normalized, and gets key 0 at this level. The shipped build records a "rcCxTransform" monitor timer; not reproduced.
hkBool hkpConvexTransformShape::castRay(const hkpShapeRayCastInput& input, hkpShapeRayCastOutput& results) const
{
    hkVector4 invScale; invScale.setReciprocal<HK_ACC_23_BIT, HK_DIV_IGNORE>(m_transform.m_scale);

    hkpShapeRayCastInput subInput = input;
    hkVector4 from; from.setSub(input.m_from, m_transform.m_translation);
    subInput.m_from._setRotatedInverseDir(m_transform.m_rotation, from);
    subInput.m_from.mul(invScale);
    hkVector4 to; to.setSub(input.m_to, m_transform.m_translation);
    subInput.m_to._setRotatedInverseDir(m_transform.m_rotation, to);
    subInput.m_to.mul(invScale);

    results.changeLevel(1);
    const hkBool hit = getChildShape()->castRay(subInput, results);
    results.changeLevel(-1);
    if (hit)
    {
        hkVector4 normal; normal.setMul(results.m_normal, invScale);
        results.m_normal._setRotatedDir(m_transform.m_rotation, normal);
        results.m_normal.normalize<3, HK_ACC_23_BIT, HK_SQRT_SET_ZERO>();
        results.setKey(0);
    }
    return hit;
}
