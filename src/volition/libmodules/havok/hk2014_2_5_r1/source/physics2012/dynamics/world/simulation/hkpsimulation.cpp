// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/simulation/hkpsimulation.cpp
//
// Havok 2014.2.5 (havok_impl.md HK7): the discrete simulation, from rfg.exe (the base of hkpContinuousSimulation,
// which the game uses). integrate: actions, then every active island either through the solver or, without
// constraints, hkRigidMotionUtilApplyForcesAndStep; collide: the broadphase for every active island, then their
// agent tracks; advanceTime: the post simulation callbacks once the frame is reached.
//
// The monitor timers ("TtPhysics 2012", "StIntegrate", ...) are not reproduced.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/simulation/hkpsimulation.h"

#include <math.h>
#include <string.h>

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/config/hkoptionalcomponent.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/localarray/hklocalarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/localarray/hklocalbuffer.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/pointermap/hkmap.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/debugutil/memoryexceptiontestingutil/hkmemoryexceptiontestingutil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/swepttransform/hkswepttransformutil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/system/hkmemorysystem.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraintsolver/constraint/hkpconstraintqueryin.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpprocesscollisioninput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpprocesscollisionoutput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/contactmgr/hkpcontactmgr.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent3/machine/nn/hkpagentnnmachine.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent3/machine/nn/hkplinkedcollidable.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/broadphase/hkpbroadphase.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/broadphase/hkpbroadphasehandlepair.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/dispatch/broadphase/hkptypedbroadphasedispatcher.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/dispatch/hkpcollisiondispatcher.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/filter/hkpcollisionfilter.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/action/hkpaction.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/response/hkpsimplecollisionresponse.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/setup/hkpconstraintsolversetup.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/hkpentity.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/util/hkpentityaabbutil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/motion/util/hkprigidmotionutil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/hkpsimulationisland.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/maintenance/hkpworldmaintenancemgr.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/memory/hkpworldmemoryavailablewatchdog.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/util/hkpworldcallbackutil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/util/hkpworldmemoryutil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/util/hkpworldoperationutil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/dynamics/world/simulation/continuous/hkptoievent.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/solver/atom/hkpbuildjacobianfromatoms.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/solver/simpleconstraints/hkpsimpleconstraintutil.h"

// 0x03A2ADE8, constructed by a dynamic initializer: hkProductFeatures::initialize sets hkpSimulation::createDiscrete
// through it (hkpSimulation::create marks it used).
HK_OPTIONAL_COMPONENT_DEFINE(hkpSimulation, hkpSimulation::createDiscrete, hkpSimulation::create);

hkpSimulation* (*hkpSimulation::createDiscrete)(hkpWorld*) = nullptr;
hkpSimulation* (*hkpSimulation::createContinuous)(hkpWorld*) = nullptr;
hkpSimulation* (*hkpSimulation::createMultithreaded)(hkpWorld*) = nullptr;

namespace
{
// hkpWorld::unlockAndAttemptToExecutePendingOperations (inlined)
inline void hkSimulation_unlock(hkpWorld* world)
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

// the step from the current PSI time over physicsDeltaTime (hkStepInfo(start, end))
inline hkStepInfo hkSimulation_stepInfo(float start, float end)
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

// hkpSolverInfo::incrementDeactivationFlags (0x00DF5BB0, inlined). Flag 0 is xored with 1 and with 2 (kept).
inline void hkSimulation_incrementDeactivationFlags(hkpSolverInfo& si)
{
    si.m_deactivationIntegrateCounter++;
    const unsigned char counter = si.m_deactivationIntegrateCounter;
    if (((counter - 4) & 7) == 0)
    {
        si.m_deactivationNumInactiveFramesSelectFlag[0] ^= 1;
    }
    if ((counter & 7) == 0)
    {
        si.m_deactivationNumInactiveFramesSelectFlag[0] ^= 2;
    }
    if ((counter & 0xF) == 0)
    {
        si.m_deactivationIntegrateCounter = 0;
        si.m_deactivationNumInactiveFramesSelectFlag[1] = (unsigned char)(1 - si.m_deactivationNumInactiveFramesSelectFlag[1]);
    }
}

inline void hkSimulation_setWorldSteps(hkpWorld* world, const hkStepInfo& stepInfo)
{
    world->m_dynamicsStepInfo.m_stepInfo = stepInfo;
    world->m_collisionInput->m_stepInfo = stepInfo;
}

inline void hkSimulation_setSolverSteps(hkpWorld* world, const hkStepInfo& stepInfo)
{
    hkpSolverInfo& si = world->m_dynamicsStepInfo.m_solverInfo;
    si.m_deltaTime = si.m_invNumSteps * stepInfo.m_deltaTime.val();
    si.m_invDeltaTime = (float)si.m_numSteps * stepInfo.m_invDeltaTime.val();
}
}

