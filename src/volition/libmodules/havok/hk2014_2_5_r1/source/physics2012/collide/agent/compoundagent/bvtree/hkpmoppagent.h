// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/compoundagent/bvtree/hkpmoppagent.h
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
class hkpMoppAgent : public hkpBvTreeAgent
{
public:
    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int nbytes) 0x00DB4610, delete(void* p, unsigned int nbytes) 0x00DB4950
    static void registerAgent(hkpCollisionDispatcher* dispatcher); // 0x00DB4890
    static void staticLinearCast(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpLinearCastCollisionInput& input, hkpCdPointCollector& collector, hkpCdPointCollector* startCollector); // 0x00DB4630
    static hkpCollisionAgent* createBvBvAgent(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpContactMgr* mgr); // 0x00DB4810
public:
    hkpMoppAgent(hkpContactMgr* mgr); // 0x00DB47F0
public:
    virtual ~hkpMoppAgent(); // vtbl+0x00 0x00DB4990

};
