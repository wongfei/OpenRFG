// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/simulation/continuous/hkpcontinuoussimulation.cpp
//
// Havok 2014.2.5 (havok_impl.md HK7): the continuous simulation (the one rfg.exe uses), from rfg.exe. collide also
// gathers the agents' times of impact into m_toiEvents; advanceTime handles them in time order up to the frame:
// simple TOIs (debris) freeze the bodies, the others run the localized TOI solver (simulateToi and the hkLs_ functions,
// HK8: an incremental constraint solve from the TOI time, activating the entities the violated constraints reach).
// The monitor timers are not reproduced.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/simulation/continuous/hkpcontinuoussimulation.h"

#include <math.h>
#include <string.h>

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/algorithm/sort/hksort.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/config/hkoptionalcomponent.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/localarray/hklocalarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/localarray/hklocalbuffer.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/debugutil/memoryexceptiontestingutil/hkmemoryexceptiontestingutil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/swepttransform/hkswepttransformutil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/system/hkmemorysystem.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/contactmgr/hkpcontactmgr.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpcollisionqualityinfo.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpprocesscollisioninput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpprocesscollisionoutput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent3/machine/nn/hkpagentnnmachine.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent3/machine/nn/hkplinkedcollidable.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/broadphase/hkpbroadphase.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/broadphase/hkpbroadphasehandlepair.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/dispatch/broadphase/hkptypedbroadphasedispatcher.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/dispatch/hkpcollisiondispatcher.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/filter/hkpcollisionfilter.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraint/data/hkpconstraintdata.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraintsolver/accumulator/hkpvelocityaccumulator.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraintsolver/constraint/hkpconstraintqueryin.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraintsolver/solve/hkpsolve.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraintsolver/solve/hkpsolverinfo.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/collide/hkpdynamicscontactmgr.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/atom/hkpsimplecontactconstraintatom.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/contact/hkpsimplecontactconstraintdata.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/setup/hkpconstraintsolversetup.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/hkpentity.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/util/hkpentityaabbutil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/motion/util/hkprigidmotionutil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/hkpsimulationisland.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/hkpworld.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/util/hkpworldagentutil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/util/hkpworldcallbackutil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/dynamics/world/simulation/continuous/hkptoievent.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/solver/atom/hkpbuildjacobianfromatoms.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/solver/solve/hkpsolvergettoivelocities.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/dynamics/world/simulation/continuous/toiresourcemgr/default/hkpdefaulttoiresourcemgr.h"

// 0x03A2ADC4, constructed by a dynamic initializer: hkProductFeatures::initialize sets
// hkpSimulation::createContinuous through it (hkpContinuousSimulation::create marks it used).
HK_OPTIONAL_COMPONENT_DEFINE(hkpContinuousSimulation, hkpSimulation::createContinuous, hkpContinuousSimulation::create);

namespace
{
// hkpWorld::unlockAndAttemptToExecutePendingOperations (inlined)
inline void hkContinuousSimulation_unlock(hkpWorld* world)
{
    world->m_criticalOperationsLockCount--;
    if (world->m_criticalOperationsLockCount == 0 && !world->m_blockExecutingPendingOperations.m_bool)
    {
        if (world->m_pendingOperationsCount != 0)
        {
            world->internal_executePendingOperations();
        }
        if (world->m_pendingOperationQueueCount == 1 && world->m_pendingBodyOperationsCount != 0)
        {
            world->internal_executePendingBodyOperations();
        }
    }
}

inline hkpEntity* hkContinuousSimulation_entity(const hkpCollidable* collidable)
{
    return static_cast<hkpEntity*>((hkpWorldObject*)collidable->getOwner());
}

// the island an event's TOI belongs to: the first entity's unless that one is fixed
inline hkpSimulationIsland* hkContinuousSimulation_eventIsland(const hkpToiEvent& event)
{
    hkpEntity* entity = event.m_entities[0];
    if (entity->m_motion.m_type.m_storage == hkpMotion::MOTION_FIXED)
    {
        entity = event.m_entities[1];
    }
    return entity->m_simulationIsland;
}

// m_toiEvents.removeAt(index): the last event moves into the hole
inline void hkContinuousSimulation_removeEvent(hkArray<hkpToiEvent, hkContainerHeapAllocator>& events, int index)
{
    events.m_size--;
    if (events.getSize() != index)
    {
        memcpy(&events[index], &events[events.getSize()], sizeof(hkpToiEvent));
    }
}

// the event's TOI leaves its contact manager (removeToiImpl) and the queue
inline void hkContinuousSimulation_removeEventAndToi(hkArray<hkpToiEvent, hkContainerHeapAllocator>& events, int index)
{
    hkpToiEvent& event = events[index];
    event.m_contactMgr->removeToiImpl(*hkContinuousSimulation_eventIsland(event), event.m_properties);
    hkContinuousSimulation_removeEvent(events, index);
}

inline hkStepInfo hkContinuousSimulation_stepInfo(float start, float end)
{
    hkStepInfo info;
    info.m_startTime = start;
    info.m_endTime = end;
    info.m_deltaTime = end - start;
    info.m_invDeltaTime = 0.0f;
    if (info.m_deltaTime.val() != 0.0f)
    {
        info.m_invDeltaTime = 1.0f / info.m_deltaTime.val();
    }
    return info;
}

// the collision output as the continuous passes reset it per agent
inline void hkContinuousSimulation_resetOutput(hkpProcessCollisionOutput& output)
{
    output.m_firstFreeContactPoint = &output.m_contactPoints[0];
    output.m_toi.m_time = HK_REAL_MAX;
    output.m_potentialContacts = nullptr;
}
}

// 0x00DFB940: a default TOI resource manager; the event queue takes m_sizeOfToiEventQueue events
hkpContinuousSimulation::hkpContinuousSimulation(hkpWorld* world)
    : hkpSimulation(world)
{
    m_lastProcessingStep.m_storage = COLLIDE;
    m_toiResourceMgr = new hkpDefaultToiResourceMgr();
    m_toiCounter = 0;
    const int size = world->m_sizeOfToiEventQueue;
    const int capacity = m_toiEvents.getCapacity();
    if (capacity < size)
    {
        hkArrayUtil::_reserve(hkContainerHeapAllocator::s_alloc, &m_toiEvents, capacity * 2 > size ? capacity * 2 : size, sizeof(hkpToiEvent));
    }
}

// 0x00DFB9E0
hkpContinuousSimulation::~hkpContinuousSimulation()
{
    if (m_toiResourceMgr != nullptr)
    {
        delete m_toiResourceMgr;
    }
}

// 0x00DFBA50
hkpSimulation* hkpContinuousSimulation::create(hkpWorld* world)
{
    HK_OPTIONAL_COMPONENT_MARK_USED(hkpContinuousSimulation);
    return new hkpContinuousSimulation(world);
}

// 0x00DFA300, 0x00DFA320
void hkpContinuousSimulation::assertThereIsNoCollisionInformationForEntities(hkpEntity** entities, int numEntities, hkpWorld* world)
{
}

void hkpContinuousSimulation::assertThereIsNoCollisionInformationForAgent(hkpAgentNnEntry* agent)
{
}

// 0x00DFA330: the queued TOIs move with the time (the events of the world's simulation)
void hkpContinuousSimulation::warpTime(float warpDeltaTime)
{
    hkpContinuousSimulation* simulation = static_cast<hkpContinuousSimulation*>(m_world->m_simulation);
    for (int i = 0; i < simulation->m_toiEvents.getSize(); i++)
    {
        simulation->m_toiEvents[i].m_time += warpDeltaTime;
    }
}

// 0x00DFA370
static hkBool less_EntityPtr(const hkpEntity* a, const hkpEntity* b)
{
    hkBool result;
    result.m_bool = a->m_uid < b->m_uid;
    return result;
}

// 0x00DFA390: by the uid of the master entity, then by internal address
static hkBool less_ConstraintPtr(const hkpConstraintInstance* a, const hkpConstraintInstance* b)
{
    const hkConstraintInternal* ia = a->m_internal;
    const hkConstraintInternal* ib = b->m_internal;
    const unsigned int uidA = ia->m_entities[ia->m_whoIsMaster]->m_uid;
    const unsigned int uidB = ib->m_entities[ib->m_whoIsMaster]->m_uid;
    hkBool result;
    if (uidA < uidB)
    {
        result.m_bool = 1;
    }
    else if (uidA == uidB)
    {
        result.m_bool = ia < ib;
    }
    else
    {
        result.m_bool = 0;
    }
    return result;
}

// 0x00DFA4E0: a full queue drops the TOI (with a warning in the binary)
void hkpContinuousSimulation::addToiEvent(const hkpProcessCollisionOutput& processOutput, const hkpAgentNnEntry& entry)
{
    if (m_toiEvents.getSize() == m_toiEvents.getCapacity())
    {
        // "TOI event queue full, consider using HK_COLLIDABLE_QUALITY_DEBRIS for some objects or increase
        // hkpWorldCinfo::m_sizeOfToiEventQueue" (warning 0xF0323454)
        return;
    }
    hkpToiEvent& event = m_toiEvents.begin()[m_toiEvents.m_size++];
    const hkpProcessCollisionData::ToiInfo& toi = processOutput.m_toi;
    event.m_time = toi.m_time;
    event.m_useSimpleHandling = m_world->m_collisionDispatcher->m_collisionQualityInfo[(signed char)entry.m_collisionQualityIndex].m_useSimpleToiHandling;
    event.m_seperatingVelocity = toi.m_seperatingVelocity;
    event.m_contactPoint.m_position = toi.m_contactPoint.m_position;
    event.m_contactPoint.m_separatingNormal = toi.m_contactPoint.m_separatingNormal;
    event.m_entities[0] = hkContinuousSimulation_entity(entry.m_collidable[0]);
    event.m_entities[1] = hkContinuousSimulation_entity(entry.m_collidable[1]);
    static_cast<hkpSolverResults&>(event.m_properties) = static_cast<const hkpSolverResults&>(toi.m_properties);
    static_cast<hkContactPointMaterial&>(event.m_properties) = static_cast<const hkContactPointMaterial&>(toi.m_properties);
    event.m_properties.m_internalDataA = toi.m_properties.m_internalDataA;
    event.m_contactMgr = static_cast<hkpDynamicsContactMgr*>(entry.m_contactMgr);
    for (int i = 0; i < 7; i++)
    {
        event.m_extendedUserDatas[i] = toi.m_properties.m_extendedUserDatas[i];
    }
}

