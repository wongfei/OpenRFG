// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/util/hkpworldoperationutil.cpp
//
// Havok 2014.2.5 (havok_impl.md HK7): the world's structural operations, from rfg.exe. Entities in and out of the
// broadphase and the simulation islands, constraints in and out of the world, island activation, deactivation,
// merging and splitting (union find over the island's agents, constraints and actions; small groups are packed
// together up to the world's m_minDesiredIslandSize), motion type changes (the dynamic motion is saved while a body
// is fixed or keyframed).
//
// The island bit fields are 2-bit hkBool fields: setting one writes 01 (the binary's "& ~mask | low bit").

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/util/hkpworldoperationutil.h"

#include <new>
#include <string.h>

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/algorithm/sort/hksort.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/algorithm/unionfind/hkunionfind.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/localarray/hklocalarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/localarray/hklocalbuffer.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/swepttransform/hkswepttransformutil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/router/hkmemoryrouter.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/thread/criticalsection/hkcriticalsection.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpprocesscollisioninput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent3/machine/nn/hkpagentnnmachine.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent3/machine/nn/hkplinkedcollidable.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/broadphase/hkpbroadphase.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/broadphase/hkpbroadphasehandlepair.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/dispatch/broadphase/hkptypedbroadphasedispatcher.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/filter/hkpcollisionfilter.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/action/hkpaction.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/hkpconstraintinstance.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/util/hkpconstraintcallbackutil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/hkpentity.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/hkprigidbody.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/util/hkpentityaabbutil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/motion/rigid/hkpfixedrigidmotion.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/motion/rigid/hkpkeyframedrigidmotion.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/motion/util/hkprigidmotionutil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/phantom/hkpphantom.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/hkpsimulationisland.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/simulation/hkpsimulation.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/util/hkpworldagentutil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/util/hkpworldcallbackutil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/util/hkpworldconstraintutil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/util/hkpworldoperationqueue.h"

namespace
{
inline hkpEntity* hkWorldOperationUtil_entity(const hkpCollidable* collidable)
{
    return static_cast<hkpEntity*>((hkpWorldObject*)collidable->getOwner());
}

// the island owning an agent: the first entity's unless that one is in the fixed island (storage index 0xFFFF)
inline hkpSimulationIsland* hkWorldOperationUtil_agentIsland(const hkpAgentNnEntry* entry)
{
    hkpSimulationIsland* island = hkWorldOperationUtil_entity(entry->m_collidable[0])->m_simulationIsland;
    if (island->m_storageIndex == 0xFFFF)
    {
        island = hkWorldOperationUtil_entity(entry->m_collidable[1])->m_simulationIsland;
    }
    return island;
}

inline const hkpCollidableCollidableFilter* hkWorldOperationUtil_filter(const hkpWorld* world)
{
    return world->m_collisionFilter != nullptr ? static_cast<const hkpCollidableCollidableFilter*>(world->m_collisionFilter) : nullptr;
}

// hkpWorld::unlockAndAttemptToExecutePendingOperations (inlined)
inline void hkWorldOperationUtil_unlock(hkpWorld* world)
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

// the world's motion select flags into a motion's inactive frame counters (hkpMotion::setWorldSelectFlagsNeg)
inline void hkWorldOperationUtil_setWorldSelectFlags(hkpWorld* world, hkpMotion& motion)
{
    const hkpSolverInfo& si = world->m_dynamicsStepInfo.m_solverInfo;
    motion.setWorldSelectFlagsNeg(si.m_deactivationNumInactiveFramesSelectFlag[0], si.m_deactivationNumInactiveFramesSelectFlag[1], si.m_deactivationIntegrateCounter);
}
}

// 0x00E06270: an active, marked island small enough to take islandSizeAddon more (from the back); it becomes sparse
static hkpSimulationIsland* findSmallSparseActiveIsland(hkpWorld* world, hkArray<hkpSimulationIsland*, hkContainerHeapAllocator>& islands, int islandSizeAddon, hkpSimulationIsland* islandToIgnore)
{
    if ((int)world->m_minDesiredIslandSize <= islandSizeAddon || !world->m_wantSimulationIslands.m_bool)
    {
        return nullptr;
    }
    for (int i = islands.getSize() - 1; i >= 0; i--)
    {
        hkpSimulationIsland* island = islands[i];
        if (island == islandToIgnore)
        {
            continue;
        }
        int size = island->m_entities.getSize();
        if (size <= island->m_numConstraints)
        {
            size = island->m_numConstraints;
        }
        if (island->m_activeMark && size + islandSizeAddon < (int)world->m_minDesiredIslandSize)
        {
            island->m_isSparse = true;
            return island;
        }
    }
    return nullptr;
}

// 0x00DEC870: islands whose activation or action list changed are handled in cleanupDirtyIslands
void hkpWorldOperationUtil::putIslandOnDirtyList(hkpWorld* world, hkpSimulationIsland* island)
{
    world->m_islandDirtyListCriticalSection->enter();
    if (island->m_dirtyListIndex == 0xFFFF)
    {
        island->m_dirtyListIndex = (unsigned short)world->m_dirtySimulationIslands.getSize();
        world->m_dirtySimulationIslands.pushBack(island);
    }
    world->m_islandDirtyListCriticalSection->leave();
}

// 0x00E0ACD0: the active island with the most entities is kept at index 0
void hkpWorldOperationUtil::sortBigIslandToFront(hkpWorld* world, hkpSimulationIsland* island)
{
    const unsigned short index = island->m_storageIndex;
    if (index != 0)
    {
        hkpSimulationIsland* front = world->m_activeSimulationIslands[0];
        if (front->m_entities.getSize() < island->m_entities.getSize())
        {
            island->m_storageIndex = 0;
            front->m_storageIndex = index;
            world->m_activeSimulationIslands[0] = island;
            world->m_activeSimulationIslands[index] = front;
        }
    }
}

// 0x00E062E0
void hkpWorldOperationUtil::addConstraintToCriticalLockedIsland(hkpWorld* world, hkpConstraintInstance* constraint)
{
    hkpWorldConstraintUtil::addConstraint(world, constraint);
    if (world->m_constraintListeners.getSize() != 0)
    {
        hkpWorldCallbackUtil::fireConstraintAdded(world, constraint);
    }
    if (constraint->m_listeners.getSize() != 0)
    {
        hkpConstraintCallbackUtil::fireConstraintAdded(constraint);
    }
}

// 0x00E06320
void hkpWorldOperationUtil::removeConstraintFromCriticalLockedIsland(hkpWorld* world, hkpConstraintInstance* constraint)
{
    if (world->m_constraintListeners.getSize() != 0)
    {
        hkpWorldCallbackUtil::fireConstraintRemoved(world, constraint);
    }
    if (constraint->m_listeners.getSize() != 0)
    {
        hkpConstraintCallbackUtil::fireConstraintRemoved(constraint);
    }
    hkpWorldConstraintUtil::removeConstraint(constraint);
}

// 0x00E06360
hkpConstraintInstance* hkpWorldOperationUtil::addConstraintImmediately(hkpWorld* world, hkpConstraintInstance* constraint, FireCallbacks fireCallbacks)
{
    world->m_criticalOperationsLockCount++;
    hkpWorldConstraintUtil::addConstraint(world, constraint);
    if (fireCallbacks != DO_NOT_FIRE_CALLBACKS_AND_SUPPRESS_EXECUTION_OF_PENDING_OPERATIONS)
    {
        if (world->m_constraintListeners.getSize() != 0)
        {
            hkpWorldCallbackUtil::fireConstraintAdded(world, constraint);
        }
        if (constraint->m_listeners.getSize() != 0)
        {
            hkpConstraintCallbackUtil::fireConstraintAdded(constraint);
        }
    }
    hkWorldOperationUtil_unlock(world);
    return constraint;
}

// 0x00E063F0: outside a locked section the constraint listeners are told, the instance's own are not (original)
void hkpWorldOperationUtil::removeConstraintImmediately(hkpWorld* world, hkpConstraintInstance* constraint, FireCallbacks fireCallbacks)
{
    static_cast<hkpSimulationIsland*>(constraint->m_owner)->m_splitCheckRequested = true;
    if (world->m_criticalOperationsLockCount == 0)
    {
        world->m_criticalOperationsLockCount = 1;
        if (fireCallbacks != DO_NOT_FIRE_CALLBACKS_AND_SUPPRESS_EXECUTION_OF_PENDING_OPERATIONS && world->m_constraintListeners.getSize() != 0)
        {
            hkpWorldCallbackUtil::fireConstraintRemoved(world, constraint);
        }
        hkpWorldConstraintUtil::removeConstraint(constraint);
        hkWorldOperationUtil_unlock(world);
    }
    else
    {
        if (fireCallbacks != DO_NOT_FIRE_CALLBACKS_AND_SUPPRESS_EXECUTION_OF_PENDING_OPERATIONS)
        {
            if (world->m_constraintListeners.getSize() != 0)
            {
                hkpWorldCallbackUtil::fireConstraintRemoved(world, constraint);
            }
            if (constraint->m_listeners.getSize() != 0)
            {
                hkpConstraintCallbackUtil::fireConstraintRemoved(constraint);
            }
        }
        hkpWorldConstraintUtil::removeConstraint(constraint);
    }
}

