// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/compoundagent/bvtree/hkpbvcompressedmeshagent.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/compoundagent/bvtree/hkpbvtreeagent.h"

class hkpCdBody;
class hkpCdPointCollector;
class hkpCollisionAgent;
class hkpCollisionDispatcher;
struct hkpCollisionInput;
class hkpContactMgr;
struct hkpLinearCastCollisionInput;

// size 0x40
class hkpBvCompressedMeshAgent : public hkpBvTreeAgent
{
public:
    enum
    {
        HK_MAX_NUM_HITS_PER_AABB_QUERY = 2048,
    };

    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int nbytes) 0x00DB1F30, delete(void* p, unsigned int nbytes) 0x00DB2410
    static void registerAgent(hkpCollisionDispatcher* dispatcher); // 0x00DB2280
    static hkpCollisionAgent* createShapeVsStaticMeshAgent(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpContactMgr* mgr); // 0x00DB2180
    static hkpCollisionAgent* createStaticMeshVsShapeAgent(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpContactMgr* mgr); // 0x00DB21B0
    static hkpCollisionAgent* createStaticMeshVsStaticMeshAgent(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpContactMgr* mgr); // 0x00DB21E0
    virtual void linearCast(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpLinearCastCollisionInput& input, hkpCdPointCollector& collector, hkpCdPointCollector* startCollector); // vtbl+0x18 0x00DB2130
    static void staticLinearCast(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpLinearCastCollisionInput& input, hkpCdPointCollector& castCollector, hkpCdPointCollector* startCollector); // 0x00DB1FD0
public:
    hkpBvCompressedMeshAgent(hkpContactMgr* mgr); // 0x00DB2160
public:
    virtual ~hkpBvCompressedMeshAgent(); // vtbl+0x00 0x00DB2450

};
