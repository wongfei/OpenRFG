// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/convex/convextranslate/hkpconvextranslateshape.cpp
//
// Havok 2014.2.5 (havok_impl.md HK3): a convex child shape moved by a translation, with the out-of-line copies of its
// inline members (hkpConvexTranslateShape.inl in Havok 2014.1).

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/convex/convextranslate/hkpconvextranslateshape.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/matrix/hktransformf.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/geometry/aabb/hkaabb.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/geometry/internal/types/hkcdvertex.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcdbody.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/query/hkprayhitcollector.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/query/hkpshaperaycastinput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/query/hkpshaperaycastoutput.h"

// 0x00D74F00
hkpConvexTranslateShape::hkpConvexTranslateShape(hkFinishLoadedObjectFlag flag)
    : hkpConvexTransformShapeBase(flag)
{
    m_type.m_storage = hkcdShapeType::CONVEX_TRANSLATE;
}

// 0x0071B650 (inline in Havok, hkpConvexTranslateShape.inl): the translation's w is cleared
hkpConvexTranslateShape::hkpConvexTranslateShape(const hkpConvexShape* childShape, const hkVector4f& translation, hkpShapeContainer::ReferencePolicy ref)
    : hkpConvexTransformShapeBase(hkcdShapeType::CONVEX_TRANSLATE, childShape->getRadius(), childShape, ref)
{
    m_translation.setXYZ_0(translation);
    m_childShapeSizeForSpu = 0;
}

hkpConvexTranslateShape::hkpConvexTranslateShape()
{
}

// 0x0071B7B0
hkpConvexTranslateShape::~hkpConvexTranslateShape()
{
}

// 0x0071B750 (hkpconvextranslateshape.inl:10)
const hkpConvexShape* hkpConvexTranslateShape::getChildShape() const
{
    return static_cast<const hkpConvexShape*>(m_childShape.getChild());
}

// 0x0080D5D0 (hkpconvextranslateshape.inl:29)
const hkVector4f& hkpConvexTranslateShape::getTranslation() const
{
    return m_translation;
}

// 0x0080D5E0 (hkpconvextranslateshape.inl:34)
hkVector4f& hkpConvexTranslateShape::getTranslation()
{
    return m_translation;
}

// 0x00D74CF0: the child's size, or the child placed right after this shape
int hkpConvexTranslateShape::calcSizeForSpu(const hkpShape::CalcSizeForSpuInput& input, int spuBufferSizeLeft) const
{
    spuBufferSizeLeft -= sizeof(*this);
    const int childSize = getChildShape()->calcSizeForSpu(input, spuBufferSizeLeft);
    if (childSize < 0 || childSize > spuBufferSizeLeft)
    {
        return -1;
    }
    hkpConvexTranslateShape* self = const_cast<hkpConvexTranslateShape*>(this);
    if ((const void*)m_childShape.getChild() == (const void*)(this + 1))
    {
        self->m_childShapeSizeForSpu = 0;
        return sizeof(*this) + childSize;
    }
    self->m_childShapeSizeForSpu = childSize;
    return sizeof(*this);
}

// 0x00D74D40
const hkpShapeContainer* hkpConvexTranslateShape::getContainer() const
{
    return &m_childShape;
}

// 0x00D74D50
void hkpConvexTranslateShape::getSupportingVertex(const hkVector4f& direction, hkcdVertex& supportingVertexOut) const
{
    getChildShape()->getSupportingVertex(direction, supportingVertexOut);
    hkVector4 v; v.setAdd(supportingVertexOut, m_translation);
    supportingVertexOut.setXYZ(v); // the vertex id in w stays
}

// 0x00D74D90
void hkpConvexTranslateShape::convertVertexIdsToVertices(const unsigned short* ids, int numIds, hkcdVertex* verticesOut) const
{
    getChildShape()->convertVertexIdsToVertices(ids, numIds, verticesOut);
    for (int i = 0; i < numIds; i++)
    {
        hkVector4 v; v.setAdd(verticesOut[i], m_translation);
        verticesOut[i].setXYZ(v);
    }
}

// 0x00D74DE0
void hkpConvexTranslateShape::getCentre(hkVector4f& centreOut) const
{
    getChildShape()->getCentre(centreOut);
    centreOut.add(m_translation);
}

// 0x00D74E10
void hkpConvexTranslateShape::getFirstVertex(hkVector4f& v) const
{
    getChildShape()->getFirstVertex(v);
    v.add(m_translation);
}

// 0x0071B760 (hkpconvextranslateshape.inl:71)
int hkpConvexTranslateShape::getNumCollisionSpheres() const
{
    return getChildShape()->getNumCollisionSpheres();
}

// 0x00D74E40: the child's spheres moved (the radius in w gets the translation's w, which is 0)
const hkSphere* hkpConvexTranslateShape::getCollisionSpheres(hkSphere* sphereBuffer) const
{
    const hkSphere* spheres = getChildShape()->getCollisionSpheres(sphereBuffer);
    const int numSpheres = getChildShape()->getNumCollisionSpheres();
    const hkVector4* in = reinterpret_cast<const hkVector4*>(spheres);
    hkVector4* out = reinterpret_cast<hkVector4*>(sphereBuffer);
    for (int i = 0; i < numSpheres; i++)
    {
        out[i].setAdd(in[i], m_translation);
    }
    return sphereBuffer;
}

// 0x00D74E90
void hkpConvexTranslateShape::getAabb(const hkTransformf& localToWorld, float tolerance, hkAabb& out) const
{
    getChildShape()->getAabb(localToWorld, tolerance, out);
    hkVector4 translationWs; translationWs._setRotatedDir(localToWorld.getRotation(), m_translation);
    out.m_min.add(translationWs);
    out.m_max.add(translationWs);
}

// 0x00D74F30
float hkpConvexTranslateShape::getMaximumProjection(const hkVector4f& direction) const
{
    const float childProjection = getChildShape()->getMaximumProjection(direction);
    return childProjection + m_translation.dot<3>(direction).getReal();
}

// 0x00D74F80
// The ray in the child's space; a hit gets key 0 at this level. The shipped build records a "rcConvTransl" monitor
// timer around the query (TLS hkMonitorStream); it is not reproduced.
hkBool hkpConvexTranslateShape::castRay(const hkpShapeRayCastInput& input, hkpShapeRayCastOutput& results) const
{
    hkpShapeRayCastInput subInput = input;
    subInput.m_from.setSub(input.m_from, m_translation);
    subInput.m_to.setSub(input.m_to, m_translation);

    results.changeLevel(1);
    const hkBool hit = getChildShape()->castRay(subInput, results);
    results.changeLevel(-1);
    if (hit)
    {
        results.setKey(0);
    }
    return hit;
}

// 0x00D75080
void hkpConvexTranslateShape::castRayWithCollector(const hkpShapeRayCastInput& input, const hkpCdBody& cdBody, hkpRayHitCollector& collector) const
{
    hkpShapeRayCastInput subInput = input;
    subInput.m_from.setSub(input.m_from, m_translation);
    subInput.m_to.setSub(input.m_to, m_translation);

    hkTransform localToParent;
    localToParent.getRotation().setIdentity();
    localToParent.getTranslation() = m_translation;
    hkTransform childToWorld; childToWorld.setMul(cdBody.getTransform(), localToParent);

    hkpCdBody childBody(&cdBody, &childToWorld);
    childBody.setShape(getChildShape(), 0);
    getChildShape()->castRayWithCollector(subInput, childBody, collector);
}