// 0x00DFA480: the agent's TOIs leave the queue (without telling the contact manager)
void hkpContinuousSimulation::removeCollisionInformationForAgent(hkpAgentNnEntry* agent)
{
    for (int i = m_toiEvents.getSize() - 1; i >= 0; i--)
    {
        if (m_toiEvents[i].m_contactMgr == agent->m_contactMgr)
        {
            hkContinuousSimulation_removeEvent(m_toiEvents, i);
        }
    }
}

// 0x00DFA640: the TOIs of any of the entities (front to back; the moved-in event is checked again)
void hkpContinuousSimulation::removeToiEventsOfEntities(hkpEntity** entities, int numEntities)
{
    if (m_toiEvents.getSize() == 0)
    {
        return;
    }
    hkMap<unsigned long, unsigned long> map;
    if (numEntities != 0)
    {
        map.reserve(numEntities);
        for (int i = 0; i < numEntities; i++)
        {
            map.insert((unsigned long)entities[i], 0);
        }
    }
    for (int i = 0; i < m_toiEvents.getSize(); i++)
    {
        const hkpToiEvent& event = m_toiEvents[i];
        const int a = map.findKey((unsigned long)event.m_entities[0]);
        const int b = map.findKey((unsigned long)event.m_entities[1]);
        if (map.isValid(a) || map.isValid(b))
        {
            hkContinuousSimulation_removeEventAndToi(m_toiEvents, i);
            i--;
        }
    }
}

// 0x00DFA780
void hkpContinuousSimulation::removeToiEventsOfEntity(hkpEntity* entity)
{
    for (int i = 0; i < m_toiEvents.getSize(); i++)
    {
        if (m_toiEvents[i].m_entities[0] == entity || m_toiEvents[i].m_entities[1] == entity)
        {
            hkContinuousSimulation_removeEventAndToi(m_toiEvents, i);
            i--;
        }
    }
}

// 0x00E006C0
void hkpContinuousSimulation::fireToiEventRemoved(hkpToiEvent& event)
{
    event.m_contactMgr->removeToiImpl(*hkContinuousSimulation_eventIsland(event), event.m_properties);
}

// 0x00DFAB60
void hkpContinuousSimulation::resetCollisionInformationForEntities(hkpEntity** entities, int numEntities, hkpWorld* world, hkpSimulation::ResetCollisionInformation resetInfo)
{
    hkpSimulation::resetCollisionInformationForEntities(entities, numEntities, world, resetInfo);
    if (resetInfo & RESET_TOI)
    {
        if (numEntities != 1)
        {
            removeToiEventsOfEntities(entities, numEntities);
            return;
        }
        removeToiEventsOfEntity(entities[0]);
    }
}

// 0x00DFAF30: every agent with its own quality (predictive when continuous); TOIs are queued
void hkpContinuousSimulation::collideIslandNarrowPhaseContinuous(hkpSimulationIsland* isle, const hkpProcessCollisionInput& input)
{
    hkpProcessCollisionOutput output(isle);
    output.m_toi.m_time = HK_REAL_MAX;
    output.m_toi.m_properties.m_impulseApplied = 0.0f;
    output.m_toi.m_properties.m_internalSolverData = 0.0f;
    hkpAgentNnTrack* tracks[2] = { &isle->m_narrowphaseAgentTrack, &isle->m_midphaseAgentTrack };
    for (int t = 0; t < 2; t++)
    {
        HK_FOR_ALL_AGENT_ENTRIES_BEGIN(*tracks[t], entry)
        {
            hkpCollisionQualityInfo* quality = &input.m_dispatcher.val()->m_collisionQualityInfo[(signed char)entry->m_collisionQualityIndex];
            input.m_collisionQualityInfo = quality;
            hkContinuousSimulation_resetOutput(output);
            const_cast<hkpProcessCollisionInput&>(input).m_createPredictiveAgents = quality->m_useContinuousPhysics.val();
            hkAgentNnMachine_ProcessAgent(entry, input, output, entry->m_contactMgr);
            if (hkOutOfMemoryState == hkMemoryAllocator::MEMORY_STATE_OUT_OF_MEMORY)
            {
                return;
            }
            if (!output.isEmpty().m_bool)
            {
                entry->m_contactMgr->processContactImpl(*entry->m_collidable[0], *entry->m_collidable[1], input, output);
            }
            if (output.m_toi.m_time.val() != HK_REAL_MAX)
            {
                addToiEvent(output, *entry);
            }
        }
        HK_FOR_ALL_AGENT_ENTRIES_END;
    }
}

// 0x00DFB160
void hkpContinuousSimulation::processAgentCollideContinuous(hkpAgentNnEntry* entry, const hkpProcessCollisionInput& processInput, hkpProcessCollisionOutput& processOutput)
{
    hkContinuousSimulation_resetOutput(processOutput);
    hkpCollisionQualityInfo* quality = processInput.m_dispatcher.val()->m_collisionQualityInfo + (signed char)entry->m_collisionQualityIndex;
    processInput.m_collisionQualityInfo = quality;
    const_cast<hkpProcessCollisionInput&>(processInput).m_createPredictiveAgents = quality->m_useContinuousPhysics.val();
    hkAgentNnMachine_ProcessAgent(entry, processInput, processOutput, entry->m_contactMgr);
    if (hkOutOfMemoryState == hkMemoryAllocator::MEMORY_STATE_OUT_OF_MEMORY)
    {
        return;
    }
    if (!processOutput.isEmpty().m_bool)
    {
        entry->m_contactMgr->processContactImpl(*entry->m_collidable[0], *entry->m_collidable[1], processInput, processOutput);
    }
    if (processOutput.m_toi.m_time.val() != HK_REAL_MAX)
    {
        addToiEvent(processOutput, *entry);
    }
}

// 0x00DFBA90
void hkpContinuousSimulation::collideEntitiesNarrowPhaseContinuous(hkpEntity** entities, int numEntities, const hkpProcessCollisionInput& input)
{
    processAgentsOfEntities(entities, numEntities, input, static_cast<AgentEntryProcessFunction>(&hkpContinuousSimulation::processAgentCollideContinuous), FIND_CONTACTS_DEFAULT);
}

// 0x00E00610: spins until the solver export (multithreaded simulation) is done; unused single threaded
void hkpContinuousSimulation::waitForSolverExport(signed char* exportFinished)
{
    if (exportFinished == nullptr)
    {
        return;
    }
    unsigned int spins = 0;
    while (*exportFinished != 1 && *exportFinished != 2)
    {
        for (int i = 1 << (spins & 0x1F); i > 0; i--)
        {
        }
        if (spins < 0x10)
        {
            spins++;
        }
    }
}

// 0x00DFBAB0: the broadphase with the entities' expanded boxes (the swept motion), under the broadphase's lock; the
// pairs change with the entities' island locked for constraint updates (new agents only if 1 KB per pair fits)
void hkpContinuousSimulation::collideEntitiesBroadPhaseContinuous(hkpEntity** entities, int numEntities, hkpWorld* world, signed char* exportFinished)
{
    if (numEntities == 0)
    {
        return;
    }
    hkLocalArray<hkpBroadPhaseHandlePair> delPairs(world->m_broadPhaseUpdateSize);
    hkLocalArray<hkpBroadPhaseHandlePair> newPairs(world->m_broadPhaseUpdateSize);
    {
        hkLocalBuffer<hkAabbUint32> aabbs(numEntities);
        hkLocalBuffer<hkpBroadPhaseHandle*> handles(numEntities);
        for (int i = 0; i < numEntities; i++)
        {
            hkpEntity* entity = entities[i];
            handles[i] = &entity->m_collidable.m_broadPhaseHandle;
            hkpCollidable::BoundingVolumeData& bv = entity->m_collidable.m_boundingVolumeData;
            if (bv.m_max[0] < bv.m_min[0])
            {
                hkpEntityAabbUtil::entityBatchRecalcAabb(world->m_collisionInput, &entity, 1);
            }
            // hkAabbUtil::uncompressExpandedAabbUint32 (the expansion bytes of the local box stay unset)
            const int shift = bv.m_expansionShift;
            hkAabbUint32& aabb = aabbs[i];
            for (int k = 0; k < 3; k++)
            {
                aabb.m_min[k] = bv.m_min[k] - ((unsigned int)bv.m_expansionMin[k] << shift);
                aabb.m_max[k] = bv.m_max[k] + ((unsigned int)bv.m_expansionMax[k] << shift);
            }
        }
        hkpBroadPhase* broadPhase = world->m_broadPhase;
        if (broadPhase->m_criticalSection != nullptr)
        {
            broadPhase->lockImplementation();
        }
        world->m_broadPhase->updateAabbsUint32(handles.begin(), aabbs.begin(), numEntities, newPairs, delPairs);
        if (world->m_broadPhase->m_criticalSection != nullptr)
        {
            world->m_broadPhase->unlockImplementation();
        }
    }
    if (delPairs.getSize() + newPairs.getSize() > 0)
    {
        hkpTypedBroadPhaseDispatcher::removeDuplicates(newPairs, delPairs);
    }
    waitForSolverExport(exportFinished);
    if (delPairs.getSize() + newPairs.getSize() > 0)
    {
        world->lockIslandForConstraintUpdate(entities[0]->m_simulationIsland);
        world->m_broadPhaseDispatcher->removePairs(reinterpret_cast<hkpTypedBroadPhaseHandlePair*>(delPairs.begin()), delPairs.getSize());
        if (hkMemorySystem::getInstance().heapCanAllocTotal(newPairs.getSize() << 10))
        {
            const hkpCollidableCollidableFilter* filter = world->m_collisionFilter != nullptr ? static_cast<const hkpCollidableCollidableFilter*>(world->m_collisionFilter) : nullptr;
            world->m_broadPhaseDispatcher->addPairs(reinterpret_cast<hkpTypedBroadPhaseHandlePair*>(newPairs.begin()), newPairs.getSize(), filter);
        }
        world->unlockIslandForConstraintUpdate(entities[0]->m_simulationIsland);
    }
}

