// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/collide/deprecated/hkpreportcontactmgr.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/contactmgr/hkpcontactmgr.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/dispatch/contactmgr/hkpcontactmgrfactory.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/collide/hkpdynamicscontactmgr.h"

class hkContactPoint;
struct hkContainerHeapAllocator;
struct hkResult;
class hkpCdBody;
class hkpCollidable;
struct hkpCollisionInput;
class hkpConstraintOwner;
class hkpContactPointProperties;
class hkpEntity;
class hkpGskCache;
struct hkpProcessCollisionData;
struct hkpProcessCollisionInput;
struct hkpProcessCollisionOutput;
class hkpRigidBody;
struct hkpToiEvent;
class hkpWorld;

// size 0x1C
class hkpReportContactMgr : public hkpDynamicsContactMgr
{
public:
    // size 0xC
    class Factory : public hkpContactMgrFactory
    {
    public:
        HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int nbytes) 0x00DEA7E0, delete(void* p, unsigned int nbytes) 0x00E1A300
        Factory(hkpWorld* mgr); // 0x00E19E40
        virtual ~Factory(); // vtbl+0x00 0x00E1A2F0
        virtual hkpContactMgr* createContactMgr(const hkpCollidable& a, const hkpCollidable& b, const hkpCollisionInput& env); // vtbl+0x10 0x00E19E60

    public:
        hkpWorld* m_world; // 0x08
    };

    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int nbytes) 0x00E19FB0, delete(void* p, unsigned int nbytes) 0x00E1A2B0
    hkpReportContactMgr(hkpWorld* sm, hkpRigidBody* bodyA, hkpRigidBody* bodyB); // 0x00E198A0
    virtual ~hkpReportContactMgr(); // vtbl+0x00 0x00E198F0
    virtual unsigned short addContactPointImpl(const hkpCdBody& a, const hkpCdBody& b, const hkpProcessCollisionInput& input, hkpProcessCollisionOutput& output, const hkpGskCache* contactCache, hkContactPoint& pcp); // vtbl+0x10 0x00E19900
    virtual hkResult reserveContactPointsImpl(int numPoints); // vtbl+0x14 0x00E1A280
    virtual void removeContactPointImpl(unsigned short cpId, hkpConstraintOwner& info); // vtbl+0x18 0x00E19C10
    virtual void processContactImpl(const hkpCollidable& a, const hkpCollidable& b, const hkpProcessCollisionInput& input, hkpProcessCollisionData& collisionData); // vtbl+0x1C 0x00E19C90
    virtual hkpContactMgr::ToiAccept addToiImpl(const hkpCdBody& a, const hkpCdBody& b, const hkpProcessCollisionInput& input, hkpProcessCollisionOutput& output, float toi, hkContactPoint& cp, const hkpGskCache* gskCache, float& projectedVelocity, hkpContactPointProperties& properties); // vtbl+0x20 0x00E19A80
    virtual void removeToiImpl(hkpConstraintOwner& constraintOwner, hkpContactPointProperties& properties); // vtbl+0x24 0x00E19B90
    virtual void cleanup(); // vtbl+0x28 0x00E1A290
    virtual hkpContactMgr::Type getType() const; // vtbl+0x38 0x00E1A2A0
    virtual void confirmToi(hkpToiEvent& event, float rotateNormal, hkArray<hkpEntity*,hkContainerHeapAllocator>& outToBeActivated); // vtbl+0x4C 0x00E19D80

public:
    hkpRigidBody* m_bodyA; // 0x10
    hkpRigidBody* m_bodyB; // 0x14
    short m_skipNextNprocessCallbacks; // 0x18
};
