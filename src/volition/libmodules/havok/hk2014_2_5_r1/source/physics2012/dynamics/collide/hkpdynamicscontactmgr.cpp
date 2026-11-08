// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/collide/hkpdynamicscontactmgr.cpp
//
// Havok 2014.2.5 (havok_impl.md HK8): the base of the contact managers that create dynamics (constraints, TOI
// responses), from rfg.exe. The members are inline in the SDK header; their out-of-line copies are listed here.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/collide/hkpdynamicscontactmgr.h"

// 0x00E19EC0
hkpDynamicsContactMgr::hkpDynamicsContactMgr(hkpContactMgr::Type type)
    : hkpContactMgr(type)
{
}

// 0x00E19FA0
hkpDynamicsContactMgr::~hkpDynamicsContactMgr()
{
}

// 0x00E19EE0
hkpContactPointProperties* hkpDynamicsContactMgr::getContactPointProperties(unsigned short id)
{
    return nullptr;
}

// 0x00E19EF0
hkContactPoint* hkpDynamicsContactMgr::getContactPoint(unsigned short id)
{
    return nullptr;
}

// 0x00E19F00
void hkpDynamicsContactMgr::getAllContactPointIds(hkArray<unsigned short, hkContainerHeapAllocator>& contactPointIds) const
{
}

// 0x00E19F10
void hkpDynamicsContactMgr::toiCollisionResponseBeginCallback(const hkContactPoint& cp, hkpSimpleConstraintInfoInitInput& inA, hkpBodyVelocity& velA, hkpSimpleConstraintInfoInitInput& inB, hkpBodyVelocity& velB)
{
}

// 0x00E19F20
void hkpDynamicsContactMgr::toiCollisionResponseEndCallback(const hkContactPoint& cp, float impulseApplied, hkpSimpleConstraintInfoInitInput& inA, hkpBodyVelocity& velA, hkpSimpleConstraintInfoInitInput& inB, hkpBodyVelocity& velB)
{
}

// 0x00E19F30
hkpConstraintInstance* hkpDynamicsContactMgr::getConstraintInstance()
{
    return nullptr;
}

// 0x00E19F40
hkBool hkpDynamicsContactMgr::fireCallbacksForEarliestToi(hkpToiEvent& event, float& rotateNormal)
{
    return false;
}

// 0x00E19F50
void hkpDynamicsContactMgr::confirmToi(hkpToiEvent& event, float rotateNormal, hkArray<hkpEntity*, hkContainerHeapAllocator>& outToBeActivated)
{
}
