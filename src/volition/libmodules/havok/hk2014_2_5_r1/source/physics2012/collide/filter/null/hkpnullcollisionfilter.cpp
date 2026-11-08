// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/filter/null/hkpnullcollisionfilter.cpp
//
// Havok 2014.2.5 (havok_impl.md HK4): the filter that enables everything, from rfg.exe (the constructor and the
// tests are inline in the SDK header).

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/filter/null/hkpnullcollisionfilter.h"

// 0x00D7EED0 (stub_impl.md S4 SD)
hkpNullCollisionFilter::hkpNullCollisionFilter(hkFinishLoadedObjectFlag flag)
    : hkpCollisionFilter(flag)
{
    if (flag.m_finishing)
    {
        m_type.m_storage = HK_FILTER_NULL;
    }
}

// 0x00D9ED00 (.h:24)
hkpNullCollisionFilter::hkpNullCollisionFilter()
{
    m_type.m_storage = HK_FILTER_NULL;
}

// 0x00DAA7F0
hkpNullCollisionFilter::~hkpNullCollisionFilter()
{
}

// 0x00D7EE50 (.h:54)
void hkpNullCollisionFilter::afterReflectNew()
{
    m_type.m_storage = HK_FILTER_NULL;
}

// 0x00D7EF30 (.h:30)
hkBool hkpNullCollisionFilter::isCollisionEnabled(const hkpCollidable& a, const hkpCollidable& b) const
{
    return true;
}

// 0x00D7EF40 (.h:33)
hkBool hkpNullCollisionFilter::isCollisionEnabled(const hkpCollisionInput& input, const hkpCdBody& a, const hkpCdBody& b, const hkpShapeContainer& bContainer, unsigned int bKey) const
{
    return true;
}

// 0x00D7EF50 (.h:36)
hkBool hkpNullCollisionFilter::isCollisionEnabled(const hkpCollisionInput& input, const hkpCdBody& collectionBodyA, const hkpCdBody& collectionBodyB, const hkpShapeContainer& containerShapeA, const hkpShapeContainer& containerShapeB, unsigned int keyA, unsigned int keyB) const
{
    return true;
}

// 0x00D7EF60 (.h:39)
hkBool hkpNullCollisionFilter::isCollisionEnabled(const hkpShapeRayCastInput& aInput, const hkpShapeContainer& bContainer, unsigned int bKey) const
{
    return true;
}

// 0x00D7EF70 (.h:42)
hkBool hkpNullCollisionFilter::isCollisionEnabled(const hkpWorldRayCastInput& a, const hkpCollidable& collidableB) const
{
    return true;
}
