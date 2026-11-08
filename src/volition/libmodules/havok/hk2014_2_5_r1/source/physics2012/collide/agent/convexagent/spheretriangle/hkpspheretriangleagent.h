// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/convexagent/spheretriangle/hkpspheretriangleagent.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/util/linearcast/hkpiterativelinearcastagent.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/dispatch/hkpcollisiondispatcher.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/util/hkpcollidetriangleutil.h"

class hkContactPoint;
class hkpCdBody;
class hkpCdBodyPairCollector;
class hkpCdPointCollector;
class hkpCollisionAgent;
struct hkpCollisionInput;
class hkpConstraintOwner;
class hkpContactMgr;
struct hkpFeatureOutput;
struct hkpProcessCollisionInput;
struct hkpProcessCollisionOutput;

// size 0x20
class hkpSphereTriangleAgent : public hkpIterativeLinearCastAgent
{
public:
    enum ClosestPointResult
    {
        ST_CP_MISS = 0,
        ST_CP_FACE = 1,
        ST_CP_EDGE = 2,
    };

    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int nbytes) 0x00DC00F0, delete(void* p, unsigned int nbytes) 0x00DC0150
    static void initAgentFunc(hkpCollisionDispatcher::AgentFuncs& af); // 0x00DC0B70
    static void initAgentFuncInverse(hkpCollisionDispatcher::AgentFuncs& af); // 0x00DC0BA0
    static void registerAgent(hkpCollisionDispatcher* dispatcher); // 0x00DC0BD0
    static void registerAgent2(hkpCollisionDispatcher* dispatcher); // 0x00DC0C30
    virtual void getPenetrations(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdBodyPairCollector& collector); // vtbl+0x10 0x00DC02A0
    virtual void getClosestPoints(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdPointCollector& collector); // vtbl+0x14 0x00DC0710
    static void staticGetClosestPoints(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdPointCollector& collector); // 0x00DC08D0
    virtual void processCollision(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpProcessCollisionInput& input, hkpProcessCollisionOutput& result); // vtbl+0x1C 0x00DC1040
    static void staticGetPenetrations(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdBodyPairCollector& collector); // 0x00DC0430
    virtual void cleanup(hkpConstraintOwner& constraintOwner); // vtbl+0x20 0x00DC0200
public:
    hkpSphereTriangleAgent(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpContactMgr* mgr); // 0x00DC0110
    static hkpCollisionAgent* createSphereTriangleAgent(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpContactMgr* mgr); // 0x00DC01A0
    static hkpCollisionAgent* createTriangleSphereAgent(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpContactMgr* mgr); // 0x00DC0240
public:
    static ClosestPointResult getClosestPoint(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCollideTriangleUtil::ClosestPointTriangleCache& cache, hkContactPoint& cpoint); // 0x00DC05C0
    static ClosestPointResult getClosestPointInl(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCollideTriangleUtil::ClosestPointTriangleCache& cache, hkContactPoint& cpoint, hkpFeatureOutput* featureOutput); // 0x00DC0EF0
    virtual ~hkpSphereTriangleAgent(); // vtbl+0x00 0x00DC0190

    unsigned short m_contactPointId; // 0x0C
    hkpCollideTriangleUtil::ClosestPointTriangleCache m_closestPointTriangleCache; // 0x10
};