// 0x00DFC190: the entities' agents to other entities (or to entities of the island not in the list) are processed
// again for TOIs only; agents without continuous physics, or with simple TOI handling, are left for the PSI pass of
// their entity (referenced in entitiesNeedingPsiCollisionDetection)
void hkpContinuousSimulation::collideEntitiesOfOneIslandNarrowPhaseContinuous_toiOnly(hkpEntity** entities, int numEntities, const hkpProcessCollisionInput& input, hkPointerMap<unsigned int, hkpEntity*, hkContainerHeapAllocator>& entitiesNeedingPsiCollisionDetection)
{
    hkpSimulationIsland* island = entities[0]->m_simulationIsland;
    const int islandSize = island->m_entities.getSize();
    hkLocalArray<unsigned char> inList(islandSize);
    memset(inList.begin(), 0, islandSize);
    hkpProcessCollisionOutput output(island);
    output.m_toi.m_time = HK_REAL_MAX;
    output.m_toi.m_properties.m_impulseApplied = 0.0f;
    output.m_toi.m_properties.m_internalSolverData = 0.0f;
    for (int e = 0; e < numEntities; e++)
    {
        hkpEntity* entity = entities[e];
        inList.begin()[entity->m_storageIndex] = 1;
        hkArray<hkpLinkedCollidable::CollisionEntry, hkContainerHeapAllocator> entries;
        entity->m_collidable.getCollisionEntriesSorted(entries);
        for (int i = 0; i < entries.getSize(); i++)
        {
            hkpEntity* partner = hkContinuousSimulation_entity(entries[i].m_partner);
            if (partner->m_simulationIsland == island && inList.begin()[partner->m_storageIndex] != 0)
            {
                continue; // processed from the partner
            }
            hkpAgentNnEntry* entry = entries[i].m_agentEntry;
            const hkpCollisionQualityInfo& quality = m_world->m_collisionDispatcher->m_collisionQualityInfo[(signed char)entry->m_collisionQualityIndex];
            if (quality.m_useContinuousPhysics.val() == 0 || quality.m_useSimpleToiHandling.m_bool)
            {
                if (entitiesNeedingPsiCollisionDetection.getWithDefault(entity->m_uid, nullptr) == nullptr)
                {
                    entity->addReference();
                    entitiesNeedingPsiCollisionDetection.insert(entity->m_uid, entity);
                }
                continue;
            }
            hkContinuousSimulation_resetOutput(output);
            hkpCollisionQualityInfo* q = input.m_dispatcher.val()->m_collisionQualityInfo + (signed char)entry->m_collisionQualityIndex;
            input.m_collisionQualityInfo = q;
            const_cast<hkpProcessCollisionInput&>(input).m_createPredictiveAgents = q->m_useContinuousPhysics.val();
            hkAgentNnMachine_ProcessAgent(entry, input, output, entry->m_contactMgr);
            if (hkOutOfMemoryState == hkMemoryAllocator::MEMORY_STATE_OUT_OF_MEMORY)
            {
                return;
            }
            if (!output.isEmpty().m_bool)
            {
                entry->m_contactMgr->processContactImpl(*entry->m_collidable[0], *entry->m_collidable[1], input, output);
            }
            if (output.m_toi.m_time.val() != HK_REAL_MAX)
            {
                addToiEvent(output, *entry);
            }
        }
    }
}

// 0x00DFC560: the PSI pass of the entities waiting for it (in hash table order): their agents without continuous
// physics, or with simple handling once the entity is frozen (inverse delta time 0), are processed once with the
// PSI quality; the references taken when they were queued go
void hkpContinuousSimulation::collideEntitiesNeedingPsiCollisionDetectionNarrowPhase_toiOnly(const hkpProcessCollisionInput& input, hkPointerMap<unsigned int, hkpEntity*, hkContainerHeapAllocator>& entitiesNeedingPsiCollisionDetection)
{
    struct AgentAndOwner
    {
        hkpAgentNnEntry* m_entry;
        hkpConstraintOwner* m_owner;
    };
    m_world->m_criticalOperationsLockCount++;
    hkMap<unsigned long, unsigned long> processed;
    hkArray<AgentAndOwner, hkContainerHeapAllocator> agents;
    hkPointerMap<unsigned int, hkpEntity*, hkContainerHeapAllocator>::Map& map = entitiesNeedingPsiCollisionDetection.m_map;
    for (int slot = 0; slot <= map.m_hashMod; slot++)
    {
        if (map.m_elem[slot].key == (unsigned long)-1)
        {
            continue;
        }
        hkpEntity* entity = (hkpEntity*)map.m_elem[slot].val;
        hkArray<hkpLinkedCollidable::CollisionEntry, hkContainerHeapAllocator>& entries = entity->m_collidable.m_collisionEntries;
        const int numEntries = entries.getSize();
        for (int i = 0; i < numEntries; i++)
        {
            hkpAgentNnEntry* entry = entries[i].m_agentEntry;
            if (processed.getWithDefault((unsigned long)entry, 0) != 0)
            {
                continue;
            }
            const hkpCollisionQualityInfo& quality = m_world->m_collisionDispatcher->m_collisionQualityInfo[(signed char)entry->m_collisionQualityIndex];
            const bool frozen = ((const float*)&entity->m_motion.m_motionState.getSweptTransform().m_centerOfMass1)[3] == 0.0f;
            if (quality.m_useContinuousPhysics.val() != 0 && !(quality.m_useSimpleToiHandling.m_bool && frozen))
            {
                continue;
            }
            hkpSimulationIsland* owner = entity->m_simulationIsland;
            if (entity->m_motion.m_type.m_storage == hkpMotion::MOTION_FIXED)
            {
                // the partner: entityA ^ entityB ^ entity
                hkpEntity* a = hkContinuousSimulation_entity(entry->m_collidable[0]);
                hkpEntity* b = hkContinuousSimulation_entity(entry->m_collidable[1]);
                owner = ((hkpEntity*)((size_t)a ^ (size_t)b ^ (size_t)entity))->m_simulationIsland;
            }
            processed.insert((unsigned long)entry, 1);
            AgentAndOwner& item = agents.expandOne();
            item.m_entry = entry;
            item.m_owner = owner;
        }
        entity->removeReference();
    }
    map.clear();

    hkpProcessCollisionOutput output(nullptr);
    for (int i = 0; i < agents.getSize(); i++)
    {
        hkpAgentNnEntry* entry = agents[i].m_entry;
        output.m_constraintOwner = agents[i].m_owner;
        output.m_firstFreeContactPoint = &output.m_contactPoints[0];
        output.m_toi.m_properties.m_impulseApplied = 0.0f;
        output.m_toi.m_properties.m_internalSolverData = 0.0f;
        output.m_toi.m_time = HK_REAL_MAX;
        output.m_potentialContacts = nullptr;
        hkpCollisionDispatcher* dispatcher = input.m_dispatcher.val();
        input.m_collisionQualityInfo = dispatcher->m_collisionQualityInfo + 1;
        const_cast<hkpProcessCollisionInput&>(input).m_createPredictiveAgents = dispatcher->m_collisionQualityInfo[(signed char)entry->m_collisionQualityIndex].m_useContinuousPhysics.val();
        hkAgentNnMachine_ProcessAgent(entry, input, output, entry->m_contactMgr);
        if (hkOutOfMemoryState == hkMemoryAllocator::MEMORY_STATE_OUT_OF_MEMORY)
        {
            break;
        }
        if (!output.isEmpty().m_bool)
        {
            entry->m_contactMgr->processContactImpl(*entry->m_collidable[0], *entry->m_collidable[1], input, output);
        }
    }
    hkContinuousSimulation_unlock(m_world);
}

// 0x00DFC920: the contact manager confirms the TOI; bodies of the simple TOI quality are frozen at the TOI time, lose
// their queued TOIs and are collided again for TOIs only
void hkpContinuousSimulation::handleSimpleToi(hkpWorld* world, hkpToiEvent& event, float physicsDeltaTime, float rotateNormal)
{
    m_world->m_criticalOperationsLockCount++;
    hkInplaceArray<hkpEntity*, 2, hkContainerHeapAllocator> toBeActivated;
    event.m_contactMgr->confirmToi(event, rotateNormal, toBeActivated);
    hkInplaceArray<hkpEntity*, 2, hkContainerHeapAllocator> entitiesToRecollide;
    for (int i = 0; i < 2; i++)
    {
        hkpEntity* entity = event.m_entities[i];
        // HK_COLLIDABLE_QUALITY_DEBRIS_SIMPLE_TOI
        if (entity->m_collidable.m_broadPhaseHandle.m_objectQualityType != 3)
        {
            continue;
        }
        hkSweptTransformUtil::freezeMotionState(event.m_time, entity->m_motion.m_motionState);
        hkpWorldAgentUtil::invalidateTim(entity, *world->m_collisionInput);
        for (int j = m_toiEvents.getSize() - 1; j >= 0; j--)
        {
            if (m_toiEvents[j].m_entities[0] == entity || m_toiEvents[j].m_entities[1] == entity)
            {
                hkContinuousSimulation_removeEventAndToi(m_toiEvents, j);
            }
        }
        entitiesToRecollide.begin()[entitiesToRecollide.m_size++] = entity;
    }
    event.m_contactMgr->removeToiImpl(*hkContinuousSimulation_eventIsland(event), event.m_properties);
    if (entitiesToRecollide.getSize() != 0)
    {
        collideEntitiesOfOneIslandNarrowPhaseContinuous_toiOnly(entitiesToRecollide.begin(), entitiesToRecollide.getSize(), *world->m_collisionInput, m_entitiesNeedingPsiCollisionDetection);
    }
    hkContinuousSimulation_unlock(m_world);
}