// 0x00E06500: islands by the uid of their first entity, null islands last
static hkBool worldOperationUtil_less_hkSimulationIslandPtr(const hkpSimulationIsland* a, const hkpSimulationIsland* b)
{
    hkBool result;
    if (b == nullptr)
    {
        result.m_bool = 0;
    }
    else if (a == nullptr)
    {
        result.m_bool = 1;
    }
    else
    {
        result.m_bool = a->m_entities[0]->m_uid < b->m_entities[0]->m_uid;
    }
    return result;
}

// 0x00E06540
void hkpWorldOperationUtil::removeIslandFromDirtyList(hkpWorld* world, hkpSimulationIsland* island)
{
    if (island->m_dirtyListIndex != 0xFFFF)
    {
        world->m_dirtySimulationIslands[island->m_dirtyListIndex] = nullptr;
        island->m_dirtyListIndex = 0xFFFF;
    }
}

// 0x00E06570: swapped out of the active or inactive array
void hkpWorldOperationUtil::removeIsland(hkpWorld* world, hkpSimulationIsland* island)
{
    hkArray<hkpSimulationIsland*, hkContainerHeapAllocator>& islands = island->m_isInActiveIslandsArray ? world->m_activeSimulationIslands : world->m_inactiveSimulationIslands;
    islands[island->m_storageIndex] = islands[islands.getSize() - 1];
    islands[island->m_storageIndex]->m_storageIndex = island->m_storageIndex;
    islands.m_size--;
    removeIslandFromDirtyList(world, island);
}

// 0x00E067A0: the entity's agents on the track whose partner is fixed (or, becoming fixed, that live in the entity's
// island) go to agentsRemoved; agents to dynamic partners move to the partner's island
void hkpWorldOperationUtil::removeAttachedAgentsConnectingTheEntityAndAFixedPartnerEntityPlus(hkpAgentNnTrack& trackToScan, hkpEntity* entity, hkpAgentNnTrack& agentsRemoved, hkpMotion::MotionType newMotionType)
{
    hkArray<hkpLinkedCollidable::CollisionEntry, hkContainerHeapAllocator>& entries = entity->m_collidable.m_collisionEntries;
    for (int i = 0; i < entries.getSize(); i++)
    {
        hkpLinkedCollidable::CollisionEntry& ce = entries[i];
        hkpAgentNnEntry* entry = ce.m_agentEntry;
        if (entry->m_nnTrackType.m_storage != trackToScan.m_nnTrackType.m_storage)
        {
            continue;
        }
        hkpEntity* partner = hkWorldOperationUtil_entity(ce.m_partner);
        hkpAgentNnTrack* destTrack = &agentsRemoved;
        if (partner->m_motion.m_type.m_storage != hkpMotion::MOTION_FIXED)
        {
            if (newMotionType != hkpMotion::MOTION_FIXED || hkpWorldAgentUtil::getIslandFromAgentEntry(entry, entity->m_simulationIsland, partner->m_simulationIsland) != entity->m_simulationIsland)
            {
                continue;
            }
            hkpSimulationIsland* island = partner->m_simulationIsland;
            destTrack = ce.m_agentEntry->m_nnTrackType.m_storage == HK_AGENT3_NARROWPHASE_TRACK ? &island->m_narrowphaseAgentTrack : &island->m_midphaseAgentTrack;
        }
        hkAgentNnMachine_CopyAndRelinkAgentEntry(*destTrack, ce.m_agentEntry);
        hkAgentNnMachine_InternalDeallocateEntry(trackToScan, entry);
    }
}

// 0x00E06870: fixed entities join the fixed island, active ones a small sparse island if there is one, otherwise
// each gets a new island (all share the first active island without simulation islands)
void hkpWorldOperationUtil::addEntitySI(hkpWorld* world, hkpEntity* entity, hkpEntityActivation initialActivationState)
{
    entity->m_world = world;
    if (entity->m_motion.m_type.m_storage == hkpMotion::MOTION_FIXED)
    {
        world->m_fixedIsland->internalAddEntity(entity);
        return;
    }
    hkpSimulationIsland* island;
    if (!world->m_wantSimulationIslands.m_bool)
    {
        island = world->m_activeSimulationIslands[0];
    }
    else
    {
        const bool active = initialActivationState == HK_ENTITY_ACTIVATION_DO_ACTIVATE;
        hkArray<hkpSimulationIsland*, hkContainerHeapAllocator>* islands;
        if (active)
        {
            islands = &world->m_activeSimulationIslands;
            island = findSmallSparseActiveIsland(world, *islands, 4, nullptr);
            if (island != nullptr)
            {
                island->internalAddEntity(entity);
                return;
            }
        }
        else
        {
            islands = &world->m_inactiveSimulationIslands;
        }
        island = new hkpSimulationIsland(world);
        island->m_isInActiveIslandsArray = active;
        island->m_activeMark = active;
        island->m_storageIndex = (unsigned short)islands->getSize();
        island->m_splitCheckFrameCounter = (unsigned char)islands->getSize();
        islands->pushBack(island);
    }
    island->internalAddEntity(entity);
    island->m_splitCheckFrameCounter = (unsigned char)entity->m_uid;
}

// 0x00E06980: empty islands go (not the fixed one, not without simulation islands)
void hkpWorldOperationUtil::removeEntitySI(hkpWorld* world, hkpEntity* entity)
{
    hkpSimulationIsland* island = entity->m_simulationIsland;
    entity->m_world = nullptr;
    island->internalRemoveEntity(entity);
    if (island->m_storageIndex != 0xFFFF && island->m_entities.getSize() == 0 && world->m_wantSimulationIslands.m_bool)
    {
        removeIsland(world, island);
        delete island;
    }
}

// 0x00E069D0: the entity's actions living in the fixed island (referenced, to be added again later)
void hkpWorldOperationUtil::removeAttachedActionsFromFixedIsland(hkpWorld* world, hkpEntity* entity, hkArray<hkpAction*, hkContainerHeapAllocator>& actionsToBeMoved)
{
    for (int i = 0; i < entity->m_actions.getSize(); i++)
    {
        hkpAction* action = entity->m_actions[i];
        if (action->m_island == world->m_fixedIsland)
        {
            actionsToBeMoved.pushBack(action);
            action->addReference();
            world->m_fixedIsland->removeAction(action);
            world->m_fixedIsland->m_actionListCleanupNeeded = true;
            putIslandOnDirtyList(world, world->m_fixedIsland);
        }
    }
}

