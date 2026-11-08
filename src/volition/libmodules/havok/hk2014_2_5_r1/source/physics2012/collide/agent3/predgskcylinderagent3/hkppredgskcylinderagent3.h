// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent3/predgskcylinderagent3/hkppredgskcylinderagent3.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once


class hkVector4f;
struct hkpAgent3Input;
struct hkpAgent3ProcessInput;
struct hkpAgentEntry;
class hkpCollisionDispatcher;
struct hkpProcessCollisionOutput;

namespace hkPredGskCylinderAgent3
{
    void registerAgent3(hkpCollisionDispatcher* dispatcher); // 0x00DC4D10
    void* create(const hkpAgent3Input& input, hkpAgentEntry* entry, void* agentData); // 0x00DC41C0
    void* process(const hkpAgent3ProcessInput& input, hkpAgentEntry* entry, void* agentData, hkVector4f* separatingNormal, hkpProcessCollisionOutput& output); // 0x00DC43C0
}