// 0x00DFD240: the earliest TOI before minTime (ties by the entities' uids) first; the contact manager may reject it.
// Each TOI moves the current time to it and runs locked.
hkpStepResult hkpContinuousSimulation::handleAllToisTill(float minTime)
{
    for (;;)
    {
        hkpWorld* world = m_world;
        world->m_criticalOperationsLockCount++;
        for (;;)
        {
            float rotateNormal = m_world->m_toiCollisionResponseRotateNormal;
            int best = -1;
            float bestTime = minTime;
            for (int i = 0; i < m_toiEvents.getSize(); i++)
            {
                const hkpToiEvent& event = m_toiEvents[i];
                if (bestTime <= event.m_time)
                {
                    if (event.m_time == bestTime && best != -1)
                    {
                        const hkpToiEvent& b = m_toiEvents[best];
                        const unsigned int uid0 = event.m_entities[0]->m_uid;
                        const unsigned int bestUid0 = b.m_entities[0]->m_uid;
                        if (uid0 < bestUid0 || (uid0 == bestUid0 && event.m_entities[1]->m_uid < b.m_entities[1]->m_uid))
                        {
                            best = i;
                            bestTime = event.m_time;
                        }
                    }
                }
                else
                {
                    best = i;
                    bestTime = event.m_time;
                }
            }
            if (best < 0)
            {
                hkContinuousSimulation_unlock(world);
                return HK_STEP_RESULT_SUCCESS;
            }

            hkpToiEvent& candidate = m_toiEvents[best];
            candidate.m_properties.m_flags |= 1; // hkContactPointMaterial::CONTACT_IS_NEW: the TOI is processed
            if (!candidate.m_contactMgr->fireCallbacksForEarliestToi(candidate, rotateNormal).m_bool)
            {
                hkContinuousSimulation_removeEvent(m_toiEvents, best);
                continue;
            }

            hkpToiEvent event;
            memcpy(&event, &candidate, sizeof(hkpToiEvent));
            hkContinuousSimulation_removeEvent(m_toiEvents, best);
            const hkStepInfo stepInfo = hkContinuousSimulation_stepInfo(event.m_time, m_currentPsiTime);
            m_currentTime = event.m_time;
            m_world->m_dynamicsStepInfo.m_stepInfo = stepInfo;
            m_world->m_collisionInput->m_stepInfo = stepInfo;
            if (!event.m_useSimpleHandling.m_bool)
            {
                simulateToi(m_world, event, m_physicsDeltaTime, rotateNormal);
            }
            else
            {
                handleSimpleToi(m_world, event, m_physicsDeltaTime, rotateNormal);
            }
            m_toiCounter++;
            if (hkOutOfMemoryState == hkMemoryAllocator::MEMORY_STATE_OUT_OF_MEMORY)
            {
                hkContinuousSimulation_unlock(world);
                return HK_STEP_RESULT_MEMORY_FAILURE_DURING_TOI_SOLVE;
            }
            break;
        }
        hkContinuousSimulation_unlock(world);
    }
}

// 0x00DFD590
hkpStepResult hkpContinuousSimulation::advanceTimeInternal()
{
    if (m_previousStepResult != HK_STEP_RESULT_SUCCESS)
    {
        const hkpStepResult result = reCollideAfterStepFailure();
        if (result != HK_STEP_RESULT_SUCCESS)
        {
            return result;
        }
    }
    const float minTime = snapSimulateTimeAndGetTimeToAdvanceTo();
    if (m_toiEvents.getSize() != 0)
    {
        const hkpStepResult result = handleAllToisTill(minTime);
        if (result != HK_STEP_RESULT_SUCCESS)
        {
            m_previousStepResult = result;
            return result;
        }
    }
    m_currentTime = minTime;
    m_previousStepResult = HK_STEP_RESULT_SUCCESS;
    return HK_STEP_RESULT_SUCCESS;
}

// 0x00DFD790: like hkpSimulation's from the current time to the PSI time, with the collision input's step restored
void hkpContinuousSimulation::reintegrateAndRecollideEntities(hkpEntity** entities, int numEntities, hkpWorld* world, int reintegrateRecollideMode)
{
    world->m_criticalOperationsLockCount++;
    hkpProcessCollisionInput* input = world->m_collisionInput;
    const hkStepInfo savedStepInfo = input->m_stepInfo;
    const hkStepInfo stepInfo = hkContinuousSimulation_stepInfo(m_currentTime, m_currentPsiTime);
    input->m_stepInfo = stepInfo;
    if (reintegrateRecollideMode & 1)
    {
        for (int i = 0; i < numEntities; i++)
        {
            if (entities[i]->m_motion.m_type.m_storage != hkpMotion::MOTION_FIXED)
            {
                hkSweptTransformUtil::backStepMotionState(m_currentTime, entities[i]->m_motion.m_motionState);
            }
        }
        hkRigidMotionUtilStep(stepInfo, (hkpMotion* const*)entities, numEntities, 0xE0); // the offset of hkpEntity::m_motion
        hkpEntityAabbUtil::entityBatchRecalcAabb(world->m_collisionInput, entities, numEntities);
    }
    resetCollisionInformationForEntities(entities, numEntities, world, RESET_TOI);
    if (reintegrateRecollideMode & 2)
    {
        collideEntitiesBroadPhaseContinuous(entities, numEntities, world, nullptr);
    }
    if (reintegrateRecollideMode & 4)
    {
        collideEntitiesNarrowPhaseContinuous(entities, numEntities, *world->m_collisionInput);
    }
    else if (reintegrateRecollideMode & 1)
    {
        resetCollisionInformationForEntities(entities, numEntities, world, RESET_TIM);
    }
    hkContinuousSimulation_unlock(world);
    world->m_collisionInput->m_stepInfo = savedStepInfo;
}

// 0x00DFD940: after a failed step the queued TOIs are dropped; then the broadphase and the continuous narrowphase of
// every active island
void hkpContinuousSimulation::collideInternal(const hkStepInfo& stepInfoIn)
{
    if (m_previousStepResult != HK_STEP_RESULT_SUCCESS)
    {
        for (int i = m_toiEvents.getSize() - 1; i >= 0; i--)
        {
            hkpToiEvent& event = m_toiEvents[i];
            event.m_contactMgr->removeToiImpl(*hkContinuousSimulation_eventIsland(event), event.m_properties);
        }
        m_toiEvents.m_size = 0;
    }
    m_world->m_dynamicsStepInfo.m_stepInfo = stepInfoIn;
    m_world->m_collisionInput->m_stepInfo = stepInfoIn;
    {
        hkpSolverInfo& si = m_world->m_dynamicsStepInfo.m_solverInfo;
        si.m_deltaTime = si.m_invNumSteps * stepInfoIn.m_deltaTime.val();
        si.m_invDeltaTime = (float)si.m_numSteps * stepInfoIn.m_invDeltaTime.val();
    }

    m_world->m_criticalOperationsLockCount++;
    hkpWorld* world = m_world;
    for (int i = 0; i < world->m_activeSimulationIslands.getSize(); i++)
    {
        hkpSimulationIsland* island = world->m_activeSimulationIslands[i];
        collideEntitiesBroadPhaseContinuous(island->m_entities.begin(), island->m_entities.getSize(), m_world, nullptr);
        if (hkOutOfMemoryState == hkMemoryAllocator::MEMORY_STATE_OUT_OF_MEMORY)
        {
            hkContinuousSimulation_unlock(m_world);
            return;
        }
    }
    hkContinuousSimulation_unlock(m_world);

    m_world->m_criticalOperationsLockCount++;
    world = m_world;
    for (int i = 0; i < world->m_activeSimulationIslands.getSize(); i++)
    {
        hkpSimulationIsland* island = world->m_activeSimulationIslands[i];
        collideIslandNarrowPhaseContinuous(island, *m_world->m_collisionInput);
        if (hkOutOfMemoryState == hkMemoryAllocator::MEMORY_STATE_OUT_OF_MEMORY)
        {
            hkContinuousSimulation_unlock(m_world);
            return;
        }
        if (m_world->m_islandPostCollideListeners.getSize() != 0)
        {
            hkpWorldCallbackUtil::fireIslandPostCollideCallback(m_world, island, stepInfoIn);
        }
    }
    hkContinuousSimulation_unlock(m_world);
}

// 0x00DFEB60: the PSI pass of the entities left by simple TOIs, then the post simulation callbacks once the frame is
// reached
hkpStepResult hkpContinuousSimulation::advanceTime()
{
    const hkpStepResult result = advanceTimeInternal();
    if (m_entitiesNeedingPsiCollisionDetection.getSize() != 0)
    {
        collideEntitiesNeedingPsiCollisionDetectionNarrowPhase_toiOnly(*m_world->m_collisionInput, m_entitiesNeedingPsiCollisionDetection);
    }
    if (hkOutOfMemoryState == hkMemoryAllocator::MEMORY_STATE_OUT_OF_MEMORY)
    {
        return HK_STEP_RESULT_MEMORY_FAILURE_DURING_TOI_SOLVE;
    }
    if (m_simulateUntilTime <= m_currentTime)
    {
        m_world->checkConstraintsViolated();
        if (m_world->m_worldPostSimulationListeners.getSize() != 0)
        {
            hkpWorldCallbackUtil::firePostSimulationCallback(m_world);
        }
    }
    return result;
}

// 0x00DFB210: the entries processed once with the given quality override (no out-of-memory check here); DISABLE_TOIS
// uses quality 1 (PSI), DISABLE_TOIS_AND_EXPAND_MANIFOLD quality 7 (6 for agents of quality 6); the predictive flag
// stays the agent's own
void hkpContinuousSimulation::processAgentNnEntries(hkpAgentNnEntry* const * entries, int numEntries, const hkpProcessCollisionInput& collisionInput, hkpSimulationIsland* island, CollisionQualityOverride collisionQualityOverride)
{
    hkpProcessCollisionOutput output(island);
    for (int i = 0; i < numEntries; i++)
    {
        hkpAgentNnEntry* entry = entries[i];
        output.m_toi.m_properties.m_impulseApplied = 0.0f;
        output.m_toi.m_properties.m_internalSolverData = 0.0f;
        output.m_firstFreeContactPoint = &output.m_contactPoints[0];
        output.m_constraintOwner = island;
        output.m_toi.m_time = HK_REAL_MAX;
        output.m_potentialContacts = nullptr;
        hkpCollisionDispatcher* dispatcher = collisionInput.m_dispatcher.val();
        if (collisionQualityOverride == PROCESS_NORMALLY)
        {
            collisionInput.m_collisionQualityInfo = dispatcher->m_collisionQualityInfo + (signed char)entry->m_collisionQualityIndex;
        }
        else if (collisionQualityOverride == DISABLE_TOIS)
        {
            collisionInput.m_collisionQualityInfo = dispatcher->m_collisionQualityInfo + 1;
        }
        else if (collisionQualityOverride == DISABLE_TOIS_AND_EXPAND_MANIFOLD)
        {
            collisionInput.m_collisionQualityInfo = dispatcher->m_collisionQualityInfo + 7;
            if (entry->m_collisionQualityIndex == 6)
            {
                collisionInput.m_collisionQualityInfo = dispatcher->m_collisionQualityInfo + 6;
            }
        }
        const_cast<hkpProcessCollisionInput&>(collisionInput).m_createPredictiveAgents = dispatcher->m_collisionQualityInfo[(signed char)entry->m_collisionQualityIndex].m_useContinuousPhysics.val();
        hkAgentNnMachine_ProcessAgent(entry, collisionInput, output, entry->m_contactMgr);
        if (!output.isEmpty().m_bool)
        {
            entry->m_contactMgr->processContactImpl(*entry->m_collidable[0], *entry->m_collidable[1], collisionInput, output);
        }
        if (output.m_toi.m_time.val() != HK_REAL_MAX)
        {
            addToiEvent(output, *entry);
        }
    }
}