// 0x00E07020: the island with fewer entities (or the later one) is merged into the other one and deleted
void hkpWorldOperationUtil::internalMergeTwoIslands(hkpWorld* world, hkpSimulationIsland* islandA, hkpSimulationIsland* islandB)
{
    hkpSimulationIsland* island = islandA;
    hkpSimulationIsland* other = islandB;
    const int sizeA = islandA->m_entities.getSize();
    const int sizeB = islandB->m_entities.getSize();
    if (sizeA <= sizeB && (sizeA < sizeB || islandB->m_storageIndex < islandA->m_storageIndex))
    {
        island = islandB;
        other = islandA;
    }
    world->m_criticalOperationsLockCount++;

    const bool anyInActiveArray = island->m_isInActiveIslandsArray || other->m_isInActiveIslandsArray;
    const bool activeMark = island->m_activeMark || other->m_activeMark;
    if (anyInActiveArray)
    {
        if (!island->m_isInActiveIslandsArray)
        {
            island->m_activeMark = true;
            internalActivateIsland(world, island, hkBool(false));
        }
        else if (!other->m_isInActiveIslandsArray)
        {
            other->m_activeMark = true;
            internalActivateIsland(world, other, hkBool(false));
        }
    }
    // the sparse bits ored (byte 0x25 bits 2-3)
    *((unsigned char*)island + 0x25) |= *((unsigned char*)other + 0x25) & 0xC;

    island->markForWrite();
    other->markForWrite();
    world->m_fixedIsland->markForWrite();
    hkAgentNnMachine_AppendTrack(island->m_narrowphaseAgentTrack, other->m_narrowphaseAgentTrack);
    hkAgentNnMachine_AppendTrack(island->m_midphaseAgentTrack, other->m_midphaseAgentTrack);

    // entities
    {
        int index = island->m_entities.getSize();
        const int newSize = other->m_entities.getSize() + index;
        const int capacity = island->m_entities.getCapacity();
        if (capacity < newSize)
        {
            hkArrayUtil::_reserve(hkContainerHeapAllocator::s_alloc, &island->m_entities, capacity * 2 > newSize ? capacity * 2 : newSize, sizeof(hkpEntity*));
        }
        island->m_entities.m_size = newSize;
        for (int i = 0; i < other->m_entities.getSize(); i++, index++)
        {
            hkpEntity* entity = other->m_entities[i];
            island->m_entities[index & 0xFFFF] = entity;
            entity->m_simulationIsland = island;
            entity->m_storageIndex = (unsigned short)index;
        }
    }

    // actions (the null entries of removed actions are dropped)
    {
        hkArray<hkpAction*, hkContainerHeapAllocator>& actions = island->m_actions;
        int count = actions.getSize();
        const int newSize = other->m_actions.getSize() + count;
        int capacity = actions.getCapacity();
        if (capacity < newSize)
        {
            hkArrayUtil::_reserve(hkContainerHeapAllocator::s_alloc, &actions, capacity * 2 > newSize ? capacity * 2 : newSize, sizeof(hkpAction*));
        }
        actions.m_size = newSize;
        for (int i = 0; i < other->m_actions.getSize(); i++)
        {
            hkpAction* action = other->m_actions[i];
            if (action != nullptr)
            {
                actions[count] = action;
                actions[count]->m_island = island;
                count++;
            }
        }
        capacity = actions.getCapacity();
        if (capacity < count)
        {
            hkArrayUtil::_reserve(hkContainerHeapAllocator::s_alloc, &actions, capacity * 2 > count ? capacity * 2 : count, sizeof(hkpAction*));
        }
        actions.m_size = count;
    }

    // constraints
    for (int i = 0; i < other->m_entities.getSize(); i++)
    {
        hkpEntity* entity = other->m_entities[i];
        for (int c = 0; c < entity->m_constraintsMaster.getSize(); c++)
        {
            entity->m_constraintsMaster[c].m_constraint->m_owner = island;
        }
    }
    island->mergeConstraintInfo(*other);
    island->m_numConstraints += other->m_numConstraints;
    other->m_numConstraints = 0;

    {
        const unsigned short index = other->m_storageIndex;
        hkArray<hkpSimulationIsland*, hkContainerHeapAllocator>& islands = other->m_isInActiveIslandsArray ? world->m_activeSimulationIslands : world->m_inactiveSimulationIslands;
        if ((int)index < islands.getSize() - 1)
        {
            islands[index] = islands[islands.getSize() - 1];
            islands[index]->m_storageIndex = index;
        }
        islands.m_size--;
    }

    island->m_splitCheckRequested = island->m_splitCheckRequested || other->m_splitCheckRequested;
    island->m_actionListCleanupNeeded = island->m_actionListCleanupNeeded || other->m_actionListCleanupNeeded;
    island->m_activeMark = activeMark;
    if (other->m_dirtyListIndex != 0xFFFF)
    {
        putIslandOnDirtyList(world, island);
    }
    removeIslandFromDirtyList(world, other);
    delete other;

    if (island->m_isInActiveIslandsArray)
    {
        sortBigIslandToFront(world, island);
    }
    world->checkDeterminismOfIslandBroadPhase(island);
    hkWorldOperationUtil_unlock(world);
}

// 0x00E073E0: the entity's constraints leave the world (referenced; masters from the back, then slaves)
void hkpWorldOperationUtil::removeAttachedConstraints(hkpEntity* entity, hkArray<hkpConstraintInstance*, hkContainerHeapAllocator>& removedConstraints)
{
    hkpWorld* world = entity->m_world;
    for (int i = entity->m_constraintsMaster.getSize() - 1; i >= 0; i--)
    {
        hkpConstraintInstance* constraint = entity->m_constraintsMaster[i].m_constraint;
        constraint->addReference();
        removeConstraintImmediately(world, constraint, DO_NOT_FIRE_CALLBACKS_AND_SUPPRESS_EXECUTION_OF_PENDING_OPERATIONS);
        removedConstraints.pushBack(constraint);
    }
    for (int i = entity->m_constraintsSlave.getSize() - 1; i >= 0; i--)
    {
        hkpConstraintInstance* constraint = entity->m_constraintsSlave[i];
        constraint->addReference();
        removeConstraintImmediately(world, constraint, DO_NOT_FIRE_CALLBACKS_AND_SUPPRESS_EXECUTION_OF_PENDING_OPERATIONS);
        removedConstraints.pushBack(constraint);
    }
}

// 0x00E07530: the motion states restart at time 0 with their counters reset, the agents warp from the deactivation
// time; with allowIslandMerges small sparse islands join it
void hkpWorldOperationUtil::internalActivateIsland(hkpWorld* world, hkpSimulationIsland* island, hkBool allowIslandMerges)
{
    {
        hkArray<hkpSimulationIsland*, hkContainerHeapAllocator>& inactive = world->m_inactiveSimulationIslands;
        inactive[island->m_storageIndex] = inactive[inactive.getSize() - 1];
        inactive[island->m_storageIndex]->m_storageIndex = island->m_storageIndex;
        inactive.m_size--;
    }
    hkArray<hkpSimulationIsland*, hkContainerHeapAllocator>& islands = world->m_activeSimulationIslands;
    islands.pushBack(island);
    island->m_timeSinceLastHighFrequencyCheck = 0.0f;
    island->m_timeSinceLastLowFrequencyCheck = 0.0f;
    island->m_storageIndex = (unsigned short)(islands.getSize() - 1);
    island->m_isInActiveIslandsArray = true;

    const hkSimdFloat32 zero = hkSimdFloat32::getConstant<HK_QUADREAL_0>();
    for (int i = 0; i < island->m_entities.getSize(); i++)
    {
        hkpEntity* entity = island->m_entities[i];
        hkSweptTransformUtil::setTimeInformation(zero, zero, entity->m_motion.m_motionState);
        entity->m_motion.m_deactivationNumInactiveFrames[0] = 0;
        entity->m_motion.m_deactivationNumInactiveFrames[1] = 0;
        hkWorldOperationUtil_setWorldSelectFlags(world, entity->m_motion);
    }
    hkpWorldAgentUtil::warpTime(island, island->m_timeOfDeactivation, world->m_dynamicsStepInfo.m_stepInfo.m_startTime.val(), *world->m_collisionInput);

    // the first entity tells whether a listener moved the island
    hkpEntity* entity = island->m_entities[0];
    entity->addReference();
    hkpWorldCallbackUtil::fireIslandActivated(world, island);
    if (entity->m_simulationIsland != island)
    {
        entity->removeReference();
        return;
    }
    entity->removeReference();

    if (allowIslandMerges.m_bool)
    {
        int numMerges = 0;
        for (;;)
        {
            int size = island->m_numConstraints + 3;
            const int numEntities = island->m_entities.getSize() + 1;
            if (size < numEntities)
            {
                size = numEntities;
            }
            hkpSimulationIsland* small = findSmallSparseActiveIsland(world, islands, size, island);
            if (small == nullptr)
            {
                break;
            }
            hkpEntity* first = island->m_entities[0];
            internalMergeTwoIslands(world, small, island);
            island = first->m_simulationIsland;
            numMerges++;
        }
        if (numMerges != 0)
        {
            return;
        }
    }
    sortBigIslandToFront(world, island);
}

// 0x00E07810: only when every motion may deactivate (hkRigidMotionUtilCanDeactivateFinal); the motion states are
// frozen and the velocities zeroed
void hkpWorldOperationUtil::internalDeactivateIsland(hkpWorld* world, hkpSimulationIsland* island)
{
    if (!hkRigidMotionUtilCanDeactivateFinal(world->m_dynamicsStepInfo.m_stepInfo, (hkpMotion* const*)island->m_entities.begin(), island->m_entities.getSize(), 0xE0)) // the offset of hkpEntity::m_motion
    {
        island->m_activeMark = true;
        return;
    }
    world->m_inactiveSimulationIslands.pushBack(island);
    {
        hkArray<hkpSimulationIsland*, hkContainerHeapAllocator>& active = world->m_activeSimulationIslands;
        active[island->m_storageIndex] = active[active.getSize() - 1];
        active[island->m_storageIndex]->m_storageIndex = island->m_storageIndex;
        active.m_size--;
    }
    island->m_isInActiveIslandsArray = false;
    island->m_storageIndex = (unsigned short)(world->m_inactiveSimulationIslands.getSize() - 1);
    island->m_timeOfDeactivation = world->m_dynamicsStepInfo.m_stepInfo.m_startTime.val();
    for (int i = 0; i < island->m_entities.getSize(); i++)
    {
        hkpEntity* entity = island->m_entities[i];
        hkSweptTransformUtil::freezeMotionState(island->m_timeOfDeactivation, entity->m_motion.m_motionState);
        entity->m_motion.setLinearVelocity(hkVector4f::getConstant<HK_QUADREAL_0>());
        entity->m_motion.setAngularVelocity(hkVector4f::getConstant<HK_QUADREAL_0>());
    }
    hkpWorldCallbackUtil::fireIslandDeactivated(world, island);
}