// 0x00DEF310 (stub_impl.md S4 SD)
hkpSimulation::hkpSimulation(hkFinishLoadedObjectFlag flag)
    : hkReferencedObject(flag)
{
}

// 0x00E01390
hkpSimulation::hkpSimulation(hkpWorld* world)
{
    m_currentTime = 0.0f;
    m_currentPsiTime = 0.0f;
    m_world = world;
    m_simulateUntilTime = -1.0f;
    m_frameMarkerPsiSnap = 0.0001f; // 0x38D1B717
    m_previousStepResult = HK_STEP_RESULT_SUCCESS;
    m_lastProcessingStep.m_storage = COLLIDE;
    m_determinismCheckFrameCounter = 0;
}

// 0x00E01200
hkpSimulation::~hkpSimulation()
{
}

// 0x00E013E0
hkpSimulation* hkpSimulation::create(hkpWorld* world)
{
    HK_OPTIONAL_COMPONENT_MARK_USED(hkpSimulation);
    return new hkpSimulation(world);
}

// 0x00E01270: within m_frameMarkerPsiSnap of the PSI time the frame marker snaps to it; the time to advance to is
// the earlier of the two (the PSI time without a marker)
float hkpSimulation::snapSimulateTimeAndGetTimeToAdvanceTo()
{
    if (m_simulateUntilTime != -1.0f && fabsf(m_simulateUntilTime - m_currentPsiTime) < m_frameMarkerPsiSnap)
    {
        m_simulateUntilTime = m_currentPsiTime;
    }
    if (m_simulateUntilTime == -1.0f)
    {
        return m_currentPsiTime;
    }
    return m_simulateUntilTime <= m_currentPsiTime ? m_simulateUntilTime : m_currentPsiTime;
}

// 0x00E012F0: after a failed step only the failed stage and the ones behind it run
hkpStepResult hkpSimulation::stepDeltaTime(float physicsDeltaTime)
{
    if (m_previousStepResult == HK_STEP_RESULT_SUCCESS || m_previousStepResult == HK_STEP_RESULT_MEMORY_FAILURE_BEFORE_INTEGRATION)
    {
        integrate(physicsDeltaTime);
    }
    if (m_previousStepResult == HK_STEP_RESULT_SUCCESS || m_previousStepResult == HK_STEP_RESULT_MEMORY_FAILURE_DURING_COLLIDE)
    {
        collide();
    }
    if (m_previousStepResult == HK_STEP_RESULT_SUCCESS || m_previousStepResult == HK_STEP_RESULT_MEMORY_FAILURE_DURING_TOI_SOLVE)
    {
        advanceTime();
    }
    if (m_previousStepResult != HK_STEP_RESULT_SUCCESS && m_world->getMemoryWatchDog() != nullptr)
    {
        hkpWorldMemoryUtil::tryToRecoverFromMemoryErrors(m_world);
    }
    return (hkpStepResult)m_previousStepResult;
}

