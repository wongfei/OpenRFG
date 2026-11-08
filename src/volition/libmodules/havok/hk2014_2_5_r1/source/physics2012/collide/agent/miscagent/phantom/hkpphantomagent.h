// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/miscagent/phantom/hkpphantomagent.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/geometry/collide/shapes/hkcdshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpcollisionagent.h"

class hkpCdBody;
class hkpCdBodyPairCollector;
class hkpCdPointCollector;
class hkpCollidable;
class hkpCollisionDispatcher;
struct hkpCollisionInput;
class hkpConstraintOwner;
class hkpContactMgr;
struct hkpLinearCastCollisionInput;
class hkpPhantomCallbackShape;
struct hkpProcessCollisionInput;
struct hkpProcessCollisionOutput;

// size 0x24
class hkpPhantomAgent : public hkpCollisionAgent
{
public:
    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int nbytes) 0x00DC6190, delete(void* p, unsigned int nbytes) 0x00DC61C0
    static void registerAgent(hkpCollisionDispatcher* dispatcher); // 0x00DC6130
    virtual void getPenetrations(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdBodyPairCollector& collector); // vtbl+0x10 0x00DC5EE0
    static void staticGetPenetrations(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdBodyPairCollector& collector); // 0x00DC5F00
    virtual void getClosestPoints(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdPointCollector& pointDetails); // vtbl+0x14 0x00DC5F20
    virtual void linearCast(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpLinearCastCollisionInput& input, hkpCdPointCollector& collector, hkpCdPointCollector* startCollector); // vtbl+0x18 0x00DC5F40
    static void staticGetClosestPoints(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdPointCollector& pointDetails); // 0x00DC5F30
    virtual void processCollision(const hkpCdBody& bodyAin, const hkpCdBody& bodyB, const hkpProcessCollisionInput& input, hkpProcessCollisionOutput& result); // vtbl+0x1C 0x00DC5ED0
    static void staticLinearCast(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpLinearCastCollisionInput& input, hkpCdPointCollector& collector, hkpCdPointCollector* startCollector); // 0x00DC5F50
    virtual void cleanup(hkpConstraintOwner& constraintOwner); // vtbl+0x20 0x00DC5E80
public:
    hkpPhantomAgent(const hkpCdBody& bodyA, const hkpCdBody& bodyB, hkpContactMgr* contactMgr); // 0x00DC5F60
    static hkpCollisionAgent* createPhantomAgent(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpContactMgr* contactMgr); // 0x00DC5FD0
    static hkpCollisionAgent* createNoPhantomAgent(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpContactMgr* mgr); // 0x00DC60A0
public:
    virtual ~hkpPhantomAgent(); // vtbl+0x00 0x00DC61B0

public:
    const hkpCollidable* m_collidableA; // 0x0C
    const hkpCollidable* m_collidableB; // 0x10
    hkpPhantomCallbackShape* m_shapeA; // 0x14
    hkpPhantomCallbackShape* m_shapeB; // 0x18
    hkcdShapeType::ShapeTypeEnum m_bodyTypeA; // 0x1C
    hkcdShapeType::ShapeTypeEnum m_bodyTypeB; // 0x20
};
