// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/misc/bv/hkpbvshape.cpp
//
// Havok 2014.2.5 (havok_impl.md HK3): a child shape behind a bounding volume shape (which gives the AABB and the
// broadphase/phantom volume), from rfg.exe. The shipped build records "rcBvShape" monitor timers (TLS
// hkMonitorStream) around the ray casts; they are not reproduced.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/misc/bv/hkpbvshape.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcdbody.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/query/hkpshaperaycastinput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/query/hkpshaperaycastoutput.h"

// 0x00D9CB20: both shapes get a reference
hkpBvShape::hkpBvShape(const hkpShape* boundingVolumeShape, const hkpShape* childShape)
    : hkpShape(hkcdShapeType::BV)
    , m_boundingVolumeShape(boundingVolumeShape)
    , m_childShape(childShape, hkpShapeContainer::REFERENCE_POLICY_INCREMENT)
{
    m_boundingVolumeShape->addReference();
}

// 0x00D9CB90
hkpBvShape::hkpBvShape(hkFinishLoadedObjectFlag flag)
    : hkpShape(flag)
    , m_childShape(flag)
{
    m_type.m_storage = hkcdShapeType::BV;
}

// 0x00D9CBC0 (the child's reference goes with m_childShape)
hkpBvShape::~hkpBvShape()
{
    m_boundingVolumeShape->removeReference();
}

// 0x00D9CAC0
const hkpShape* hkpBvShape::getBoundingVolumeShape() const
{
    return m_boundingVolumeShape;
}

// 0x00D9CAD0
const hkpShape* hkpBvShape::getChildShape() const
{
    return m_childShape.getChild();
}

// 0x00D9CB10
const hkpShapeContainer* hkpBvShape::getContainer() const
{
    return &m_childShape;
}

// 0x00D9CAE0: the bounding volume's box
void hkpBvShape::getAabb(const hkTransformf& localToWorld, float tolerance, hkAabb& out) const
{
    m_boundingVolumeShape->getAabb(localToWorld, tolerance, out);
}

// 0x00D9CC00: the child, one key level down (key 0)
hkBool hkpBvShape::castRay(const hkpShapeRayCastInput& input, hkpShapeRayCastOutput& results) const
{
    results.changeLevel(1);
    const hkBool hit = m_childShape.getChild()->castRay(input, results);
    results.changeLevel(-1);
    if (hit)
    {
        results.setKey(0);
    }
    return hit;
}

// 0x00D9CCA0: the child as key 0 under the bv shape's body
void hkpBvShape::castRayWithCollector(const hkpShapeRayCastInput& input, const hkpCdBody& cdBody, hkpRayHitCollector& collector) const
{
    const hkpShape* child = m_childShape.getChild();
    hkpCdBody body(&cdBody);
    body.setShape(child, 0);
    child->castRayWithCollector(input, body, collector);
}
