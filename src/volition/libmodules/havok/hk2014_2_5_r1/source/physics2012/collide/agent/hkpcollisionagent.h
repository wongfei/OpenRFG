// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpcollisionagent.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0); inline members of the 2014.1 SDK added by hand
// (HK6). Hand-maintained.
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/object/hkreferencedobject.h"

class hkpCdBody;
class hkpCdBodyPairCollector;
class hkpCdPointCollector;
class hkpCollisionDispatcher;
struct hkpCollisionInput;
class hkpConstraintOwner;
class hkpContactMgr;
struct hkpLinearCastCollisionInput;
struct hkpProcessCollisionInput;
struct hkpProcessCollisionOutput;

// size 0xC
class hkpCollisionAgent : public hkReferencedObject
{
public:
    typedef void (*RegisterAgentFunction)(hkpCollisionDispatcher*);

    HK_DECLARE_CLASS_ALLOCATOR(); // delete(void* p, unsigned int nbytes) 0x00D78880
    virtual void getPenetrations(const hkpCdBody&, const hkpCdBody&, const hkpCollisionInput&, hkpCdBodyPairCollector&) = 0; // vtbl+0x10
    virtual void getClosestPoints(const hkpCdBody&, const hkpCdBody&, const hkpCollisionInput&, hkpCdPointCollector&) = 0; // vtbl+0x14
    virtual void linearCast(const hkpCdBody&, const hkpCdBody&, const hkpLinearCastCollisionInput&, hkpCdPointCollector&, hkpCdPointCollector*) = 0; // vtbl+0x18
    virtual void processCollision(const hkpCdBody&, const hkpCdBody&, const hkpProcessCollisionInput&, hkpProcessCollisionOutput&) = 0; // vtbl+0x1C
    virtual void cleanup(hkpConstraintOwner&) = 0; // vtbl+0x20
    virtual void updateShapeCollectionFilter(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpConstraintOwner& constraintOwner) {} // vtbl+0x24 0x00D783F0 (.h)
    virtual void invalidateTim(const hkpCollisionInput& input); // vtbl+0x28 0x00D9CDE0
    virtual void warpTime(float oldTime, float newTime, const hkpCollisionInput& input); // vtbl+0x2C 0x00D9CDF0
    virtual void removePoint(unsigned short idToRemove); // vtbl+0x30 0x00D9CE00
    virtual void commitPotential(unsigned short newId); // vtbl+0x34 0x00D9CE10
    virtual void createZombie(unsigned short idTobecomeZombie); // vtbl+0x38 0x00D9CE20
public:
    virtual ~hkpCollisionAgent() {} // vtbl+0x00 0x00D783C0 (hkpcollisionagent.inl)
    hkpCollisionAgent(hkpContactMgr* contactMgr) { m_contactMgr = contactMgr; } // 0x00D783D0 (hkpcollisionagent.inl)
public:
    hkpContactMgr* getContactMgr() { return m_contactMgr; }

public:
    hkpContactMgr* m_contactMgr; // 0x08
public:
    static void (*registerHeightFieldAgent)(hkpCollisionDispatcher*); // 0x03A28814
};
