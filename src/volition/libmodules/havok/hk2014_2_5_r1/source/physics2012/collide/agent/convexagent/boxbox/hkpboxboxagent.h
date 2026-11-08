// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/convexagent/boxbox/hkpboxboxagent.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/util/linearcast/hkpiterativelinearcastagent.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/boxbox/hkpboxboxmanifold.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/dispatch/hkpcollisiondispatcher.h"

class hkBool;
class hkpCdBody;
class hkpCdBodyPairCollector;
class hkpCdPointCollector;
class hkpCollisionAgent;
struct hkpCollisionInput;
class hkpConstraintOwner;
class hkpContactMgr;
struct hkpProcessCollisionInput;
struct hkpProcessCollisionOutput;

// size 0x50
class hkpBoxBoxAgent : public hkpIterativeLinearCastAgent
{
public:
    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int nbytes) 0x00DC35E0, delete(void* p, unsigned int nbytes) 0x00DC3630
    static void initAgentFunc(hkpCollisionDispatcher::AgentFuncs& af); // 0x00DC3D90
    static void registerAgent(hkpCollisionDispatcher* dispatcher); // 0x00DC3DC0
    virtual void getPenetrations(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdBodyPairCollector& collector); // vtbl+0x10 0x00DC3D70
    virtual void getClosestPoints(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdPointCollector& collector); // vtbl+0x14 0x00DC3B60
    static void staticGetClosestPoints(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdPointCollector& collector); // 0x00DC3940
    virtual void processCollision(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpProcessCollisionInput& input, hkpProcessCollisionOutput& result); // vtbl+0x1C 0x00DC3780
    static void staticGetPenetrations(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdBodyPairCollector& collector); // 0x00DC3B80
    virtual void cleanup(hkpConstraintOwner& constraintOwner); // vtbl+0x20 0x00DC3680
    static void setAttemptToFindAllEdges(hkBool);
    static hkBool getAttemptToFindAllEdges();
public:
    hkpBoxBoxAgent(hkpContactMgr* contactMgr); // 0x00DC3600
    static hkpCollisionAgent* createBoxBoxAgent(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpContactMgr* contactMgr); // 0x00DC36D0
public:
    virtual ~hkpBoxBoxAgent(); // vtbl+0x00 0x00DC3670

public:
    hkpBoxBoxManifold m_manifold; // 0x10
    static hkBool m_attemptToFindAllEdges; // 0x03A29D71
};
