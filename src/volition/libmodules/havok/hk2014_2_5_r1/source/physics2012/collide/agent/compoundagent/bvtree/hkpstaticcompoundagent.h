// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/compoundagent/bvtree/hkpstaticcompoundagent.h
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
class hkpStaticCompoundAgent : public hkpBvTreeAgent
{
public:
    enum
    {
        HK_MAX_NUM_HITS_PER_AABB_QUERY = 2048,
    };

    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int nbytes) 0x00DB19B0, delete(void* p, unsigned int nbytes) 0x00DB1E00
    static void registerAgent(hkpCollisionDispatcher* dispatcher); // 0x00DB1C70
    static hkpCollisionAgent* createShapeVsStaticCompoundAgent(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpContactMgr* mgr); // 0x00DB1BE0
    static hkpCollisionAgent* createStaticCompoundVsShapeAgent(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpContactMgr* mgr); // 0x00DB1C10
    static hkpCollisionAgent* createStaticCompoundVsStaticCompoundAgent(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpContactMgr* mgr); // 0x00DB1C40
    virtual void linearCast(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpLinearCastCollisionInput& input, hkpCdPointCollector& collector, hkpCdPointCollector* startCollector); // vtbl+0x18 0x00DB1B90
    static void staticLinearCast(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpLinearCastCollisionInput& input, hkpCdPointCollector& collector, hkpCdPointCollector* startCollector); // 0x00DB1AB0
public:
    hkpStaticCompoundAgent(hkpContactMgr* mgr); // 0x00DB1BC0
public:
    virtual ~hkpStaticCompoundAgent(); // vtbl+0x00 0x00DB1E40

};