// 0x00E07930
void hkpWorldOperationUtil::markIslandInactive(hkpWorld* world, hkpSimulationIsland* island)
{
    island->m_activeMark = false;
    putIslandOnDirtyList(world, island);
}

// 0x00E07A10: an island reactivated before it left the active array tries to grow again
void hkpWorldOperationUtil::markIslandActive(hkpWorld* world, hkpSimulationIsland* island)
{
    if (!island->m_activeMark && island->m_isInActiveIslandsArray)
    {
        island->m_tryToIncreaseIslandSizeMark = true;
    }
    island->m_activeMark = true;
    putIslandOnDirtyList(world, island);
}

// 0x00E07AA0: the body's motion object (in place) for the new type. Becoming fixed or keyframed keeps the dynamic
// motion as the saved motion (new reference, quality type saved); becoming dynamic again restores it; a change
// between dynamic types recreates the motion from the old mass, inertia and center of mass.
void hkpWorldOperationUtil::replaceMotionObject(hkpRigidBody* body, hkpMotion::MotionType newState, hkBool newStateNeedsInertia, hkBool oldStateNeedsInertia, hkpWorld* world)
{
    hkpMaxSizeMotion& motion = body->m_motion;
    // the old motion: a byte copy (vtable included), destroyed as a keyframed motion at the end
    alignas(16) unsigned char oldStorage[sizeof(hkpMaxSizeMotion)];
    hkpMaxSizeMotion* oldMotion = reinterpret_cast<hkpMaxSizeMotion*>(oldStorage);

    if (newStateNeedsInertia.m_bool)
    {
        if (!oldStateNeedsInertia.m_bool)
        {
            // the saved dynamic motion comes back with the current state
            hkpMaxSizeMotion* saved = motion.m_savedMotion;
            motion.getMotionStateAndVelocitiesAndDeactivationType(saved);
            saved->m_deactivationNumInactiveFrames[0] = 0;
            saved->m_deactivationNumInactiveFrames[1] = 0;
            body->m_collidable.m_broadPhaseHandle.m_objectQualityType = (signed char)motion.m_savedQualityTypeIndex;
            memcpy(&motion, saved, sizeof(hkpMaxSizeMotion));
            // (the binary also memsets 0 bytes at body + 0x200 here)
            saved->removeReference();
            body->setCachedShapeData(world, body->m_collidable.m_shape);
        }
        if (motion.m_type.m_storage == newState || newState == hkpMotion::MOTION_DYNAMIC)
        {
            body->m_collidable.m_motion = &motion.m_motionState;
            if (world != nullptr)
            {
                hkWorldOperationUtil_setWorldSelectFlags(world, motion);
            }
            return;
        }
        memcpy(oldStorage, &motion, sizeof(hkpMaxSizeMotion));
        hkMatrix3f inertiaLocal;
        motion.getInertiaLocal(inertiaLocal);
        const float maxAngularVelocity = (float)oldMotion->m_motionState.m_maxAngularVelocity;
        const float maxLinearVelocity = (float)oldMotion->m_motionState.m_maxLinearVelocity;
        const hkSweptTransformf& st = oldMotion->m_motionState.getSweptTransform();
        hkpRigidBody::createDynamicRigidMotion(newState, oldMotion->m_motionState.getTransform().getTranslation(), st.m_rotation1, oldMotion->getMass(), inertiaLocal, st.m_centerOfMassLocal, maxLinearVelocity, maxAngularVelocity, &motion);
        oldMotion->hkpMotion::getMotionStateAndVelocitiesAndDeactivationType(&motion);
        // the damping halves rounded again (hkHalf::setReal<true>(getReal()))
        const unsigned short angularDamping = *(const unsigned short*)&oldMotion->m_motionState.m_angularDamping;
        const unsigned short linearDamping = *(const unsigned short*)&oldMotion->m_motionState.m_linearDamping;
        unsigned int u = (unsigned int)angularDamping << 16;
        float f;
        memcpy(&f, &u, 4);
        f *= 1.00390625f; // 0x3F808000
        memcpy(&u, &f, 4);
        *(unsigned short*)&motion.m_motionState.m_angularDamping = (unsigned short)(u >> 16);
        u = (unsigned int)linearDamping << 16;
        memcpy(&f, &u, 4);
        f *= 1.00390625f;
        memcpy(&u, &f, 4);
        *(unsigned short*)&motion.m_motionState.m_linearDamping = (unsigned short)(u >> 16);
    }
    else
    {
        memcpy(oldStorage, &motion, sizeof(hkpMaxSizeMotion));
        hkpMotion* newMotion;
        if (newState == hkpMotion::MOTION_FIXED)
        {
            newMotion = new (&motion) hkpFixedRigidMotion(motion.m_motionState.getTransform().getTranslation(), motion.m_motionState.getSweptTransform().m_rotation1);
            newMotion->m_motionState = oldMotion->m_motionState;
            newMotion->m_deactivationIntegrateCounter = oldMotion->m_deactivationIntegrateCounter;
            // frozen at the end of the last step (or the world's current time)
            const __m128 invDeltaTime = _mm_shuffle_ps(oldMotion->m_motionState.getSweptTransform().m_centerOfMass1.m_quad, oldMotion->m_motionState.getSweptTransform().m_centerOfMass1.m_quad, _MM_SHUFFLE(3, 3, 3, 3));
            if (!(_mm_cvtss_f32(invDeltaTime) == 0.0f) && !(_mm_cvtss_f32(invDeltaTime) != _mm_cvtss_f32(invDeltaTime)))
            {
                const __m128 r = _mm_rcp_ps(invDeltaTime);
                const __m128 deltaTime = _mm_mul_ps(_mm_sub_ps(_mm_set1_ps(2.0f), _mm_mul_ps(r, invDeltaTime)), r);
                const __m128 baseTime = _mm_shuffle_ps(oldMotion->m_motionState.getSweptTransform().m_centerOfMass0.m_quad, oldMotion->m_motionState.getSweptTransform().m_centerOfMass0.m_quad, _MM_SHUFFLE(3, 3, 3, 3));
                hkSimdFloat32 time;
                time.m_real = _mm_add_ps(baseTime, deltaTime);
                if (world != nullptr)
                {
                    time.m_real = _mm_set1_ps(world->getCurrentTime());
                }
                hkSweptTransformUtil::freezeMotionState(time, newMotion->m_motionState);
            }
        }
        else
        {
            newMotion = new (&motion) hkpKeyframedRigidMotion(motion.m_motionState.getTransform().getTranslation(), motion.m_motionState.getSweptTransform().m_rotation1);
            oldMotion->hkpMotion::getMotionStateAndVelocitiesAndDeactivationType(newMotion);
        }

        if (oldStateNeedsInertia.m_bool)
        {
            // the dynamic motion is saved (a copy with its own reference count)
            hkpMaxSizeMotion* saved = static_cast<hkpMaxSizeMotion*>(hkMemoryRouter::getInstance().heap().blockAlloc(sizeof(hkpMaxSizeMotion)));
            new (saved) hkpMaxSizeMotion();
            const unsigned short memSize = *((const unsigned short*)saved + 3);
            memcpy(saved, oldStorage, sizeof(hkpMaxSizeMotion));
            const unsigned int memSizeAndRefCount = ((unsigned int)memSize << 16) | 1;
            memcpy((char*)saved + 4, &memSizeAndRefCount, 4);
            newMotion->m_savedMotion = saved;
            newMotion->m_savedQualityTypeIndex = (unsigned short)(short)body->m_collidable.m_broadPhaseHandle.m_objectQualityType;
            body->setCachedShapeData(world, body->m_collidable.m_shape);
        }
        else
        {
            static_cast<hkpKeyframedRigidMotion*>(newMotion)->setStoredMotion(oldMotion->m_savedMotion);
            newMotion->m_savedQualityTypeIndex = oldMotion->m_savedQualityTypeIndex;
        }
        body->m_solverData = 0;
        // HK_COLLIDABLE_QUALITY_FIXED (0) or HK_COLLIDABLE_QUALITY_KEYFRAMED (1)
        body->m_collidable.m_broadPhaseHandle.m_objectQualityType = newState != hkpMotion::MOTION_FIXED;
    }
    oldMotion->hkpKeyframedRigidMotion::~hkpKeyframedRigidMotion();

    body->m_collidable.m_motion = &motion.m_motionState;
    if (world != nullptr)
    {
        hkWorldOperationUtil_setWorldSelectFlags(world, motion);
    }
}