// ---- the localized TOI solver (hkLs_): solves the constraints around a TOI, activating the entities it reaches ----
//
// Entity states (hkFixedArray<unsigned char> by storage index in the island):
namespace
{
enum
{
    HK_LS_ENTITY_UNTOUCHED = 0,
    HK_LS_ENTITY_TRANSFORM_AT_TOI = 1, // its transform moved to the TOI time
    HK_LS_ENTITY_TOUCHED = 2, // in the touched list (accumulators and constraints in the solver)
    HK_LS_ENTITY_TO_BE_ACTIVATED = 4, // flag: a violated constraint activates it
    HK_LS_ENTITY_ACTIVATING = 8, // its agents are processed (or it cannot be activated: no resources)
    HK_LS_ENTITY_ACTIVE = 16, // integrated from the TOI time with the solver's velocities
};

// the transform at the end of the swept transform (hkSweptTransformUtil::calcTransAtT1, inlined)
inline void hkLs_calcTransAtT1(hkMotionState& ms)
{
    hkSweptTransformf& st = ms.getSweptTransform();
    ms.m_transform.m_rotation.set(st.m_rotation1);
    const hkRotationf& r = ms.m_transform.m_rotation;
    const __m128 local = st.m_centerOfMassLocal.m_quad;
    const __m128 rotated = _mm_add_ps(_mm_add_ps(_mm_mul_ps(_mm_shuffle_ps(local, local, 0x55), r.m_col1.m_quad), _mm_mul_ps(_mm_shuffle_ps(local, local, 0x00), r.m_col0.m_quad)), _mm_mul_ps(_mm_shuffle_ps(local, local, 0xAA), r.m_col2.m_quad));
    ms.m_transform.m_translation.m_quad = _mm_sub_ps(st.m_centerOfMass1.m_quad, rotated);
}

// v rotated by the columns c0, c1, c2: (y * c1 + x * c0) + z * c2
inline __m128 hkLs_rotate(const __m128& c0, const __m128& c1, const __m128& c2, const __m128& v)
{
    return _mm_add_ps(_mm_add_ps(_mm_mul_ps(_mm_shuffle_ps(v, v, 0x55), c1), _mm_mul_ps(_mm_shuffle_ps(v, v, 0x00), c0)), _mm_mul_ps(_mm_shuffle_ps(v, v, 0xAA), c2));
}

inline void hkLs_swapStatus(hkArray<hkpConstraintSchemaInfo, hkContainerHeapAllocator>& constraintStatus, int a, int b)
{
    const hkpConstraintSchemaInfo t = constraintStatus[b];
    constraintStatus[b] = constraintStatus[a];
    constraintStatus[a] = t;
}

// a touched or activated entity: its transform at the TOI time (once), then the new state
inline void hkLs_moveToToi(hkpEntity* entity, float time, hkFixedArray<unsigned char>& entityState)
{
    unsigned char& state = entityState[entity->m_storageIndex];
    if (state == HK_LS_ENTITY_UNTOUCHED)
    {
        state = HK_LS_ENTITY_TRANSFORM_AT_TOI;
        hkSweptTransformUtil::lerp2(entity->m_motion.m_motionState.getSweptTransform(), time, entity->m_motion.m_motionState.m_transform);
    }
}

// the constraint to an entity reached from an activated one: the constraint is solved (a fixed partner, or a partner
// not active yet, which joins the touched entities)
inline void hkLs_touchConstraint(hkpConstraintInstance* constraint, hkpEntity* partner, float time, hkFixedArray<unsigned char>& entityState, hkArray<hkpEntity*, hkContainerHeapAllocator>& newTouchedEntities, hkArray<hkpConstraintInstance*, hkContainerHeapAllocator>& newTouchedConstraints)
{
    if (partner->m_motion.m_type.m_storage == hkpMotion::MOTION_FIXED)
    {
        newTouchedConstraints.pushBack(constraint);
        return;
    }
    if (entityState[partner->m_storageIndex] == HK_LS_ENTITY_ACTIVE)
    {
        return;
    }
    newTouchedConstraints.pushBack(constraint);
    if (entityState[partner->m_storageIndex] < HK_LS_ENTITY_TOUCHED)
    {
        newTouchedEntities.pushBack(partner);
        hkLs_moveToToi(partner, time, entityState);
        entityState[partner->m_storageIndex] = HK_LS_ENTITY_TOUCHED;
    }
}
}

// 0x00DFA400: the touched entities' accumulators get their motions' velocities back (the angular one into the core
// space through the backed up rotation)
void hkLs_toiResetVelocityAccumulatorsForEntities(hkpConstraintSolverResources& solverResources, const hkArray<hkpEntity*, hkContainerHeapAllocator>& entities)
{
    for (int i = 0; i < entities.getSize(); i++)
    {
        hkpEntity* entity = entities[i];
        hkpVelocityAccumulator* acc = reinterpret_cast<hkpVelocityAccumulator*>(reinterpret_cast<char*>(solverResources.m_accumulators) + entity->m_solverData);
        acc->m_linearVel = entity->m_motion.m_linearVelocity;
        const hkRotationf& r = solverResources.m_accumulatorsBackup[entity->m_solverData >> 7].m_coreTworldRotation;
        acc->m_angularVel.m_quad = hkLs_rotate(r.m_col0.m_quad, r.m_col1.m_quad, r.m_col2.m_quad, entity->m_motion.m_angularVelocity.m_quad);
    }
}

// 0x00DFA810: the row with the largest violation over the step (|velocity| * time left, compared at double precision
// on the x87: the product of two floats is exact there) beyond the allowed violation of the constraint's quality
// (indexed by the constraint's priority) times its allowed penetration depth; -1 when none
int hkLs_areVelocitiesOk(float psiTimeLeft, const hkpConstraintSchemaInfo& constraintStatus, const float* velocities, int numVelocities, const hkpProcessCollisionInput& processInput)
{
    float maxViolation = processInput.m_dispatcher.val()->m_collisionQualityInfo[(signed char)constraintStatus.m_constraint->m_priority.m_storage].m_maxContraintViolation * constraintStatus.m_allowedPenetrationDepth;
    int worst = -1;
    for (int i = 0; i < numVelocities; i++)
    {
        const double violation = (double)fabsf(velocities[i]) * (double)psiTimeLeft;
        if (violation > (double)maxViolation)
        {
            maxViolation = (float)violation;
            worst = i;
        }
    }
    return worst;
}

// 0x00DFA8A0: of the last added constraints, those to fixed or keyframed entities become active
void hkLs_toiActivateConstraintsLinkingToFixedAndKeyframedEntities(hkArray<hkpConstraintSchemaInfo, hkContainerHeapAllocator>& constraintStatus, int& firstNonActiveConstraintStatus, int numOfConstraintsToCheckAtTheEndOfTheList)
{
    for (int i = constraintStatus.getSize() - numOfConstraintsToCheckAtTheEndOfTheList; i < constraintStatus.getSize(); i++)
    {
        const hkConstraintInternal* internal = constraintStatus[i].m_constraint->m_internal;
        const unsigned char typeA = internal->m_entities[0]->m_motion.m_type.m_storage;
        const unsigned char typeB = internal->m_entities[1]->m_motion.m_type.m_storage;
        if (typeA == hkpMotion::MOTION_FIXED || typeA == hkpMotion::MOTION_KEYFRAMED || typeB == hkpMotion::MOTION_FIXED || typeB == hkpMotion::MOTION_KEYFRAMED)
        {
            hkLs_swapStatus(constraintStatus, firstNonActiveConstraintStatus++, i);
        }
    }
}

// 0x00DFA930: inactive constraints between two active entities become active
void hkLs_toiActivateConstraintsLinkingActivatedEntities(hkArray<hkpConstraintSchemaInfo, hkContainerHeapAllocator>& constraintStatus, int& firstNonActiveConstraintStatus, hkFixedArray<unsigned char>& entityState)
{
    for (int i = firstNonActiveConstraintStatus; i < constraintStatus.getSize(); i++)
    {
        const hkConstraintInternal* internal = constraintStatus[i].m_constraint->m_internal;
        if (entityState[internal->m_entities[0]->m_storageIndex] == HK_LS_ENTITY_ACTIVE && entityState[internal->m_entities[1]->m_storageIndex] == HK_LS_ENTITY_ACTIVE)
        {
            hkLs_swapStatus(constraintStatus, firstNonActiveConstraintStatus++, i);
        }
    }
}

// 0x00DFA9D0: entities only moved to the TOI time (1) get their end transforms back; entities that were in the
// solver without being activated (8) keep the solver's velocities (the angular one back to world space through the
// transposed backup rotation; its w lane mixes the matrix's padding, kept)
void hkLs_restoreTransformOnBodiesWithUpdatedTransform(hkpSimulationIsland* island, hkFixedArray<unsigned char>& entityState, hkpConstraintSolverResources& solverResources)
{
    for (int i = 0; i < island->m_entities.getSize(); i++)
    {
        const unsigned char state = entityState[i];
        if (state == HK_LS_ENTITY_TRANSFORM_AT_TOI || state == HK_LS_ENTITY_TOUCHED)
        {
            hkLs_calcTransAtT1(island->m_entities[i]->m_motion.m_motionState);
        }
        if (state == HK_LS_ENTITY_ACTIVATING || state == HK_LS_ENTITY_TOUCHED)
        {
            hkpEntity* entity = island->m_entities[i];
            const hkpVelocityAccumulator* acc = reinterpret_cast<const hkpVelocityAccumulator*>(reinterpret_cast<const char*>(solverResources.m_accumulators) + entity->m_solverData);
            const hkRotationf& r = solverResources.m_accumulatorsBackup[entity->m_solverData >> 7].m_coreTworldRotation;
            entity->m_motion.m_linearVelocity = acc->m_linearVel;
            // hkMatrix3f::_transpose: (c0.x c1.x c2.x c2.y), (c0.y c1.y c2.y c2.w), (c0.z c1.z c2.z c2.w)
            const __m128 c0 = r.m_col0.m_quad;
            const __m128 c1 = r.m_col1.m_quad;
            const __m128 c2 = r.m_col2.m_quad;
            const __m128 lo = _mm_unpacklo_ps(c0, c1);
            const __m128 hi = _mm_unpackhi_ps(c0, c1);
            const __m128 t0 = _mm_movelh_ps(lo, c2);
            const __m128 t1 = _mm_shuffle_ps(_mm_movehl_ps(t0, lo), c2, 0xD4);
            const __m128 t2 = _mm_shuffle_ps(hi, c2, 0xE4);
            entity->m_motion.m_angularVelocity.m_quad = hkLs_rotate(t0, t1, t2, acc->m_angularVel.m_quad);
        }
    }
}

