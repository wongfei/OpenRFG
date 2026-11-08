// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/deprecated/multisphere/hkpmultisphereagent.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/hkcontainerallocators.h"
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

// size 0x38
class hkpMultiSphereAgent : public hkpCollisionAgent
{
public:
    // size 0x8
    struct KeyAgentPair
    {
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(KeyAgentPair);

        unsigned int m_key; // 0x00
        hkpCollisionAgent* m_agent; // 0x04
    };

    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int nbytes) 0x00DC8320, delete(void* p, unsigned int nbytes) 0x00DC90B0
    static void registerAgent(hkpCollisionDispatcher* dispatcher); // 0x00DC8CD0
    virtual void getPenetrations(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdBodyPairCollector& collector); // vtbl+0x10 0x00DC8880
    virtual void getClosestPoints(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdPointCollector& collector); // vtbl+0x14 0x00DC8490
    static void staticLinearCast(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpLinearCastCollisionInput& input, hkpCdPointCollector& collector, hkpCdPointCollector* startCollector); // 0x00DC8770
    virtual void linearCast(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpLinearCastCollisionInput& input, hkpCdPointCollector& collector, hkpCdPointCollector* startCollector); // vtbl+0x18 0x00DC8680
    static void staticGetClosestPoints(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdPointCollector& collector); // 0x00DC8570
    virtual void processCollision(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpProcessCollisionInput& input, hkpProcessCollisionOutput& result); // vtbl+0x1C 0x00DC8380
    static void staticGetPenetrations(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdBodyPairCollector& collector); // 0x00DC8970
    virtual void cleanup(hkpConstraintOwner& info); // vtbl+0x20 0x00DC8340
public:
    hkpMultiSphereAgent(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpContactMgr* mgr); // 0x00DC8A80
    static hkpCollisionAgent* createListAAgent(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpContactMgr* mgr); // 0x00DC8C40
    static hkpCollisionAgent* createListBAgent(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpContactMgr* mgr); // 0x00DC8C80
public:
    virtual ~hkpMultiSphereAgent(); // vtbl+0x00 0x00DC90F0

public:
    hkInplaceArray<KeyAgentPair,4,hkContainerHeapAllocator> m_agents; // 0x0C
};
