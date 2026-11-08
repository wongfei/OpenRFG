// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/filter/defaultconvexlist/hkpdefaultconvexlistfilter.cpp
//
// Havok 2014.2.5 (havok_impl.md HK4): how convex list shapes collide (the world's default convex list filter), from
// rfg.exe. The constructors and destructors are inline in the SDK headers.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/filter/defaultconvexlist/hkpdefaultconvexlistfilter.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcdbody.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpcollisioninput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/dispatch/hkpcollisiondispatcher.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/hkpshape.h"

// 0x00D9D0C0
hkpConvexListFilter::hkpConvexListFilter()
{
}

// 0x00D7F300 (stub_impl.md S4 SD)
hkpConvexListFilter::hkpConvexListFilter(hkFinishLoadedObjectFlag flag)
    : hkReferencedObject(flag)
{
}

// 0x00D7F310
hkpConvexListFilter::~hkpConvexListFilter()
{
}

// 0x00D7F320 (stub_impl.md S4 SD)
hkpDefaultConvexListFilter::hkpDefaultConvexListFilter(hkFinishLoadedObjectFlag flag)
    : hkpConvexListFilter(flag)
{
}

// 0x00D9D0D0
hkpDefaultConvexListFilter::hkpDefaultConvexListFilter()
{
}

// 0x00D7F330
hkpDefaultConvexListFilter::~hkpDefaultConvexListFilter()
{
}

// 0x00DAB4D0: as a list against shapes that can act as a BV tree (the dispatcher's alternate types, bit
// hkcdShapeType::BV_TREE = 22), else as a convex shape.
hkpConvexListFilter::ConvexListCollisionType hkpDefaultConvexListFilter::getConvexListCollisionType(const hkpCdBody& convexListBody, const hkpCdBody& otherBody, const hkpCollisionInput& input) const
{
    const unsigned int alternates = input.m_dispatcher.val()->m_hasAlternateType[otherBody.m_shape->m_type.m_storage];
    return (ConvexListCollisionType)((alternates >> hkcdShapeType::BV_TREE) & TREAT_CONVEX_LIST_AS_LIST);
}
