// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent3/predgskagent3/hkppredgskagent3.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/geometry/collide/shapes/hkcdshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/dispatch/hkpcollisiondispatcher.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/gjk/hkpgskcache.h"

class hkVector4f;
struct hkpAgent3Input;
struct hkpAgent3ProcessInput;
struct hkpAgentEntry;
class hkpConstraintOwner;
class hkpContactMgr;
struct hkpProcessCollisionOutput;

namespace hkPredGskAgent3
{
    void initAgentFunc(hkpCollisionDispatcher::Agent3Funcs& f); // 0x00DC55F0
    void registerAgent3(hkpCollisionDispatcher* dispatcher, hkcdShapeType::ShapeTypeEnum typeA, hkcdShapeType::ShapeTypeEnum typeB); // 0x00DC5630
    void* create(const hkpAgent3Input& input, hkpAgentEntry* entry, void* agentData); // 0x00DC5090
    void sepNormal(const hkpAgent3Input& input, hkpAgentEntry* entry, void* agentData, hkVector4f& separatingNormalOut); // 0x00DC50F0
    void removePoint(hkpAgentEntry* entry, void* agentData, unsigned short idToRemove); // 0x00DC4F80
    void commitPotential(hkpAgentEntry* entry, void* agentData, unsigned short idToCommit); // 0x00DC4FC0
    void createZombie(hkpAgentEntry* entry, void* agentData, unsigned short idToConvert); // 0x00DC5000
    void* cleanup(hkpAgentEntry* entry, void* agentData, hkpContactMgr* mgr, hkpConstraintOwner& constraintOwner); // 0x00DC5030
    void destroy(hkpAgentEntry* entry, void* agentData, hkpContactMgr* mgr, hkpConstraintOwner& constraintOwner, hkpCollisionDispatcher* dispatcher); // 0x00DC5070
    void* process(const hkpAgent3ProcessInput& input, hkpAgentEntry* entry, void* agentData, hkVector4f* separatingNormal, hkpProcessCollisionOutput& output); // 0x00DC5210
    int getGskFlag(const void* agentData, hkpGskCache::GskFlagValues mask); // 0x00DB4E70
    void setGskFlagToTrue(void* agentData, hkpGskCache::GskFlagValues mask); // 0x00DB4E80
    void setGskFlagToFalse(void* agentData, hkpGskCache::GskFlagValues mask); // 0x00DB4E90
}
