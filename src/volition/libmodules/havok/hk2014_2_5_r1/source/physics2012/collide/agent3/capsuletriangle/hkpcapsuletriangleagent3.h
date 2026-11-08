// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent3/capsuletriangle/hkpcapsuletriangleagent3.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once


class hkVector4f;
struct hkpAgent3Input;
struct hkpAgent3ProcessInput;
struct hkpAgentEntry;
class hkpCollisionDispatcher;
class hkpConstraintOwner;
class hkpContactMgr;
struct hkpProcessCollisionOutput;

namespace hkCapsuleTriangleAgent3
{
    void registerAgent3(hkpCollisionDispatcher* dispatcher); // 0x00DBC200
    void* create(const hkpAgent3Input& input, hkpAgentEntry* entry, void* agentData); // 0x00DBBB30
    void sepNormal(const hkpAgent3Input& input, hkpAgentEntry* entry, void* agentData, hkVector4f& separatingNormalOut); // 0x00DBBBE0
    void* cleanup(hkpAgentEntry* entry, void* agentData, hkpContactMgr* mgr, hkpConstraintOwner& constraintOwner); // 0x00DBBB70
    void removePoint(hkpAgentEntry* entry, void* agentData, unsigned short idToRemove); // 0x00DBBAC0
    void commitPotential(hkpAgentEntry* entry, void* agentData, unsigned short idToCommit); // 0x00DBBAF0
    void createZombie(hkpAgentEntry* entry, void* agentData, unsigned short idToConvert); // 0x00DBBB20
    void destroy(hkpAgentEntry* entry, void* agentData, hkpContactMgr* mgr, hkpConstraintOwner& constraintOwner, hkpCollisionDispatcher* dispatcher); // 0x00DBBBC0
    void* process(const hkpAgent3ProcessInput& input, hkpAgentEntry* entry, void* agentData, hkVector4f* separatingNormal, hkpProcessCollisionOutput& output); // 0x00DBBD70
}