// 0x00DFB3A0: the inactive constraints (of at least the given priority) are checked with the touched entities moving
// at their swept velocities (the accumulators' velocities are swapped out meanwhile). A constraint to an entity that
// is to be activated becomes active; one that is violated over the rest of the step activates its inactive entity.
void hkLs_toiCheckValidityOfConstraints(hkpConstraintSolverResources& solverResources, hkpProcessCollisionInput& processInput, hkArray<hkpConstraintSchemaInfo, hkContainerHeapAllocator>& constraintStatus, int& firstNonActiveConstraintStatus, hkpConstraintInstance::ConstraintPriority minPriorityToProcess, hkFixedArray<unsigned char>& entityState, const hkArray<hkpEntity*, hkContainerHeapAllocator>& touchedEntities, hkArray<hkpEntity*, hkContainerHeapAllocator>& toBeActivated)
{
    hkLocalBuffer<hkVector4f> saved(touchedEntities.getSize() * 2);
    for (int i = 0; i < touchedEntities.getSize(); i++)
    {
        hkpEntity* entity = touchedEntities[i];
        hkpVelocityAccumulator* acc = reinterpret_cast<hkpVelocityAccumulator*>(reinterpret_cast<char*>(solverResources.m_accumulators) + entity->m_solverData);
        saved[i * 2] = acc->m_linearVel;
        saved[i * 2 + 1] = acc->m_angularVel;
        const hkSweptTransformf& st = entity->m_motion.m_motionState.getSweptTransform();
        const __m128 invDeltaTime = _mm_shuffle_ps(st.m_centerOfMass1.m_quad, st.m_centerOfMass1.m_quad, 0xFF);
        acc->m_linearVel.m_quad = _mm_mul_ps(_mm_sub_ps(st.m_centerOfMass1.m_quad, st.m_centerOfMass0.m_quad), invDeltaTime);
        acc->m_angularVel.m_quad = _mm_mul_ps(entity->m_motion.m_motionState.m_deltaAngle.m_quad, invDeltaTime);
    }
    for (int i = firstNonActiveConstraintStatus; i < constraintStatus.getSize(); i++)
    {
        hkpConstraintSchemaInfo& status = constraintStatus[i];
        if ((int)minPriorityToProcess > (int)status.m_constraint->m_priority.m_storage)
        {
            continue;
        }
        // the entity that is not active
        const hkConstraintInternal* internal = status.m_constraint->m_internal;
        hkpEntity* entity = internal->m_entities[entityState[internal->m_entities[0]->m_storageIndex] == HK_LS_ENTITY_ACTIVE ? 1 : 0];
        if ((entityState[entity->m_storageIndex] & HK_LS_ENTITY_TO_BE_ACTIVATED) != 0)
        {
            hkLs_swapStatus(constraintStatus, firstNonActiveConstraintStatus++, i);
            continue;
        }
        float velocities[256];
        const int numVelocities = hkSolveGetToiViolatingConstraintVelocity(*solverResources.m_solverInfo, status.m_schema, solverResources.m_accumulators, 256, velocities);
        if (hkLs_areVelocitiesOk(solverResources.m_stepInfo->m_deltaTime, status, velocities, numVelocities, processInput) != -1)
        {
            entityState[entity->m_storageIndex] |= HK_LS_ENTITY_TO_BE_ACTIVATED;
            toBeActivated.pushBack(entity);
            hkLs_swapStatus(constraintStatus, firstNonActiveConstraintStatus++, i);
        }
    }
    for (int i = 0; i < touchedEntities.getSize(); i++)
    {
        hkpVelocityAccumulator* acc = reinterpret_cast<hkpVelocityAccumulator*>(reinterpret_cast<char*>(solverResources.m_accumulators) + touchedEntities[i]->m_solverData);
        acc->m_linearVel = saved[i * 2];
        acc->m_angularVel = saved[i * 2 + 1];
    }
}

// 0x00DFB6F0: every entity of the island is backstepped to the time and frozen there (start and end of the swept
// transform at the time, inverse delta time to the PSI time on the x87); those not active yet join the active ones
void hkLs_backstepAndFreezeEntireIsland(float time, hkpSimulationIsland* island, hkFixedArray<unsigned char>& entityState, hkArray<hkpEntity*, hkContainerHeapAllocator>& activeEntities)
{
    for (int i = 0; i < island->m_entities.getSize(); i++)
    {
        hkpEntity* entity = island->m_entities[i];
        hkSweptTransformUtil::backStepMotionState(time, entity->m_motion.m_motionState);
        if (entityState[i] != HK_LS_ENTITY_ACTIVE)
        {
            activeEntities.pushBack(entity);
            entityState[i] = HK_LS_ENTITY_ACTIVE;
        }
        hkSweptTransformf& st = entity->m_motion.m_motionState.getSweptTransform();
        st.m_centerOfMass0 = st.m_centerOfMass1;
        st.m_rotation0 = st.m_rotation1;
        st.m_centerOfMass0.m_quad.m128_f32[3] = time;
        st.m_centerOfMass1.m_quad.m128_f32[3] = (float)(1.0 / ((double)island->m_world->getCurrentPsiTime() - (double)time));
    }
}

// 0x00DFB7D0: whether the critical (PRIORITY_TOI_FORCED) constraints are still violated after the solver pass. With
// a list, every violated one is reported (the worst contact point and its properties for contacts).
hkBool hkLs_toiCheckFinalValidityOfCriticalConstraints(hkpConstraintSolverResources& solverResources, const hkArray<hkpConstraintSchemaInfo, hkContainerHeapAllocator>& constraintStatus, hkpProcessCollisionInput& processInput, hkArray<hkpToiResourceMgr::ConstraintViolationInfo, hkContainerHeapAllocator>* violatedConstraints)
{
    hkBool result;
    result.m_bool = 1;
    for (int i = 0; i < constraintStatus.getSize(); i++)
    {
        const hkpConstraintSchemaInfo& status = constraintStatus[i];
        if (status.m_constraint->m_priority.m_storage != hkpConstraintInstance::PRIORITY_TOI_FORCED)
        {
            continue;
        }
        float velocities[256];
        const int numVelocities = hkSolveGetToiViolatingConstraintVelocity(*solverResources.m_solverInfo, status.m_schema, solverResources.m_accumulators, 256, velocities);
        const int worst = hkLs_areVelocitiesOk(solverResources.m_stepInfo->m_deltaTime, status, velocities, numVelocities, processInput);
        if (worst < 0)
        {
            continue;
        }
        if (violatedConstraints == nullptr)
        {
            result.m_bool = 0;
            return result;
        }
        result.m_bool = 0;
        hkpToiResourceMgr::ConstraintViolationInfo& info = violatedConstraints->expandOne();
        info.m_constraint = status.m_constraint;
        if (status.m_constraint->m_data->getType() == hkpConstraintData::CONSTRAINT_TYPE_CONTACT)
        {
            const hkpSimpleContactConstraintAtom* atom = static_cast<hkpSimpleContactConstraintData*>(status.m_constraint->m_data)->m_atom;
            const char* points = reinterpret_cast<const char*>(atom) + 0x30; // the contact points follow the atom
            info.m_contactPoint = reinterpret_cast<const hkContactPoint*>(points + worst * 0x20);
            info.m_contactPointProperties = reinterpret_cast<const hkpContactPointProperties*>(points + atom->m_numReservedContactPoints * 0x20 + atom->m_contactPointPropertiesStriding * (short)worst);
        }
        else
        {
            info.m_contactPoint = nullptr;
            info.m_contactPointProperties = nullptr;
        }
    }
    return result;
}

// 0x00DFCBC0: the entity's agents (in sorted order) of at least the given quality: partners not fixed get their
// transforms at the time; partners already activating or active are skipped (unless fixed)
void hkLs_collectAgentEntriesToProcess(const hkpEntity* entity, hkpConstraintInstance::ConstraintPriority minPriorityToProcess, float time, hkFixedArray<unsigned char>& entityState, hkLocalArray<hkpAgentNnEntry*>& entriesOut)
{
    hkArray<hkpLinkedCollidable::CollisionEntry, hkContainerHeapAllocator> entries;
    entity->m_collidable.getCollisionEntriesSorted(entries);
    for (int i = 0; i < entries.getSize(); i++)
    {
        if ((int)minPriorityToProcess > (int)entries[i].m_agentEntry->m_collisionQualityIndex)
        {
            continue;
        }
        hkpEntity* partner = hkContinuousSimulation_entity(entries[i].m_partner);
        if (partner->m_motion.m_type.m_storage != hkpMotion::MOTION_FIXED)
        {
            if (entityState[partner->m_storageIndex] >= HK_LS_ENTITY_ACTIVATING)
            {
                continue;
            }
            hkLs_moveToToi(partner, time, entityState);
        }
        entriesOut.pushBack(entries[i].m_agentEntry);
    }
}

