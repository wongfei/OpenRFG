// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent3/bvtree3/hkpbvtreeagent3.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/dispatch/hkpcollisiondispatcher.h"

class hkAabb;
class hkTransformf;
class hkVector4f;
struct hkpAgent3ProcessInput;
struct hkpAgentEntry;
class hkpCdBody;
struct hkpCollisionInput;
class hkpConstraintOwner;
class hkpContactMgr;
struct hkpProcessCollisionInput;
struct hkpProcessCollisionOutput;
struct hkpShapeBufferStorage;
class hkpShapeCollection;
class hkpShapeContainer;

namespace hkBvTreeAgent3
{
    void initAgentFunc(hkpCollisionDispatcher::Agent3Funcs& f); // 0x00DB1800
    void registerAgent3(hkpCollisionDispatcher* dispatcher); // 0x00DB1850
    void* process(const hkpAgent3ProcessInput& input, hkpAgentEntry* entry, void* agentData, hkVector4f* separatingNormal, hkpProcessCollisionOutput& output); // 0x00DB1650
    void updateFilter(hkpAgentEntry* entry, void* agentData, const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpContactMgr* mgr, hkpConstraintOwner& constraintOwner); // 0x00DB11E0
    void invalidateTim(hkpAgentEntry* entry, void* agentData, const hkpCollisionInput& input); // 0x00DB1180
    void warpTime(hkpAgentEntry* entry, void* agentData, float oldTime, float newTime, const hkpCollisionInput& input); // 0x00DB11A0
    int calcAabbAndQueryTree(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkTransformf& bTa, const hkVector4f& linearTimInfo, const hkpProcessCollisionInput& input, hkAabb* cachedAabb, unsigned int* hitListOut, int hitListCapacity); // 0x00DB1240
    const hkpShapeCollection* getShapeCollectionIfBvTreeSupportsAabbQueries(const hkpCdBody* body, hkpShapeBufferStorage& buffer); // 0x00DB1630
    const hkpShapeContainer* getShapeContainerFrom(const hkpCdBody* body, hkpShapeBufferStorage& buffer); // 0x00DAB4C0
}
