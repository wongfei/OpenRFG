// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/deprecated/convexlist/hkpconvexlistagent.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/convexagent/gjk/hkppredgskfagent.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent3/machine/1n/hkpagent1ntrack.h"

class hkpCdBody;
class hkpCdBodyPairCollector;
class hkpCdPointCollector;
class hkpCollisionAgent;
class hkpCollisionDispatcher;
struct hkpCollisionInput;
class hkpConstraintOwner;
class hkpContactMgr;
struct hkpLinearCastCollisionInput;
struct hkpProcessCollisionInput;
struct hkpProcessCollisionOutput;

// size 0x90
class alignas(16) hkpConvexListAgent : public hkpPredGskfAgent
{
public:
    // size 0x10
    struct StreamData
    {
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(StreamData);

        hkpAgent1nTrack m_agentTrack; // 0x00
        float m_inStreamModeTimDist; // 0x0C
    };

    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int nbytes) 0x00DB7180, delete(void* p, unsigned int nbytes) 0x00DB7220
    static void registerAgent(hkpCollisionDispatcher* dispatcher); // 0x00DB6F80
    virtual void getPenetrations(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdBodyPairCollector& collector); // vtbl+0x10 0x00DB6380
    virtual void getClosestPoints(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdPointCollector& collector); // vtbl+0x14 0x00DB6270
    static void staticLinearCast(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpLinearCastCollisionInput& input, hkpCdPointCollector& collector, hkpCdPointCollector* startCollector); // 0x00DB63A0
    virtual void linearCast(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpLinearCastCollisionInput& input, hkpCdPointCollector& collector, hkpCdPointCollector* startCollector); // vtbl+0x18 0x00DB64C0
    static void staticGetClosestPoints(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdPointCollector& collector); // 0x00DB60B0
    virtual void processCollision(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpProcessCollisionInput& input, hkpProcessCollisionOutput& output); // vtbl+0x1C 0x00DB6680
    static void staticGetPenetrations(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdBodyPairCollector& collector); // 0x00DB6290
    virtual void cleanup(hkpConstraintOwner& constraintOwner); // vtbl+0x20 0x00DB5F70
    virtual void updateShapeCollectionFilter(const hkpCdBody& bodyA, const hkpCdBody& listShapeBodyB, const hkpCollisionInput& input, hkpConstraintOwner& constraintOwner); // vtbl+0x24 0x00DB6050
    virtual void invalidateTim(const hkpCollisionInput& input); // vtbl+0x28 0x00DB5E40
    virtual void warpTime(float oldTime, float newTime, const hkpCollisionInput& input); // vtbl+0x2C 0x00DB5E70
    virtual void removePoint(unsigned short idToRemove); // vtbl+0x30 0x00DB5ED0
    virtual void commitPotential(unsigned short idToCommit); // vtbl+0x34 0x00DB5EF0
    virtual void createZombie(unsigned short idTobecomeZombie); // vtbl+0x38 0x00DB5F10
public:
    hkpConvexListAgent(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpContactMgr* mgr); // 0x00DB5FC0
    virtual ~hkpConvexListAgent(); // vtbl+0x00 0x00DB71A0
    void switchToGskMode(hkpConstraintOwner& constraintOwner); // 0x00DB5F30
    void switchToStreamMode(hkpConstraintOwner& constraintOwner); // 0x00DB6620
public:
    static hkpCollisionAgent* createConvexConvexListAgent(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpContactMgr* mgr); // 0x00DB6C80
    static hkpCollisionAgent* createConvexListConvexAgent(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpContactMgr* mgr); // 0x00DB64F0
    static hkpCollisionAgent* createConvexListConvexListAgent(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpContactMgr* mgr); // 0x00DB6DC0
public:
    StreamData& getStream(); // 0x00DB7050
    const StreamData& getStreamConst() const;

    hkpCollisionDispatcher* m_dispatcher; // 0x80
    hkBool m_inGskMode; // 0x84
    hkBool m_processFunctionCalled; // 0x85
    short m_inStreamModeCounter; // 0x86
};
