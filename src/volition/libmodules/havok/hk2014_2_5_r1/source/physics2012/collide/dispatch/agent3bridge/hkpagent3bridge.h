// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/dispatch/agent3bridge/hkpagent3bridge.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once


class hkVector4f;
struct hkpAgent3Input;
struct hkpAgent3ProcessInput;
struct hkpAgentEntry;
class hkpCdBody;
class hkpCollisionAgent;
class hkpCollisionDispatcher;
struct hkpCollisionInput;
class hkpConstraintOwner;
class hkpContactMgr;
struct hkpProcessCollisionOutput;

namespace hkAgent3Bridge
{
    int registerAgent3(hkpCollisionDispatcher* dispatcher); // 0x00DCAD40
    void* create(const hkpAgent3Input& input, hkpAgentEntry* entry, void* agentData); // 0x00DCACC0
    void* process(const hkpAgent3ProcessInput& input, hkpAgentEntry* entry, void* agentData, hkVector4f* separatingNormalOut, hkpProcessCollisionOutput& result); // 0x00DCAC90
    void destroy(hkpAgentEntry* entry, void* agentData, hkpContactMgr* mgr, hkpConstraintOwner& constraintOwner, hkpCollisionDispatcher* dispatcher); // 0x00DCAB90
    void updateFilter(hkpAgentEntry* entry, void* agentData, const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpContactMgr* mgr, hkpConstraintOwner& constraintOwner); // 0x00DCABB0
    void invalidateTim(hkpAgentEntry* entry, void* agentData, const hkpCollisionInput& input); // 0x00DCABE0
    void warpTime(hkpAgentEntry* entry, void* agentData, float oldTime, float newTime, const hkpCollisionInput& input); // 0x00DCAC00
    void removePoint(hkpAgentEntry* entry, void* agentData, unsigned short idToRemove); // 0x00DCAC30
    void commitPotential(hkpAgentEntry* entry, void* agentData, unsigned short newId); // 0x00DCAC50
    void createZombie(hkpAgentEntry* entry, void* agentData, unsigned short idTobecomeZombie); // 0x00DCAC70
    hkpCollisionAgent*& getChildAgent(void* agentData); // 0x00DCAB80
    void* getAgentDataEnd(void* agentData); // 0x00DCADC0
}
