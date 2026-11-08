// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/hkpsimulationisland.cpp
//
// Havok 2014.2.5 (havok_impl.md HK7): the simulation island (its entities, actions, agent tracks and constraint
// info), from rfg.exe.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/hkpsimulationisland.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/algorithm/unionfind/hkunionfind.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/localarray/hklocalbuffer.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/action/hkpaction.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/hkpconstraintinstance.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/hkpentity.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/hkpworld.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/util/hkpworldoperationutil.h"

// 0x00E1DA90: active, in the active list, no entities; both agent tracks start with a full last sector (512 bytes
// used, so the first agent opens a sector)
hkpSimulationIsland::hkpSimulationIsland(hkpWorld* world)
    : m_midphaseAgentTrack(HK_AGENT3_MIDPHASE_TRACK)
    , m_narrowphaseAgentTrack(HK_AGENT3_NARROWPHASE_TRACK)
{
    m_splitCheckRequested = false;
    m_actionListCleanupNeeded = false;
    m_world = world;
    m_isInActiveIslandsArray = true;
    m_inIntegrateJob = false;
    m_tryToIncreaseIslandSizeMark = false;
    m_activeMark = true;
    m_dirtyListIndex = 0xFFFF;
    m_multiThreadCheck.m_threadId = 0xFFFFFFF1;
    m_multiThreadCheck.m_markCount = 0x8000;
    m_timeSinceLastHighFrequencyCheck = 0.0f;
    m_timeSinceLastLowFrequencyCheck = 0.0f;
    m_timeOfDeactivation = -10.0f;
    m_allowIslandLocking = false;
    m_constraintInfo.m_maxSizeOfSchema = 0;
    m_constraintInfo.m_sizeOfSchemas = 0;
    m_constraintInfo.m_numSolverResults = 0;
    m_constraintInfo.m_numSolverElemTemps = 0;
    m_isSparse = false;
    m_numConstraints = 0;
}

// 0x00E1D8F0: the arrays free their storage
hkpSimulationIsland::~hkpSimulationIsland()
{
}

// 0x00E1DCC0: the reflection class (hkpSimulationIslandClass) is not part of this port
const hkClass* hkpSimulationIsland::getClassType() const
{
    return nullptr;
}

// 0x00E1D4B0
void hkpSimulationIsland::addCallbackRequest(hkpConstraintInstance* constraint, int request)
{
    constraint->m_internal->m_callbackRequest |= (unsigned char)request;
}

// 0x00E1D4D0
void hkpSimulationIsland::markForWrite()
{
}

// 0x00E1D540
void hkpSimulationIsland::addConstraintToCriticalLockedIsland(hkpConstraintInstance* constraint)
{
    hkpWorldOperationUtil::addConstraintToCriticalLockedIsland(constraint->m_entities[0]->m_world, constraint);
}

// 0x00E1D560
void hkpSimulationIsland::removeConstraintFromCriticalLockedIsland(hkpConstraintInstance* constraint)
{
    hkpWorldOperationUtil::removeConstraintFromCriticalLockedIsland(constraint->m_entities[0]->m_world, constraint);
}

// 0x00E1D610: the entity's storage index is its slot in the island
void hkpSimulationIsland::internalAddEntity(hkpEntity* entity)
{
    entity->m_simulationIsland = this;
    entity->m_storageIndex = (unsigned short)m_entities.getSize();
    m_entities.pushBack(entity);
}

// 0x00E1D580: the last entity moves into the slot
void hkpSimulationIsland::internalRemoveEntity(hkpEntity* entity)
{
    m_entities[entity->m_storageIndex] = m_entities[m_entities.getSize() - 1];
    m_entities[entity->m_storageIndex]->m_storageIndex = entity->m_storageIndex;
    m_entities.popBack();
    entity->m_simulationIsland = nullptr;
    entity->m_storageIndex = 0xFFFF;
}

// 0x00E1D660
void hkpSimulationIsland::addAction(hkpAction* act)
{
    m_actions.pushBack(act);
    act->m_island = this;
}

// 0x00E1D4F0: the slot is cleared (compacted later) and the island asks for a split check
void hkpSimulationIsland::removeAction(hkpAction* act)
{
    const int i = m_actions.indexOf(act);
    m_actions[i] = nullptr;
    act->m_island = nullptr;
    m_splitCheckRequested = true;
    m_actionListCleanupNeeded = true;
}