// 0x00E01430: the filter is reapplied to the whole world before the failed collide repeats
hkpStepResult hkpSimulation::reCollideAfterStepFailure()
{
    m_world->updateCollisionFilterOnWorld(HK_UPDATE_FILTER_ON_WORLD_FULL_CHECK, HK_UPDATE_COLLECTION_FILTER_PROCESS_SHAPE_COLLECTIONS);
    hkStepInfo stepInfo;
    if (m_previousStepResult == HK_STEP_RESULT_MEMORY_FAILURE_DURING_TOI_SOLVE)
    {
        stepInfo = hkSimulation_stepInfo(m_currentTime, m_currentPsiTime);
    }
    else
    {
        stepInfo = hkSimulation_stepInfo(m_currentPsiTime, m_currentPsiTime + m_physicsDeltaTime);
    }
    collideInternal(stepInfo);
    if (hkOutOfMemoryState == hkMemoryAllocator::MEMORY_STATE_OUT_OF_MEMORY)
    {
        return (hkpStepResult)m_previousStepResult;
    }
    if (m_previousStepResult == HK_STEP_RESULT_MEMORY_FAILURE_DURING_COLLIDE)
    {
        m_currentPsiTime = m_physicsDeltaTime + m_currentPsiTime;
        if (m_world->m_worldPostCollideListeners.getSize() != 0)
        {
            hkpWorldCallbackUtil::firePostCollideCallback(m_world, stepInfo);
        }
        m_lastProcessingStep.m_storage = COLLIDE;
    }
    m_previousStepResult = HK_STEP_RESULT_SUCCESS;
    return HK_STEP_RESULT_SUCCESS;
}

// 0x00E015A0
hkpStepResult hkpSimulation::advanceTime()
{
    m_currentTime = snapSimulateTimeAndGetTimeToAdvanceTo();
    if (m_simulateUntilTime <= m_currentTime && m_world->m_worldPostSimulationListeners.getSize() != 0)
    {
        hkpWorldCallbackUtil::firePostSimulationCallback(m_world);
    }
    m_previousStepResult = HK_STEP_RESULT_SUCCESS;
    return HK_STEP_RESULT_SUCCESS;
}

// 0x00E01640
void hkpSimulation::applyActions()
{
    hkpWorld* world = m_world;
    for (int i = 0; i < world->m_activeSimulationIslands.getSize(); i++)
    {
        hkpSimulationIsland* island = world->m_activeSimulationIslands[i];
        for (int a = 0; a < island->m_actions.getSize(); a++)
        {
            island->m_actions[a]->applyAction(m_world->m_dynamicsStepInfo.m_stepInfo);
        }
    }
}

// 0x00E01790
hkpStepResult hkpSimulation::collide()
{
    if (m_previousStepResult != HK_STEP_RESULT_SUCCESS)
    {
        return reCollideAfterStepFailure();
    }
    const hkStepInfo stepInfo = hkSimulation_stepInfo(m_currentPsiTime, m_currentPsiTime + m_physicsDeltaTime);
    collideInternal(stepInfo);
    if (hkOutOfMemoryState == hkMemoryAllocator::MEMORY_STATE_OUT_OF_MEMORY)
    {
        m_previousStepResult = HK_STEP_RESULT_MEMORY_FAILURE_DURING_COLLIDE;
        return HK_STEP_RESULT_MEMORY_FAILURE_DURING_COLLIDE;
    }
    m_currentPsiTime = m_currentPsiTime + m_physicsDeltaTime;
    m_world->checkConstraintsViolated();
    if (m_world->m_worldPostCollideListeners.getSize() != 0)
    {
        hkpWorldCallbackUtil::firePostCollideCallback(m_world, stepInfo);
    }
    m_lastProcessingStep.m_storage = COLLIDE;
    m_previousStepResult = HK_STEP_RESULT_SUCCESS;
    return HK_STEP_RESULT_SUCCESS;
}

