// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent3/collection3/hkpcollectionagent3.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/dispatch/hkpcollisiondispatcher.h"

class hkVector4f;
struct hkpAgent3ProcessInput;
struct hkpAgentEntry;
struct hkpProcessCollisionOutput;

namespace hkpCollectionAgent3
{
    void initAgentFunc(hkpCollisionDispatcher::Agent3Funcs& f); // 0x00DB9EB0
    void registerAgent3(hkpCollisionDispatcher* dispatcher); // 0x00DB9ED0
    void* process(const hkpAgent3ProcessInput& input, hkpAgentEntry* entry, void* agentData, hkVector4f* separatingNormal, hkpProcessCollisionOutput& output); // 0x00DB9CD0
}
