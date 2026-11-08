// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/dispatch/contactmgr/hkpnullcontactmgr.h
//
// Havok 2014.2.5 (havok_impl.md HK7): the contact manager that keeps nothing (response RESPONSE_NONE) and its
// factory, which hands out its one manager for every pair; from rfg.exe (inline in the SDK headers, emitted with
// hkpworld.cpp).

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/dispatch/contactmgr/hkpnullcontactmgr.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/dispatch/contactmgr/hkpnullcontactmgrfactory.h"

// 0x00DE85C0
hkpNullContactMgr::hkpNullContactMgr()
    : hkpContactMgr(TYPE_NULL_CONTACT_MGR)
{
}

// 0x00DE8650
hkpNullContactMgr::~hkpNullContactMgr()
{
}

// 0x00DE85E0: every point gets id 0
unsigned short hkpNullContactMgr::addContactPointImpl(const hkpCdBody& a, const hkpCdBody& b, const hkpProcessCollisionInput& env, hkpProcessCollisionOutput& output, const hkpGskCache* contactCache, hkContactPoint& cp)
{
    return 0;
}

// 0x00DE8630
hkResult hkpNullContactMgr::reserveContactPointsImpl(int numPoints)
{
    return hkResult(HK_SUCCESS);
}

// 0x00DE85F0
void hkpNullContactMgr::removeContactPointImpl(unsigned short cpId, hkpConstraintOwner& constraintOwner)
{
}

// 0x00DE8600
void hkpNullContactMgr::processContactImpl(const hkpCollidable& a, const hkpCollidable& b, const hkpProcessCollisionInput& input, hkpProcessCollisionData& collisionData)
{
}

// 0x00DE8610: every TOI is accepted
hkpContactMgr::ToiAccept hkpNullContactMgr::addToiImpl(const hkpCdBody& a, const hkpCdBody& b, const hkpProcessCollisionInput& input, hkpProcessCollisionOutput& output, float toi, hkContactPoint& cp, const hkpGskCache* gskCache, float& projectedVelocity, hkpContactPointProperties& properties)
{
    return TOI_ACCEPT;
}

// 0x00DE8620
void hkpNullContactMgr::removeToiImpl(hkpConstraintOwner& constraintOwner, hkpContactPointProperties& properties)
{
}

// 0x00DE8640
void hkpNullContactMgr::cleanup()
{
}

// 0x00DE9FA0
hkpNullContactMgrFactory::hkpNullContactMgrFactory()
{
}

// 0x00DE9FE0
hkpNullContactMgrFactory::~hkpNullContactMgrFactory()
{
}

// 0x00DE9FD0
hkpContactMgr* hkpNullContactMgrFactory::createContactMgr(const hkpCollidable& a, const hkpCollidable& b, const hkpCollisionInput& env)
{
    return &m_contactMgr;
}
