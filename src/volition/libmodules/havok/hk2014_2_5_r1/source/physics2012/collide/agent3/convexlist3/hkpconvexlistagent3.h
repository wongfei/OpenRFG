// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent3/convexlist3/hkpconvexlistagent3.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/dispatch/hkpcollisiondispatcher.h"

struct hkConvexListAgent3Data;
struct hkPredGskAgent3Data;
class hkVector4f;
struct hkpAgent1nTrack;
struct hkpAgent3Input;
struct hkpAgent3ProcessInput;
struct hkpAgentEntry;
class hkpCdBody;
struct hkpCollisionInput;
class hkpConstraintOwner;
class hkpContactMgr;
struct hkpProcessCollisionOutput;

namespace hkConvexListAgent3
{
    void initAgentFunc(hkpCollisionDispatcher::Agent3Funcs& f); // 0x00DB5580
    void registerAgent3(hkpCollisionDispatcher* dispatcher); // 0x00DB55E0
    unsigned int isInGskMode(const void* agentData); // 0x00DB4EC0
    hkConvexListAgent3Data* getConvexListaData(const hkpAgentEntry* entry, void* data); // 0x00DB4ED0
    hkPredGskAgent3Data* getPredGskData(const hkpAgentEntry* entry, void* data); // 0x00DB4EE0
    void switchToStreamMode(const hkpAgent3ProcessInput& input, hkpAgentEntry* entry, void* agentData, hkpProcessCollisionOutput& output); // 0x00DB5D90
    void switchToGskMode(const hkpAgent3ProcessInput& input, hkpAgentEntry* entry, void* agentData, hkpProcessCollisionOutput& output); // 0x00DB5960
    hkpAgent1nTrack* getAgent1nTrack(const hkpAgentEntry* entry, void* data); // 0x00DB4EF0
    const hkpAgent1nTrack* getAgent1nTrack(const hkpAgentEntry* entry, const void* data); // 0x00DB4F00
    void updateFilter(hkpAgentEntry* entry, void* agentData, const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpContactMgr* mgr, hkpConstraintOwner& constraintOwner); // 0x00DB5040
    void invalidateTim(hkpAgentEntry* entry, void* agentData, const hkpCollisionInput& input); // 0x00DB4F10
    void warpTime(hkpAgentEntry* entry, void* agentData, float oldTime, float newTime, const hkpCollisionInput& input); // 0x00DB4F30
    void* create(const hkpAgent3Input& input, hkpAgentEntry* entry, void* agentData); // 0x00DB4F70
    void destroy(hkpAgentEntry* entry, void* agentData, hkpContactMgr* mgr, hkpConstraintOwner& constraintOwner, hkpCollisionDispatcher* dispatcher); // 0x00DB4FA0
    void sepNormal(const hkpAgent3Input& input, hkpAgentEntry* entry, void* agentData, hkVector4f& separatingNormalOut); // 0x00DB50A0
    void* cleanup(hkpAgentEntry* entry, void* agentData, hkpContactMgr* mgr, hkpConstraintOwner& constraintOwner); // 0x00DB4FD0
    void removePoint(hkpAgentEntry* entry, void* agentData, unsigned short idToRemove); // 0x00DB4FE0
    void commitPotential(hkpAgentEntry* entry, void* agentData, unsigned short idToCommit); // 0x00DB5000
    void createZombie(hkpAgentEntry* entry, void* agentData, unsigned short idToConvert); // 0x00DB5020
    void* process(const hkpAgent3ProcessInput& input, hkpAgentEntry* entry, void* agentData, hkVector4f* separatingNormal, hkpProcessCollisionOutput& output); // 0x00DB50E0
}
