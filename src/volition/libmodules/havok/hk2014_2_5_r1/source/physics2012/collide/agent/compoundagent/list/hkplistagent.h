// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/compoundagent/list/hkplistagent.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpcollisionagent.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent3/machine/1n/hkpagent1ntrack.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/dispatch/hkpcollisiondispatcher.h"

class hkpCdBody;
class hkpCdBodyPairCollector;
class hkpCdPointCollector;
struct hkpCollisionInput;
class hkpConstraintOwner;
class hkpContactMgr;
struct hkpLinearCastCollisionInput;
struct hkpProcessCollisionInput;
struct hkpProcessCollisionOutput;

// size 0x1C
class hkpListAgent : public hkpCollisionAgent
{
public:
    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int nbytes) 0x00DBA410, delete(void* p, unsigned int nbytes) 0x00DBA4D0
    static void initAgentFunc(hkpCollisionDispatcher::AgentFuncs& af); // 0x00DBA380
    static void initAgentFuncInverse(hkpCollisionDispatcher::AgentFuncs& af); // 0x00DBA300
    static void registerAgent(hkpCollisionDispatcher* dispatcher); // 0x00DBA3B0
    virtual ~hkpListAgent(); // vtbl+0x00 0x00DBA430
    virtual void getPenetrations(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdBodyPairCollector& collector); // vtbl+0x10 0x00DBA250
    static void staticLinearCast(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpLinearCastCollisionInput& input, hkpCdPointCollector& collector, hkpCdPointCollector* startCollector); // 0x00DBA010
    virtual void getClosestPoints(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdPointCollector& collector); // vtbl+0x14 0x00DBA020
    static void staticGetClosestPoints(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdPointCollector& collector); // 0x00DBA000
    virtual void linearCast(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpLinearCastCollisionInput& input, hkpCdPointCollector& collector, hkpCdPointCollector* startCollector); // vtbl+0x18 0x00DBA040
    static void staticGetPenetrations(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdBodyPairCollector& collector); // 0x00DBA070
    virtual void processCollision(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpProcessCollisionInput& input, hkpProcessCollisionOutput& output); // vtbl+0x1C 0x00DBA080
    virtual void cleanup(hkpConstraintOwner& constraintOwner); // vtbl+0x20 0x00DB9F20
    virtual void updateShapeCollectionFilter(const hkpCdBody& bodyA, const hkpCdBody& listShapeBodyB, const hkpCollisionInput& input, hkpConstraintOwner& constraintOwner); // vtbl+0x24 0x00DB9FA0
    virtual void invalidateTim(const hkpCollisionInput& input); // vtbl+0x28 0x00DB9F50
    hkpListAgent(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpContactMgr* contactMgr); // 0x00DBA270
    static hkpCollisionAgent* createListAAgent(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpContactMgr* contactMgr); // 0x00DBA330
    static hkpCollisionAgent* createListBAgent(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpContactMgr* contactMgr); // 0x00DBA2C0
    virtual void warpTime(float oldTime, float newTime, const hkpCollisionInput& input); // vtbl+0x2C 0x00DB9F70

public:
    hkpCollisionDispatcher* m_dispatcher; // 0x0C
    hkpAgent1nTrack m_agentTrack; // 0x10
};
