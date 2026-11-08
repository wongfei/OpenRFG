// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/convexagent/spherebox/hkpsphereboxagent.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/util/linearcast/hkpiterativelinearcastagent.h"

class hkBool;
class hkContactPoint;
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

// size 0x10
class alignas(16) hkpSphereBoxAgent : public hkpIterativeLinearCastAgent
{
public:
    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int nbytes) 0x00DBFA60, delete(void* p, unsigned int nbytes) 0x00DBFAB0
    static void registerAgent(hkpCollisionDispatcher* dispatcher); // 0x00DBF9E0
    virtual void getPenetrations(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdBodyPairCollector& collector); // vtbl+0x10 0x00DBF050
    virtual void getClosestPoints(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdPointCollector& collector); // vtbl+0x14 0x00DBF3D0
    static void staticGetClosestPoints(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdPointCollector& collector); // 0x00DBF6E0
    virtual void processCollision(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpProcessCollisionInput& input, hkpProcessCollisionOutput& result); // vtbl+0x1C 0x00DBF070
    static void staticGetPenetrations(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdBodyPairCollector& collector); // 0x00DBEEA0
    virtual void cleanup(hkpConstraintOwner& constraintOwner); // vtbl+0x20 0x00DBEE20
public:
    static hkpCollisionAgent* createSphereBoxAgent(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpContactMgr* mgr); // 0x00DBEDE0
    static hkpCollisionAgent* createBoxSphereAgent(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpContactMgr* mgr); // 0x00DBEE60
    hkpSphereBoxAgent(hkpContactMgr* mgr); // 0x00DBFA80
public:
    static hkBool getClosestPoint(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkContactPoint& contactOut); // 0x00DBFD40
public:
    virtual ~hkpSphereBoxAgent(); // vtbl+0x00 0x00DBFAF0

public:
    unsigned short m_contactPointId; // 0x0C
};