// 0x00DFCCF0: the entities to be activated: their agents run once at the time (no TOIs, expanded manifolds); then
// they are active and their constraints to entities not active yet (or fixed) are added, with those entities. Each
// entity's new constraints are sorted from its slave constraints on (an original quirk: the master ones added
// before the sort range stay unsorted), its new entities likewise.
void hkLs_toiActivateEntitiesAndCheckConstraints(hkpProcessCollisionInput& collisionInput, hkpConstraintInstance::ConstraintPriority minPriorityToProcess, float time, const hkArray<hkpEntity*, hkContainerHeapAllocator>& toBeActivated, hkFixedArray<unsigned char>& entityState, hkArray<hkpEntity*, hkContainerHeapAllocator>& newTouchedEntities, hkArray<hkpConstraintInstance*, hkContainerHeapAllocator>& newTouchedConstraints)
{
    {
        hkLocalArray<hkpAgentNnEntry*> entries(1000);
        for (int i = 0; i < toBeActivated.getSize(); i++)
        {
            hkpEntity* entity = toBeActivated[i];
            hkLs_moveToToi(entity, time, entityState);
            entityState[entity->m_storageIndex] = HK_LS_ENTITY_ACTIVATING;
            hkLs_collectAgentEntriesToProcess(entity, minPriorityToProcess, time, entityState, entries);
        }
        const hkStepInfo savedStepInfo = collisionInput.m_stepInfo;
        collisionInput.m_stepInfo.m_endTime = time;
        const float deltaTime = time - collisionInput.m_stepInfo.m_startTime.val();
        collisionInput.m_stepInfo.m_deltaTime = deltaTime;
        float invDeltaTime = 0.0f;
        if (deltaTime > 0.0f)
        {
            invDeltaTime = 1.0f / deltaTime;
        }
        collisionInput.m_stepInfo.m_invDeltaTime = invDeltaTime;
        hkpEntity* first = toBeActivated[0];
        static_cast<hkpContinuousSimulation*>(first->m_world->m_simulation)->processAgentNnEntries(entries.begin(), entries.getSize(), collisionInput, first->m_simulationIsland, hkpContinuousSimulation::DISABLE_TOIS_AND_EXPAND_MANIFOLD);
        collisionInput.m_stepInfo = savedStepInfo;
    }

    for (int i = 0; i < toBeActivated.getSize(); i++)
    {
        hkpEntity* entity = toBeActivated[i];
        entityState[entity->m_storageIndex] = HK_LS_ENTITY_ACTIVE;
        for (int c = 0; c < entity->m_constraintsMaster.getSize(); c++)
        {
            const hkConstraintInternal& internal = entity->m_constraintsMaster[c];
            if ((int)minPriorityToProcess > (int)internal.m_constraint->m_priority.m_storage)
            {
                continue;
            }
            hkpEntity* partner = (hkpEntity*)((hkUlong)internal.m_entities[1] ^ (hkUlong)internal.m_entities[0] ^ (hkUlong)entity);
            hkLs_touchConstraint(internal.m_constraint, partner, time, entityState, newTouchedEntities, newTouchedConstraints);
        }
        const int firstNewConstraint = newTouchedConstraints.getSize();
        const int firstNewEntity = newTouchedEntities.getSize();
        for (int c = 0; c < entity->m_constraintsSlave.getSize(); c++)
        {
            const hkConstraintInternal* internal = entity->m_constraintsSlave[c]->m_internal;
            if ((int)minPriorityToProcess > (int)internal->m_constraint->m_priority.m_storage)
            {
                continue;
            }
            hkpEntity* partner = (hkpEntity*)((hkUlong)internal->m_entities[1] ^ (hkUlong)internal->m_entities[0] ^ (hkUlong)entity);
            hkLs_touchConstraint(internal->m_constraint, partner, time, entityState, newTouchedEntities, newTouchedConstraints);
        }
        const int numNewConstraints = newTouchedConstraints.getSize() - firstNewConstraint;
        if (numNewConstraints > 1)
        {
            hkAlgorithm::quickSortRecursive(newTouchedConstraints.begin() + firstNewConstraint, 0, numNewConstraints - 1, less_ConstraintPtr);
        }
        const int numNewEntities = newTouchedEntities.getSize() - firstNewEntity;
        if (numNewEntities > 1)
        {
            hkAlgorithm::quickSortRecursive(newTouchedEntities.begin() + firstNewEntity, 0, numNewEntities - 1, less_EntityPtr);
        }
    }
}

// 0x00DFDCD0: the TOI's entities move to the TOI time and the contact manager responds (confirmToi). Without entities
// to activate (a simple response) they go back to their end transforms and the result is HK_FAILURE. Otherwise up to
// m_numToiSolverIterations rounds: entities to activate join (their agents, constraints and constraint partners), the
// new constraints are solved incrementally, and while nothing new joins, inactive constraints that are violated
// activate more entities; each round ends with a full solver pass. Critical constraints still violated get up to
// m_numForcedToiFinalSolverIterations passes, then the resource manager decides (it may backstep and freeze the
// whole island). The active entities are then integrated from the TOI time with the solver's velocities.
hkResult hkLs_localizedSolveToi(const hkpToiResources& toiResources, hkpConstraintSolverResources& solverResources, hkpToiEvent& event, hkpToiResourceMgr& toiResourceMgr, hkpWorld* world, hkArray<hkpEntity*, hkContainerHeapAllocator>& activeEntities, hkFixedArray<unsigned char>& entityState, float rotateNormal)
{
    hkInplaceArray<hkpConstraintSchemaInfo, 64, hkContainerHeapAllocator> constraintStatus;
    int firstNonActiveConstraintStatus = 0;
    hkpSimulationIsland* island = hkContinuousSimulation_eventIsland(event);
    hkLocalArray<hkpEntity*> touchedEntities(island->m_entities.getSize());
    for (int i = 0; i < 2; i++)
    {
        hkpEntity* entity = event.m_entities[i];
        if (entity->m_motion.m_type.m_storage == hkpMotion::MOTION_FIXED)
        {
            continue;
        }
        touchedEntities.begin()[touchedEntities.m_size++] = entity;
        hkSweptTransformUtil::lerp2(entity->m_motion.m_motionState.getSweptTransform(), event.m_time, entity->m_motion.m_motionState.m_transform);
        entityState[entity->m_storageIndex] = HK_LS_ENTITY_TOUCHED;
    }

    hkInplaceArray<hkpEntity*, 64, hkContainerHeapAllocator> toBeActivated;
    event.m_contactMgr->confirmToi(event, rotateNormal, toBeActivated);
    if (toBeActivated.getSize() == 0)
    {
        for (int i = 0; i < 2; i++)
        {
            hkpEntity* entity = event.m_entities[i];
            if (entity->m_motion.m_type.m_storage != hkpMotion::MOTION_FIXED)
            {
                hkLs_calcTransAtT1(entity->m_motion.m_motionState);
            }
        }
        return HK_FAILURE;
    }
    hkpEntity* toiEntities[2] = { toBeActivated[0], toBeActivated.getSize() > 1 ? toBeActivated[1] : nullptr };
    hkpConstraintSolverSetup::internalAddAccumulators(solverResources, touchedEntities.begin(), touchedEntities.getSize());

    hkpProcessCollisionInput& processInput = *world->m_collisionInput;
    hkInplaceArray<hkpEntity*, 64, hkContainerHeapAllocator> newTouchedEntities;
    hkInplaceArray<hkpConstraintInstance*, 64, hkContainerHeapAllocator> newTouchedConstraints;
    bool firstRound = true;
    for (int iterations = toiResources.m_numToiSolverIterations; iterations > 0; )
    {
        newTouchedConstraints.m_size = 0;
        newTouchedEntities.m_size = 0;
        int firstNonActive = firstNonActiveConstraintStatus;
        bool solveAll = false;
        if (!firstRound)
        {
            if (firstNonActive >= constraintStatus.getSize())
            {
                solveAll = true;
            }
            else
            {
                toBeActivated.m_size = 0;
                hkLs_toiCheckValidityOfConstraints(solverResources, processInput, constraintStatus, firstNonActive, toiResources.m_minPriorityToProcess, entityState, touchedEntities, toBeActivated);
                solveAll = toBeActivated.getSize() == 0;
            }
        }
        if (!solveAll)
        {
            hkLs_toiActivateEntitiesAndCheckConstraints(processInput, toiResources.m_minPriorityToProcess, event.m_time, toBeActivated, entityState, newTouchedEntities, newTouchedConstraints);
            firstRound = false;
            const bool fits = hkpConstraintSolverSetup::internalIsMemoryOkForNewAccumulators(solverResources, newTouchedEntities.begin(), newTouchedEntities.getSize()).m_bool
                && hkpConstraintSolverSetup::internalIsMemoryOkForNewJacobianSchemas(solverResources, newTouchedConstraints.begin(), newTouchedConstraints.getSize()).m_bool
                && newTouchedConstraints.getSize() + constraintStatus.getSize() <= toiResources.m_maxNumConstraints
                && activeEntities.getSize() + newTouchedEntities.getSize() + touchedEntities.getSize() <= toiResources.m_maxNumEntities
                && activeEntities.getSize() + toBeActivated.getSize() <= toiResources.m_maxNumActiveEntities;
            if (fits)
            {
                hkpConstraintSolverSetup::internalAddAccumulators(solverResources, newTouchedEntities.begin(), newTouchedEntities.getSize());
                hkpConstraintSolverSetup::internalAddJacobianSchemas(solverResources, newTouchedConstraints.begin(), newTouchedConstraints.getSize(), constraintStatus);
                firstNonActiveConstraintStatus = firstNonActive;
                // the activated entities leave the touched ones
                for (int i = 0; i < touchedEntities.getSize(); i++)
                {
                    if (entityState[touchedEntities[i]->m_storageIndex] == HK_LS_ENTITY_ACTIVE)
                    {
                        touchedEntities.removeAt(i);
                        i--;
                    }
                }
                activeEntities.insertAt(activeEntities.getSize(), toBeActivated.begin(), toBeActivated.getSize());
                const int needed = newTouchedEntities.getSize() + touchedEntities.getSize();
                const int capacity = touchedEntities.getCapacity();
                if (capacity < needed)
                {
                    hkArrayUtil::_reserve(hkContainerHeapAllocator::s_alloc, &touchedEntities, capacity * 2 > needed ? capacity * 2 : needed, sizeof(hkpEntity*));
                }
                for (int i = 0; i < newTouchedEntities.getSize(); i++)
                {
                    if (newTouchedEntities[i]->m_motion.m_type.m_storage != hkpMotion::MOTION_KEYFRAMED)
                    {
                        touchedEntities.begin()[touchedEntities.m_size++] = newTouchedEntities[i];
                    }
                }
                hkLs_toiActivateConstraintsLinkingToFixedAndKeyframedEntities(constraintStatus, firstNonActiveConstraintStatus, newTouchedConstraints.getSize());
                hkLs_toiActivateConstraintsLinkingActivatedEntities(constraintStatus, firstNonActiveConstraintStatus, entityState);
                hkpConstraintSolverSetup::subSolve(solverResources, hkpConstraintSolverSetup::SOLVER_MODE_INCREMENTAL_CONTINUE);
                if (newTouchedConstraints.getSize() != 0 || newTouchedEntities.getSize() != 0)
                {
                    continue; // another round without counting this one
                }
            }
            else
            {
                // out of resources: the entities stay as they are
                for (int i = 0; i < toBeActivated.getSize(); i++)
                {
                    entityState[toBeActivated[i]->m_storageIndex] = HK_LS_ENTITY_ACTIVATING;
                }
                for (int i = 0; i < newTouchedEntities.getSize(); i++)
                {
                    entityState[newTouchedEntities[i]->m_storageIndex] = HK_LS_ENTITY_TRANSFORM_AT_TOI;
                }
                if (toiResourceMgr.resourcesDepleted() == HK_TOI_RESOURCE_MGR_RESPONSE_BACKSTEP)
                {
                    hkLs_backstepAndFreezeEntireIsland(event.m_time, island, entityState, activeEntities);
                    for (int i = 0; i < activeEntities.getSize(); i++)
                    {
                        hkpWorldAgentUtil::invalidateTim(activeEntities[i], processInput);
                    }
                    activeEntities.m_size = 0;
                    return HK_SUCCESS;
                }
            }
        }
        hkpConstraintSolverSetup::subSolve(solverResources, hkpConstraintSolverSetup::SOLVER_MODE_PROCESS_ALL);
        iterations--;
    }

    if (solverResources.m_schemas[2].m_begin != solverResources.m_schemas[2].m_current)
    {
        // critical constraints: forced passes over their schemas until they hold
        hkLs_toiResetVelocityAccumulatorsForEntities(solverResources, touchedEntities);
        bool valid = hkLs_toiCheckFinalValidityOfCriticalConstraints(solverResources, constraintStatus, processInput, nullptr).m_bool != 0;
        for (int forced = toiResources.m_numForcedToiFinalSolverIterations; !valid && --forced >= 0; )
        {
            hkSolveStepJacobians(*solverResources.m_solverInfo, solverResources.m_schemas[2].m_begin, solverResources.m_accumulators, solverResources.m_elemTemp);
            hkLs_toiResetVelocityAccumulatorsForEntities(solverResources, touchedEntities);
            valid = hkLs_toiCheckFinalValidityOfCriticalConstraints(solverResources, constraintStatus, processInput, nullptr).m_bool != 0;
        }
        if (!valid)
        {
            hkInplaceArray<hkpToiResourceMgr::ConstraintViolationInfo, 32, hkContainerHeapAllocator> violatedConstraints;
            hkLs_toiCheckFinalValidityOfCriticalConstraints(solverResources, constraintStatus, processInput, &violatedConstraints);
            if (toiResourceMgr.cannotSolve(violatedConstraints) == HK_TOI_RESOURCE_MGR_RESPONSE_BACKSTEP)
            {
                hkLs_backstepAndFreezeEntireIsland(event.m_time, island, entityState, activeEntities);
                for (int i = 0; i < activeEntities.getSize(); i++)
                {
                    hkpWorldAgentUtil::invalidateTim(activeEntities[i], processInput);
                }
                activeEntities.m_size = 0;
                return HK_SUCCESS;
            }
        }
    }

    if (activeEntities.getSize() == 0)
    {
        activeEntities.insertAt(0, toiEntities[0]);
        entityState[activeEntities[0]->m_storageIndex] = HK_LS_ENTITY_ACTIVE;
        if (toiEntities[1] != nullptr)
        {
            activeEntities.insertAt(1, toiEntities[1]);
            entityState[activeEntities[1]->m_storageIndex] = HK_LS_ENTITY_ACTIVE;
        }
    }
    for (int i = 0; i < activeEntities.getSize(); i++)
    {
        hkSweptTransformUtil::backStepMotionState(event.m_time, activeEntities[i]->m_motion.m_motionState);
    }
    hkpConstraintSolverSetup::oneStepIntegrate(*solverResources.m_solverInfo, *solverResources.m_stepInfo, solverResources.m_accumulators, activeEntities.begin(), activeEntities.getSize());
    hkLs_restoreTransformOnBodiesWithUpdatedTransform(island, entityState, solverResources);
    for (int i = 0; i < activeEntities.getSize(); i++)
    {
        hkpWorldAgentUtil::invalidateTim(activeEntities[i], processInput);
    }
    return HK_SUCCESS;
}