// 0x00E07FB0: queued while the world is locked
void hkpWorldOperationUtil::mergeIslands(hkpWorld* world, hkpEntity* entityA, hkpEntity* entityB)
{
    if (world->m_criticalOperationsLockCount != 0)
    {
        hkWorldOperation::MergeIslands op;
        op.m_type.m_storage = hkWorldOperation::ISLAND_MERGE;
        op.m_entities[0] = entityA;
        op.m_entities[1] = entityB;
        world->queueOperation(op);
        return;
    }
    internalMergeTwoIslands(world, entityA->m_simulationIsland, entityB->m_simulationIsland);
}

// 0x00E08010: the dirty islands sorted by first entity uid, then from the back: action list compaction, activation
// changes, and growing (merging small sparse islands into islands marked to grow)
void hkpWorldOperationUtil::cleanupDirtyIslands(hkpWorld* world)
{
    hkArray<hkpSimulationIsland*, hkContainerHeapAllocator>& dirty = world->m_dirtySimulationIslands;
    if (dirty.getSize() == 0)
    {
        return;
    }
    if (dirty.getSize() > 1)
    {
        hkAlgorithm::quickSortRecursive(dirty.begin(), 0, dirty.getSize() - 1, worldOperationUtil_less_hkSimulationIslandPtr);
    }
    for (int i = 0; i < dirty.getSize(); i++)
    {
        if (dirty[i] != nullptr)
        {
            dirty[i]->m_dirtyListIndex = (unsigned short)i;
        }
    }
    while (dirty.getSize() != 0)
    {
        hkpSimulationIsland* island = dirty[dirty.getSize() - 1];
        dirty.m_size--;
        if (island == nullptr)
        {
            continue;
        }
        island->m_dirtyListIndex = 0xFFFF;
        if (island->m_actionListCleanupNeeded)
        {
            hkArray<hkpAction*, hkContainerHeapAllocator>& actions = island->m_actions;
            const int size = actions.getSize();
            int write = -1;
            int read = 0;
            for (; read < size; read++)
            {
                if (actions[read] == nullptr)
                {
                    write = read;
                    read++;
                    break;
                }
            }
            for (; read < actions.getSize(); read++)
            {
                if (actions[read] != nullptr)
                {
                    actions[write] = actions[read];
                    write++;
                }
            }
            if (write != -1)
            {
                const int capacity = actions.getCapacity();
                if (capacity < write)
                {
                    hkArrayUtil::_reserve(hkContainerHeapAllocator::s_alloc, &actions, capacity * 2 > write ? capacity * 2 : write, sizeof(hkpAction*));
                }
                actions.m_size = write;
            }
            island->m_actionListCleanupNeeded = false;
        }

        if (island->m_activeMark == island->m_isInActiveIslandsArray)
        {
            if (island->m_tryToIncreaseIslandSizeMark && island->m_activeMark)
            {
                island->m_tryToIncreaseIslandSizeMark = false;
                for (;;)
                {
                    int size = island->m_entities.getSize();
                    if (size <= island->m_numConstraints)
                    {
                        size = island->m_numConstraints;
                    }
                    hkpSimulationIsland* small = findSmallSparseActiveIsland(world, world->m_activeSimulationIslands, size, island);
                    if (small == nullptr)
                    {
                        break;
                    }
                    hkpEntity* first = island->m_entities[0];
                    internalMergeTwoIslands(world, island, small);
                    island = first->m_simulationIsland;
                }
            }
        }
        else if (!island->m_activeMark)
        {
            internalDeactivateIsland(world, island);
        }
        else
        {
            internalActivateIsland(world, island, hkBool(true));
        }
    }
    world->checkDeterminism();
}

// 0x00E081A0: the broadphase box from the entity's (expanded) cached box; queued while locked
void hkpWorldOperationUtil::updateEntityBP(hkpWorld* world, hkpEntity* entity)
{
    if (world->m_criticalOperationsLockCount != 0)
    {
        hkWorldOperation::UpdateEntityBP op;
        op.m_type.m_storage = hkWorldOperation::ENTITY_UPDATE_BROAD_PHASE;
        op.m_entity = entity;
        world->queueOperation(op);
        return;
    }
    world->m_criticalOperationsLockCount = 1;
    if (entity->m_collidable.m_shape != nullptr)
    {
        hkpCollidable::BoundingVolumeData& bv = entity->m_collidable.m_boundingVolumeData;
        if (bv.m_min[0] > bv.m_max[0])
        {
            hkpEntityAabbUtil::entityBatchRecalcAabb(world->m_collisionInput, &entity, 1);
        }
        // hkAabbUtil::uncompressExpandedAabbUint32 (the padding bytes of the local box are not written)
        hkAabbUint32 aabb;
        const int shift = bv.m_expansionShift;
        for (int k = 0; k < 3; k++)
        {
            aabb.m_min[k] = bv.m_min[k] - ((unsigned int)bv.m_expansionMin[k] << shift);
            aabb.m_max[k] = bv.m_max[k] + ((unsigned int)bv.m_expansionMax[k] << shift);
        }
        hkLocalArray<hkpBroadPhaseHandlePair> newPairs(world->m_broadPhaseQuerySize);
        hkLocalArray<hkpBroadPhaseHandlePair> delPairs(world->m_broadPhaseQuerySize);
        hkpBroadPhaseHandle* handle = &entity->m_collidable.m_broadPhaseHandle;
        world->m_broadPhase->updateAabbsUint32(&handle, &aabb, 1, newPairs, delPairs);
        if (delPairs.getSize() + newPairs.getSize() > 0)
        {
            hkpTypedBroadPhaseDispatcher::removeDuplicates(newPairs, delPairs);
            world->m_broadPhaseDispatcher->removePairs(reinterpret_cast<hkpTypedBroadPhaseHandlePair*>(delPairs.begin()), delPairs.getSize());
            world->m_broadPhaseDispatcher->addPairs(reinterpret_cast<hkpTypedBroadPhaseHandlePair*>(newPairs.begin()), newPairs.getSize(), hkWorldOperationUtil_filter(world));
        }
    }
    hkWorldOperationUtil_unlock(world);
}

// 0x00E08570
void hkpWorldOperationUtil::addEntityBP(hkpWorld* world, hkpEntity* entity)
{
    if (entity->m_collidable.m_shape == nullptr)
    {
        return;
    }
    hkLocalArray<hkpBroadPhaseHandlePair> newPairs(world->m_broadPhaseQuerySize);
    hkpEntityAabbUtil::entityBatchRecalcAabb(world->m_collisionInput, &entity, 1);
    const hkAabbUint32 aabb = reinterpret_cast<const hkAabbUint32&>(entity->m_collidable.m_boundingVolumeData);
    world->m_broadPhase->addObject(&entity->m_collidable.m_broadPhaseHandle, aabb, newPairs, false);
    if (newPairs.getSize() > 0)
    {
        world->m_broadPhaseDispatcher->addPairs(reinterpret_cast<hkpTypedBroadPhaseHandlePair*>(newPairs.begin()), newPairs.getSize(), hkWorldOperationUtil_filter(world));
    }
}

// 0x00E08710: phantoms (type BROAD_PHASE_PHANTOM) are added with the special phantom flag
void hkpWorldOperationUtil::addPhantomBP(hkpWorld* world, hkpPhantom* phantom)
{
    hkLocalArray<hkpBroadPhaseHandlePair> newPairs(world->m_broadPhaseQuerySize);
    const bool isPhantom = phantom->m_collidable.m_broadPhaseHandle.m_type == hkpWorldObject::BROAD_PHASE_PHANTOM;
    hkAabb aabb;
    phantom->calcAabb(aabb);
    world->m_broadPhase->addObject(&phantom->m_collidable.m_broadPhaseHandle, aabb, newPairs, isPhantom);
    phantom->setBoundingVolumeData(aabb);
    if (newPairs.getSize() != 0)
    {
        world->m_broadPhaseDispatcher->addPairs(reinterpret_cast<hkpTypedBroadPhaseHandlePair*>(newPairs.begin()), newPairs.getSize(), hkWorldOperationUtil_filter(world));
    }
}

// 0x00E08880: the simulation forgets the entity's TOIs and TIMs first
void hkpWorldOperationUtil::removeEntityBP(hkpWorld* world, hkpEntity* entity)
{
    if (entity->m_collidable.m_shape == nullptr)
    {
        return;
    }
    world->m_simulation->resetCollisionInformationForEntities(&entity, 1, world, hkpSimulation::RESET_TOI);
    hkLocalArray<hkpBroadPhaseHandlePair> removedPairs(world->m_broadPhaseQuerySize);
    world->m_broadPhase->removeObject(&entity->m_collidable.m_broadPhaseHandle, removedPairs);
    if (removedPairs.getSize() > 0)
    {
        world->m_broadPhaseDispatcher->removePairs(reinterpret_cast<hkpTypedBroadPhaseHandlePair*>(removedPairs.begin()), removedPairs.getSize());
    }
}

