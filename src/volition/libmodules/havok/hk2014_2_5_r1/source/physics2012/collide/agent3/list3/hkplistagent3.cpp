// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent3/list3/hkplistagent3.cpp
//
// Havok 2014.2.5 (havok_impl.md HK6): the list agent3, from rfg.exe: a body against the enabled children of a list
// shape B through a 1n agent track (the agent data: 4 unused bytes, then the track; 0x10 bytes in all).
//
// The shipped build records the "TtList3" monitor timer; it is not reproduced.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent3/list3/hkplistagent3.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/router/hkmemoryrouter.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcdbody.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpprocesscollisionoutput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent3/machine/1n/hkpagent1nmachine.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent3/machine/1n/hkpagent1ntrack.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/dispatch/hkpcollisiondispatcher.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/compound/collection/list/hkplistshape.h"

#include <new>

static inline hkpAgent1nTrack& hkListAgent3_track(void* agentData)
{
    return *reinterpret_cast<hkpAgent1nTrack*>(static_cast<char*>(agentData) + 4);
}

static inline void* hkListAgent3_end(void* agentData)
{
    return static_cast<char*>(agentData) + 0x10;
}

// 0x00DCFB30
void hkListAgent3::initAgentFunc(hkpCollisionDispatcher::Agent3Funcs& f)
{
    f.m_createFunc = create;
    f.m_processFunc = process;
    f.m_sepNormalFunc = nullptr;
    f.m_cleanupFunc = nullptr;
    f.m_removePointFunc = nullptr;
    f.m_commitPotentialFunc = nullptr;
    f.m_createZombieFunc = nullptr;
    f.m_updateFilterFunc = updateFilter;
    f.m_invalidateTimFunc = invalidateTim;
    f.m_warpTimeFunc = warpTime;
    f.m_destroyFunc = destroy;
    f.m_isPredictive = true;
}

// 0x00DCFB80 (not called by hkpAgentRegisterUtil)
void hkListAgent3::registerAgent3(hkpCollisionDispatcher* dispatcher)
{
    hkpCollisionDispatcher::Agent3Funcs f;
    initAgentFunc(f);
    dispatcher->registerAgent3(f, hkcdShapeType::ALL_SHAPE_TYPES, hkcdShapeType::LIST);
}

// 0x00DCFB00: an empty track
void* hkListAgent3::create(const hkpAgent3Input& input, hkpAgentEntry* entry, void* agentData)
{
    hkpAgent1nTrack* track = new (&hkListAgent3_track(agentData)) hkpAgent1nTrack();
    hkAgent1nMachine_Create(*track);
    return hkListAgent3_end(agentData);
}

// 0x00DCF8F0
void hkListAgent3::destroy(hkpAgentEntry* entry, void* agentData, hkpContactMgr* mgr, hkpConstraintOwner& constraintOwner, hkpCollisionDispatcher* dispatcher)
{
    hkAgent1nMachine_Destroy(hkListAgent3_track(agentData), dispatcher, mgr, constraintOwner);
}

// 0x00DCF910
void hkListAgent3::invalidateTim(hkpAgentEntry* entry, void* agentData, const hkpCollisionInput& input)
{
    hkAgent1nMachine_InvalidateTim(hkListAgent3_track(agentData), input);
}

// 0x00DCF930
void hkListAgent3::warpTime(hkpAgentEntry* entry, void* agentData, float oldTime, float newTime, const hkpCollisionInput& input)
{
    hkAgent1nMachine_WarpTime(hkListAgent3_track(agentData), oldTime, newTime, input);
}

// 0x00DCF960
void hkListAgent3::updateFilter(hkpAgentEntry* entry, void* agentData, const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpContactMgr* mgr, hkpConstraintOwner& constraintOwner)
{
    hkpAgent1nMachine_VisitorInput vin;
    vin.m_containerShapeB = bodyB.getShape()->getContainer();
    vin.m_bodyA = &bodyA;
    vin.m_contactMgr = mgr;
    vin.m_input = &input;
    vin.m_collectionBodyB = &bodyB;
    vin.m_constraintOwner = &constraintOwner;
    hkAgent1nMachine_UpdateShapeCollectionFilter(hkListAgent3_track(agentData), vin);
}

// 0x00DCF9C0: the keys of the enabled children (every child past the 256 the enable bits cover), in order
void* hkListAgent3::process(const hkpAgent3ProcessInput& input, hkpAgentEntry* entry, void* agentData, hkVector4f* separatingNormal, hkpProcessCollisionOutput& output)
{
    const hkpListShape* list = static_cast<const hkpListShape*>(input.m_bodyB.val()->getShape());
    const int numChildren = list->m_childInfo.getSize();
    hkLifoAllocator& stack = hkMemoryRouter::getInstance().stack();
    const int bytes = (numChildren * (int)sizeof(unsigned int) + (int)sizeof(unsigned int) + 127) & ~127; // hkAllocateStack: 128-byte multiples
    unsigned int* newKeys = static_cast<unsigned int*>(stack.fastBlockAlloc(bytes));
    int numKeys = 0;
    unsigned int bit = 1;
    for (int i = 0; i < numChildren; i++)
    {
        if (i > 255 || (list->m_enabledChildren[i >> 5] & bit) != 0)
        {
            newKeys[numKeys++] = (unsigned int)i;
        }
        bit = (bit << 1) | (bit >> 31);
    }
    newKeys[numKeys] = 0xFFFFFFFF; // HK_INVALID_SHAPE_KEY
    hkAgent1nMachine_Process(hkListAgent3_track(agentData), input, list, newKeys, output);
    stack.fastBlockFree(newKeys, bytes);
    return hkListAgent3_end(agentData);
}
