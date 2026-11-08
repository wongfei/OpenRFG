// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/compoundagent/bvtreestream/hkpbvtreestreamagent.h
//
// Havok 2014.2.5: declaration from rfg.pdb (havok_impl.md HK6; hand-written, the class was out of the HK0 header set).
// The BV tree agent2 over a 1n agent track (agent3 children). Hand-maintained.
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/hkcontainerallocators.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/geometry/aabb/hkaabb.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpcollisionagent.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent3/machine/1n/hkpagent1ntrack.h"

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

// size 0x40
class hkpBvTreeStreamAgent : public hkpCollisionAgent
{
public:
    static void registerAgent(hkpCollisionDispatcher* dispatcher); // 0x00DB3560
    static hkpCollisionAgent* createShapeBvAgent(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpContactMgr* mgr); // 0x00DB2D10
    static hkpCollisionAgent* createBvTreeShapeAgent(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpContactMgr* mgr); // 0x00DB3510

protected:
    hkpCollisionDispatcher* m_dispatcher; // 0x0C
    hkAabb m_cachedAabb; // 0x10
    hkpAgent1nTrack m_agentTrack; // 0x30
};