// 0x00E01960: islands are integrated from the back; an island whose motions rested long enough (more than 5
// inactive frames) is marked inactive
hkpStepResult hkpSimulation::integrateInternal(const hkStepInfo& stepInfoIn)
{
    hkpWorld* world = m_world;
    world->m_dynamicsStepInfo.m_stepInfo = stepInfoIn;
    hkSimulation_setSolverSteps(world, stepInfoIn);
    hkpSolverInfo& si = world->m_dynamicsStepInfo.m_solverInfo;
    si.m_globalAccelerationPerSubStep.m_quad = _mm_mul_ps(_mm_set1_ps(si.m_deltaTime), world->m_gravity.m_quad);
    si.m_globalAccelerationPerStep.m_quad = _mm_mul_ps(_mm_set1_ps(stepInfoIn.m_deltaTime.val()), world->m_gravity.m_quad);

    {
        hkWorldMemoryAvailableWatchDog::MemUsageInfo memInfo;
        world->calcRequiredSolverBufferSize(memInfo);
        if (!hkMemorySystem::getInstance().solverCanAllocSingleBlock(memInfo.m_maxRuntimeBlockSize))
        {
            if (world->getMemoryWatchDog() == nullptr)
            {
                m_previousStepResult = HK_STEP_RESULT_MEMORY_FAILURE_BEFORE_INTEGRATION;
                return HK_STEP_RESULT_MEMORY_FAILURE_BEFORE_INTEGRATION;
            }
            hkpWorldMemoryUtil::checkMemoryForIntegration(world);
        }
    }

    // actions may add phantoms, not bodies
    m_world->m_criticalOperationsLockCount++;
    m_world->m_criticalOperationsLockCountForPhantoms--;
    applyActions();
    m_world->m_criticalOperationsLockCountForPhantoms++;
    hkSimulation_unlock(m_world);

    m_world->m_criticalOperationsLockCount++;
    hkSimulation_incrementDeactivationFlags(m_world->m_dynamicsStepInfo.m_solverInfo);

    world = m_world;
    hkpConstraintQueryIn constraintQueryIn(hkpBeginConstraints);
    constraintQueryIn.set(world->m_dynamicsStepInfo.m_solverInfo, stepInfoIn, world->m_violatedConstraintArray);

    hkArray<hkpSimulationIsland*, hkContainerHeapAllocator>& islands = m_world->m_activeSimulationIslands;
    for (int i = islands.getSize() - 1; i >= 0; i--)
    {
        world = m_world;
        hkpSimulationIsland* island = islands[i];
        int numInactiveFrames;
        if (island->m_constraintInfo.m_sizeOfSchemas == 0)
        {
            numInactiveFrames = hkRigidMotionUtilApplyForcesAndStep(world->m_dynamicsStepInfo.m_solverInfo, world->m_dynamicsStepInfo.m_stepInfo, world->m_dynamicsStepInfo.m_solverInfo.m_globalAccelerationPerStep, (hkpMotion* const*)island->m_entities.begin(), island->m_entities.getSize(), 0xE0); // the offset of hkpEntity::m_motion
            hkpEntityAabbUtil::entityBatchRecalcAabb(island->m_world->m_collisionInput, island->m_entities.begin(), island->m_entities.getSize());
        }
        else
        {
            numInactiveFrames = hkpConstraintSolverSetup::solve(world->m_dynamicsStepInfo.m_stepInfo, world->m_dynamicsStepInfo.m_solverInfo, constraintQueryIn, *island, nullptr, 0, island->m_entities.begin(), island->m_entities.getSize());
        }
        // NUM_INACTIVE_FRAMES_TO_DEACTIVATE
        if (numInactiveFrames > 5 && island->m_activeMark && island->m_world->m_wantDeactivation.m_bool)
        {
            hkpWorldOperationUtil::markIslandInactive(island->m_world, island);
        }
        if (m_world->m_islandPostIntegrateListeners.getSize() != 0)
        {
            hkpWorldCallbackUtil::fireIslandPostIntegrateCallback(m_world, island, stepInfoIn);
        }
    }
    hkSimulation_unlock(m_world);

    if (m_world->m_worldPostIntegrateListeners.getSize() != 0)
    {
        hkpWorldCallbackUtil::firePostIntegrateCallback(m_world, stepInfoIn);
    }
    return HK_STEP_RESULT_SUCCESS;
}

// The info the impulse limit breached listeners get for a TOI contact (hkpContactImpulseLimitBreachedListenerInfo's
// single element: type 2, isContact 1).
struct hkSimulation_SingleImpulseElem
{
    unsigned short m_type; // 0x00
    unsigned short m_isContact; // 0x02
    hkpConstraintInstance* m_constraintInstance; // 0x04
    hkpContactPointProperties* m_properties; // 0x08
    hkContactPoint* m_contactPoint; // 0x0C
};

