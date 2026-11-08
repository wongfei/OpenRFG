// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent3/collectioncollection3/hkpcollectioncollectionagent3.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/dispatch/hkpcollisiondispatcher.h"

class hkVector4f;
struct hkpAgent3ProcessInput;
struct hkpAgentEntry;
class hkpCdBody;
struct hkpCollisionInput;
class hkpConstraintOwner;
class hkpContactMgr;
struct hkpProcessCollisionOutput;
class hkpShapeContainer;
class hkpShapeKeyPair;

namespace hkpCollectionCollectionAgent3
{
    extern bool g_agentRegistered; // 0x03A29D70
    void initAgentFunc(hkpCollisionDispatcher::Agent3Funcs& f); // 0x00DB9100
    void registerAgent3(hkpCollisionDispatcher* dispatcher); // 0x00DB9150
    int process_gatherShapeKeys(const hkpAgent3ProcessInput& input, const hkpShapeContainer* shapeContainerA, const hkpShapeContainer* shapeContainerB, hkpShapeKeyPair* shapeKeyPairs); // 0x00DB8410
    void* process(const hkpAgent3ProcessInput& input, hkpAgentEntry* entry, void* agentData, hkVector4f* separatingNormal, hkpProcessCollisionOutput& output); // 0x00DB8DD0
    void updateFilter(hkpAgentEntry* entry, void* agentData, const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpContactMgr* mgr, hkpConstraintOwner& constraintOwner); // 0x00DB7A70
    void invalidateTim(hkpAgentEntry* entry, void* agentData, const hkpCollisionInput& input); // 0x00DB7A10
    void warpTime(hkpAgentEntry* entry, void* agentData, float oldTime, float newTime, const hkpCollisionInput& input); // 0x00DB7A30
}
