// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/collide/hkpdynamicscontactmgr.h
//
// Seam only (Havok 2014.2.5): the virtual interface (vtable layout) and data, no implementation.
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/contactmgr/hkpcontactmgr.h"

class hkpConstraintInstance;
class hkpEntity;
class hkpWorld;
struct hkpBodyVelocity;
struct hkpSimpleConstraintInfoInitInput;
struct hkpToiEvent;

// size 0x10
class hkpDynamicsContactMgr : public hkpContactMgr
{
public:
    hkpDynamicsContactMgr(hkpContactMgr::Type type); // 0x00E19EC0
    virtual ~hkpDynamicsContactMgr(); // vtbl+0x00 0x00E19FA0
    virtual hkpContactPointProperties* getContactPointProperties(unsigned short id); // vtbl+0x2C 0x00E19EE0
    virtual hkContactPoint* getContactPoint(unsigned short id); // vtbl+0x30 0x00E19EF0
    virtual void getAllContactPointIds(hkArray<unsigned short, hkContainerHeapAllocator>& contactPointIds) const; // vtbl+0x34 0x00E19F00
    virtual hkpContactMgr::Type getType() const = 0; // vtbl+0x38
    virtual void toiCollisionResponseBeginCallback(const hkContactPoint& cp, hkpSimpleConstraintInfoInitInput& inA, hkpBodyVelocity& velA, hkpSimpleConstraintInfoInitInput& inB, hkpBodyVelocity& velB); // vtbl+0x3C 0x00E19F10
    virtual void toiCollisionResponseEndCallback(const hkContactPoint& cp, float impulseApplied, hkpSimpleConstraintInfoInitInput& inA, hkpBodyVelocity& velA, hkpSimpleConstraintInfoInitInput& inB, hkpBodyVelocity& velB); // vtbl+0x40 0x00E19F20
    virtual hkpConstraintInstance* getConstraintInstance(); // vtbl+0x44 0x00E19F30
    virtual hkBool fireCallbacksForEarliestToi(hkpToiEvent& event, float& rotateNormal); // vtbl+0x48 0x00E19F40
    virtual void confirmToi(hkpToiEvent& event, float rotateNormal, hkArray<hkpEntity*, hkContainerHeapAllocator>& outToBeActivated); // vtbl+0x4C 0x00E19F50

protected:
    hkpWorld* m_world; // 0x0C
};