// 0x00E01EE0: a TOI without the TOI solver: one contact impulse (friction and restitution of the contact, 0.1
// extra separating velocity); the bodies whose velocity the keyframed check marks are activated (not fixed or
// keyframed ones). toBeActivated has room for them (no capacity check).
void hkLs_doSimpleCollisionResponse(hkpWorld* world, const hkpToiEvent& event, float rotateNormal, hkArray<hkpEntity*, hkContainerHeapAllocator>& toBeActivated)
{
    hkpSimpleConstraintUtilCollideParams params;
    params.m_externalSeperatingVelocity = event.m_seperatingVelocity;
    hkpEntity* entityB = event.m_entities[1];
    hkpEntity* entityA = event.m_entities[0];
    params.m_extraSeparatingVelocity = 0.1f; // 0x3DCCCCCD
    params.m_extraSlope = rotateNormal;
    params.m_friction = (float)event.m_properties.m_friction;
    params.m_restitution = (float)event.m_properties.m_restitution * 0.0078125f; // 1 / 128
    if (event.m_properties.m_maxImpulse.m_value == 0)
    {
        params.m_maxImpulse = HK_REAL_MAX;
    }
    else
    {
        params.m_maxImpulse = (float)event.m_properties.m_maxImpulse;
    }

    hkpSimpleCollisionResponse::SolveSingleOutput output;
    hkpSimpleCollisionResponse::solveSingleContact(event.m_contactPoint, event.m_time, params, &entityA->m_motion, &entityB->m_motion, event.m_contactMgr, output);
    if (params.m_contactImpulseLimitBreached.m_bool)
    {
        hkSimulation_SingleImpulseElem info;
        info.m_type = 2;
        info.m_isContact = 1;
        info.m_constraintInstance = (hkpConstraintInstance*)((char*)event.m_contactMgr + 0x44); // hkpSimpleConstraintContactMgr::m_constraint
        info.m_properties = const_cast<hkpContactPointProperties*>(&event.m_properties);
        info.m_contactPoint = const_cast<hkContactPoint*>(&event.m_contactPoint);
        hkpWorldCallbackUtil::fireContactImpulseLimitBreached(world, reinterpret_cast<const hkpContactImpulseLimitBreachedListenerInfo*>(&info), 1);
        return;
    }

    const float vA = output.m_velocityKeyframedA;
    const float vB = output.m_velocityKeyframedB;
    const bool fixedA = entityA->m_motion.m_type.m_storage == hkpMotion::MOTION_FIXED || entityA->m_motion.m_type.m_storage == hkpMotion::MOTION_KEYFRAMED;
    const bool fixedB = entityB->m_motion.m_type.m_storage == hkpMotion::MOTION_FIXED || entityB->m_motion.m_type.m_storage == hkpMotion::MOTION_KEYFRAMED;
    hkpEntity** out = toBeActivated.begin();
    int& size = toBeActivated.m_size;
    if (vA < 0.0f)
    {
        if (vB < 0.0f)
        {
            if (!fixedA)
            {
                out[size++] = entityA;
            }
            if (!fixedB)
            {
                out[size++] = entityB;
            }
            return;
        }
        if (vB >= 0.0f)
        {
            out[size++] = fixedA ? entityB : entityA;
            return;
        }
    }
    if (vA >= 0.0f && vB < 0.0f)
    {
        out[size++] = fixedB ? entityA : entityB;
        return;
    }
    if (fixedA)
    {
        out[size++] = entityB;
    }
    else if (fixedB || !(vA > vB))
    {
        out[size++] = entityA;
    }
    else
    {
        out[size++] = entityB;
    }
}

// 0x00E02110: the world's maintenance (deactivation checks, island splits) runs first
hkpStepResult hkpSimulation::integrate(float physicsDeltaTime)
{
    hkStepInfo stepInfo = hkSimulation_stepInfo(m_currentPsiTime, m_currentPsiTime + physicsDeltaTime);
    m_physicsDeltaTime = physicsDeltaTime;
    hkSimulation_setWorldSteps(m_world, stepInfo);
    m_world->m_maintenanceMgr->performMaintenance(m_world, stepInfo);
    const hkpStepResult result = integrateInternal(stepInfo);
    m_previousStepResult = result;
    if (result == HK_STEP_RESULT_SUCCESS)
    {
        m_lastProcessingStep.m_storage = INTEGRATE;
    }
    return result;
}

