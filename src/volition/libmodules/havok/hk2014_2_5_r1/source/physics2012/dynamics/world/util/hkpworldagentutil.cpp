// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/util/hkpworldagentutil.cpp
//
// Havok 2014.2.5 (havok_impl.md HK7): agents between world entities (created on new broadphase pairs, in the
// island of the non-fixed entity, with the contact manager of the two materials' response types), from rfg.exe.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/util/hkpworldagentutil.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcollidable.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/contactmgr/hkpcontactmgr.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpprocesscollisioninput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent3/machine/nn/hkpagentnnmachine.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent3/machine/nn/hkplinkedcollidable.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/dispatch/contactmgr/hkpcontactmgrfactory.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/dispatch/hkpcollisiondispatcher.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/hkpshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/hkpentity.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/hkpsimulationisland.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/hkpworld.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/simulation/hkpsimulation.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/util/hkpworldoperationutil.h"

static inline hkpEntity* hkWorldAgentUtil_entity(const hkpCollidable* collidable)
{
    return static_cast<hkpEntity*>((hkpWorldObject*)collidable->getOwner());
}

static inline hkpAgentNnTrack& hkWorldAgentUtil_track(hkpSimulationIsland* island, unsigned char nnTrackType)
{
    return nnTrackType == HK_AGENT3_NARROWPHASE_TRACK ? island->m_narrowphaseAgentTrack : island->m_midphaseAgentTrack;
}

// 0x00E1DDB0: the island whose track holds the entry (only the track with fewer sectors is searched)
hkpSimulationIsland* hkpWorldAgentUtil::getIslandFromAgentEntry(hkpAgentNnEntry* entry, hkpSimulationIsland* candidateA, hkpSimulationIsland* candidateB)
{
    hkpAgentNnTrack& trackA = hkWorldAgentUtil_track(candidateA, entry->m_nnTrackType.m_storage);
    hkpAgentNnTrack& trackB = hkWorldAgentUtil_track(candidateB, entry->m_nnTrackType.m_storage);
    const bool searchA = trackA.m_sectors.getSize() <= trackB.m_sectors.getSize();
    hkpAgentNnTrack& track = searchA ? trackA : trackB;
    // hkAgentNnMachine_IsEntryOnTrack (inlined)
    bool found = false;
    for (int i = 0; i < track.m_sectors.getSize(); i++)
    {
        hkpAgentNnSector* sector = track.m_sectors[i];
        if (sector->getBegin() <= entry && entry < sector->getEnd())
        {
            found = true;
            break;
        }
    }
    return found == searchA ? candidateA : candidateB;
}

// 0x00E1DE40
void hkpWorldAgentUtil::invalidateTim(hkpEntity* entity, const hkpCollisionInput& collisionInput)
{
    hkArray<hkpLinkedCollidable::CollisionEntry>& entries = entity->m_collidable.m_collisionEntries;
    for (int i = 0; i < entries.getSize(); i++)
    {
        hkAgentNnMachine_InvalidateTimInAgent(entries[i].m_agentEntry, collisionInput);
    }
}

// 0x00E1DE70: every agent of the island, narrowphase track first
void hkpWorldAgentUtil::warpTime(hkpSimulationIsland* island, float oldTime, float newTime, const hkpCollisionInput& collisionInput)
{
    HK_FOR_ALL_AGENT_ENTRIES_BEGIN(island->m_narrowphaseAgentTrack, entry)
    {
        hkAgentNnMachine_WarpTimeInAgent(entry, oldTime, newTime, collisionInput);
    }
    HK_FOR_ALL_AGENT_ENTRIES_END;
    HK_FOR_ALL_AGENT_ENTRIES_BEGIN(island->m_midphaseAgentTrack, entry)
    {
        hkAgentNnMachine_WarpTimeInAgent(entry, oldTime, newTime, collisionInput);
    }
    HK_FOR_ALL_AGENT_ENTRIES_END;
}