// 0x00DFEC20: a TOI that is not simple. The resource manager gives the limits and the scratchpad; the solver runs one
// step and micro step over the time from the TOI to the PSI time (tau 0.5, damping 1.4, no gravity). After the
// localized solve the TOI leaves its contact manager; on success the queued TOIs of the island's active entities are
// dropped and those entities (still in the world) are collided again for TOIs only.
void hkpContinuousSimulation::simulateToi(hkpWorld* world, hkpToiEvent& event, float physicsDeltaTime, float rotateNormal)
{
    hkpToiResources toiResources;
    toiResources.m_minPriorityToProcess = hkpConstraintInstance::PRIORITY_TOI;
    toiResources.m_maxNumEntities = 1000;
    toiResources.m_maxNumActiveEntities = 1000;
    toiResources.m_maxNumConstraints = 1000;
    toiResources.m_numToiSolverIterations = 4;
    toiResources.m_numForcedToiFinalSolverIterations = 4;
    toiResources.m_scratchpad = nullptr;
    toiResources.m_scratchpadSize = 0;
    toiResources.m_priorityClassMap = nullptr;
    toiResources.m_priorityClassRatios = nullptr;
    if (m_toiResourceMgr->beginToiAndSetupResources(event, m_toiEvents, toiResources).m_enum == HK_FAILURE)
    {
        return;
    }
    m_world->m_criticalOperationsLockCount++;
    hkInplaceArray<hkpEntity*, 64, hkContainerHeapAllocator> activeEntities;

    // hkLs_buildStepInfos (inlined)
    hkStepInfo stepInfo;
    stepInfo.m_startTime = event.m_time;
    stepInfo.m_endTime = world->getCurrentPsiTime();
    stepInfo.m_deltaTime = stepInfo.m_endTime.val() - stepInfo.m_startTime.val();
    stepInfo.m_invDeltaTime = 1.0f / stepInfo.m_deltaTime.val();
    hkpSolverInfo solverInfo = world->m_dynamicsStepInfo.m_solverInfo;
    solverInfo.m_tau = 0.5f;
    solverInfo.m_damping = 1.4f; // 0x3FB33333
    solverInfo.m_frictionTau = 0.25f;
    solverInfo.m_globalAccelerationPerSubStep.setZero();
    solverInfo.m_globalAccelerationPerStep.setZero();
    solverInfo.m_integrateVelocityFactor.m_quad = _mm_set1_ps(0.357142866f); // 0x3EB6DB6E: tau / damping
    solverInfo.m_invIntegrateVelocityFactor.m_quad = _mm_set1_ps(2.8f); // 0x40333333
    solverInfo.m_dampDivTau = 2.8f; // 0x40333333
    solverInfo.m_tauDivDamp = 0.357142866f; // 0x3EB6DB6E
    solverInfo.m_dampDivFrictionTau = 5.6f; // 0x40B33333
    solverInfo.m_frictionTauDivDamp = 0.178571433f; // 0x3E36DB6E
    solverInfo.m_deltaTime = physicsDeltaTime;
    solverInfo.m_invDeltaTime = 1.0f / physicsDeltaTime;
    solverInfo.m_numSteps = 1;
    solverInfo.m_numMicroSteps = 1;
    solverInfo.m_invNumSteps = 1.0f; // m_invNumMicroSteps stays the world's
    hkpConstraintQueryIn constraintQueryIn(hkpBeginConstraints);
    constraintQueryIn.set(solverInfo, stepInfo, world->m_violatedConstraintArray);

    hkpConstraintSolverResources solverResources;
    hkpConstraintSolverSetup::initializeSolverState(stepInfo, solverInfo, constraintQueryIn, toiResources.m_scratchpad, toiResources.m_scratchpadSize, toiResources.m_priorityClassMap, toiResources.m_priorityClassRatios, solverResources);
    hkpSimulationIsland* island = hkContinuousSimulation_eventIsland(event);
    {
        hkLocalBuffer<unsigned char> entityState(island->m_entities.getSize());
        memset(entityState.begin(), 0, island->m_entities.getSize());
        const hkResult result = hkLs_localizedSolveToi(toiResources, solverResources, event, *m_toiResourceMgr, world, activeEntities, entityState, rotateNormal);
        hkpConstraintSolverSetup::shutdownSolver(solverResources);
        event.m_contactMgr->removeToiImpl(*hkContinuousSimulation_eventIsland(event), event.m_properties);
        if (result.m_enum == HK_SUCCESS)
        {
            for (int i = m_toiEvents.getSize() - 1; i >= 0; i--)
            {
                const hkpToiEvent& other = m_toiEvents[i];
                const hkpEntity* a = other.m_entities[0];
                const hkpEntity* b = other.m_entities[1];
                if ((a->m_simulationIsland == island && entityState[a->m_storageIndex] == HK_LS_ENTITY_ACTIVE) || (b->m_simulationIsland == island && entityState[b->m_storageIndex] == HK_LS_ENTITY_ACTIVE))
                {
                    hkContinuousSimulation_removeEventAndToi(m_toiEvents, i);
                }
            }
            hkReferencedObject::addReferences(reinterpret_cast<hkReferencedObject* const*>(activeEntities.begin()), activeEntities.getSize(), sizeof(hkpEntity*));
        }
        else
        {
            activeEntities.m_size = 0;
        }
    }
    m_toiResourceMgr->endToiAndFreeResources(event, m_toiEvents, toiResources);

    if (activeEntities.getSize() != 0)
    {
        // entities removed from the world by callbacks are dropped
        for (int i = activeEntities.getSize() - 1; i >= 0; i--)
        {
            hkpEntity* entity = activeEntities[i];
            if (entity->m_world == nullptr)
            {
                activeEntities.m_size--;
                if (activeEntities.getSize() != i)
                {
                    activeEntities[i] = activeEntities[activeEntities.getSize()];
                }
                entity->removeReference();
            }
        }
        if (activeEntities.getSize() != 0)
        {
            collideEntitiesBroadPhaseContinuous(activeEntities.begin(), activeEntities.getSize(), world, nullptr);
            if (hkOutOfMemoryState != hkMemoryAllocator::MEMORY_STATE_OUT_OF_MEMORY)
            {
                collideEntitiesOfOneIslandNarrowPhaseContinuous_toiOnly(activeEntities.begin(), activeEntities.getSize(), *world->m_collisionInput, m_entitiesNeedingPsiCollisionDetection);
            }
        }
        hkReferencedObject::removeReferences(reinterpret_cast<hkReferencedObject* const*>(activeEntities.begin()), activeEntities.getSize(), sizeof(hkpEntity*));
    }
    hkContinuousSimulation_unlock(m_world);
}