// 0x00E02230: the entities' cached boxes (recalculated when invalid) into the broadphase; new pairs get agents
// (if 640 bytes per pair fit in the heap), removed pairs lose theirs
void hkpSimulation::collideEntitiesBroadPhaseDiscrete(hkpEntity** entities, int numEntities, hkpWorld* world)
{
    hkLocalArray<hkpBroadPhaseHandlePair> newPairs(world->m_broadPhaseUpdateSize);
    hkLocalArray<hkpBroadPhaseHandlePair> delPairs(world->m_broadPhaseUpdateSize);
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
            memcpy(&aabbs[i], &bv, sizeof(hkAabbUint32));
        }
        world->m_broadPhase->updateAabbsUint32(handles.begin(), aabbs.begin(), numEntities, newPairs, delPairs);
    }
    if (delPairs.getSize() + newPairs.getSize() > 0)
    {
        hkpTypedBroadPhaseDispatcher::removeDuplicates(newPairs, delPairs);
        world->m_broadPhaseDispatcher->removePairs(reinterpret_cast<hkpTypedBroadPhaseHandlePair*>(delPairs.begin()), delPairs.getSize());
        if (!hkMemorySystem::getInstance().heapCanAllocTotal(newPairs.getSize() * 640))
        {
            hkSetOutOfMemoryState(hkMemoryAllocator::MEMORY_STATE_OUT_OF_MEMORY);
            return;
        }
        const hkpCollidableCollidableFilter* filter = world->m_collisionFilter != nullptr ? static_cast<const hkpCollidableCollidableFilter*>(world->m_collisionFilter) : nullptr;
        world->m_broadPhaseDispatcher->addPairs(reinterpret_cast<hkpTypedBroadPhaseHandlePair*>(newPairs.begin()), newPairs.getSize(), filter);
    }
}

// 0x00E028A0: the broadphase of every active island, then the agent tracks with the second quality info (PSI)
void hkpSimulation::collideInternal(const hkStepInfo& stepInfoIn)
{
    hkSimulation_setWorldSteps(m_world, stepInfoIn);
    hkSimulation_setSolverSteps(m_world, stepInfoIn);

    m_world->m_criticalOperationsLockCount++;
    hkpWorld* world = m_world;
    for (int i = 0; i < world->m_activeSimulationIslands.getSize(); i++)
    {
        hkpSimulationIsland* island = world->m_activeSimulationIslands[i];
        collideEntitiesBroadPhaseDiscrete(island->m_entities.begin(), island->m_entities.getSize(), m_world);
        if (hkOutOfMemoryState == hkMemoryAllocator::MEMORY_STATE_OUT_OF_MEMORY)
        {
            hkSimulation_unlock(m_world);
            return;
        }
    }
    hkSimulation_unlock(m_world);

    m_world->m_criticalOperationsLockCount++;
    world = m_world;
    for (int i = 0; i < world->m_activeSimulationIslands.getSize(); i++)
    {
        hkpSimulationIsland* island = world->m_activeSimulationIslands[i];
        hkpProcessCollisionInput* input = m_world->m_collisionInput;
        input->m_collisionQualityInfo = input->m_dispatcher.val()->m_collisionQualityInfo + 1;
        input->m_createPredictiveAgents = 0;
        hkAgentNnMachine_ProcessTrack(island, island->m_narrowphaseAgentTrack, *input);
        hkAgentNnMachine_ProcessTrack(island, island->m_midphaseAgentTrack, *input);
        if (hkOutOfMemoryState == hkMemoryAllocator::MEMORY_STATE_OUT_OF_MEMORY)
        {
            break;
        }
        if (m_world->m_islandPostCollideListeners.getSize() != 0)
        {
            hkpWorldCallbackUtil::fireIslandPostCollideCallback(m_world, island, stepInfoIn);
        }
    }
    hkSimulation_unlock(m_world);
}

