// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/collide/deprecated/hkpcollisionevents.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0). The constructors from the .inl (HK8). Hand-maintained.
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"

class hkContactPoint;
class hkpCdBody;
class hkpConstraintOwner;
class hkpContactPointProperties;
class hkpDynamicsContactMgr;
class hkpEntity;
class hkpGskCache;
struct hkpManifoldPointAddedEvent;
struct hkpProcessCollisionInput;
struct hkpProcessCollisionOutput;
struct hkpSimpleContactConstraintAtom;
struct hkpToiPointAddedEvent;

enum hkpContactPointAccept
{
    HK_CONTACT_POINT_ACCEPT = 0,
    HK_CONTACT_POINT_REJECT = 1,
};

// size 0x30
struct hkpContactPointAddedEvent
{
    enum Type
    {
        TYPE_TOI = 0,
        TYPE_MANIFOLD = 1,
    };

    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpContactPointAddedEvent);
    int getEntityIndex(const hkpEntity*) const;
    const hkpSimpleContactConstraintAtom* getAtom() const;
    int getNumExtendedUserDatas(const hkpEntity*) const;
    void getExtendedUserDatas(const hkpEntity*, unsigned int*, int) const;
    void setExtendedUserDatas(const hkpEntity*, unsigned int*, int) const;
    hkBool isToi() const;
    hkpToiPointAddedEvent& asToiEvent();
    hkpManifoldPointAddedEvent& asManifoldEvent(); // 0x00711020 (hkpcollisionevents.inl:120), inline below
    // 0x00E19FD0 (hkpcollisionevents.inl): m_callbackFiredFrom is set by the callback utils
    hkpContactPointAddedEvent(hkpDynamicsContactMgr* contactMgr, const hkpProcessCollisionInput* collisionInput, hkpProcessCollisionOutput* collisionOutput, const hkpCdBody* a, const hkpCdBody* b, hkContactPoint* ccp, const hkpGskCache* gskCache, hkpContactPointProperties* dcp, float projectedVelocity, Type pointType)
        : m_bodyA(a)
        , m_bodyB(b)
        , m_type{pointType}
        , m_contactPoint(ccp)
        , m_gskCache(gskCache)
        , m_contactPointProperties(dcp)
        , m_projectedVelocity(projectedVelocity)
        , m_status(HK_CONTACT_POINT_ACCEPT)
        , m_internalContactMgr(contactMgr)
        , m_collisionInput(collisionInput)
        , m_collisionOutput(collisionOutput)
    {
    }

    const hkpCdBody* m_bodyA; // 0x00
    const hkpCdBody* m_bodyB; // 0x04
    const hkEnum<enum Type,int> m_type; // 0x08
    hkpEntity* m_callbackFiredFrom; // 0x0C
    const hkContactPoint* m_contactPoint; // 0x10
    const hkpGskCache* m_gskCache; // 0x14
    hkpContactPointProperties* m_contactPointProperties; // 0x18
    float m_projectedVelocity; // 0x1C
    hkpContactPointAccept m_status; // 0x20
    hkpDynamicsContactMgr* m_internalContactMgr; // 0x24
    const hkpProcessCollisionInput* m_collisionInput; // 0x28
    hkpProcessCollisionOutput* m_collisionOutput; // 0x2C
};

// size 0x34
struct hkpManifoldPointAddedEvent : public hkpContactPointAddedEvent
{
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpManifoldPointAddedEvent);
    // 0x00E1A090 (hkpcollisionevents.inl): the delay is set by the caller
    hkpManifoldPointAddedEvent(unsigned short contactPointId, hkpDynamicsContactMgr* contactMgr, const hkpProcessCollisionInput* collisionInput, hkpProcessCollisionOutput* collisionOutput, const hkpCdBody* a, const hkpCdBody* b, hkContactPoint* cp, const hkpGskCache* gskCache, hkpContactPointProperties* cpp, float projectedVelocity)
        : hkpContactPointAddedEvent(contactMgr, collisionInput, collisionOutput, a, b, cp, gskCache, cpp, projectedVelocity, TYPE_MANIFOLD)
        , m_contactPointId(contactPointId)
    {
    }

    unsigned short m_contactPointId; // 0x30
    unsigned short m_nextProcessCallbackDelay; // 0x32
};