// 0x00E089E0
void hkpWorldOperationUtil::removePhantomBP(hkpWorld* world, hkpPhantom* phantom)
{
    hkLocalArray<hkpBroadPhaseHandlePair> removedPairs(world->m_broadPhaseQuerySize);
    world->m_broadPhase->removeObject(&phantom->m_collidable.m_broadPhaseHandle, removedPairs);
    if (removedPairs.getSize() != 0)
    {
        world->m_broadPhaseDispatcher->removePairs(reinterpret_cast<hkpTypedBroadPhaseHandlePair*>(removedPairs.begin()), removedPairs.getSize());
    }
}

// 0x00E08B10: the entity's actions whose other entities are all fixed leave its island (referenced)
void hkpWorldOperationUtil::removeAttachedActionsFromDynamicIsland(hkpWorld* world, hkpEntity* entity, hkArray<hkpAction*, hkContainerHeapAllocator>& actionsToBeMoved)
{
    hkpSimulationIsland* island = entity->m_simulationIsland;
    for (int i = 0; i < entity->m_actions.getSize(); i++)
    {
        hkpAction* action = entity->m_actions[i];
        hkInplaceArray<hkpEntity*, 16, hkContainerHeapAllocator> entities;
        action->getEntities(entities);
        bool attachedToDynamic = false;
        for (int j = 0; j < entities.getSize(); j++)
        {
            if (entities[j]->m_motion.m_type.m_storage != hkpMotion::MOTION_FIXED && entities[j] != entity)
            {
                attachedToDynamic = true;
                break;
            }
        }
        if (attachedToDynamic)
        {
            continue;
        }
        actionsToBeMoved.pushBack(action);
        action->addReference();
        island->removeAction(action);
        island->m_actionListCleanupNeeded = true;
        putIslandOnDirtyList(world, island);
    }
}

// 0x00E08CC0: the actions join the entity's island (its other dynamic entities' islands merge with it); the
// references taken on removal go
void hkpWorldOperationUtil::addActionsToEntitysIsland(hkpWorld* world, hkpEntity* entity, hkArray<hkpAction*, hkContainerHeapAllocator>& actionsToBeMoved)
{
    hkpSimulationIsland* island = entity->m_simulationIsland;
    hkInplaceArray<hkpEntity*, 16, hkContainerHeapAllocator> entities;
    for (int i = 0; i < actionsToBeMoved.getSize(); i++)
    {
        hkpAction* action = actionsToBeMoved[i];
        if (island == world->m_fixedIsland)
        {
            world->m_fixedIsland->addAction(action);
            action->removeReference();
            continue;
        }
        island->addAction(action);
        action->removeReference();
        entities.m_size = 0;
        action->getEntities(entities);
        for (int j = 0; j < entities.getSize(); j++)
        {
            hkpEntity* other = entities[j];
            if (other != entity && other->m_motion.m_type.m_storage != hkpMotion::MOTION_FIXED && entity->m_motion.m_type.m_storage != hkpMotion::MOTION_FIXED && other->m_simulationIsland != entity->m_simulationIsland)
            {
                mergeIslands(other->m_world, other, entity);
            }
        }
    }
}

// The hkpConstraintOwner::addConstraintInfo inlined in the split (the delta's maximum schema size is the old island's)
static inline void hkWorldOperationUtil_moveConstraintInfo(hkpSimulationIsland* from, hkpSimulationIsland* to, hkConstraintInternal& ci)
{
    const int deltaMaxSizeOfSchema = from->m_constraintInfo.m_maxSizeOfSchema;
    const int sizeOfSchemas = (int)ci.m_sizeOfSchemas;
    const unsigned short numSolverResults = ci.m_numSolverResults;
    const int numSolverElemTemps = (int)ci.m_numSolverElemTemps;
    from->m_numConstraints--;
    hkpConstraintInstance* constraint = ci.m_constraint;
    if (constraint->m_internal != nullptr)
    {
        from->m_constraintInfo.m_sizeOfSchemas -= sizeOfSchemas;
        from->m_constraintInfo.m_numSolverResults -= numSolverResults;
        from->m_constraintInfo.m_numSolverElemTemps -= numSolverElemTemps;
        hkConstraintInternal* internal = constraint->m_internal;
        internal->m_numSolverResults = (unsigned short)(internal->m_numSolverResults - numSolverResults);
        internal->m_numSolverElemTemps -= numSolverElemTemps;
        internal->m_sizeOfSchemas -= sizeOfSchemas;
    }
    to->m_numConstraints++;
    ci.m_constraint->m_owner = to;
    constraint = ci.m_constraint;
    if (constraint->m_internal != nullptr)
    {
        int maxSize = to->m_constraintInfo.m_maxSizeOfSchema;
        if (deltaMaxSizeOfSchema >= maxSize)
        {
            maxSize = deltaMaxSizeOfSchema;
        }
        to->m_constraintInfo.m_maxSizeOfSchema = maxSize;
        if (maxSize <= sizeOfSchemas)
        {
            maxSize = sizeOfSchemas;
        }
        to->m_constraintInfo.m_sizeOfSchemas += sizeOfSchemas;
        to->m_constraintInfo.m_numSolverResults += numSolverResults;
        to->m_constraintInfo.m_numSolverElemTemps += numSolverElemTemps;
        to->m_constraintInfo.m_maxSizeOfSchema = maxSize;
        hkConstraintInternal* internal = constraint->m_internal;
        internal->m_numSolverResults = (unsigned short)(internal->m_numSolverResults + numSolverResults);
        internal->m_numSolverElemTemps += numSolverElemTemps;
        internal->m_sizeOfSchemas += sizeOfSchemas;
        int internalSize = (int)constraint->m_internal->m_sizeOfSchemas;
        if (internalSize < to->m_constraintInfo.m_maxSizeOfSchema)
        {
            internalSize = to->m_constraintInfo.m_maxSizeOfSchema;
        }
        to->m_constraintInfo.m_maxSizeOfSchema = internalSize;
    }
}

// hkArray::optimizeCapacity(3) of an agent track's sector array (inlined)
static inline void hkWorldOperationUtil_optimizeSectors(hkpAgentNnTrack& track)
{
    hkArray<hkpAgentNnSector*, hkContainerHeapAllocator>& sectors = track.m_sectors;
    if (sectors.m_capacityAndFlags < 0)
    {
        return;
    }
    unsigned int n = (unsigned int)sectors.getSize() + 3;
    n |= n >> 16;
    n |= n >> 8;
    n |= n >> 4;
    n |= n >> 2;
    int capacity = (int)((n >> 1) | n) + 1;
    char* inplace = nullptr;
    if (capacity < 1)
    {
        capacity = 1;
        inplace = (char*)track.m_sectors.m_storage;
    }
    else if ((sectors.m_capacityAndFlags & 0x3FFFFFFF) <= capacity)
    {
        return;
    }
    hkArrayUtil::_reduce(hkContainerHeapAllocator::s_alloc, &sectors, sizeof(hkpAgentNnSector*), inplace, capacity);
}

