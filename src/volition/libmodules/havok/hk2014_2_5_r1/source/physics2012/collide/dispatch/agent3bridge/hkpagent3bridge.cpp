// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/dispatch/agent3bridge/hkpagent3bridge.cpp
//
// Havok 2014.2.5 (havok_impl.md HK6): the agent3 bridge, from rfg.exe. It runs an old style (agent2) collision agent
// inside an agent3 stream: the agent data holds the agent pointer, padded to 16 bytes, and every stream function
// forwards to the agent's virtual.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/dispatch/agent3bridge/hkpagent3bridge.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcdbody.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpcollisionagent.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpprocesscollisioninput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent3/hkpagent3.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/dispatch/hkpcollisiondispatcher.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/hkpshape.h"

// 0x00DCAB80
hkpCollisionAgent*& hkAgent3Bridge::getChildAgent(void* agentData)
{
    return *reinterpret_cast<hkpCollisionAgent**>(agentData);
}

// 0x00DCADC0: the agent pointer padded to 16 bytes
void* hkAgent3Bridge::getAgentDataEnd(void* agentData)
{
    return hkAddByteOffset(agentData, 16);
}

// 0x00DCACC0
// The pair's agent2 agent (predictive or not, as the input asks); the contact point count 0xFF tells the machines that
// the agent manages its own contact points.
void* hkAgent3Bridge::create(const hkpAgent3Input& input, hkpAgentEntry* entry, void* agentData)
{
    entry->m_streamCommand = (unsigned char)((entry->m_streamCommand & hkAgent3::TRANSFORM_FLAG) | hkAgent3::STREAM_CALL_AGENT);
    const hkpCdBody* bodyA = input.m_bodyA;
    const hkpCdBody* bodyB = input.m_bodyB;
    const hkpProcessCollisionInput* in = input.m_input;
    hkpCollisionDispatcher* dispatcher = in->m_dispatcher;
    getChildAgent(agentData) = dispatcher->getNewCollisionAgent(*bodyA, *bodyB, *in, input.m_contactMgr);
    entry->m_numContactPoints = 0xFF;
    return getAgentDataEnd(agentData);
}

// 0x00DCAC90
void* hkAgent3Bridge::process(const hkpAgent3ProcessInput& input, hkpAgentEntry* entry, void* agentData, hkVector4f* separatingNormalOut, hkpProcessCollisionOutput& result)
{
    getChildAgent(agentData)->processCollision(*input.m_bodyA, *input.m_bodyB, *input.m_input, result);
    return getAgentDataEnd(agentData);
}

// 0x00DCAB90
void hkAgent3Bridge::destroy(hkpAgentEntry* entry, void* agentData, hkpContactMgr* mgr, hkpConstraintOwner& constraintOwner, hkpCollisionDispatcher* dispatcher)
{
    getChildAgent(agentData)->cleanup(constraintOwner);
}

// 0x00DCABB0
void hkAgent3Bridge::updateFilter(hkpAgentEntry* entry, void* agentData, const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpContactMgr* mgr, hkpConstraintOwner& constraintOwner)
{
    getChildAgent(agentData)->updateShapeCollectionFilter(bodyA, bodyB, input, constraintOwner);
}

// 0x00DCABE0
void hkAgent3Bridge::invalidateTim(hkpAgentEntry* entry, void* agentData, const hkpCollisionInput& input)
{
    getChildAgent(agentData)->invalidateTim(input);
}

// 0x00DCAC00
void hkAgent3Bridge::warpTime(hkpAgentEntry* entry, void* agentData, float oldTime, float newTime, const hkpCollisionInput& input)
{
    getChildAgent(agentData)->warpTime(oldTime, newTime, input);
}

// 0x00DCAC30
void hkAgent3Bridge::removePoint(hkpAgentEntry* entry, void* agentData, unsigned short idToRemove)
{
    getChildAgent(agentData)->removePoint(idToRemove);
}

// 0x00DCAC50
void hkAgent3Bridge::commitPotential(hkpAgentEntry* entry, void* agentData, unsigned short newId)
{
    getChildAgent(agentData)->commitPotential(newId);
}

// 0x00DCAC70
void hkAgent3Bridge::createZombie(hkpAgentEntry* entry, void* agentData, unsigned short idTobecomeZombie)
{
    getChildAgent(agentData)->createZombie(idTobecomeZombie);
}

// 0x00DCAD40
// Agent3 type 1, for every pair (registered first by resetCreationFunctions).
int hkAgent3Bridge::registerAgent3(hkpCollisionDispatcher* dispatcher)
{
    hkpCollisionDispatcher::Agent3Funcs f;
    f.m_ignoreSymmetricVersion = false;
    f.m_reusePreviousEntry = false;
    f.m_sepNormalFunc = nullptr;
    f.m_cleanupFunc = nullptr;
    f.m_commitPotentialFunc = commitPotential;
    f.m_createZombieFunc = createZombie;
    f.m_removePointFunc = removePoint;
    f.m_createFunc = create;
    f.m_processFunc = process;
    f.m_destroyFunc = destroy;
    f.m_updateFilterFunc = updateFilter;
    f.m_invalidateTimFunc = invalidateTim;
    f.m_warpTimeFunc = warpTime;
    f.m_isPredictive = true;
    return dispatcher->registerAgent3(f, hkcdShapeType::ALL_SHAPE_TYPES, hkcdShapeType::ALL_SHAPE_TYPES);
}