// 0x00E02C10: every agent of the entities once (agents between two of them only from the first); the TIM is reset
// before the processing function runs. rfg.exe never passes FIND_CONTACTS_EXTRA (only the vtable reaches
// collideEntitiesDiscrete), so the extra contact search (the body turned slightly around the contact normal) is not
// reproduced.
void hkpSimulation::processAgentsOfEntities(hkpEntity** entities, int numEntities, const hkpProcessCollisionInput& processInput, void (hkpSimulation::*processingFunction)(hkpAgentNnEntry*, const hkpProcessCollisionInput&, hkpProcessCollisionOutput&), FindContacts findExtraContacts)
{
    hkMap<unsigned long, unsigned long> processed; // keyed by collidable address
    if (numEntities != 0)
    {
        processed.reserve(numEntities);
    }
    hkpProcessCollisionOutput output(nullptr);
    output.m_toi.m_time = HK_REAL_MAX;
    output.m_potentialContacts = nullptr;
    hkArray<hkpLinkedCollidable::CollisionEntry, hkContainerHeapAllocator> entries;
    for (int e = 0; e < numEntities; e++)
    {
        hkpEntity* entity = entities[e];
        processed.insert((unsigned long)&entity->m_collidable, 0);
        entity->m_collidable.getCollisionEntriesSorted(entries);
        for (int i = 0; i < entries.getSize(); i++)
        {
            if (processed.isValid(processed.findKey((unsigned long)entries[i].m_partner)))
            {
                continue;
            }
            hkpAgentNnEntry* entry = entries[i].m_agentEntry;
            hkpEntity* entityA = static_cast<hkpEntity*>((hkpWorldObject*)entry->m_collidable[0]->getOwner());
            hkpEntity* entityB = static_cast<hkpEntity*>((hkpWorldObject*)entry->m_collidable[1]->getOwner());
            output.m_constraintOwner = entityA->m_motion.m_type.m_storage == hkpMotion::MOTION_FIXED ? entityB->m_simulationIsland : entityA->m_simulationIsland;
            hkAgentNnMachine_InvalidateTimInAgent(entry, processInput);
            (this->*processingFunction)(entry, processInput, output);
            if (hkOutOfMemoryState == hkMemoryAllocator::MEMORY_STATE_OUT_OF_MEMORY)
            {
                return;
            }
        }
    }
}

// 0x00E016A0: the agent with the second quality info, predictive when the agent's quality is continuous; new points
// go to the contact manager
void hkpSimulation::processAgentCollideDiscrete(hkpAgentNnEntry* entry, const hkpProcessCollisionInput& processInput, hkpProcessCollisionOutput& processOutput)
{
    processOutput.reset();
    hkpCollisionDispatcher* dispatcher = processInput.m_dispatcher.val();
    processInput.m_collisionQualityInfo = dispatcher->m_collisionQualityInfo + 1;
    const_cast<hkpProcessCollisionInput&>(processInput).m_createPredictiveAgents = dispatcher->m_collisionQualityInfo[(signed char)entry->m_collisionQualityIndex].m_useContinuousPhysics.val();
    hkAgentNnMachine_ProcessAgent(entry, processInput, processOutput, entry->m_contactMgr);
    if (hkOutOfMemoryState != hkMemoryAllocator::MEMORY_STATE_OUT_OF_MEMORY && !processOutput.isEmpty().m_bool)
    {
        entry->m_contactMgr->processContactImpl(*entry->m_collidable[0], *entry->m_collidable[1], processInput, processOutput);
    }
}

// 0x00E01370
void hkpSimulation::processAgentResetCollisionInformation(hkpAgentNnEntry* entry, const hkpProcessCollisionInput& processInput, hkpProcessCollisionOutput& processOutput)
{
    hkAgentNnMachine_InvalidateTimInAgent(entry, processInput);
}

// 0x00E03540
void hkpSimulation::collideEntitiesNarrowPhaseDiscrete(hkpEntity** entities, int numEntities, const hkpProcessCollisionInput& input, FindContacts findExtraContacts)
{
    processAgentsOfEntities(entities, numEntities, input, &hkpSimulation::processAgentCollideDiscrete, findExtraContacts);
}