// 0x00E08E60: the island's entities grouped by connectivity (hkUnionFind over agents, constraints and actions);
// with m_minDesiredIslandSize (or a sparse island) the small active groups are packed together again. The biggest
// group stays in the island, the others get new islands (appended to newIslandsOut); entities, constraint info,
// actions and agents move to their island.
void hkpWorldOperationUtil::splitSimulationIsland(hkpSimulationIsland* currentIsland, hkpWorld* world, hkArray<hkpSimulationIsland*, hkContainerHeapAllocator>& newIslandsOut, hkArray<hkpEntity*, hkContainerHeapAllocator>* oldEntitiesOut)
{
    const int numEntities = currentIsland->m_entities.getSize();
    hkLocalBuffer<int> entityStatus(numEntities);
    hkUnionFind checkConnectivity(entityStatus, currentIsland->m_entities.getSize());
    if (currentIsland->isFullyConnected(checkConnectivity))
    {
        currentIsland->m_isSparse = false;
        return;
    }

    hkInplaceArray<int, 32, hkContainerHeapAllocator> sizeOfIslands;
    checkConnectivity.assignGroups(sizeOfIslands);
    hkLocalBuffer<hkBool> isSparse(sizeOfIslands.getSize() + 1);

    if (currentIsland->m_isSparse || world->m_minDesiredIslandSize != 0)
    {
        const int numGroups = sizeOfIslands.getSize();
        hkLocalBuffer<int> groupSize(numGroups);
        hkLocalBuffer<int> groupRemap(numGroups); // first "group is active", then the new group index
        for (int g = 0; g < numGroups; g++)
        {
            groupRemap[g] = 0;
            groupSize[g] = 0;
        }
        const hkpSolverInfo& si = world->m_dynamicsStepInfo.m_solverInfo;
        for (int i = 0; i < currentIsland->m_entities.getSize(); i++)
        {
            hkpEntity* entity = currentIsland->m_entities[i];
            const int g = checkConnectivity.m_parents[i];
            if (!world->m_wantDeactivation.m_bool)
            {
                groupRemap[g] = 1;
            }
            else
            {
                // getNumInactiveFramesMt of both counters; a group is active while any entity rests < 6 frames
                int frames1 = entity->m_motion.m_deactivationNumInactiveFrames[1];
                if ((int)si.m_deactivationNumInactiveFramesSelectFlag[1] == frames1 >> 14)
                {
                    frames1 >>= 7;
                }
                int frames0 = entity->m_motion.m_deactivationNumInactiveFrames[0];
                if ((int)si.m_deactivationNumInactiveFramesSelectFlag[0] == frames0 >> 14)
                {
                    frames0 >>= 7;
                }
                unsigned int frames = (unsigned int)frames1 & 0x7F;
                if (frames < ((unsigned int)frames0 & 0x7F))
                {
                    frames = (unsigned int)frames0 & 0x7F;
                }
                if (frames < 6)
                {
                    groupRemap[g] = 1;
                }
            }
            const int numConstraints = entity->m_constraintsMaster.getSize();
            groupSize[g] += numConstraints != 0 ? numConstraints : 1;
        }
        for (int g = 0; g < numGroups; g++)
        {
            if (groupRemap[g] == 0)
            {
                groupSize[g] = -1; // inactive groups are not packed
            }
        }
        int numNewGroups = 0;
        for (int g = 0; g < numGroups; g++)
        {
            int size = groupSize[g];
            if (size == -2)
            {
                continue; // already packed into an earlier group
            }
            isSparse[numNewGroups].m_bool = 0;
            groupRemap[g] = numNewGroups;
            if (size >= 0)
            {
                for (int h = g + 1; h < numGroups; h++)
                {
                    // groups closer than 10 to the desired size stay as they are
                    if ((int)world->m_minDesiredIslandSize <= size + 10)
                    {
                        break;
                    }
                    if (groupSize[h] >= 0 && groupSize[h] + size < (int)world->m_minDesiredIslandSize)
                    {
                        size += groupSize[h];
                        isSparse[numNewGroups].m_bool = 1;
                        groupRemap[h] = numNewGroups;
                        groupSize[h] = -2;
                    }
                }
            }
            numNewGroups++;
        }
        checkConnectivity.reindex(groupRemap, numNewGroups, sizeOfIslands);
    }
    else
    {
        memset(isSparse.begin(), 0, sizeOfIslands.getSize() + 1);
    }

    const int biggest = checkConnectivity.moveBiggestGroupToIndexZero(sizeOfIslands);
    {
        const hkBool tmp = isSparse[0];
        isSparse[0] = isSparse[biggest];
        isSparse[biggest] = tmp;
    }
    const int numEntitiesInBiggest = sizeOfIslands[0];
    const int numOldEntities = currentIsland->m_entities.getSize();
    const int numGroups = sizeOfIslands.getSize();
    if (numGroups == 1)
    {
        return;
    }

    hkLocalBuffer<hkpSimulationIsland*> islands(numGroups);
    currentIsland->m_isSparse = isSparse[0].m_bool != 0;
    islands[0] = currentIsland;
    for (int g = 1; g < numGroups; g++)
    {
        hkpSimulationIsland* island = new hkpSimulationIsland(world);
        island->m_isInActiveIslandsArray = true;
        island->m_activeMark = true;
        island->m_isSparse = isSparse[g].m_bool != 0;
        island->m_tryToIncreaseIslandSizeMark = true;
        island->m_storageIndex = (unsigned short)newIslandsOut.getSize();
        newIslandsOut.pushBack(island);
        islands[g] = island;
        if (island->m_entities.getCapacity() < sizeOfIslands[g])
        {
            hkArrayUtil::_reserve(hkContainerHeapAllocator::s_alloc, &island->m_entities, sizeOfIslands[g], sizeof(hkpEntity*));
        }
    }

    // the entities (and the constraint info of the moved ones) go to their island; the old array is read while the
    // biggest group is written back into it
    hkpEntity* const* oldEntities = currentIsland->m_entities.begin();
    if (oldEntitiesOut == nullptr)
    {
        currentIsland->m_entities.m_size = 0;
    }
    else
    {
        currentIsland->m_entities.swap(*oldEntitiesOut);
        currentIsland->m_entities.m_size = 0;
        if (currentIsland->m_entities.getCapacity() < sizeOfIslands[0])
        {
            hkArrayUtil::_reserve(hkContainerHeapAllocator::s_alloc, &currentIsland->m_entities, sizeOfIslands[0], sizeof(hkpEntity*));
        }
    }
    for (int i = 0; i < numOldEntities; i++)
    {
        hkpSimulationIsland* island = islands[entityStatus[i]];
        hkpEntity* entity = oldEntities[i];
        entity->m_simulationIsland = island;
        entity->m_storageIndex = (unsigned short)island->m_entities.getSize();
        island->m_entities.begin()[island->m_entities.m_size++] = entity;
        if (island != currentIsland)
        {
            for (int c = 0; c < entity->m_constraintsMaster.getSize(); c++)
            {
                hkWorldOperationUtil_moveConstraintInfo(currentIsland, island, entity->m_constraintsMaster[c]);
            }
        }
    }

    // the actions go to the island of their first non-fixed entity
    {
        hkpAction** actions = currentIsland->m_actions.begin();
        const int numActions = currentIsland->m_actions.getSize();
        currentIsland->m_actions.m_size = 0;
        for (int a = 0; a < numActions; a++)
        {
            hkpAction* action = actions[a];
            if (action == nullptr)
            {
                continue;
            }
            hkpSimulationIsland* island = nullptr;
            {
                hkInplaceArray<hkpEntity*, 16, hkContainerHeapAllocator> entities;
                action->getEntities(entities);
                for (int j = 0; j < entities.getSize(); j++)
                {
                    island = entities[j]->m_simulationIsland;
                    if (island != world->m_fixedIsland)
                    {
                        break;
                    }
                }
            }
            action->m_island = island;
            island->m_actions.pushBack(action);
        }
    }

    world->lockForIslandSplit(currentIsland);
    hkpAgentNnTrack* tracks[2] = { &currentIsland->m_narrowphaseAgentTrack, &currentIsland->m_midphaseAgentTrack };
    if ((numOldEntities - numEntitiesInBiggest) * 8 < numOldEntities)
    {
        // few entities left: the agents of other islands are moved out of the tracks
        for (int t = 0; t < 2; t++)
        {
            hkpAgentNnTrack& track = *tracks[t];
            for (int s = 0; s < track.m_sectors.getSize(); s++)
            {
                hkpAgentNnEntry* sectorBegin = track.m_sectors[s]->getBegin();
                hkpAgentNnEntry* entry = sectorBegin;
                while (entry < hkAddByteOffset(sectorBegin, track.getSectorSize(s)))
                {
                    hkpSimulationIsland* island = hkWorldOperationUtil_agentIsland(entry);
                    if (island == currentIsland)
                    {
                        entry = hkAddByteOffset(entry, entry->m_size);
                        continue;
                    }
                    const int offset = (int)((char*)entry - (char*)sectorBegin);
                    hkpAgentNnTrack& destTrack = t == 0 ? island->m_narrowphaseAgentTrack : island->m_midphaseAgentTrack;
                    hkAgentNnMachine_CopyAndRelinkAgentEntry(destTrack, entry);
                    hkAgentNnMachine_InternalDeallocateEntry(track, entry);
                    if (track.m_sectors.getSize() <= s)
                    {
                        break;
                    }
                    // the last entry was moved into the hole
                    sectorBegin = track.m_sectors[s]->getBegin();
                    entry = hkAddByteOffset(sectorBegin, offset);
                }
            }
        }
    }
    else
    {
        // many entities moved: every agent is copied to its island and the old sectors are freed
        for (int t = 0; t < 2; t++)
        {
            hkpAgentNnTrack& track = *tracks[t];
            hkInplaceArray<hkpAgentNnSector*, 1, hkContainerHeapAllocator> sectors;
            if (track.m_sectors.getSize() == 1)
            {
                sectors.m_storage[0] = track.m_sectors[0];
                sectors.m_size = 1;
                track.m_sectors.m_size = 0;
            }
            else if (track.m_sectors.getSize() > 1)
            {
                hkArrayUtil::_reserve(hkContainerHeapAllocator::s_alloc, &sectors, 2, sizeof(hkpAgentNnSector*));
                sectors.swap(track.m_sectors);
            }
            const unsigned short bytesUsedInLastSector = track.m_bytesUsedInLastSector;
            track.m_bytesUsedInLastSector = HK_AGENT3_SECTOR_SIZE;
            for (int s = 0; s < sectors.getSize(); s++)
            {
                hkpAgentNnSector* sector = sectors[s];
                const int bytes = s + 1 == sectors.getSize() ? bytesUsedInLastSector : HK_AGENT3_SECTOR_SIZE;
                for (hkpAgentNnEntry* entry = sector->getBegin(); entry < hkAddByteOffset(sector->getBegin(), bytes); entry = hkAddByteOffset(entry, entry->m_size))
                {
                    hkpSimulationIsland* island = hkWorldOperationUtil_agentIsland(entry);
                    hkAgentNnMachine_CopyAndRelinkAgentEntry(t == 0 ? island->m_narrowphaseAgentTrack : island->m_midphaseAgentTrack, entry);
                }
                hkMemoryRouter::getInstance().heap().blockFree(sector, HK_AGENT3_SECTOR_SIZE);
            }
        }
    }
    world->unlockForIslandSplit(currentIsland);
    hkWorldOperationUtil_optimizeSectors(currentIsland->m_narrowphaseAgentTrack);
    hkWorldOperationUtil_optimizeSectors(currentIsland->m_midphaseAgentTrack);

    for (int g = 1; g < numGroups; g++)
    {
        islands[g]->m_splitCheckFrameCounter = (unsigned char)(currentIsland->m_splitCheckFrameCounter + g);
    }
}

