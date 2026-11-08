// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/query/collector/raycollector/hkpclosestrayhitcollector.cpp
//
// Havok 2014.2.5 (havok_impl.md HK3): the ray hit collector that keeps the closest hit, with the out-of-line copies of
// its inline members (hkpClosestRayHitCollector.inl in Havok 2014.1).

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/query/collector/raycollector/hkpclosestrayhitcollector.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcdbody.h"

// 0x0080D8B0 (hkpclosestrayhitcollector.inl:16)
hkpClosestRayHitCollector::hkpClosestRayHitCollector()
{
    reset();
}

// 0x0080D880 (hkpclosestrayhitcollector.inl:10)
void hkpClosestRayHitCollector::reset()
{
    m_rayHit.reset();
    hkpRayHitCollector::reset();
}

// 0x0080D8E0 (hkpclosestrayhitcollector.inl:21)
hkpClosestRayHitCollector::~hkpClosestRayHitCollector()
{
}

// 0x0080D8F0 (hkpclosestrayhitcollector.inl:25)
hkBool hkpClosestRayHitCollector::hasHit() const
{
    return m_rayHit.m_rootCollidable != nullptr;
}

// 0x0080D900 (hkpclosestrayhitcollector.inl:30)
const hkpWorldRayCastOutput& hkpClosestRayHitCollector::getHit() const
{
    return m_rayHit;
}

// 0x00D7FC60
void hkpClosestRayHitCollector::addRayHit(const hkpCdBody& cdBody, const hkpShapeRayCastCollectorOutput& hitInfo)
{
    if (hitInfo.m_hitFraction < m_rayHit.m_hitFraction)
    {
        static_cast<hkpShapeRayCastCollectorOutput&>(m_rayHit) = hitInfo;
        hkpRayHitCollector::shapeKeysFromCdBody(m_rayHit.m_shapeKeys, hkpShapeRayCastOutput::MAX_HIERARCHY_DEPTH, cdBody);

        // hkpCdBody::getRootCollidable
        const hkpCdBody* root = &cdBody;
        while (root->m_parent != nullptr)
        {
            root = root->m_parent;
        }
        m_rayHit.m_rootCollidable = (const hkpCollidable*)root;
        m_earlyOutHitFraction = hitInfo.m_hitFraction;
    }
}
