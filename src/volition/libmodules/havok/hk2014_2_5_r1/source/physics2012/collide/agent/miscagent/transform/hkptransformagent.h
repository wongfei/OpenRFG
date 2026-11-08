// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/miscagent/transform/hkptransformagent.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpcollisionagent.h"

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

// size 0x10
class hkpTransformAgent : public hkpCollisionAgent
{
public:
    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int nbytes) 0x00DC6C00, delete(void* p, unsigned int nbytes) 0x00DC6CF0
    static void registerAgent(hkpCollisionDispatcher* dispatcher); // 0x00DC6B40
    virtual void getPenetrations(const hkpCdBody& bodyAin, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdBodyPairCollector& collector); // vtbl+0x10 0x00DC63E0
    virtual void getClosestPoints(const hkpCdBody& bodyAin, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdPointCollector& pointDetails); // vtbl+0x14 0x00DC6370
    static void staticLinearCast(const hkpCdBody& bodyAin, const hkpCdBody& bodyB, const hkpLinearCastCollisionInput& input, hkpCdPointCollector& collector, hkpCdPointCollector* startCollector); // 0x00DC68C0
    virtual void linearCast(const hkpCdBody& bodyAin, const hkpCdBody& bodyB, const hkpLinearCastCollisionInput& input, hkpCdPointCollector& collector, hkpCdPointCollector* startCollector); // vtbl+0x18 0x00DC6300
    static void staticGetClosestPoints(const hkpCdBody& bodyAin, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdPointCollector& pointDetails); // 0x00DC6940
    virtual void processCollision(const hkpCdBody& bodyAin, const hkpCdBody& bodyB, const hkpProcessCollisionInput& input, hkpProcessCollisionOutput& result); // vtbl+0x1C 0x00DC65C0
    static void staticGetPenetrations(const hkpCdBody& bodyAin, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdBodyPairCollector& collector); // 0x00DC69C0
    virtual void cleanup(hkpConstraintOwner& constraintOwner); // vtbl+0x20 0x00DC6270
    virtual void invalidateTim(const hkpCollisionInput& input); // vtbl+0x28 0x00DC6290
    virtual void warpTime(float oldTime, float newTime, const hkpCollisionInput& input); // vtbl+0x2C 0x00DC62A0
    virtual void removePoint(unsigned short idToRemove); // vtbl+0x30 0x00DC62D0
    virtual void commitPotential(unsigned short newId); // vtbl+0x34 0x00DC62E0
    virtual void createZombie(unsigned short idTobecomeZombie); // vtbl+0x38 0x00DC62F0
public:
    hkpTransformAgent(const hkpCdBody& bodyAIn, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpContactMgr* mgr); // 0x00DC6C20
    virtual void updateShapeCollectionFilter(const hkpCdBody& bodyAin, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpConstraintOwner& constraintOwner); // vtbl+0x24 0x00DC6450
    static hkpCollisionAgent* createTransformAAgent(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpContactMgr* mgr); // 0x00DC64C0
    static hkpCollisionAgent* createTransformBAgent(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpContactMgr* mgr); // 0x00DC6A40
public:
    virtual ~hkpTransformAgent(); // vtbl+0x00 0x00DC6BE0

public:
    hkpCollisionAgent* m_childAgent; // 0x0C
};