// 0x00E03570: RESET_TIM invalidates the agents' TIMs, RESET_AABB the cached boxes (no TOIs in the discrete simulation)
void hkpSimulation::resetCollisionInformationForEntities(hkpEntity** entities, int numEntities, hkpWorld* world, ResetCollisionInformation resetInfo)
{
    if (resetInfo & RESET_TIM)
    {
        processAgentsOfEntities(entities, numEntities, *world->m_collisionInput, &hkpSimulation::processAgentResetCollisionInformation, FIND_CONTACTS_DEFAULT);
    }
    if (resetInfo & RESET_AABB)
    {
        hkpEntityAabbUtil::entityBatchInvalidateAabb(entities, numEntities);
    }
}

// 0x00E035B0: mode bit 1 steps the entities again from the current time to the PSI time, bit 2 the broadphase, bit 4
// the narrowphase (without it a reintegration resets the agents' TIMs)
void hkpSimulation::reintegrateAndRecollideEntities(hkpEntity** entities, int numEntities, hkpWorld* world, int reintegrateRecollideMode)
{
    world->m_criticalOperationsLockCount++;
    const float currentTime = world->getCurrentTime();
    const hkStepInfo stepInfo = hkSimulation_stepInfo(currentTime, world->getCurrentPsiTime());
    world->m_collisionInput->m_stepInfo = stepInfo;
    if (reintegrateRecollideMode & 1)
    {
        for (int i = 0; i < numEntities; i++)
        {
            if (entities[i]->m_motion.m_type.m_storage != hkpMotion::MOTION_FIXED)
            {
                hkSweptTransformUtil::backStepMotionState(world->getCurrentTime(), entities[i]->m_motion.m_motionState);
            }
        }
        hkRigidMotionUtilStep(stepInfo, (hkpMotion* const*)entities, numEntities, 0xE0); // the offset of hkpEntity::m_motion
        hkpEntityAabbUtil::entityBatchRecalcAabb(world->m_collisionInput, entities, numEntities);
    }
    if (reintegrateRecollideMode & 2)
    {
        collideEntitiesBroadPhaseDiscrete(entities, numEntities, world);
    }
    if (reintegrateRecollideMode & 4)
    {
        collideEntitiesNarrowPhaseDiscrete(entities, numEntities, *world->m_collisionInput, FIND_CONTACTS_DEFAULT);
    }
    else if (reintegrateRecollideMode & 1)
    {
        resetCollisionInformationForEntities(entities, numEntities, world, RESET_TIM);
    }
    hkSimulation_unlock(world);
}

// 0x00E03730: with a copy of the world's collision input over the given step
void hkpSimulation::collideEntitiesDiscrete(hkpEntity** entities, int numEntities, hkpWorld* world, const hkStepInfo& stepInfo, FindContacts findExtraContacts)
{
    hkpProcessCollisionInput input;
    input = *world->m_collisionInput;
    input.m_stepInfo = stepInfo;
    world->m_criticalOperationsLockCount++;
    collideEntitiesBroadPhaseDiscrete(entities, numEntities, world);
    collideEntitiesNarrowPhaseDiscrete(entities, numEntities, input, findExtraContacts);
    hkSimulation_unlock(world);
}

// 0x00DEF320, 0x00DEF330, 0x00DEF340, 0x00DEF350: multithreading is not part of the discrete simulation
hkpStepResult hkpSimulation::stepBeginSt(hkJobQueue* queue, float physicsDeltaTime)
{
    return HK_STEP_RESULT_SUCCESS;
}

hkpStepResult hkpSimulation::finishMtStep(hkJobQueue* queue, hkThreadPool* threadPool)
{
    return HK_STEP_RESULT_SUCCESS;
}

void hkpSimulation::getMultithreadConfig(hkpMultithreadConfig& config)
{
}

void hkpSimulation::setMultithreadConfig(const hkpMultithreadConfig& config, hkJobQueue* queue)
{
}

// 0x00DEF360, 0x00DEF370, 0x00DEF380, 0x00DEF390
void hkpSimulation::assertThereIsNoCollisionInformationForEntities(hkpEntity** entities, int numEntities, hkpWorld* world)
{
}

void hkpSimulation::removeCollisionInformationForAgent(hkpAgentNnEntry* agent)
{
}

void hkpSimulation::assertThereIsNoCollisionInformationForAgent(hkpAgentNnEntry* agent)
{
}

void hkpSimulation::warpTime(float deltaTime)
{
}