// 0x00E1D5E0
void hkpSimulationIsland::mergeConstraintInfo(hkpSimulationIsland& other)
{
    int maxSize = other.m_constraintInfo.m_maxSizeOfSchema;
    if (maxSize < m_constraintInfo.m_maxSizeOfSchema)
    {
        maxSize = m_constraintInfo.m_maxSizeOfSchema;
    }
    m_constraintInfo.m_maxSizeOfSchema = maxSize;
    m_constraintInfo.m_sizeOfSchemas += other.m_constraintInfo.m_sizeOfSchemas;
    m_constraintInfo.m_numSolverResults += other.m_constraintInfo.m_numSolverResults;
    m_constraintInfo.m_numSolverElemTemps += other.m_constraintInfo.m_numSolverElemTemps;
}

// 0x00E1D6A0: unites the island's entities (nodes = storage indices) along their agents, their constraints and
// their actions, ignoring fixed entities; stops as soon as everything is one group
bool hkpSimulationIsland::isFullyConnected(hkUnionFind& checkConnectivityOut)
{
    checkConnectivityOut.beginAddEdges();
    for (int i = 0; i < m_entities.getSize(); i++)
    {
        hkpEntity* entity = m_entities[i];
        for (int j = 0; j < entity->m_collidable.m_collisionEntries.getSize(); j++)
        {
            const hkpEntity* partner = static_cast<const hkpEntity*>(entity->m_collidable.m_collisionEntries[j].m_partner->getOwner());
            if (partner->m_motion.m_type.m_storage == hkpMotion::MOTION_FIXED)
            {
                continue;
            }
            checkConnectivityOut.addEdge(i, partner->m_storageIndex);
            if (checkConnectivityOut.m_parents[0] == -checkConnectivityOut.m_numNodes)
            {
                return true;
            }
        }
    }
    for (int i = 0; i < m_entities.getSize(); i++)
    {
        hkpEntity* entity = m_entities[i];
        for (int j = 0; j < entity->m_constraintsMaster.getSize(); j++)
        {
            const hkConstraintInternal& c = entity->m_constraintsMaster[j];
            if (c.m_entities[0]->m_motion.m_type.m_storage == hkpMotion::MOTION_FIXED || c.m_entities[1]->m_motion.m_type.m_storage == hkpMotion::MOTION_FIXED)
            {
                continue;
            }
            checkConnectivityOut.addEdge(c.m_entities[0]->m_storageIndex, c.m_entities[1]->m_storageIndex);
            if (checkConnectivityOut.m_parents[0] == -checkConnectivityOut.m_numNodes)
            {
                return true;
            }
        }
    }
    hkInplaceArray<hkpEntity*, 10, hkContainerHeapAllocator> actionEntities;
    for (int i = 0; i < m_actions.getSize(); i++)
    {
        if (m_actions[i] == nullptr)
        {
            continue;
        }
        actionEntities.clear();
        m_actions[i]->getEntities(actionEntities);
        // the first entity that is not fixed is linked to every other one
        int k = 0;
        int first = -1;
        while (k < actionEntities.getSize())
        {
            if (actionEntities[k]->m_motion.m_type.m_storage != hkpMotion::MOTION_FIXED)
            {
                first = k;
            }
            k++;
            if (first != -1)
            {
                break;
            }
        }
        for (; k < actionEntities.getSize(); k++)
        {
            if (actionEntities[k]->m_motion.m_type.m_storage == hkpMotion::MOTION_FIXED)
            {
                continue;
            }
            checkConnectivityOut.addEdge(actionEntities[first]->m_storageIndex, actionEntities[k]->m_storageIndex);
            if (checkConnectivityOut.m_parents[0] == -checkConnectivityOut.m_numNodes)
            {
                return true;
            }
        }
    }
    return checkConnectivityOut.m_parents[0] == -checkConnectivityOut.m_numNodes;
}

// 0x00E1D9D0: with a union-find over a stack buffer
bool hkpSimulationIsland::isFullyConnected()
{
    hkLocalBuffer<int> parents(m_entities.getSize());
    hkUnionFind checkConnectivity(parents, m_entities.getSize());
    return isFullyConnected(checkConnectivity);
}

// ---- reflection data, generated by tools/hdrgen/reflgen.py from rfg.exe (do not edit)
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/hkclass.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/hkclassmember.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/hkinternalclassmember.h"

extern const hkClass hkReferencedObjectClass;

//
// Class hkpSimulationIsland
//
extern const hkClass hkpSimulationIslandClass;
const hkClass hkpSimulationIslandClass(
    "hkpSimulationIsland",
    &hkReferencedObjectClass, // parent
    132, // object size
    nullptr,
    0, // interfaces
    nullptr,
    0, // enums
    nullptr,
    0, // members
    nullptr, // defaults
    nullptr, // attributes
    0, // flags
    0 // version
    );
// ---- end of generated reflection data
