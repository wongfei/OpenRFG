// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/deprecated/multispheretriangle/hkpmultispheretriangleagent.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/util/linearcast/hkpiterativelinearcastagent.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/util/hkpcollidetriangleutil.h"

class hkpCdBody;
class hkpCdBodyPairCollector;
class hkpCdPointCollector;
class hkpCollisionAgent;
class hkpCollisionDispatcher;
struct hkpCollisionInput;
class hkpConstraintOwner;
class hkpContactMgr;
struct hkpProcessCollisionInput;
struct hkpProcessCollisionOutput;

// size 0x2C
class hkpMultiSphereTriangleAgent : public hkpIterativeLinearCastAgent
{
public:
    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int nbytes) 0x00DBAA00, delete(void* p, unsigned int nbytes) 0x00DBAA70
    static void registerAgent(hkpCollisionDispatcher* dispatcher); // 0x00DBB400
    virtual void getPenetrations(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdBodyPairCollector& collector); // vtbl+0x10 0x00DBB0D0
    static void staticGetPenetrations(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdBodyPairCollector& collector); // 0x00DBB270
    virtual void getClosestPoints(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdPointCollector& collector); // vtbl+0x14 0x00DBAC00
    static void staticGetClosestPoints(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdPointCollector& collector); // 0x00DBAE60
    virtual void processCollision(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpProcessCollisionInput& input, hkpProcessCollisionOutput& result); // vtbl+0x1C 0x00DBB670
    virtual void cleanup(hkpConstraintOwner& constraintOwner); // vtbl+0x20 0x00DBAB30
public:
    hkpMultiSphereTriangleAgent(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpContactMgr* contactMgr); // 0x00DBAA20
    static hkpCollisionAgent* createMultiSphereTriangleAgent(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpContactMgr* contactMgr); // 0x00DBAAC0
    static hkpCollisionAgent* createTriangleMultiSphereAgent(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpContactMgr* contactMgr); // 0x00DBAB80
public:
    virtual ~hkpMultiSphereTriangleAgent(); // vtbl+0x00 0x00DBAAB0

public:
    hkpCollideTriangleUtil::ClosestPointTriangleCache m_closestPointTriangleCache; // 0x0C
    unsigned short m_contactPointId[8]; // 0x1C
};
