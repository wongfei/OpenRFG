// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/convexagent/capsuletriangle/hkpcapsuletriangleagent.h
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
struct hkpCdPoint;
class hkpCdPointCollector;
class hkpCollisionAgent;
struct hkpCollisionInput;
class hkpConstraintOwner;
class hkpContactMgr;
struct hkpFeatureOutput;
struct hkpProcessCollisionInput;
struct hkpProcessCollisionOutput;

// size 0x28
class hkpCapsuleTriangleAgent : public hkpIterativeLinearCastAgent
{
public:
    enum ClosestPointResult
    {
        ST_CP_MISS = 0,
        ST_CP_HIT = 1,
    };

    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int nbytes) 0x00DBC2B0, delete(void* p, unsigned int nbytes) 0x00DBC320
    static void initAgentFunc(hkpCollisionDispatcher::AgentFuncs& af); // 0x00DBD400
    static void initAgentFuncInverse(hkpCollisionDispatcher::AgentFuncs& af); // 0x00DBD430
    static void registerAgent(hkpCollisionDispatcher* dispatcher); // 0x00DBD460
    static void registerAgent2(hkpCollisionDispatcher* dispatcher); // 0x00DBD4C0
    virtual void getPenetrations(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdBodyPairCollector& collector); // vtbl+0x10 0x00DBD000
    virtual void getClosestPoints(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdPointCollector& collector); // vtbl+0x14 0x00DBC8E0
    static void staticGetClosestPoints(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdPointCollector& collector); // 0x00DBCC60
    virtual void processCollision(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpProcessCollisionInput& input, hkpProcessCollisionOutput& result); // vtbl+0x1C 0x00DBDB20
    static void staticGetPenetrations(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdBodyPairCollector& collector); // 0x00DBD200
    virtual void cleanup(hkpConstraintOwner& constraintOwner); // vtbl+0x20 0x00DBC3F0
public:
    hkpCapsuleTriangleAgent(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpContactMgr* mgr); // 0x00DBC2D0
    static hkpCollisionAgent* createCapsuleTriangleAgent(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpContactMgr* mgr); // 0x00DBC380
    static hkpCollisionAgent* createTriangleCapsuleAgent(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpContactMgr* mgr); // 0x00DBC440
public:
    static void getClosestPointsPublic(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCollideTriangleUtil::PointTriangleDistanceCache& cache, int searchManifold, hkContactPoint* points); // 0x00DBC4B0
    static void getClosestPointsInl(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCollideTriangleUtil::PointTriangleDistanceCache& cache, int searchManifold, hkContactPoint* points, hkpFeatureOutput* featureOutput); // 0x00DBD700
    static ClosestPointResult getClosestPointInternal(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCollideTriangleUtil::PointTriangleDistanceCache& cache, hkpCdPoint& cpoint); // 0x00DBD850
public:
    static ClosestPointResult getClosestPoint(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCollideTriangleUtil::PointTriangleDistanceCache& cache, hkContactPoint& cpoint); // 0x00DBC600
    virtual ~hkpCapsuleTriangleAgent(); // vtbl+0x00 0x00DBC360

public:
    unsigned short m_contactPointId[3]; // 0x0C
    hkpCollideTriangleUtil::PointTriangleDistanceCache m_triangleCache; // 0x14
};