// 0x00E1DF40: the islands of two non-fixed entities merge first; the agent goes to the first island that is not the
// fixed one (storage index 0xFFFF), on its narrowphase track
hkpAgentNnEntry* hkpWorldAgentUtil::addAgent(hkpLinkedCollidable* collA, hkpLinkedCollidable* collB, const hkpProcessCollisionInput& input)
{
    hkpEntity* entityA = hkWorldAgentUtil_entity(collA);
    hkpEntity* entityB = hkWorldAgentUtil_entity(collB);
    if (entityA->m_motion.m_type.m_storage != hkpMotion::MOTION_FIXED && entityB->m_motion.m_type.m_storage != hkpMotion::MOTION_FIXED && entityA->m_simulationIsland != entityB->m_simulationIsland)
    {
        hkpWorldOperationUtil::mergeIslands(entityA->m_world, entityA, entityB);
    }
    hkpSimulationIsland* islandA = entityA->m_simulationIsland;
    hkpSimulationIsland* islandB = entityB->m_simulationIsland;
    hkpSimulationIsland* theIsland = islandA;
    if (islandA->m_storageIndex == 0xFFFF && islandB->m_storageIndex != 0xFFFF)
    {
        theIsland = islandB;
    }

    // transform shapes are unwrapped (up to 4 levels each)
    hkPadSpu<unsigned char> cdBodyHasTransformFlag(0);
    hkpCdBody newCdBodiesA[4];
    hkMotionState newMotionStatesA[4];
    hkpCdBody newCdBodiesB[4];
    hkMotionState newMotionStatesB[4];
    const hkpCdBody* firstNonTransformBodyA = collA;
    const hkpCdBody* firstNonTransformBodyB = collB;
    if (collA->m_shape->m_type.m_storage == hkcdShapeType::TRANSFORM)
    {
        firstNonTransformBodyA = hkAgentMachine_processTransformedShapes(collA, newCdBodiesA, newMotionStatesA, 4, cdBodyHasTransformFlag);
    }
    if (collB->m_shape->m_type.m_storage == hkcdShapeType::TRANSFORM)
    {
        firstNonTransformBodyB = hkAgentMachine_processTransformedShapes(collB, newCdBodiesB, newMotionStatesB, 4, cdBodyHasTransformFlag);
    }

    int agentType;
    int isFlipped;
    hkAgentNnMachine_GetAgentType(firstNonTransformBodyA, firstNonTransformBodyB, input, agentType, isFlipped);
    if (isFlipped)
    {
        hkpLinkedCollidable* coll = collA;
        collA = collB;
        collB = coll;
        const hkpCdBody* body = firstNonTransformBodyA;
        firstNonTransformBodyA = firstNonTransformBodyB;
        firstNonTransformBodyB = body;
    }

    // the factory is picked by the unflipped entities' response types
    hkpContactMgrFactory* factory = input.m_dispatcher.val()->m_contactMgrFactory[entityA->m_material.m_responseType.m_storage][entityB->m_material.m_responseType.m_storage];
    hkpContactMgr* mgr = factory->createContactMgr(*collA, *collB, input);
    return hkAgentNnMachine_CreateAgent(theIsland->m_narrowphaseAgentTrack, collA, firstNonTransformBodyA, collB, firstNonTransformBodyB, cdBodyHasTransformFlag.val(), agentType, input, mgr);
}

// 0x00E1E0B0: both islands need a split check (one only when the islands are the same or one entity is fixed)
void hkpWorldAgentUtil::removeAgent(hkpAgentNnEntry* agent)
{
    hkpEntity* entityA = hkWorldAgentUtil_entity(agent->m_collidable[0]);
    hkpEntity* entityB = hkWorldAgentUtil_entity(agent->m_collidable[1]);
    hkpSimulationIsland* islandA = entityA->m_simulationIsland;
    hkpSimulationIsland* islandB = entityB->m_simulationIsland;
    hkpSimulationIsland* theIsland;
    if (islandA == islandB)
    {
        islandA->m_splitCheckRequested = true;
        theIsland = islandA;
    }
    else if (entityA->m_motion.m_type.m_storage == hkpMotion::MOTION_FIXED)
    {
        theIsland = islandB;
    }
    else if (entityB->m_motion.m_type.m_storage == hkpMotion::MOTION_FIXED)
    {
        theIsland = islandA;
    }
    else
    {
        theIsland = getIslandFromAgentEntry(agent, islandA, islandB);
        islandA->m_splitCheckRequested = true;
        entityB->m_simulationIsland->m_splitCheckRequested = true;
    }
    hkpAgentNnTrack& track = hkWorldAgentUtil_track(theIsland, agent->m_nnTrackType.m_storage);
    hkpContactMgr* mgr = agent->m_contactMgr;
    hkAgentNnMachine_DestroyAgent(track, agent, theIsland->m_world->m_collisionDispatcher, *theIsland);
    mgr->cleanup();
}

// 0x00E1E160
void hkpWorldAgentUtil::removeAgentAndItsToiEvents(hkpAgentNnEntry* agent)
{
    hkWorldAgentUtil_entity(agent->m_collidable[0])->m_simulationIsland->m_world->m_simulation->removeCollisionInformationForAgent(agent);
    removeAgent(agent);
}
