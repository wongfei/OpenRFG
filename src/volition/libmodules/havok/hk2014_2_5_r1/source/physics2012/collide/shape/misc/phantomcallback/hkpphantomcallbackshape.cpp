// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/misc/phantomcallback/hkpphantomcallbackshape.cpp
//
// Havok 2014.2.5 (havok_impl.md HK3): the base of shapes that report bodies entering and leaving their bounding
// volume (the child of an hkpBvShape), from rfg.exe. It has no volume of its own: rays never hit it.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/misc/phantomcallback/hkpphantomcallbackshape.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/geometry/aabb/hkaabb.h"

// 0x00815DE0 (.h:36)
hkpPhantomCallbackShape::hkpPhantomCallbackShape()
    : hkpShape(hkcdShapeType::PHANTOM_CALLBACK)
{
}

// 0x00D83F10
hkpPhantomCallbackShape::hkpPhantomCallbackShape(hkFinishLoadedObjectFlag flag)
    : hkpShape(flag)
{
    m_type.m_storage = hkcdShapeType::PHANTOM_CALLBACK;
}

// 0x0080D970
hkpPhantomCallbackShape::~hkpPhantomCallbackShape()
{
}

// 0x00D83EE0: an empty box at the origin
void hkpPhantomCallbackShape::getAabb(const hkTransformf& localToWorld, float tolerance, hkAabb& out) const
{
    out.m_min.setZero();
    out.m_max.setZero();
}

// 0x00D83F00
hkBool hkpPhantomCallbackShape::castRay(const hkpShapeRayCastInput& input, hkpShapeRayCastOutput& results) const
{
    return false;
}

// 0x00D83ED0
void hkpPhantomCallbackShape::castRayWithCollector(const hkpShapeRayCastInput& input, const hkpCdBody& cdBody, hkpRayHitCollector& collector) const
{
}