// size 0x34
struct hkpToiPointAddedEvent : public hkpContactPointAddedEvent
{
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpToiPointAddedEvent);
    // 0x00E1A030 (hkpcollisionevents.inl)
    hkpToiPointAddedEvent(hkpDynamicsContactMgr* contactMgr, const hkpProcessCollisionInput* collisionInput, hkpProcessCollisionOutput* collisionOutput, const hkpCdBody* a, const hkpCdBody* b, hkContactPoint* cp, const hkpGskCache* gskCache, hkpContactPointProperties* cpp, float toi, float projectedVelocity)
        : hkpContactPointAddedEvent(contactMgr, collisionInput, collisionOutput, a, b, cp, gskCache, cpp, projectedVelocity, TYPE_TOI)
        , m_toi(toi)
    {
    }

    float m_toi; // 0x30
};

// 0x00711020 (hkpcollisionevents.inl:120). The SDK's HK_ASSERT2 on m_type == TYPE_MANIFOLD is compiled out in the
// retail build.
inline hkpManifoldPointAddedEvent& hkpContactPointAddedEvent::asManifoldEvent()
{
    return static_cast<hkpManifoldPointAddedEvent&>(*this);
}

// size 0x1C
struct hkpContactPointRemovedEvent
{
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpContactPointRemovedEvent);
    int getEntityIndex(const hkpEntity*) const;
    const hkpSimpleContactConstraintAtom* getAtom() const;
    int getNumExtendedUserDatas(const hkpEntity*) const;
    void getExtendedUserDatas(const hkpEntity*, unsigned int*, int) const;
    void setExtendedUserDatas(const hkpEntity*, unsigned int*, int) const;
    hkBool isToi(); // 0x00711000 (hkpcollisionevents.inl:102)
    // 0x00E19830 (hkpcollisionevents.inl)
    hkpContactPointRemovedEvent(unsigned short contactPointId, hkpDynamicsContactMgr* contactMgr, hkpConstraintOwner* constraintOwner, hkpContactPointProperties* prop, hkpEntity* ca, hkpEntity* cb)
        : m_contactPointId(contactPointId)
        , m_contactPointProperties(prop)
        , m_entityA(ca)
        , m_entityB(cb)
        , m_internalContactMgr(contactMgr)
        , m_constraintOwner(constraintOwner)
    {
    }

    unsigned short m_contactPointId; // 0x00
    hkpContactPointProperties* m_contactPointProperties; // 0x04
    hkpEntity* m_entityA; // 0x08
    hkpEntity* m_entityB; // 0x0C
    hkpEntity* m_callbackFiredFrom; // 0x10
    hkpDynamicsContactMgr* m_internalContactMgr; // 0x14
    hkpConstraintOwner* m_constraintOwner; // 0x18
};

class hkpCollidable;
struct hkpProcessCollisionData;

// size 0x414
// Added by hand in HK7 (layout from rfg.pdb): the contact manager's process event, one property per manifold point.
struct hkpContactProcessEvent
{
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpContactProcessEvent);
    // 0x00E19860 (hkpcollisionevents.inl)
    hkpContactProcessEvent(hkpDynamicsContactMgr* contactMgr, const hkpCollidable* ca, const hkpCollidable* cb, hkpProcessCollisionData* data)
        : m_collidableA(ca)
        , m_collidableB(cb)
        , m_collisionData(data)
        , m_internalContactMgr(contactMgr)
    {
    }

    const hkpCollidable* m_collidableA; // 0x00
    const hkpCollidable* m_collidableB; // 0x04
    hkpEntity* m_callbackFiredFrom; // 0x08
    hkpProcessCollisionData* m_collisionData; // 0x0C
    hkpContactPointProperties* m_contactPointProperties[256]; // 0x10
    hkpDynamicsContactMgr* m_internalContactMgr; // 0x410
};

class hkpSimpleContactConstraintData;

// size 0x24
// Added by hand in HK9 (layout from rfg.pdb): hkpCollisionListener::contactPointCallback fills it on its stack for
// contactPointConfirmedCallback. The SDK's inline constructor is not used by rfg.exe.
struct hkpContactPointConfirmedEvent
{
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpContactPointConfirmedEvent);

    const hkpCollidable* m_collidableA; // 0x00
    const hkpCollidable* m_collidableB; // 0x04
    hkpEntity* m_callbackFiredFrom; // 0x08
    hkContactPoint* m_contactPoint; // 0x0C
    hkpContactPointProperties* m_contactPointProperties; // 0x10
    float m_rotateNormal; // 0x14
    float m_projectedVelocity; // 0x18
    hkpContactPointAddedEvent::Type m_type; // 0x1C, hkEnum<Type,int> in the SDK (const there)
    const hkpSimpleContactConstraintData* m_contactData; // 0x20
};
