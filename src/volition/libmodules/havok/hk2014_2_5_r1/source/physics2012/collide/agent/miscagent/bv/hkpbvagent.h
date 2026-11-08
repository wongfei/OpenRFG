// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/miscagent/bv/hkpbvagent.h
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

// size 0x14
class hkpBvAgent : public hkpCollisionAgent
{
public:
    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int nbytes) 0x00DCA3F0, delete(void* p, unsigned int nbytes) 0x00DCA410
    static void registerAgent(hkpCollisionDispatcher* dispatcher); // 0x00DCA350
    virtual void getPenetrations(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdBodyPairCollector& collector); // vtbl+0x10 0x00DCA020
    virtual void getClosestPoints(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdPointCollector& collector); // vtbl+0x14 0x00DC9D40
    static void staticLinearCast(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpLinearCastCollisionInput& input, hkpCdPointCollector& pointDetails, hkpCdPointCollector* startCollector); // 0x00DC9BB0
    virtual void linearCast(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpLinearCastCollisionInput& input, hkpCdPointCollector& pointDetails, hkpCdPointCollector* startCollector); // vtbl+0x18 0x00DC99F0
    static void staticGetClosestPoints(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdPointCollector& collector); // 0x00DC9EC0
    virtual void processCollision(const hkpCdBody& bodyAin, const hkpCdBody& bodyB, const hkpProcessCollisionInput& input, hkpProcessCollisionOutput& result); // vtbl+0x1C 0x00DC9850
    static void staticGetPenetrations(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdBodyPairCollector& collector); // 0x00DCA1A0
    virtual void cleanup(hkpConstraintOwner& constraintOwner); // vtbl+0x20 0x00DC95B0
    virtual void updateShapeCollectionFilter(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpConstraintOwner& constraintOwner); // vtbl+0x24 0x00DC96E0
    virtual void invalidateTim(const hkpCollisionInput& input); // vtbl+0x28 0x00DC95F0
    virtual void warpTime(float oldTime, float newTime, const hkpCollisionInput& input); // vtbl+0x2C 0x00DC9620
    virtual void removePoint(unsigned short idToRemove); // vtbl+0x30 0x00DC9680
    virtual void commitPotential(unsigned short newId); // vtbl+0x34 0x00DC96A0
    virtual void createZombie(unsigned short idTobecomeZombie); // vtbl+0x38 0x00DC96C0
public:
    hkpBvAgent(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpContactMgr* mgr); // 0x00DC9760
    virtual ~hkpBvAgent(); // vtbl+0x00 0x00DCA3D0
    static hkpCollisionAgent* createBvShapeAgent(const hkpCdBody& A, const hkpCdBody& B, const hkpCollisionInput& input, hkpContactMgr* mgr); // 0x00DC9810
    static hkpCollisionAgent* createShapeBvAgent(const hkpCdBody& A, const hkpCdBody& B, const hkpCollisionInput& input, hkpContactMgr* mgr); // 0x00DCA300

    hkpCollisionAgent* m_boundingVolumeAgent; // 0x0C
    hkpCollisionAgent* m_childAgent; // 0x10
};
