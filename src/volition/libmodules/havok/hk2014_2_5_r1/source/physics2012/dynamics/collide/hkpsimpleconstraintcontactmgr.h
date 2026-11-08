// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/collide/hkpsimpleconstraintcontactmgr.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/contactmgr/hkpcontactmgr.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/dispatch/contactmgr/hkpcontactmgrfactory.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/collide/hkpdynamicscontactmgr.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/contact/hkpsimplecontactconstraintdata.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/hkpconstraintinstance.h"

class hkBool;
class hkClass;
class hkContactPoint;
struct hkContainerHeapAllocator;
class hkFinishLoadedObjectFlag;
struct hkResult;
struct hkpBodyVelocity;
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
struct hkpSimpleConstraintInfoInitInput;
struct hkpSimpleContactConstraintAtom;
struct hkpToiEvent;
class hkpWorld;

// size 0x80
class hkpSimpleConstraintContactMgr : public hkpDynamicsContactMgr
{
public:
    // size 0xC
    class Factory : public hkpContactMgrFactory
    {
    public:
        HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int nbytes) 0x00DEA7C0, delete(void* p, unsigned int nbytes) 0x00E1B5A0
        Factory(hkpWorld* mgr); // 0x00E1A970
        virtual ~Factory(); // vtbl+0x00 0x00E1B590
        virtual hkpContactMgr* createContactMgr(const hkpCollidable& a, const hkpCollidable& b, const hkpCollisionInput& input); // vtbl+0x10 0x00E1B100

    public:
        hkpWorld* m_world; // 0x08
    };

    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int nbytes) 0x00E1B340, delete(void* p, unsigned int nbytes) 0x00E1B8C0
    hkpSimpleConstraintContactMgr(hkFinishLoadedObjectFlag);
    hkpSimpleConstraintContactMgr(hkpWorld* sm, hkpRigidBody* bodyA, hkpRigidBody* bodyB); // 0x00E1AF80
    virtual ~hkpSimpleConstraintContactMgr(); // vtbl+0x00 0x00E1B0A0
    virtual unsigned short addContactPointImpl(const hkpCdBody& a, const hkpCdBody& b, const hkpProcessCollisionInput& input, hkpProcessCollisionOutput& output, const hkpGskCache* contactCache, hkContactPoint& cp); // vtbl+0x10 0x00E1AAD0
    virtual hkResult reserveContactPointsImpl(int numPoints); // vtbl+0x14 0x00E1A4C0
    void setRollingFrictionMultiplier(float multiplier); // 0x00E1B360
    float getRollingFrictionMultiplier() const;
    virtual void removeContactPointImpl(unsigned short cpId, hkpConstraintOwner& constraintOwner); // vtbl+0x18 0x00E1A840
    virtual void processContactImpl(const hkpCollidable& a, const hkpCollidable& b, const hkpProcessCollisionInput& input, hkpProcessCollisionData& collisionData); // vtbl+0x1C 0x00E1ADE0
    virtual hkpContactMgr::ToiAccept addToiImpl(const hkpCdBody& a, const hkpCdBody& b, const hkpProcessCollisionInput& input, hkpProcessCollisionOutput& output, float toi, hkContactPoint& cp, const hkpGskCache* gskCache, float& projectedVelocity, hkpContactPointProperties& properties); // vtbl+0x20 0x00E1A600
    virtual void removeToiImpl(hkpConstraintOwner& constraintOwner, hkpContactPointProperties& properties); // vtbl+0x24 0x00E1A580
    virtual void cleanup(); // vtbl+0x28 0x00E1B890
    virtual hkpContactPointProperties* getContactPointProperties(unsigned short id); // vtbl+0x2C 0x00E1A930
    virtual hkContactPoint* getContactPoint(unsigned short id); // vtbl+0x30 0x00E1A910
    virtual void getAllContactPointIds(hkArray<unsigned short,hkContainerHeapAllocator>& contactPointIds) const; // vtbl+0x34 0x00E1ADD0
    virtual hkpContactMgr::Type getType() const; // vtbl+0x38 0x00E1B8A0
    virtual void toiCollisionResponseBeginCallback(const hkContactPoint& cp, hkpSimpleConstraintInfoInitInput& inA, hkpBodyVelocity& velA, hkpSimpleConstraintInfoInitInput& inB, hkpBodyVelocity& velB); // vtbl+0x3C 0x00E1A510
    virtual void toiCollisionResponseEndCallback(const hkContactPoint& cp, float impulseApplied, hkpSimpleConstraintInfoInitInput& inA, hkpBodyVelocity& velA, hkpSimpleConstraintInfoInitInput& inB, hkpBodyVelocity& velB); // vtbl+0x40 0x00E1A520
    const hkpSimpleContactConstraintAtom* getAtom() const;
    hkpSimpleContactConstraintAtom* getAtom(); // 0x00E1E2D0
    virtual hkpConstraintInstance* getConstraintInstance(); // vtbl+0x44 0x00E1A4A0
    virtual hkBool fireCallbacksForEarliestToi(hkpToiEvent& event, float& rotateNormal); // vtbl+0x48 0x00E1A990
    virtual void confirmToi(hkpToiEvent& event, float rotateNormal, hkArray<hkpEntity*,hkContainerHeapAllocator>& outToBeActivated); // vtbl+0x4C 0x00E1AA80
    virtual const hkpConstraintInstance* getConstraintInstance() const; // vtbl+0x50 0x00E1A4B0
    hkBool isConstraintDisabled() const; // 0x00E1B6B0
public:
    hkpRigidBody* setContactPointProperties(const hkpCdBody& a, const hkpCdBody& b, int maxNumExtraDataInEvent, hkpContactPointProperties* cpi); // 0x00E1B390
    virtual const hkClass* getClassType() const; // vtbl+0x08 0x00E1B8B0
    static const hkClass& staticClass();

public:
    unsigned short m_reservedContactPoints; // 0x10
    unsigned short m_contactPointCallbackDelay; // 0x12
    hkpSimpleContactConstraintData m_contactConstraintData; // 0x14
    hkpConstraintInstance m_constraint; // 0x44
    unsigned int m_pad[1]; // 0x7C
};