// 0x00E0A190: the new islands share the island's activation; a split of an active island marked inactive puts them
// on the dirty list too
void hkpWorldOperationUtil::splitSimulationIsland(hkpWorld* world, hkpSimulationIsland* currentIsland)
{
    const bool inActiveArray = currentIsland->m_isInActiveIslandsArray;
    hkArray<hkpSimulationIsland*, hkContainerHeapAllocator>& newIslandsOut = inActiveArray ? world->m_activeSimulationIslands : world->m_inactiveSimulationIslands;
    currentIsland->m_splitCheckRequested = false;
    const int oldSize = newIslandsOut.getSize();
    splitSimulationIsland(currentIsland, world, newIslandsOut, nullptr);
    for (int i = oldSize; i < newIslandsOut.getSize(); i++)
    {
        hkpSimulationIsland* island = newIslandsOut[i];
        island->m_isInActiveIslandsArray = currentIsland->m_isInActiveIslandsArray;
        island->m_activeMark = currentIsland->m_isInActiveIslandsArray;
        if (inActiveArray && !currentIsland->m_activeMark)
        {
            markIslandInactive(world, island);
        }
        island->m_splitCheckFrameCounter = (unsigned char)(currentIsland->m_splitCheckFrameCounter - (char)oldSize + (char)i);
    }
}

// 0x00E0A250: active islands from the back
void hkpWorldOperationUtil::splitSimulationIslands(hkpWorld* world)
{
    if (!world->m_wantSimulationIslands.m_bool)
    {
        return;
    }
    for (int i = world->m_activeSimulationIslands.getSize() - 1; i >= 0; i--)
    {
        hkpSimulationIsland* island = world->m_activeSimulationIslands[i];
        if (island->m_splitCheckRequested)
        {
            splitSimulationIsland(world, island);
        }
    }
}

// 0x00E0A290: fixed <-> movable changes leave and re-enter the islands (constraints, actions and agents are moved);
// other changes replace the motion in place. Becoming dynamic needs the saved dynamic motion (a body created
// keyframed has none: nothing happens).
void hkpWorldOperationUtil::setRigidBodyMotionType(hkpRigidBody* body, hkpMotion::MotionType newState, hkpEntityActivation initialActivationState, hkpUpdateCollisionFilterOnEntityMode collisionFilterUpdateMode)
{
    const hkpMotion::MotionType oldState = (hkpMotion::MotionType)body->m_motion.m_type.m_storage;
    if (oldState == newState)
    {
        return;
    }
    if (oldState == hkpMotion::MOTION_FIXED)
    {
        body->m_collidable.sortEntries();
        body->sortConstraintsSlavesDeterministically();
    }
    hkBool newStateNeedsInertia;
    newStateNeedsInertia.m_bool = newState != hkpMotion::MOTION_FIXED && newState != hkpMotion::MOTION_KEYFRAMED;
    hkBool oldStateNeedsInertia;
    oldStateNeedsInertia.m_bool = oldState != hkpMotion::MOTION_FIXED && oldState != hkpMotion::MOTION_KEYFRAMED;
    if (newStateNeedsInertia.m_bool && !oldStateNeedsInertia.m_bool && body->m_motion.m_savedMotion == nullptr)
    {
        return;
    }

    body->addReference();
    hkpWorld* world = body->m_world;
    if (world == nullptr || (oldState == hkpMotion::MOTION_FIXED) == (newState == hkpMotion::MOTION_FIXED))
    {
        replaceMotionObject(body, newState, newStateNeedsInertia, oldStateNeedsInertia, world);
        if (world != nullptr)
        {
            world->updateCollisionFilterOnEntity(body, collisionFilterUpdateMode, HK_UPDATE_COLLECTION_FILTER_PROCESS_SHAPE_COLLECTIONS);
        }
    }
    else
    {
        hkInplaceArray<hkpConstraintInstance*, 16, hkContainerHeapAllocator> constraints;
        hkInplaceArray<hkpAction*, 16, hkContainerHeapAllocator> actions;
        hkpAgentNnTrack narrowphaseAgents(HK_AGENT3_NARROWPHASE_TRACK);
        hkpAgentNnTrack midphaseAgents(HK_AGENT3_MIDPHASE_TRACK);
        world->m_blockExecutingPendingOperations.m_bool = 1;
        world->m_criticalOperationsAllowed.m_bool = 0;

        removeAttachedConstraints(body, constraints);
        if (newState == hkpMotion::MOTION_FIXED)
        {
            removeAttachedActionsFromDynamicIsland(world, body, actions);
        }
        else
        {
            removeAttachedActionsFromFixedIsland(world, body, actions);
        }
        removeAttachedAgentsConnectingTheEntityAndAFixedPartnerEntityPlus(body->m_simulationIsland->m_narrowphaseAgentTrack, body, narrowphaseAgents, newState);
        removeAttachedAgentsConnectingTheEntityAndAFixedPartnerEntityPlus(body->m_simulationIsland->m_midphaseAgentTrack, body, midphaseAgents, newState);
        if (oldState != hkpMotion::MOTION_FIXED && body->m_simulationIsland->m_entities.getSize() > 2)
        {
            body->m_simulationIsland->m_splitCheckRequested = true;
        }
        removeEntitySI(world, body);
        replaceMotionObject(body, newState, newStateNeedsInertia, oldStateNeedsInertia, world);
        addEntitySI(world, body, initialActivationState);
        addActionsToEntitysIsland(world, body, actions);
        world->m_criticalOperationsAllowed.m_bool = 1;

        hkArray<hkpLinkedCollidable::CollisionEntry, hkContainerHeapAllocator> entries;
        for (int i = 0; i < constraints.getSize(); i++)
        {
            hkpConstraintInstance* constraint = constraints[i];
            addConstraintImmediately(world, constraint, DO_NOT_FIRE_CALLBACKS_AND_SUPPRESS_EXECUTION_OF_PENDING_OPERATIONS);
            constraint->removeReference();
        }
        if (newState != hkpMotion::MOTION_FIXED)
        {
            body->m_collidable.getCollisionEntriesSorted(entries);
            for (int i = 0; i < entries.getSize(); i++)
            {
                hkpEntity* partner = hkWorldOperationUtil_entity(entries[i].m_partner);
                if (body->m_motion.m_type.m_storage != hkpMotion::MOTION_FIXED && partner->m_motion.m_type.m_storage != hkpMotion::MOTION_FIXED && body->m_simulationIsland != partner->m_simulationIsland)
                {
                    mergeIslands(body->m_world, body, partner);
                }
            }
        }
        hkAgentNnMachine_AppendTrack(body->m_simulationIsland->m_narrowphaseAgentTrack, narrowphaseAgents);
        hkAgentNnMachine_AppendTrack(body->m_simulationIsland->m_midphaseAgentTrack, midphaseAgents);
        if (newState == hkpMotion::MOTION_FIXED)
        {
            hkSweptTransformUtil::freezeMotionState(world->m_dynamicsStepInfo.m_stepInfo.m_startTime.val(), body->m_motion.m_motionState);
            world->m_criticalOperationsLockCount++;
            hkpEntity* entity = body;
            world->m_simulation->resetCollisionInformationForEntities(&entity, 1, world, hkpSimulation::RESET_ALL);
            hkpSimulation::collideEntitiesBroadPhaseDiscrete(&entity, 1, world);
            hkpWorldCallbackUtil::fireInactiveEntityMoved(body->m_world, body);
            world->m_criticalOperationsLockCount--;
        }
        world->updateCollisionFilterOnEntity(body, collisionFilterUpdateMode, HK_UPDATE_COLLECTION_FILTER_PROCESS_SHAPE_COLLECTIONS);
        world->m_blockExecutingPendingOperations.m_bool = 0;
        if (world->m_criticalOperationsLockCount == 0)
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
    body->removeReference();
}
