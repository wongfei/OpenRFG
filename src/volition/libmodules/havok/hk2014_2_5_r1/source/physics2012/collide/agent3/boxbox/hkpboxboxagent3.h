// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent3/boxbox/hkpboxboxagent3.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/dispatch/hkpcollisiondispatcher.h"

class hkVector4f;
struct hkpAgent3Input;
struct hkpAgent3ProcessInput;
struct hkpAgentEntry;
class hkpConstraintOwner;
class hkpContactMgr;
struct hkpProcessCollisionOutput;

namespace hkBoxBoxAgent3
{
    void initAgentFunc(hkpCollisionDispatcher::Agent3Funcs& f); // 0x00DC3450
    void registerAgent3(hkpCollisionDispatcher* dispatcher); // 0x00DC3490
    void* create(const hkpAgent3Input& input, hkpAgentEntry* entry, void* agentData); // 0x00DC3250
    void* cleanup(hkpAgentEntry* entry, void* agentData, hkpContactMgr* mgr, hkpConstraintOwner& constraintOwner); // 0x00DC3270
    void removePoint(hkpAgentEntry* entry, void* agentData, unsigned short idToRemove); // 0x00DC31C0
    void commitPotential(hkpAgentEntry* entry, void* agentData, unsigned short idToCommit); // 0x00DC3200
    void createZombie(hkpAgentEntry* entry, void* agentData, unsigned short idToConvert); // 0x00DC3240
    void destroy(hkpAgentEntry* entry, void* agentData, hkpContactMgr* mgr, hkpConstraintOwner& constraintOwner, hkpCollisionDispatcher* dispatcher); // 0x00DC32D0
    void* process(const hkpAgent3ProcessInput& input, hkpAgentEntry* entry, void* agentData, hkVector4f* separatingNormal, hkpProcessCollisionOutput& output); // 0x00DC32F0
}
