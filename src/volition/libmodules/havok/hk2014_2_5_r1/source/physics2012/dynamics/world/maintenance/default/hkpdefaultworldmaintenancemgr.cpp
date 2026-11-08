// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/maintenance/default/hkpdefaultworldmaintenancemgr.cpp
//
// Havok 2014.2.5 (havok_impl.md HK7): the default maintenance manager (island splits, and the world time kept in
// [33, 63) seconds for float precision), from rfg.exe, with the hkpWorldMaintenanceMgr base. The shipped build's
// monitor timers are not reproduced.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/maintenance/default/hkpdefaultworldmaintenancemgr.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/physics/hkstepinfo.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpprocesscollisioninput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/hkpentity.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/hkpsimulationisland.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/hkpworld.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/simulation/hkpsimulation.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/util/hkpworldagentutil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/util/hkpworldoperationutil.h"

// 0x00E1FC80, 0x00E1FC90
hkpWorldMaintenanceMgr::hkpWorldMaintenanceMgr()
{
}

hkpWorldMaintenanceMgr::~hkpWorldMaintenanceMgr()
{
}

// 0x00E1FCC0
hkpDefaultWorldMaintenanceMgr::hkpDefaultWorldMaintenanceMgr()
{
    m_minAllowedTimeValue = 33.0f;
    m_maxAllowedTimeValue = 63.0f;
}

// 0x00E1FE90
hkpDefaultWorldMaintenanceMgr::~hkpDefaultWorldMaintenanceMgr()
{
}

// 0x00E1FCA0: the simulation starts at the lowest allowed time
void hkpDefaultWorldMaintenanceMgr::init(hkpWorld* world)
{
    world->m_simulation->m_currentTime = m_minAllowedTimeValue;
    world->m_simulation->m_currentPsiTime = m_minAllowedTimeValue;
}

// 0x00E1FCF0
void hkpDefaultWorldMaintenanceMgr::performMaintenance(hkpWorld* world, hkStepInfo& stepInfo)
{
    hkpWorldOperationUtil::splitSimulationIslands(world);
    if (stepInfo.m_startTime.val() >= m_maxAllowedTimeValue)
    {
        resetWorldTime(world, stepInfo);
    }
}

// 0x00E1FDC0
void hkpDefaultWorldMaintenanceMgr::performMaintenanceNoSplit(hkpWorld* world, hkStepInfo& stepInfo)
{
    if (stepInfo.m_startTime.val() >= m_maxAllowedTimeValue)
    {
        resetWorldTime(world, stepInfo);
    }
}

// 0x00E1FF80: the step moves back to the lowest allowed time; every time stamp (simulation, active bodies' swept
// transforms, agents) moves with it
void hkpDefaultWorldMaintenanceMgr::resetWorldTime(hkpWorld* world, hkStepInfo& stepInfo)
{
    hkStepInfo& worldStep = world->m_dynamicsStepInfo.m_stepInfo;
    const float deltaTime = stepInfo.m_endTime.val() - stepInfo.m_startTime.val();
    worldStep.m_startTime = m_minAllowedTimeValue;
    worldStep.m_endTime = m_minAllowedTimeValue + deltaTime;
    worldStep.m_deltaTime = deltaTime;
    worldStep.m_invDeltaTime = 1.0f / deltaTime;
    const float warpDeltaTime = worldStep.m_startTime.val() - stepInfo.m_startTime.val();
    hkpSimulation* simulation = world->m_simulation;
    simulation->m_currentTime += warpDeltaTime;
    simulation->m_currentPsiTime = worldStep.m_startTime.val();
    if (simulation->m_simulateUntilTime != -1.0f)
    {
        world->m_simulation->m_simulateUntilTime += warpDeltaTime;
    }
    for (int i = 0; i < world->m_activeSimulationIslands.getSize(); i++)
    {
        hkpSimulationIsland* island = world->m_activeSimulationIslands[i];
        for (int e = 0; e < island->m_entities.getSize(); e++)
        {
            // the swept transform's base time (w of the first center of mass)
            hkVector4f& centerOfMass0 = island->m_entities[e]->m_motion.m_motionState.getSweptTransform().m_centerOfMass0;
            centerOfMass0.m_quad.m128_f32[3] += warpDeltaTime;
        }
        hkpWorldAgentUtil::warpTime(island, stepInfo.m_endTime.val(), worldStep.m_endTime.val(), *world->m_collisionInput);
    }
    world->m_simulation->warpTime(warpDeltaTime);
    stepInfo = worldStep;
    world->m_collisionInput->m_stepInfo = worldStep;
}
