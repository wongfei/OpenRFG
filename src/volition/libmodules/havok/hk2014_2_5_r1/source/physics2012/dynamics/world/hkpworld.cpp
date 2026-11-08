// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/hkpworld.cpp
//
// Havok 2014.2.5: the world, from rfg.exe. The ray casts came with HK5, linearCast/getClosestPoints/getPenetrations
// with HK6, the rest (constructor and destructor, object lists, listeners, filter updates, stepping) with HK7.
//
// The shipped build records monitor timers (TLS hkMonitorStream) in most of these functions; they are not reproduced.
// Every critical operation ends with the inlined hkpWorld::unlockAndAttemptToExecutePendingOperations
// (hkWorld_unlock below).

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/hkpworld.h"

#include <math.h>
#include <string.h>
#include <xmmintrin.h>

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/localarray/hklocalarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/swepttransform/hkswepttransformutil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/router/hkmemoryrouter.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/thread/criticalsection/hkcriticalsection.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraint/data/hkpconstraintdata.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraintsolver/constraint/hkpconstraintqueryin.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraintsolver/solve/hkpsolverinfo.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcollidable.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpcollisionagentconfig.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpprocesscollisioninput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/query/hkpcdbodypaircollector.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/query/hkpcdpointcollector.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/util/null/hkpnullagent.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent3/machine/nn/hkpagentnnmachine.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent3/machine/nn/hkplinkedcollidable.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/broadphase/hkpbroadphase.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/broadphase/hkpbroadphasehandlepair.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/dispatch/broadphase/hkptypedbroadphasedispatcher.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/dispatch/broadphase/hkptypedbroadphasehandle.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/dispatch/broadphase/hkptypedbroadphasehandlepair.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/dispatch/contactmgr/hkpnullcontactmgrfactory.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/dispatch/hkpcollisiondispatcher.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/filter/defaultconvexlist/hkpdefaultconvexlistfilter.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/filter/hkpcollidablecollidablefilter.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/filter/hkpcollisionfilter.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/filter/null/hkpnullcollisionfilter.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/query/castutil/hkplinearcastinput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/query/castutil/hkpsimpleworldraycaster.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/query/castutil/hkpworldlinearcaster.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/query/castutil/hkpworldraycaster.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/hkpshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/util/welding/hkpweldingutility.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/action/hkpaction.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/collide/contactlistener/util/hkpcollisioncallbackutil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/collide/deprecated/hkpreportcontactmgr.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/collide/hkpsimpleconstraintcontactmgr.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/breakable/hkpbreakableconstraintdata.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/hkpconstraintinstance.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/hkpentity.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/hkprigidbody.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/hkprigidbodycinfo.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/util/hkpentitycallbackutil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/phantom/hkpphantom.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/phantom/hkpphantombroadphaselistener.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/broadphaseborder/hkpbroadphaseborder.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/extensions/hkpworldextension.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/hkpsimulationisland.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/maintenance/default/hkpdefaultworldmaintenancemgr.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/simulation/continuous/hkpcontinuoussimulation.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/simulation/hkpsimulation.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/util/broadphase/hkpbroadphaseborderlistener.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/util/broadphase/hkpentityentitybroadphaselistener.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/util/hkpworldagentutil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/util/hkpworldcallbackutil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/util/hkpworldmemoryutil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/util/hkpworldoperationqueue.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/util/hkpworldoperationutil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/gjk/hkpgskexact.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/dynamics/world/simulation/continuous/toiresourcemgr/hkptoiresourcemgr.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/solver/atom/hkpbuildjacobianfromatoms.h"

// The violated constraint buffer (hkpsolverinfo.h in the 2014.1 SDK; not in rfg.pdb's headers), size 0x204. The
// solver appends to it; hkpWorld::checkConstraintsViolated reports and empties it.
struct hkpViolatedConstraintArray
{
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpViolatedConstraintArray);
    enum { CAPACITY = 128 };
    hkpViolatedConstraintArray() : m_nextFreeElement(0) {}

    unsigned int m_nextFreeElement; // 0x00 (may exceed CAPACITY; the overflow is dropped)
    hkpConstraintInstance* m_constraints[CAPACITY]; // 0x04
};

// The multithreaded simulation's per-thread data (the multithreaded simulation headers; layout from rfg.pdb), size
// 0x120. Only its construction and destruction are in scope: the world always allocates one.
struct alignas(16) hkpMtThreadStructure
{
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpMtThreadStructure);
    // the binary only sets the query's begin function, the collision input's flag and spare sector
    hkpMtThreadStructure()
        : m_constraintQueryIn(hkpBeginConstraints)
    {
    }

    hkpWorld* m_world; // 0x00
    hkpConstraintQueryIn m_constraintQueryIn; // 0x10
    hkpProcessCollisionInput m_collisionInput; // 0x90
    void* m_simulation; // 0x110 (hkPadSpu<hkpMultiThreadedSimulation*>)
    hkpWorldDynamicsStepInfo* m_dynamicsStepInfo; // 0x114
    float m_tolerance; // 0x118
    void* m_weldingTable; // 0x11C
};

void hkWorld_setupContactMgrFactories(hkpWorld* world, hkpCollisionDispatcher* dis);

namespace
{
// hkpWorldObject::BroadPhaseType
const int HK_WORLD_BROAD_PHASE_PHANTOM = 2;

// hkpWorld::unlockAndAttemptToExecutePendingOperations (inlined everywhere)
inline void hkWorld_unlock(hkpWorld* world)
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

inline const hkpCollidableCollidableFilter* hkWorld_filter(const hkpWorld* world)
{
    return world->m_collisionFilter != nullptr ? static_cast<const hkpCollidableCollidableFilter*>(world->m_collisionFilter) : nullptr;
}

inline hkpCollidable* hkWorld_owner(hkpBroadPhaseHandle* handle)
{
    return static_cast<hkpCollidable*>(static_cast<hkpTypedBroadPhaseHandle*>(handle)->getOwner());
}

// Scalar SSE arithmetic (the binary's single-lane divss/mulss/subss, kept in their order under /fp:fast).
inline float hkWorld_mul(float a, float b) { return _mm_cvtss_f32(_mm_mul_ss(_mm_set_ss(a), _mm_set_ss(b))); }
inline float hkWorld_div(float a, float b) { return _mm_cvtss_f32(_mm_div_ss(_mm_set_ss(a), _mm_set_ss(b))); }
inline float hkWorld_sub(float a, float b) { return _mm_cvtss_f32(_mm_sub_ss(_mm_set_ss(a), _mm_set_ss(b))); }

// The gravity's length (hkVector4f::length<3> at 23 bits: (y + x) + z, refined rsqrt times the square, 0 for a zero
// vector); 9.81 (0x411CF5C3) when there is none
float hkWorld_gravityLength(const hkVector4f& gravity)
{
    using namespace hkGskExact;
    const Q len2 = hadd3(mul(gravity.m_quad, gravity.m_quad));
    const Q le0 = _mm_cmple_ps(len2, _mm_setzero_ps());
    const float len = lane0(_mm_andnot_ps(le0, mul(invSqrt23(len2), len2)));
    if (len == 0.0f)
    {
        return 9.81f;
    }
    return len;
}

// The deactivation thresholds of the six deactivation classes: (velocity, relative velocity) at a 1 m/s^2 world.
// Classes 0 and 1 (invalid, off) get 2^-23 (0x34000000) and 0.
const float HK_WORLD_DEACTIVATION_VELOCITY[6] = { 1.1920929e-7f, 1.1920929e-7f, 0.01f, 0.017f, 0.02f, 0.025f };
const float HK_WORLD_DEACTIVATION_RELATIVE[6] = { 0.0f, 0.0f, 0.08f, 0.2f, 0.3f, 0.4f };
}

// 0x03A29D72: off (rfg.exe never sets it)
hkBool hkpWorld::m_forceMultithreadedSimulation = false;

// 0x00DEAAB0: leaves everything uninitialized
hkpWorldDynamicsStepInfo::hkpWorldDynamicsStepInfo()
{
}

// 0x00DE7220
hkpWorld::hkpWorld(const hkpWorldCinfo& infoBase, unsigned int sdkversion)
{
    // The original copies the cinfo (hkpWorldCinfo copy constructor 0x00DEBB50) and reads the copy; reading infoBase
    // directly gives the same values.
    const hkpWorldCinfo& info = infoBase;

    // the member's constructor (+0x50 = 0, 0x00DE7262): hkRefPtr is a seam without one, and the assignment below
    // releases what is there (HK9: a reused heap block made it release garbage)
    *(hkWorldMemoryAvailableWatchDog**)&m_memoryWatchDog = nullptr;
    m_lastEntityUid = 0xFFFFFFFF;
    m_lastConstraintUid = 0xFFFFFFFF;
    m_isLocked = 0;
    m_assertOnRunningOutOfSolverMemory.m_bool = true;
    m_violatedConstraintArray = new hkpViolatedConstraintArray();
    m_pendingOperations = new hkpWorldOperationQueue(this);
    m_pendingOperationQueues = nullptr;
    m_pendingOperationsCount = 0;
    m_pendingBodyOperationsCount = 0;
    m_criticalOperationsLockCount = 0;
    m_criticalOperationsLockCountForPhantoms = 0;
    m_blockExecutingPendingOperations.m_bool = false;
    m_criticalOperationsAllowed.m_bool = true;
    m_broadPhaseExtents[0] = info.m_broadPhaseWorldAabb.m_min;
    m_modifyConstraintCriticalSection = nullptr;
    m_propertyMasterLock = nullptr;
    m_islandDirtyListCriticalSection = nullptr;
    m_pendingOperationQueueCount = 1;
    m_destructionWorld = nullptr;
    m_broadPhaseExtents[1] = info.m_broadPhaseWorldAabb.m_max;
    m_broadPhaseNumMarkers = info.m_broadPhaseNumMarkers;
    m_broadPhaseType = info.m_broadPhaseType;

    // BROADPHASE_TYPE_SAP 0, TREE 1, HYBRID 2: the sweep and prune unless tree only, wrapped by the tree for 1 and 2
    m_broadPhase = nullptr;
    if (m_broadPhaseType.m_storage != 1)
    {
        m_broadPhase = hkpBroadPhase::s_createSweepAndPruneBroadPhaseFunction(m_broadPhaseExtents[0], m_broadPhaseExtents[1], m_broadPhaseNumMarkers);
    }
    if (m_broadPhaseType.m_storage == 1 || m_broadPhaseType.m_storage == 2)
    {
        hkpBroadPhase* sap = m_broadPhase;
        m_broadPhase = hkpBroadPhase::s_createTreeBroadPhaseFunction(sap);
        if (sap != nullptr)
        {
            sap->removeReference();
        }
    }
    m_broadPhaseQuerySize = info.m_broadPhaseQuerySize;
    m_broadPhaseUpdateSize = info.m_broadPhaseQuerySize / 2;
    m_sizeOfToiEventQueue = info.m_sizeOfToiEventQueue;

    // The binary only reports a version mismatch (hkErrorFwd::messageError 0x53C94B42) and leaves the constructor;
    // the message is not reproduced. 20140205 (0x13350AD) is Havok 2014.2.5.
    if (sdkversion != 20140205)
    {
        return;
    }

    m_gravity = info.m_gravity;
    hkpWeldingUtility::initWeldingTable(info.m_snapCollisionToConvexEdgeThreshold, info.m_snapCollisionToConcaveEdgeThreshold);
    // Original quirk kept: both thresholds get the convex one
    m_snapCollisionToConvexEdgeThreshold = info.m_snapCollisionToConvexEdgeThreshold;
    m_snapCollisionToConcaveEdgeThreshold = info.m_snapCollisionToConvexEdgeThreshold;
    m_toiCollisionResponseRotateNormal = info.m_toiCollisionResponseRotateNormal;
    m_deactivationReferenceDistance = info.m_deactivationReferenceDistance;
    m_useCompoundSpuElf = info.m_useCompoundSpuElf;
    m_enableToiWeldRejection = info.m_enableToiWeldRejection;
    m_shouldActivateOnRigidBodyTransformChange = info.m_shouldActivateOnRigidBodyTransformChange;
    m_processToisMultithreaded = info.m_processToisMultithreaded;
    m_maxSectorsPerMidphaseCollideTask = info.m_maxSectorsPerMidphaseCollideTask;
    m_maxSectorsPerNarrowphaseCollideTask = info.m_maxSectorsPerNarrowphaseCollideTask;
    m_maxNumToiCollisionPairsSinglethreaded = info.m_maxNumToiCollisionPairsSinglethreaded;
    m_maxEntriesPerToiMidphaseCollideTask = info.m_maxEntriesPerToiMidphaseCollideTask;
    m_maxEntriesPerToiNarrowphaseCollideTask = info.m_maxEntriesPerToiNarrowphaseCollideTask;

    // The solver parameters (the friction tau is half the tau)
    {
        hkpSolverInfo& si = m_dynamicsStepInfo.m_solverInfo;
        const float tau = info.m_solverTau;
        const float damp = info.m_solverDamp;
        const float frictionTau = hkWorld_mul(tau, 0.5f);
        const float invDamp = hkWorld_div(1.0f, damp);
        si.m_dampDivFrictionTau = hkWorld_div(damp, frictionTau);
        si.m_damping = damp;
        si.m_frictionTauDivDamp = hkWorld_mul(invDamp, frictionTau);
        si.m_contactRestingVelocity = info.m_contactRestingVelocity;
        si.m_frictionTau = frictionTau;
        const float dampDivTau = hkWorld_div(damp, tau);
        const float tauDivDamp = hkWorld_mul(invDamp, tau);
        si.m_numSteps = info.m_solverIterations;
        si.m_maxConstraintViolationSqrd = hkWorld_mul(info.m_maxConstraintViolation, info.m_maxConstraintViolation);
        si.m_invNumSteps = hkWorld_div(1.0f, (float)info.m_solverIterations);
        si.m_dampDivTau = dampDivTau;
        si.m_tauDivDamp = tauDivDamp;
        si.m_tau = tau;
        si.m_integrateVelocityFactor.m_quad = _mm_set1_ps(tauDivDamp);
        si.m_invIntegrateVelocityFactor.m_quad = _mm_set1_ps(dampDivTau);
        si.m_numMicroSteps = info.m_solverMicrosteps;
        si.m_invNumMicroSteps = hkWorld_div(1.0f, (float)info.m_solverMicrosteps);
        si.m_forceCoherentConstraintOrderingInSolver = info.m_forceCoherentConstraintOrderingInSolver;
        si.m_deactivationNumInactiveFramesSelectFlag[0] = info.m_deactivationNumInactiveFramesSelectFlag0;
        si.m_deactivationNumInactiveFramesSelectFlag[1] = info.m_deactivationNumInactiveFramesSelectFlag1;
        si.m_deactivationIntegrateCounter = info.m_deactivationIntegrateCounter;
        si.m_deltaTime = 0.0f;
        si.m_invDeltaTime = 0.0f;

        // The deactivation classes, scaled by the gravity length. 0.016 (0x3C83126F) is the reference step,
        // 0.1 (0x3DCCCCCD) the angular factor; a class without relative velocity never sleeps relatively
        // (2.1267625e37, 0x7D7FFFEE).
        const float g = hkWorld_gravityLength(m_gravity);
        const float angularScale = hkWorld_mul(g, 0.1f);
        const float d = info.m_deactivationReferenceDistance;
        const float maxDist0 = hkWorld_mul(d, d);
        const float d4 = hkWorld_mul(d, 4.0f);
        const float maxDist1 = hkWorld_mul(d4, d4);
        const float d2 = hkWorld_mul(d, 2.0f);
        const float maxRot0 = hkWorld_mul(d2, d2);
        const float d8 = hkWorld_mul(d, 8.0f);
        const float maxRot1 = hkWorld_mul(d8, d8);
        for (int i = 0; i < 6; i++)
        {
            const float v = HK_WORLD_DEACTIVATION_VELOCITY[i];
            const float w = HK_WORLD_DEACTIVATION_RELATIVE[i];
            hkpSolverInfo::DeactivationInfo& di = si.m_deactivationInfo[i];
            const float vg = hkWorld_mul(v, g);
            const float linInv = hkWorld_div(1.0f, vg);
            const float stepFactor = hkWorld_mul(si.m_invNumSteps, 0.016f);
            di.m_linearVelocityThresholdInv = linInv;
            di.m_slowObjectVelocityMultiplier = hkWorld_sub(1.0f, hkWorld_mul(hkWorld_mul(hkWorld_mul(w, g), linInv), stepFactor));
            di.m_angularVelocityThresholdInv = hkWorld_div(1.0f, hkWorld_mul(vg, angularScale));
            di.m_relativeSleepVelocityThreshold = w > 0.0f ? hkWorld_div(hkWorld_mul(si.m_invNumSteps, 0.016f), w) : 2.1267625e37f;
            di.m_maxDistSqrd[0] = maxDist0;
            di.m_maxDistSqrd[1] = maxDist1;
            di.m_maxRotSqrd[0] = maxRot0;
            di.m_maxRotSqrd[1] = maxRot1;
        }
    }

    m_memoryWatchDog = info.m_memoryWatchDog;
    m_wantSimulationIslands = info.m_enableSimulationIslands;
    m_wantDeactivation = info.m_enableDeactivation;
    // no deactivation without islands
    if (!m_wantSimulationIslands.m_bool && m_wantDeactivation.m_bool)
    {
        m_wantDeactivation.m_bool = false;
    }
    m_processActionsInSingleThread = info.m_processActionsInSingleThread;
    m_allowIntegrationOfIslandsWithoutConstraintsInASeparateJob = info.m_allowIntegrationOfIslandsWithoutConstraintsInASeparateJob;

    // The broadphase dispatcher and its listeners per handle type pair (1 entity, 2 phantom, 3 border)
    m_broadPhaseDispatcher = new hkpTypedBroadPhaseDispatcher();
    m_phantomBroadPhaseListener = new hkpPhantomBroadPhaseListener();
    m_entityEntityBroadPhaseListener = new hkpEntityEntityBroadPhaseListener(this);
    m_broadPhaseBorderListener = new hkpBroadPhaseBorderListener();
    m_broadPhaseDispatcher->m_broadPhaseListeners[1][2] = m_phantomBroadPhaseListener;
    m_broadPhaseDispatcher->m_broadPhaseListeners[2][1] = m_phantomBroadPhaseListener;
    m_broadPhaseDispatcher->m_broadPhaseListeners[2][2] = m_phantomBroadPhaseListener;
    m_broadPhaseDispatcher->m_broadPhaseListeners[1][1] = m_entityEntityBroadPhaseListener;
    m_broadPhaseDispatcher->m_broadPhaseListeners[1][3] = m_broadPhaseBorderListener;
    m_broadPhaseDispatcher->m_broadPhaseListeners[3][1] = m_broadPhaseBorderListener;
    m_broadPhaseDispatcher->m_broadPhaseListeners[2][3] = m_broadPhaseBorderListener;
    m_broadPhaseDispatcher->m_broadPhaseListeners[3][2] = m_broadPhaseBorderListener;
    m_broadPhaseDispatcher->m_broadPhaseListeners[3][3] = m_broadPhaseBorderListener;

    // The narrowphase dispatcher: null agents and the simple constraint contact manager by default
    {
        hkpSimpleConstraintContactMgr::Factory* factory = new hkpSimpleConstraintContactMgr::Factory(this);
        m_collisionDispatcher = new hkpCollisionDispatcher(hkpNullAgent::createNullAgent, factory);
        factory->removeReference();
    }

    if (info.m_collisionFilter.val() == nullptr)
    {
        m_collisionFilter = new hkpNullCollisionFilter();
    }
    else
    {
        m_collisionFilter = info.m_collisionFilter.val();
        m_collisionFilter->addReference();
        m_collisionFilter->init(this);
    }
    if (info.m_convexListFilter.val() == nullptr)
    {
        m_convexListFilter = new hkpDefaultConvexListFilter();
    }
    else
    {
        m_convexListFilter = info.m_convexListFilter.val();
        m_convexListFilter->addReference();
    }

    m_collisionInput = new hkpProcessCollisionInput();
    m_collisionInput->m_forceAcceptContactPoints = false;
    m_collisionInput->m_weldClosestPoints = false;
    m_collisionInput->m_dispatcher = m_collisionDispatcher;
    m_collisionInput->m_tolerance = info.m_collisionTolerance;
    m_collisionInput->m_filter = m_collisionFilter;
    m_collisionInput->m_convexListFilter = m_convexListFilter;
    m_contactPointGeneration = info.m_contactPointGeneration;
    m_collisionInput->m_allowToSkipConfirmedCallbacks = info.m_allowToSkipConfirmedCallbacks;
    m_collisionInput->m_config = new hkpCollisionAgentConfig();
    m_collisionInput->m_config->m_iterativeLinearCastEarlyOutDistance = info.m_iterativeLinearCastEarlyOutDistance;
    m_collisionInput->m_config->m_iterativeLinearCastMaxIterations = info.m_iterativeLinearCastMaxIterations;
    m_collisionInput->m_enableDeprecatedWelding = info.m_enableDeprecatedWelding;
    m_collisionInput->m_createPredictiveAgents = false;
    m_collisionInput->m_collisionQualityInfo = m_collisionDispatcher->m_collisionQualityInfo + 1;
    m_collisionInput->m_stepInfo.m_startTime = 0.0f;
    m_collisionInput->m_stepInfo.m_endTime = 0.0f;
    m_collisionInput->m_stepInfo.m_deltaTime = 0.0f;
    m_collisionInput->m_stepInfo.m_invDeltaTime = 0.0f;

    // The 32-bit AABB quantization of the world box: scale = 2147352576 / extent (0x4EFFFC00, 2^31 - 2^17, with the
    // refined reciprocal), low offset = -min, high offset = extent * 4.6566897e-10 (0x30000200, 2^-31 + 2^-45) - min;
    // w cleared. The broadphase gets the same.
    {
        using namespace hkGskExact;
        hkpCollisionInput::Aabb32Info& a32 = m_collisionInput->m_aabb32Info;
        const Q extent = sub(info.m_broadPhaseWorldAabb.m_max.m_quad, info.m_broadPhaseWorldAabb.m_min.m_quad);
        const Q xyz = laneMask(7);
        const Q scale = mul(recip23(extent), bits(0x4EFFFC00));
        const Q low = neg(info.m_broadPhaseWorldAabb.m_min.m_quad);
        const Q high = add(mul(extent, bits(0x30000200)), low);
        a32.m_bitScale.m_quad = _mm_and_ps(scale, xyz);
        a32.m_bitOffsetLow.m_quad = _mm_and_ps(low, xyz);
        a32.m_bitOffsetHigh.m_quad = _mm_and_ps(high, xyz);
        m_broadPhase->set32BitOffsetAndScale(a32.m_bitOffsetLow, a32.m_bitOffsetHigh, a32.m_bitScale);
    }
    hkWorld_setupContactMgrFactories(this, m_collisionDispatcher);

    // The simulation from its optional component; the multithreaded one (forced by m_forceMultithreadedSimulation)
    // also gets the constraint and property locks. An unknown type leaves m_simulation unset (as the binary).
    hkpWorldCinfo::SimulationType simulationType = (hkpWorldCinfo::SimulationType)info.m_simulationType.m_storage;
    if (m_forceMultithreadedSimulation.m_bool)
    {
        simulationType = (hkpWorldCinfo::SimulationType)3; // SIMULATION_TYPE_MULTITHREADED
    }
    m_simulationType.m_storage = simulationType;
    m_minDesiredIslandSize = 0;
    // Original bug kept: an unknown simulation type leaves m_simulation unset, and the write below goes through it.
    if (simulationType == 1) // SIMULATION_TYPE_DISCRETE
    {
        m_simulation = hkpSimulation::createDiscrete(this);
    }
    else if (simulationType == 2) // SIMULATION_TYPE_CONTINUOUS
    {
        m_simulation = hkpSimulation::createContinuous(this);
    }
    else if (simulationType == 3) // SIMULATION_TYPE_MULTITHREADED
    {
        m_modifyConstraintCriticalSection = new hkCriticalSection(4000); // spin count
        m_propertyMasterLock = new hkCriticalSection(4000);
        m_simulation = hkpSimulation::createMultithreaded(this);
        m_minDesiredIslandSize = info.m_minDesiredIslandSize;
    }
    m_islandDirtyListCriticalSection = new hkCriticalSection(4000);
    m_simulation->m_frameMarkerPsiSnap = info.m_frameMarkerPsiSnap;

    // The collision qualities: constraint priorities PSI 1, TOI 3, TOI_HIGHER 4, TOI_FORCED 5
    {
        hkpCollisionDispatcher::InitCollisionQualityInfo input;
        input.m_gravityLength = hkWorld_gravityLength(m_gravity);
        input.m_collisionTolerance = m_collisionInput->m_tolerance;
        input.m_minDeltaTime = info.m_expectedMinPsiDeltaTime;
        input.m_maxLinearVelocity = info.m_expectedMaxLinearVelocity;
        m_numToisTillAllowedPenetrationSimplifiedToi = info.m_numToisTillAllowedPenetrationSimplifiedToi;
        m_numToisTillAllowedPenetrationToi = info.m_numToisTillAllowedPenetrationToi;
        m_numToisTillAllowedPenetrationToiHigher = info.m_numToisTillAllowedPenetrationToiHigher;
        m_numToisTillAllowedPenetrationToiForced = info.m_numToisTillAllowedPenetrationToiForced;
        input.m_numToisTillAllowedPenetrationSimplifiedToi = m_numToisTillAllowedPenetrationSimplifiedToi;
        input.m_numToisTillAllowedPenetrationToi = m_numToisTillAllowedPenetrationToi;
        input.m_numToisTillAllowedPenetrationToiHigher = m_numToisTillAllowedPenetrationToiHigher;
        input.m_numToisTillAllowedPenetrationToiForced = m_numToisTillAllowedPenetrationToiForced;
        input.m_defaultConstraintPriority = 1;
        input.m_toiConstraintPriority = 3;
        input.m_toiHigherConstraintPriority = 4;
        input.m_toiForcedConstraintPriority = 5;
        // SIMULATION_TYPE_CONTINUOUS (2) and up; CONTACT_POINT_REJECT_MANY (2); CONTACT_POINT_REJECT_DUBIOUS (1) and up
        input.m_wantContinuousCollisionDetection = info.m_simulationType.m_storage >= 2;
        input.m_enableNegativeManifoldTims = info.m_contactPointGeneration.m_storage == 2;
        input.m_enableNegativeToleranceToCreateNon4dContacts = info.m_contactPointGeneration.m_storage >= 1;
        input.m_enableToiWeldRejection = info.m_enableToiWeldRejection;
        m_collisionDispatcher->initCollisionQualityInfo(input);
        m_collisionInput->m_collisionQualityInfo = m_collisionDispatcher->m_collisionQualityInfo + 1;
    }

    // The fixed island (storage index 0xFFFF), and the single active island when islands are off
    m_fixedIsland = new hkpSimulationIsland(this);
    m_fixedIsland->m_storageIndex = 0xFFFF;
    m_fixedIsland->m_activeMark = false;
    m_fixedIsland->m_isInActiveIslandsArray = false;
    if (!m_wantSimulationIslands.m_bool)
    {
        hkpSimulationIsland* island = new hkpSimulationIsland(this);
        m_activeSimulationIslands.pushBack(island);
        island->m_storageIndex = 0;
    }

    // The fixed rigid body (no shape, no mass) that constraints use for null bodies
    {
        hkpRigidBodyCinfo rbci;
        rbci.m_motionType.m_storage = hkpMotion::MOTION_FIXED;
        rbci.m_mass = 0.0f;
        m_fixedRigidBody = new hkpRigidBody(rbci);
        m_fixedRigidBody->m_npData = 0;
        addEntity(m_fixedRigidBody, HK_ENTITY_ACTIVATION_DO_ACTIVATE);
        m_fixedRigidBody->removeReference();
    }

    // 60 Hz until the first step (1/60 = 0.016666668, its inverse 59.999996)
    m_dynamicsStepInfo.m_stepInfo.m_startTime = 0.0f;
    m_dynamicsStepInfo.m_stepInfo.m_endTime = 0.016666668f;
    m_dynamicsStepInfo.m_stepInfo.m_deltaTime = 0.016666668f;
    m_dynamicsStepInfo.m_stepInfo.m_invDeltaTime = 59.999996f;
    m_collisionInput->m_dynamicsInfo = &m_dynamicsStepInfo;

    // BROADPHASE_BORDER_DO_NOTHING (3) leaves the world without a border
    if (info.m_broadPhaseBorderBehaviour.m_storage == 3)
    {
        m_broadPhaseBorder = nullptr;
    }
    else
    {
        m_broadPhaseBorder = new hkpBroadPhaseBorder(this, (hkpWorldCinfo::BroadPhaseBorderBehaviour)info.m_broadPhaseBorderBehaviour.m_storage, info.m_mtPostponeAndSortBroadPhaseBorderCallbacks);
    }

    hkpDefaultWorldMaintenanceMgr* maintenanceMgr = new hkpDefaultWorldMaintenanceMgr();
    m_maintenanceMgr = maintenanceMgr;
    maintenanceMgr->init(this);

    if (info.m_fireCollisionCallbacks.m_bool)
    {
        // hkpWorldExtension::requireExtension<hkpCollisionCallbackUtil> (0x00DED840), the same code
        hkpCollisionCallbackUtil::requireCollisionCallbackUtil(this);
    }
    if (info.m_simulationType.m_storage != 3)
    {
        m_multiThreadCheck.disableChecks();
    }
    m_multithreadedSimulationJobData = new hkpMtThreadStructure();
}

// 0x00DE34E0: a world from a packfile only gets its empty lists
hkpWorld::hkpWorld(hkFinishLoadedObjectFlag flag)
{
}

// 0x00DE5FA0
hkpWorld::~hkpWorld()
{
    if (m_multithreadedSimulationJobData != nullptr)
    {
        delete m_multithreadedSimulationJobData;
    }
    m_maintenanceMgr->removeReference();
    m_maintenanceMgr = nullptr;
    if (m_modifyConstraintCriticalSection != nullptr)
    {
        delete m_modifyConstraintCriticalSection;
        m_modifyConstraintCriticalSection = nullptr;
    }
    if (m_propertyMasterLock != nullptr)
    {
        delete m_propertyMasterLock;
        m_propertyMasterLock = nullptr;
    }
    m_memoryWatchDog = nullptr;

    removeAll();
    removeEntity(m_fixedRigidBody);
    m_fixedRigidBody = nullptr;
    hkpWorldOperationUtil::removeIslandFromDirtyList(this, m_fixedIsland);
    delete m_fixedIsland;
    m_fixedIsland = nullptr;
    if (m_islandDirtyListCriticalSection != nullptr)
    {
        delete m_islandDirtyListCriticalSection;
        m_islandDirtyListCriticalSection = nullptr;
    }

    const int numExtensions = m_worldExtensions.getSize();
    for (int i = 0; i < numExtensions; i++)
    {
        hkpWorldExtension* extension = m_worldExtensions[i];
        extension->performDetachments(this);
        extension->m_world = nullptr;
        extension->removeReference();
    }

    hkpWorldCallbackUtil::fireWorldDeleted(this);

    m_broadPhase->removeReference();
    m_broadPhase = nullptr;
    m_collisionDispatcher->removeReference();
    m_collisionDispatcher = nullptr;
    if (m_broadPhaseDispatcher != nullptr)
    {
        delete m_broadPhaseDispatcher;
    }
    if (m_broadPhaseBorder != nullptr)
    {
        m_broadPhaseBorder->removeReference();
    }
    if (m_phantomBroadPhaseListener != nullptr)
    {
        delete m_phantomBroadPhaseListener;
    }
    if (m_entityEntityBroadPhaseListener != nullptr)
    {
        delete m_entityEntityBroadPhaseListener;
    }
    if (m_broadPhaseBorderListener != nullptr)
    {
        delete m_broadPhaseBorderListener;
    }
    m_collisionFilter->removeReference();
    m_convexListFilter->removeReference();
    if (m_collisionInput->m_config != nullptr)
    {
        delete m_collisionInput->m_config;
    }
    if (m_collisionInput != nullptr)
    {
        delete m_collisionInput;
    }
    m_simulation->removeReference();
    if (m_pendingOperations != nullptr)
    {
        delete m_pendingOperations;
    }
    if (m_violatedConstraintArray != nullptr)
    {
        delete m_violatedConstraintArray;
    }
}

// 0x00DE5D60: every phantom, then every entity but the fixed rigid body in one batch
void hkpWorld::removeAll()
{
    // Original quirk kept: the batch is the phantom list itself, which the batch removal reorders as it goes
    removePhantomBatch(m_phantoms.begin(), m_phantoms.getSize());

    hkArray<hkpEntity*, hkContainerTempAllocator> entities;
    int numEntities = m_fixedIsland->m_entities.getSize();
    for (int i = 0; i < m_activeSimulationIslands.getSize(); i++)
    {
        numEntities += m_activeSimulationIslands[i]->m_entities.getSize();
    }
    for (int i = 0; i < m_inactiveSimulationIslands.getSize(); i++)
    {
        numEntities += m_inactiveSimulationIslands[i]->m_entities.getSize();
    }
    if (numEntities > 0)
    {
        entities.reserve(numEntities);
    }
    for (int i = 0; i < m_activeSimulationIslands.getSize(); i++)
    {
        hkpSimulationIsland* island = m_activeSimulationIslands[i];
        entities.append(island->m_entities.begin(), island->m_entities.getSize());
    }
    for (int i = 0; i < m_inactiveSimulationIslands.getSize(); i++)
    {
        hkpSimulationIsland* island = m_inactiveSimulationIslands[i];
        entities.append(island->m_entities.begin(), island->m_entities.getSize());
    }
    {
        const int fixedBodyIndex = entities.getSize() + m_fixedIsland->m_entities.indexOf(m_fixedRigidBody);
        entities.append(m_fixedIsland->m_entities.begin(), m_fixedIsland->m_entities.getSize());
        entities.removeAt(fixedBodyIndex);
    }
    removeEntityBatch(entities.begin(), entities.getSize());
    entities.clearAndDeallocate();

    if (!m_wantSimulationIslands.m_bool)
    {
        hkpSimulationIsland* island = m_activeSimulationIslands.back();
        hkpWorldOperationUtil::removeIslandFromDirtyList(this, island);
        delete m_activeSimulationIslands.back();
        m_activeSimulationIslands.popBack();
    }
    hkpWorldCallbackUtil::fireWorldRemoveAll(this);
}

// 0x00DE0E30
float hkpWorld::getCurrentTime() const
{
    return m_simulation->m_currentTime;
}

// 0x00DE0E40
float hkpWorld::getCurrentPsiTime() const
{
    return m_simulation->m_currentPsiTime;
}

// 0x00DE0F30: empty in the shipped build
void hkpWorld::checkDeterminismOfIslandBroadPhase(const hkpSimulationIsland* island)
{
}

// 0x00DE0F40: empty in the shipped build
void hkpWorld::checkDeterminism()
{
}

// 0x00DE0F60
void hkpWorld::internal_executePendingOperations()
{
    m_pendingOperationsCount = 0;
    m_pendingOperations->executeAllPending();
}

// 0x00DE0F80
void hkpWorld::internal_executePendingBodyOperations()
{
    m_pendingBodyOperationsCount = 0;
    m_pendingOperations->executeAllPendingBodyOperations();
}

// 0x00DE0FA0: the listeners see at most the buffer's capacity (128), then the buffer empties
void hkpWorld::checkConstraintsViolated()
{
    int num = (int)m_violatedConstraintArray->m_nextFreeElement;
    if (num > hkpViolatedConstraintArray::CAPACITY - 1)
    {
        num = hkpViolatedConstraintArray::CAPACITY;
    }
    for (int i = 0; i < num; i++)
    {
        hkpWorldCallbackUtil::fireConstraintViolated(this, m_violatedConstraintArray->m_constraints[i]);
    }
    m_violatedConstraintArray->m_nextFreeElement = 0;
}

// 0x00DE1000
void hkpWorld::queueOperation(const hkWorldOperation::BaseOperation& operation)
{
    m_pendingOperations->queueOperation(operation);
}

// 0x00DE1020
void hkpWorld::lock()
{
    hkReferencedObject::lockAll();
    m_isLocked++;
}

// 0x00DE1030
void hkpWorld::unlock()
{
    m_isLocked--;
    hkReferencedObject::unlockAll();
}

// 0x00DE1070
void hkpWorld::lockForIslandSplit(hkpSimulationIsland* island)
{
    if (m_modifyConstraintCriticalSection != nullptr)
    {
        m_modifyConstraintCriticalSection->enter();
    }
}

// 0x00DE22A0
void hkpWorld::unlockForIslandSplit(hkpSimulationIsland* island)
{
    if (m_modifyConstraintCriticalSection != nullptr)
    {
        m_modifyConstraintCriticalSection->leave();
    }
}

// 0x00DE2260
void hkpWorld::lockIslandForConstraintUpdate(hkpSimulationIsland* island)
{
    if (m_modifyConstraintCriticalSection != nullptr)
    {
        m_modifyConstraintCriticalSection->enter();
    }
}

// 0x00DE2280
void hkpWorld::unlockIslandForConstraintUpdate(hkpSimulationIsland* island)
{
    if (m_modifyConstraintCriticalSection != nullptr)
    {
        m_modifyConstraintCriticalSection->leave();
    }
}

// 0x00DE1A70: the simulation step, then the tree broadphase's own update (BROADPHASE_TYPE_TREE 2 / 32-bit tree 3 as
// the broadphase reports its type)
hkpStepResult hkpWorld::stepDeltaTime(float physicsDeltaTime)
{
    lock();
    hkpWorldMemoryUtil::watchHeapMemory(this);
    const hkpStepResult result = m_simulation->stepDeltaTime(physicsDeltaTime);
    if (m_broadPhase->getType() == 2)
    {
        hkpBroadPhase::s_updateTreeBroadPhaseFunction(m_broadPhase, physicsDeltaTime);
    }
    else if (m_broadPhase->getType() == 3)
    {
        hkpBroadPhase::s_updateTreeBroadPhaseFunction32(m_broadPhase, physicsDeltaTime);
    }
    unlock();
    return result;
}

// 0x00DE1BB0: the old border is deactivated before it is released
void hkpWorld::setBroadPhaseBorder(hkpBroadPhaseBorder* b)
{
    if (m_broadPhaseBorder != nullptr)
    {
        m_broadPhaseBorder->deactivate();
        m_broadPhaseBorder->removeReference();
    }
    m_broadPhaseBorder = b;
    if (b != nullptr)
    {
        b->addReference();
    }
}

// 0x00DE2830
hkWorldMemoryAvailableWatchDog* hkpWorld::getMemoryWatchDog() const
{
    return m_memoryWatchDog.val();
}

// 0x00DE2840
void hkpWorld::setMemoryWatchDog(hkWorldMemoryAvailableWatchDog* watchDog)
{
    m_memoryWatchDog = watchDog;
}

// 0x00DE1A40
hkpWorldExtension* hkpWorld::findWorldExtension(int id) const
{
    for (int i = 0; i < m_worldExtensions.getSize(); i++)
    {
        if (m_worldExtensions[i]->m_id == id)
        {
            return m_worldExtensions[i];
        }
    }
    return nullptr;
}

// 0x00DE3250
void hkpWorld::addWorldExtension(hkpWorldExtension* extension)
{
    m_worldExtensions.pushBack(extension);
    extension->addedToWorld(this);
}

// 0x00DE2480 (HK9): the extension is detached and released before it leaves the list
void hkpWorld::removeWorldExtension(hkpWorldExtension* extension)
{
    const int index = m_worldExtensions.indexOf(extension);
    extension->removedFromWorld(this);
    m_worldExtensions.removeAt(index);
}

// 0x00DE2530
void hkpWorld::getCinfo(hkpWorldCinfo& info) const
{
    info.m_gravity = m_gravity;
    info.m_broadPhaseQuerySize = m_broadPhaseQuerySize;
    info.m_broadPhaseWorldAabb.m_min = m_broadPhaseExtents[0];
    info.m_broadPhaseWorldAabb.m_max = m_broadPhaseExtents[1];
    info.m_collisionTolerance = m_collisionInput->m_tolerance;
    info.m_collisionFilter = m_collisionFilter;
    info.m_convexListFilter = m_convexListFilter;
    info.m_broadPhaseType = m_broadPhaseType;
    info.m_broadPhaseBorderBehaviour.m_storage = m_broadPhaseBorder == nullptr ? 3 : (signed char)m_broadPhaseBorder->m_type; // 3: BROADPHASE_BORDER_DO_NOTHING
    info.m_mtPostponeAndSortBroadPhaseBorderCallbacks.m_bool = m_broadPhaseBorder == nullptr ? false : m_broadPhaseBorder->m_postponeAndSortCallbacks.m_bool;
    info.m_expectedMaxLinearVelocity = m_collisionDispatcher->m_expectedMaxLinearVelocity;
    info.m_expectedMinPsiDeltaTime = m_collisionDispatcher->m_expectedMinPsiDeltaTime;
    // Original bug kept: the concave threshold overwrites the convex one, the cinfo's concave one stays as it was
    info.m_snapCollisionToConvexEdgeThreshold = m_snapCollisionToConvexEdgeThreshold;
    info.m_snapCollisionToConvexEdgeThreshold = m_snapCollisionToConcaveEdgeThreshold;
    info.m_enableToiWeldRejection = m_enableToiWeldRejection;
    info.m_memoryWatchDog = m_memoryWatchDog;
    info.m_broadPhaseNumMarkers = m_broadPhaseNumMarkers;
    info.m_sizeOfToiEventQueue = m_sizeOfToiEventQueue;
    info.m_contactPointGeneration = m_contactPointGeneration;
    info.m_allowToSkipConfirmedCallbacks = m_collisionInput->m_allowToSkipConfirmedCallbacks;
    const hkpSolverInfo& si = m_dynamicsStepInfo.m_solverInfo;
    info.m_contactRestingVelocity = si.m_contactRestingVelocity;
    info.m_solverTau = si.m_tau;
    info.m_solverDamp = si.m_damping;
    info.m_solverIterations = si.m_numSteps;
    info.m_solverMicrosteps = si.m_numMicroSteps;
    info.m_maxConstraintViolation = _mm_cvtss_f32(_mm_sqrt_ss(_mm_set_ss(si.m_maxConstraintViolationSqrd)));
    info.m_forceCoherentConstraintOrderingInSolver = si.m_forceCoherentConstraintOrderingInSolver;
    info.m_deactivationNumInactiveFramesSelectFlag0 = si.m_deactivationNumInactiveFramesSelectFlag[0];
    info.m_deactivationNumInactiveFramesSelectFlag1 = si.m_deactivationNumInactiveFramesSelectFlag[1];
    info.m_deactivationIntegrateCounter = si.m_deactivationIntegrateCounter;
    info.m_iterativeLinearCastEarlyOutDistance = m_collisionInput->m_config->m_iterativeLinearCastEarlyOutDistance;
    info.m_iterativeLinearCastMaxIterations = m_collisionInput->m_config->m_iterativeLinearCastMaxIterations;
    info.m_enableDeprecatedWelding = m_collisionInput->m_enableDeprecatedWelding;
    info.m_shouldActivateOnRigidBodyTransformChange = m_shouldActivateOnRigidBodyTransformChange;
    info.m_toiCollisionResponseRotateNormal = m_toiCollisionResponseRotateNormal;
    info.m_useCompoundSpuElf = m_useCompoundSpuElf;
    info.m_maxSectorsPerMidphaseCollideTask = m_maxSectorsPerMidphaseCollideTask;
    info.m_maxSectorsPerNarrowphaseCollideTask = m_maxSectorsPerNarrowphaseCollideTask;
    info.m_processToisMultithreaded = m_processToisMultithreaded;
    info.m_maxEntriesPerToiMidphaseCollideTask = m_maxEntriesPerToiMidphaseCollideTask;
    info.m_maxEntriesPerToiNarrowphaseCollideTask = m_maxEntriesPerToiNarrowphaseCollideTask;
    info.m_maxNumToiCollisionPairsSinglethreaded = m_maxNumToiCollisionPairsSinglethreaded;
    info.m_deactivationReferenceDistance = m_deactivationReferenceDistance;
    info.m_numToisTillAllowedPenetrationSimplifiedToi = m_numToisTillAllowedPenetrationSimplifiedToi;
    info.m_numToisTillAllowedPenetrationToi = m_numToisTillAllowedPenetrationToi;
    info.m_numToisTillAllowedPenetrationToiHigher = m_numToisTillAllowedPenetrationToiHigher;
    info.m_numToisTillAllowedPenetrationToiForced = m_numToisTillAllowedPenetrationToiForced;
    info.m_enableDeactivation = m_wantDeactivation;
    info.m_simulationType.m_storage = (signed char)m_simulationType.m_storage;
    info.m_frameMarkerPsiSnap = m_simulation->m_frameMarkerPsiSnap;
    info.m_enableSimulationIslands = m_wantSimulationIslands;
    info.m_processActionsInSingleThread = m_processActionsInSingleThread;
    info.m_allowIntegrationOfIslandsWithoutConstraintsInASeparateJob = m_allowIntegrationOfIslandsWithoutConstraintsInASeparateJob;
    info.m_minDesiredIslandSize = m_minDesiredIslandSize;
    info.m_fireCollisionCallbacks.m_bool = findWorldExtension(1001) != nullptr; // hkpCollisionCallbackUtil::s_id
}

// 0x00DE21B0: the solver buffer of every active island (as hkpConstraintSolverSetup::calcBufferSize), and for the
// continuous simulations at least the TOI resource manager's scratchpad
void hkpWorld::calcRequiredSolverBufferSize(hkWorldMemoryAvailableWatchDog::MemUsageInfo& infoOut)
{
    hkpWorldOperationUtil::cleanupDirtyIslands(this);
    infoOut.m_maxRuntimeBlockSize = 0;
    infoOut.m_sumRuntimeBlockSize = 0;
    infoOut.m_largestSimulationIsland = nullptr;
    int sum = 0;
    for (int i = m_activeSimulationIslands.getSize() - 1; i >= 0; i--)
    {
        hkpSimulationIsland* island = m_activeSimulationIslands[i];
        // 2 extra elements per 80 constraints; the temps buffer aligned to 16
        const int extra = (island->m_numConstraints / 80) * 2;
        const int size = ((((extra + 8) * 3 + island->m_constraintInfo.m_numSolverElemTemps) * 4 + 0x17) & ~0xF) + (extra + 0x12 + island->m_entities.getSize() * 8) * 0x10 + island->m_constraintInfo.m_sizeOfSchemas;
        sum += size;
        infoOut.m_sumRuntimeBlockSize = sum;
        if (infoOut.m_maxRuntimeBlockSize < size)
        {
            infoOut.m_maxRuntimeBlockSize = size;
            infoOut.m_largestSimulationIsland = island;
        }
    }
    if (m_simulationType.m_storage == 2 || m_simulationType.m_storage == 3)
    {
        int toiSize = static_cast<hkpContinuousSimulation*>(m_simulation)->m_toiResourceMgr->getScratchpadCapacity();
        if (toiSize < infoOut.m_sumRuntimeBlockSize)
        {
            toiSize = infoOut.m_sumRuntimeBlockSize;
        }
        infoOut.m_sumRuntimeBlockSize = toiSize;
    }
}

// 0x00DE8720
void hkpWorld::lockCriticalOperations()
{
    m_criticalOperationsLockCount++;
}

// 0x00DE8730
void hkpWorld::unlockCriticalOperations()
{
    m_criticalOperationsLockCount--;
}

// 0x00710FD0 (hkpworld.inl:127)
int hkpWorld::areCriticalOperationsLocked() const
{
    return m_criticalOperationsLockCount;
}

// 0x00DEA580
void hkpWorld::unlockAndAttemptToExecutePendingOperations()
{
    hkWorld_unlock(this);
}

// ---- entities

// 0x00DE13B0
hkpEntity* hkpWorld::addEntity(hkpEntity* entity, hkpEntityActivation initialActivationState)
{
    if (m_criticalOperationsLockCount != 0)
    {
        hkWorldOperation::AddEntity op;
        op.m_entity = entity;
        op.m_activation = initialActivationState;
        queueOperation(op);
        return nullptr;
    }
    entity->setCachedShapeData(this, entity->m_collidable.getShape());
    if (entity->m_collidable.m_motion == nullptr)
    {
        entity->m_collidable.m_motion = entity->getMotionState();
    }
    hkSweptTransformUtil::setTimeInformation(hkSimdFloat32::getConstant<HK_QUADREAL_0>(), hkSimdFloat32::getConstant<HK_QUADREAL_0>(), entity->m_motion.m_motionState);
    entity->m_motion.m_deactivationNumInactiveFrames[0] = 0;
    entity->m_motion.m_deactivationNumInactiveFrames[1] = 0;
    entity->m_uid = ++m_lastEntityUid;
    m_criticalOperationsAllowed.m_bool = false;
    entity->addReference();
    hkpWorldOperationUtil::addEntitySI(this, entity, initialActivationState);
    // the motion's inactive frame counters follow the world's select flags
    const hkpSolverInfo& si = m_dynamicsStepInfo.m_solverInfo;
    entity->m_motion.setWorldSelectFlagsNeg(si.m_deactivationNumInactiveFramesSelectFlag[0], si.m_deactivationNumInactiveFramesSelectFlag[1], si.m_deactivationIntegrateCounter);
    m_criticalOperationsLockCount++;
    m_criticalOperationsAllowed.m_bool = true;
    hkpWorldOperationUtil::addEntityBP(this, entity);
    hkpWorldCallbackUtil::fireEntityAdded(this, entity);
    hkpEntityCallbackUtil::fireEntityAdded(entity);
    hkWorld_unlock(this);
    return entity;
}

// 0x00DE1550
hkBool hkpWorld::removeEntity(hkpEntity* entity)
{
    if (m_criticalOperationsLockCount != 0)
    {
        hkWorldOperation::RemoveEntity op;
        op.m_entity = entity;
        queueOperation(op);
        return false;
    }
    m_criticalOperationsLockCount = 1;
    hkpWorldOperationUtil::removeEntityBP(this, entity);
    hkpWorldCallbackUtil::fireEntityRemoved(this, entity);
    hkpEntityCallbackUtil::fireEntityRemoved(entity);
    m_criticalOperationsAllowed.m_bool = false;
    hkpWorldOperationUtil::removeEntitySI(this, entity);
    if (entity->m_collidable.m_boundingVolumeData.m_childShapeAabbs != nullptr)
    {
        entity->m_collidable.m_boundingVolumeData.deallocate();
    }
    // a packfile object (no memory size) frees its internal arrays itself
    if ((entity->getMemorySizeAndFlags()) == 0)
    {
        entity->deallocateInternalArrays();
    }
    entity->removeReference();
    m_criticalOperationsAllowed.m_bool = true;
    hkWorld_unlock(this);
    return true;
}

// 0x00DE3E30: the non-fixed entities share one new island (split later); the broadphase takes them in one batch
void hkpWorld::addEntityBatch(hkpEntity* const* entityBatch, int numEntities, hkpEntityActivation initialActivationState)
{
    if (numEntities <= 0)
    {
        return;
    }
    if (m_criticalOperationsLockCount != 0)
    {
        hkWorldOperation::AddEntityBatch op;
        op.m_entities = const_cast<hkpEntity**>(entityBatch);
        op.m_numEntities = (unsigned short)numEntities;
        op.m_activation.m_storage = (unsigned char)initialActivationState;
        queueOperation(op);
        return;
    }
    m_criticalOperationsLockCount++;
    hkArray<hkpBroadPhaseHandle*, hkContainerTempAllocator> collList;
    collList.reserve(numEntities);
    hkArray<hkAabb, hkContainerHeapAllocator> aabbList;
    aabbList.reserve(numEntities);

    hkpSimulationIsland* gabriolaIsland;
    if (!m_wantSimulationIslands.m_bool)
    {
        gabriolaIsland = m_activeSimulationIslands[0];
        gabriolaIsland->m_entities.reserve(gabriolaIsland->m_entities.getSize() + numEntities);
    }
    else
    {
        gabriolaIsland = new hkpSimulationIsland(this);
        gabriolaIsland->m_splitCheckRequested = true;
        if (initialActivationState == HK_ENTITY_ACTIVATION_DO_ACTIVATE)
        {
            gabriolaIsland->m_storageIndex = (unsigned short)m_activeSimulationIslands.getSize();
            gabriolaIsland->m_isInActiveIslandsArray = true;
            gabriolaIsland->m_activeMark = true;
        }
        else
        {
            gabriolaIsland->m_isInActiveIslandsArray = false;
            gabriolaIsland->m_activeMark = false;
            gabriolaIsland->m_storageIndex = (unsigned short)m_inactiveSimulationIslands.getSize();
        }
    }

    const float extraRadius = m_collisionInput->m_tolerance * 0.5f;
    bool islandIsNotEmpty = false;
    const hkpSolverInfo& si = m_dynamicsStepInfo.m_solverInfo;
    for (int i = 0; i < numEntities; i++)
    {
        hkpEntity* entity = entityBatch[i];
        const hkpShape* shape = entity->m_collidable.getShape();
        entity->m_uid = ++m_lastEntityUid;
        entity->addReference();
        if (entity->m_collidable.m_motion == nullptr)
        {
            entity->m_collidable.m_motion = entity->getMotionState();
        }
        hkSweptTransformUtil::setTimeInformation(hkSimdFloat32::getConstant<HK_QUADREAL_0>(), hkSimdFloat32::getConstant<HK_QUADREAL_0>(), entity->m_motion.m_motionState);
        entity->m_motion.m_deactivationNumInactiveFrames[0] = 0;
        entity->m_motion.m_deactivationNumInactiveFrames[1] = 0;
        entity->m_world = this;
        entity->m_motion.setWorldSelectFlagsNeg(si.m_deactivationNumInactiveFramesSelectFlag[0], si.m_deactivationNumInactiveFramesSelectFlag[1], si.m_deactivationIntegrateCounter);
        if (entity->m_motion.m_type.m_storage == hkpMotion::MOTION_FIXED)
        {
            m_fixedIsland->internalAddEntity(entity);
        }
        else
        {
            islandIsNotEmpty = true;
            gabriolaIsland->internalAddEntity(entity);
        }
        // the bounding volume is invalid until the broadphase sets it
        entity->m_collidable.m_boundingVolumeData.m_min[0] = 1;
        entity->m_collidable.m_boundingVolumeData.m_max[0] = 0;
        if (shape != nullptr)
        {
            hkAabb& aabb = aabbList.expandOne();
            entity->m_collidable.getShape()->getAabb(entity->m_collidable.getTransform(), extraRadius, aabb);
            collList.pushBack(&entity->m_collidable.m_broadPhaseHandle);
            entity->setCachedShapeData(this, shape);
        }
    }

    if (m_wantSimulationIslands.m_bool)
    {
        if (islandIsNotEmpty)
        {
            gabriolaIsland->m_splitCheckFrameCounter = (unsigned char)gabriolaIsland->m_entities[0]->m_uid;
            hkArray<hkpSimulationIsland*, hkContainerHeapAllocator>& islands = initialActivationState == HK_ENTITY_ACTIVATION_DO_ACTIVATE ? m_activeSimulationIslands : m_inactiveSimulationIslands;
            islands.pushBack(gabriolaIsland);
        }
        else
        {
            delete gabriolaIsland;
        }
    }

    hkLocalArray<hkpBroadPhaseHandlePair> pairsOut(m_broadPhaseQuerySize);
    m_broadPhase->addObjectBatch(collList, aabbList, pairsOut);
    m_broadPhaseDispatcher->addPairs(static_cast<hkpTypedBroadPhaseHandlePair*>(pairsOut.begin()), pairsOut.getSize(), hkWorld_filter(this));
    for (int i = 0; i < numEntities; i++)
    {
        hkpWorldCallbackUtil::fireEntityAdded(this, entityBatch[i]);
        hkpEntityCallbackUtil::fireEntityAdded(entityBatch[i]);
    }
    hkWorld_unlock(this);
}

// 0x00DE44D0: the batch leaves the broadphase in one go, then each entity its island
void hkpWorld::removeEntityBatch(hkpEntity* const* entityBatch, int numEntities)
{
    if (numEntities <= 0)
    {
        return;
    }
    if (m_criticalOperationsLockCount != 0)
    {
        hkWorldOperation::RemoveEntityBatch op;
        op.m_entities = const_cast<hkpEntity**>(entityBatch);
        op.m_numEntities = (unsigned short)numEntities;
        queueOperation(op);
        return;
    }
    m_criticalOperationsLockCount = 1;
    m_simulation->resetCollisionInformationForEntities(const_cast<hkpEntity**>(entityBatch), numEntities, this, hkpSimulation::RESET_TOI);
    {
        hkArray<hkpBroadPhaseHandle*, hkContainerTempAllocator> collList;
        collList.reserve(numEntities);
        for (int i = 0; i < numEntities; i++)
        {
            if (entityBatch[i]->m_collidable.getShape() != nullptr)
            {
                collList.pushBack(&entityBatch[i]->m_collidable.m_broadPhaseHandle);
            }
        }
        if (collList.getSize() != 0)
        {
            hkLocalArray<hkpBroadPhaseHandlePair> pairsOut(m_broadPhaseQuerySize);
            m_broadPhase->removeObjectBatch(collList, pairsOut);
            m_broadPhaseDispatcher->removePairs(static_cast<hkpTypedBroadPhaseHandlePair*>(pairsOut.begin()), pairsOut.getSize());
        }
    }
    for (int i = 0; i < numEntities; i++)
    {
        hkpEntity* entity = entityBatch[i];
        hkpWorldCallbackUtil::fireEntityRemoved(this, entity);
        hkpEntityCallbackUtil::fireEntityRemoved(entity);
        hkpWorldOperationUtil::removeEntitySI(this, entity);
        if (entity->m_collidable.m_boundingVolumeData.m_childShapeAabbs != nullptr)
        {
            entity->m_collidable.m_boundingVolumeData.deallocate();
        }
        if ((entity->getMemorySizeAndFlags()) == 0)
        {
            entity->deallocateInternalArrays();
        }
        entity->removeReference();
    }
    hkWorld_unlock(this);
}

// 0x00DE1290: the pair gets its agent again when the filter allows it and none exists
void hkpWorld::reenableCollisionBetweenEntityPair(hkpEntity* entityA, hkpEntity* entityB)
{
    if (m_criticalOperationsLockCount != 0)
    {
        hkWorldOperation::UpdateFilterOnEntityPair op;
        op.m_entityA = entityA;
        op.m_entityB = entityB;
        queueOperation(op);
        return;
    }
    if (!m_broadPhase->areAabbsOverlapping(&entityA->m_collidable.m_broadPhaseHandle, &entityB->m_collidable.m_broadPhaseHandle))
    {
        return;
    }
    hkpLinkedCollidable* collA = &entityA->m_collidable;
    hkpLinkedCollidable* collB = &entityB->m_collidable;
    if (!static_cast<const hkpCollidableCollidableFilter*>(m_collisionFilter)->isCollisionEnabled(*collA, *collB).m_bool)
    {
        return;
    }
    if (hkAgentNnMachine_FindAgent(collA, collB) == nullptr)
    {
        hkpTypedBroadPhaseHandlePair pair;
        pair.m_a = &collA->m_broadPhaseHandle;
        pair.m_b = &collB->m_broadPhaseHandle;
        m_broadPhaseDispatcher->addPairs(&pair, 1, hkWorld_filter(this));
    }
}

// 0x00DE33F0: every entity overlapping the box wakes up; queued with a heap copy of the box while locked
void hkpWorld::activateRegion(const hkAabb& aabb)
{
    if (m_criticalOperationsLockCount != 0)
    {
        hkWorldOperation::ActivateRegion op;
        op.m_aabb = static_cast<hkAabb*>(hkMemoryRouter::getInstance().heap().blockAlloc(sizeof(hkAabb)));
        memcpy(op.m_aabb, &aabb, sizeof(hkAabb));
        queueOperation(op);
        return;
    }
    hkArray<hkpBroadPhaseHandlePair, hkContainerHeapAllocator> pairs;
    m_broadPhase->querySingleAabb(aabb, pairs);
    for (int i = 0; i < pairs.getSize(); i++)
    {
        hkpCollidable* coll = hkWorld_owner(pairs[i].m_b);
        if (coll->m_broadPhaseHandle.m_type == 1) // BROAD_PHASE_ENTITY
        {
            hkpEntity* entity = static_cast<hkpEntity*>(coll->getOwner());
            if (entity != nullptr)
            {
                entity->activate();
            }
        }
    }
}

// ---- collision filter updates

// 0x00DE11D0: the phantom (and a phantom partner) learns whether it overlaps the collidable under the new filter
static void hkWorld_updateFilterOnSinglePhantom(hkpPhantom* phantom, hkpCollidable* collidable, hkpCollisionFilter* filter)
{
    const hkBool oldOverlapping = phantom->isOverlappingCollidableAdded(collidable);
    if (static_cast<const hkpCollidableCollidableFilter*>(filter)->isCollisionEnabled(phantom->m_collidable, *collidable).m_bool)
    {
        if (!oldOverlapping.m_bool)
        {
            phantom->addOverlappingCollidable(collidable);
        }
        if (collidable->m_broadPhaseHandle.m_type == HK_WORLD_BROAD_PHASE_PHANTOM)
        {
            hkpPhantom* otherPhantom = static_cast<hkpPhantom*>(collidable->getOwner());
            if (!otherPhantom->isOverlappingCollidableAdded(&phantom->m_collidable).m_bool)
            {
                otherPhantom->addOverlappingCollidable(&phantom->m_collidable);
            }
        }
    }
    else
    {
        if (oldOverlapping.m_bool)
        {
            phantom->removeOverlappingCollidable(collidable);
        }
        if (collidable->m_broadPhaseHandle.m_type == HK_WORLD_BROAD_PHASE_PHANTOM)
        {
            hkpPhantom* otherPhantom = static_cast<hkpPhantom*>(collidable->getOwner());
            if (otherPhantom->isOverlappingCollidableAdded(&phantom->m_collidable).m_bool)
            {
                otherPhantom->removeOverlappingCollidable(&phantom->m_collidable);
            }
        }
    }
}

// 0x00DE3630
void hkpWorld::updateCollisionFilterOnPhantom(hkpPhantom* phantom, hkpUpdateCollectionFilterMode updateShapeCollectionFilter)
{
    if (m_criticalOperationsLockCount != 0)
    {
        hkWorldOperation::UpdateFilterOnPhantom op;
        op.m_phantom = phantom;
        op.m_updateShapeCollections.m_storage = (unsigned char)updateShapeCollectionFilter;
        queueOperation(op);
        return;
    }
    m_criticalOperationsLockCount = 1;
    hkLocalArray<hkpBroadPhaseHandlePair> newPairs(m_broadPhaseQuerySize);
    m_broadPhase->reQuerySingleObject(&phantom->m_collidable.m_broadPhaseHandle, newPairs);
    for (int i = 0; i < newPairs.getSize(); i++)
    {
        if (newPairs[i].m_b != &phantom->m_collidable.m_broadPhaseHandle)
        {
            hkWorld_updateFilterOnSinglePhantom(phantom, hkWorld_owner(newPairs[i].m_b), m_collisionFilter);
        }
    }
    if (updateShapeCollectionFilter != HK_UPDATE_COLLECTION_FILTER_IGNORE_SHAPE_COLLECTIONS)
    {
        phantom->updateShapeCollectionFilter();
    }
    hkWorld_unlock(this);
}

// 0x00DE38D0: a full check re-queries the broadphase (phantoms are updated directly, the pairs without an agent get
// one at the end); every agent the filter or the quality table no longer allows goes, an agent of another quality is
// recreated
void hkpWorld::updateCollisionFilterOnEntity(hkpEntity* entity, hkpUpdateCollisionFilterOnEntityMode updateMode, hkpUpdateCollectionFilterMode updateShapeCollectionFilter)
{
    if (m_criticalOperationsLockCount != 0)
    {
        hkWorldOperation::UpdateFilterOnEntity op;
        op.m_entity = entity;
        op.m_collisionFilterUpdateMode.m_storage = (unsigned char)updateMode;
        op.m_updateShapeCollections.m_storage = (unsigned char)updateShapeCollectionFilter;
        queueOperation(op);
        return;
    }
    m_criticalOperationsLockCount++;
    if (entity->m_motion.m_type.m_storage == hkpMotion::MOTION_FIXED)
    {
        entity->m_collidable.sortEntries();
    }
    hkpCollidable* collidable = &entity->m_collidable;
    hkInplaceArray<hkpBroadPhaseHandlePair, 128, hkContainerHeapAllocator> newPairs;
    if (updateMode == HK_UPDATE_FILTER_ON_ENTITY_FULL_CHECK)
    {
        m_broadPhase->reQuerySingleObject(&collidable->m_broadPhaseHandle, newPairs);
        for (int i = 0; i < newPairs.getSize(); i++)
        {
            hkpCollidable* coll = hkWorld_owner(newPairs[i].m_b);
            if (coll->m_broadPhaseHandle.m_type != HK_WORLD_BROAD_PHASE_PHANTOM)
            {
                continue;
            }
            hkpPhantom* phantom = static_cast<hkpPhantom*>(coll->getOwner());
            if (phantom == nullptr)
            {
                continue;
            }
            hkWorld_updateFilterOnSinglePhantom(phantom, collidable, m_collisionFilter);
            if (updateShapeCollectionFilter != HK_UPDATE_COLLECTION_FILTER_IGNORE_SHAPE_COLLECTIONS)
            {
                phantom->updateShapeCollectionFilter();
            }
            newPairs.removeAt(i);
            i--;
        }
        // the pairs that already have an agent stay as they are
        hkInplaceArray<hkpBroadPhaseHandlePair, 128, hkContainerHeapAllocator> presentPairs;
        hkArray<hkpLinkedCollidable::CollisionEntry, hkContainerHeapAllocator>& entries = entity->m_collidable.getCollisionEntriesDeterministicUnchecked();
        for (int i = 0; i < entries.getSize(); i++)
        {
            hkpBroadPhaseHandlePair& pair = presentPairs.expandOne();
            pair.m_a = &entries[i].m_agentEntry->m_collidable[0]->m_broadPhaseHandle;
            pair.m_b = &entries[i].m_agentEntry->m_collidable[1]->m_broadPhaseHandle;
        }
        hkpTypedBroadPhaseDispatcher::removeDuplicates(presentPairs, newPairs);
    }

    const int qualityA = collidable->m_broadPhaseHandle.m_objectQualityType;
    hkArray<hkpLinkedCollidable::CollisionEntry, hkContainerHeapAllocator>& entries = entity->m_collidable.getCollisionEntriesDeterministicUnchecked();
    for (int i = 0; i < entries.getSize(); i++)
    {
        hkpLinkedCollidable::CollisionEntry& entry = entries[i];
        bool removeAgent = false;
        if (!static_cast<const hkpCollidableCollidableFilter*>(m_collisionFilter)->isCollisionEnabled(*collidable, *entry.m_partner).m_bool)
        {
            removeAgent = true;
        }
        else
        {
            const int quality = m_collisionDispatcher->m_collisionQualityTable[qualityA][entry.m_partner->m_broadPhaseHandle.m_objectQualityType];
            if (quality == 0) // HK_COLLIDABLE_QUALITY_INVALID
            {
                removeAgent = true;
            }
            else if (quality != entry.m_agentEntry->m_collisionQualityIndex)
            {
                // recreated with the new quality
                hkpBroadPhaseHandlePair& pair = newPairs.expandOne();
                pair.m_a = &entry.m_agentEntry->m_collidable[0]->m_broadPhaseHandle;
                pair.m_b = &entry.m_agentEntry->m_collidable[1]->m_broadPhaseHandle;
                removeAgent = true;
            }
            else if (updateShapeCollectionFilter == HK_UPDATE_COLLECTION_FILTER_PROCESS_SHAPE_COLLECTIONS)
            {
                // a fixed entity's agents live in the partner's island
                hkpSimulationIsland* island = entity->m_motion.m_type.m_storage == hkpMotion::MOTION_FIXED ? static_cast<hkpEntity*>(entry.m_partner->getOwner())->m_simulationIsland : entity->m_simulationIsland;
                hkAgentNnMachine_UpdateShapeCollectionFilter(entry.m_agentEntry, *m_collisionInput, *island);
            }
        }
        if (removeAgent)
        {
            hkpWorldAgentUtil::removeAgentAndItsToiEvents(entry.m_agentEntry);
            i--;
            entity->m_simulationIsland->m_splitCheckRequested = true;
        }
    }

    if (newPairs.getSize() > 0)
    {
        m_broadPhaseDispatcher->addPairs(static_cast<hkpTypedBroadPhaseHandlePair*>(newPairs.begin()), newPairs.getSize(), hkWorld_filter(this));
    }
    hkWorld_unlock(this);
}

// 0x00DE68A0: a full check runs the entity and phantom updates for everything; the entity-entity mode only checks
// the agents of every island
void hkpWorld::updateCollisionFilterOnWorld(hkpUpdateCollisionFilterOnWorldMode updateMode, hkpUpdateCollectionFilterMode updateShapeCollectionFilter)
{
    if (m_criticalOperationsLockCount != 0)
    {
        hkWorldOperation::UpdateFilterOnWorld op;
        op.m_collisionFilterUpdateMode.m_storage = (unsigned char)updateMode;
        op.m_updateShapeCollections.m_storage = (unsigned char)updateShapeCollectionFilter;
        queueOperation(op);
        return;
    }
    m_blockExecutingPendingOperations.m_bool = true;
    if (updateMode == HK_UPDATE_FILTER_ON_WORLD_FULL_CHECK)
    {
        for (int i = 0; i < m_activeSimulationIslands.getSize(); i++)
        {
            hkpSimulationIsland* island = m_activeSimulationIslands[i];
            for (int e = 0; e < island->m_entities.getSize(); e++)
            {
                updateCollisionFilterOnEntity(island->m_entities[e], HK_UPDATE_FILTER_ON_ENTITY_FULL_CHECK, updateShapeCollectionFilter);
            }
        }
        for (int i = 0; i < m_inactiveSimulationIslands.getSize(); i++)
        {
            hkpSimulationIsland* island = m_inactiveSimulationIslands[i];
            for (int e = 0; e < island->m_entities.getSize(); e++)
            {
                updateCollisionFilterOnEntity(island->m_entities[e], HK_UPDATE_FILTER_ON_ENTITY_FULL_CHECK, updateShapeCollectionFilter);
            }
        }
        for (int i = 0; i < m_phantoms.getSize(); i++)
        {
            updateCollisionFilterOnPhantom(m_phantoms[i], updateShapeCollectionFilter);
        }
    }
    else
    {
        m_criticalOperationsLockCount++;
        hkArray<hkpSimulationIsland*, hkContainerHeapAllocator>* islandArrays[2] = { &m_activeSimulationIslands, &m_inactiveSimulationIslands };
        for (int a = 0; a < 2; a++)
        {
            hkArray<hkpSimulationIsland*, hkContainerHeapAllocator>& islands = *islandArrays[a];
            hkInplaceArray<hkpAgentNnEntry*, 32, hkContainerHeapAllocator> agentsToRemove;
            for (int i = 0; i < islands.getSize(); i++)
            {
                hkpSimulationIsland* island = islands[i];
                hkpAgentNnTrack* tracks[2] = { &island->m_narrowphaseAgentTrack, &island->m_midphaseAgentTrack };
                for (int t = 0; t < 2; t++)
                {
                    // Original bug kept: the list empties per track, so the narrowphase track's rejected agents are
                    // never removed (their island is still marked for a split check)
                    agentsToRemove.clear();
                    HK_FOR_ALL_AGENT_ENTRIES_BEGIN((*tracks[t]), entry)
                    {
                        bool remove = !static_cast<const hkpCollidableCollidableFilter*>(m_collisionFilter)->isCollisionEnabled(*entry->m_collidable[0], *entry->m_collidable[1]).m_bool;
                        if (!remove)
                        {
                            const int quality = m_collisionDispatcher->m_collisionQualityTable[entry->m_collidable[0]->m_broadPhaseHandle.m_objectQualityType][entry->m_collidable[1]->m_broadPhaseHandle.m_objectQualityType];
                            remove = quality == 0 || quality != entry->m_collisionQualityIndex;
                        }
                        if (remove)
                        {
                            agentsToRemove.pushBack(entry);
                            island->m_splitCheckRequested = true;
                        }
                        else if (updateShapeCollectionFilter == HK_UPDATE_COLLECTION_FILTER_PROCESS_SHAPE_COLLECTIONS)
                        {
                            hkAgentNnMachine_UpdateShapeCollectionFilter(entry, *m_collisionInput, *island);
                        }
                    }
                    HK_FOR_ALL_AGENT_ENTRIES_END;
                }
                while (agentsToRemove.getSize() != 0)
                {
                    hkpWorldAgentUtil::removeAgentAndItsToiEvents(agentsToRemove.back());
                    agentsToRemove.popBack();
                }
            }
        }
        m_criticalOperationsLockCount--;
    }
    m_blockExecutingPendingOperations.m_bool = false;
    if (m_criticalOperationsLockCount == 0)
    {
        if (m_pendingOperationsCount != 0)
        {
            internal_executePendingOperations();
        }
        if (m_pendingOperationQueueCount == 1 && m_pendingBodyOperationsCount != 0)
        {
            internal_executePendingBodyOperations();
        }
    }
}

// ---- constraints

// 0x00DE6C70
hkpConstraintInstance* hkpWorld::addConstraint(hkpConstraintInstance* constraint)
{
    if (m_criticalOperationsLockCount != 0)
    {
        hkWorldOperation::AddConstraint op;
        op.m_constraint = constraint;
        queueOperation(op);
        return nullptr;
    }
    // a breakable constraint learns its child's runtime size once
    if (constraint->m_data->getType() == hkpConstraintData::CONSTRAINT_TYPE_BREAKABLE)
    {
        hkpBreakableConstraintData* data = static_cast<hkpBreakableConstraintData*>(constraint->m_data);
        if (data->m_childRuntimeSize == 0)
        {
            hkpConstraintData::RuntimeInfo runtimeInfo;
            data->getWrappedConstraintData()->getRuntimeInfo(true, runtimeInfo);
            data->m_childRuntimeSize = (unsigned short)runtimeInfo.m_sizeOfExternalRuntime;
            data->m_childNumSolverResults = (unsigned short)runtimeInfo.m_numSolverResults;
        }
    }
    m_blockExecutingPendingOperations.m_bool = true;
    constraint->pointNullsToFixedRigidBody();
    constraint->m_uid = ++m_lastConstraintUid;
    hkpConstraintInstance* result = hkpWorldOperationUtil::addConstraintImmediately(this, constraint, hkpWorldOperationUtil::DO_FIRE_CALLBACKS);
    if (constraint->getType() == hkpConstraintInstance::TYPE_CHAIN)
    {
        // hkpConstraintChainInstance::m_action (+0x44) joins the world with the chain
        hkpAction* action = *reinterpret_cast<hkpAction**>(reinterpret_cast<char*>(constraint) + 0x44);
        if (action->m_world == nullptr)
        {
            addAction(action);
        }
        // the chain data's vtbl+0x68 (getNumConstraintInfos) of a compiled-out assert; the call stays
        typedef int(__thiscall * ChainDataCount)(hkpConstraintData*);
        (*reinterpret_cast<ChainDataCount*>(*reinterpret_cast<char**>(constraint->m_data) + 0x68))(constraint->m_data);
    }
    m_blockExecutingPendingOperations.m_bool = false;
    if (m_criticalOperationsLockCount == 0 && m_pendingOperationsCount != 0)
    {
        internal_executePendingOperations();
    }
    return result;
}

// 0x00DE6DE0
hkBool hkpWorld::removeConstraint(hkpConstraintInstance* constraint)
{
    if (m_criticalOperationsLockCount != 0)
    {
        hkWorldOperation::RemoveConstraint op;
        op.m_constraint = constraint;
        queueOperation(op);
        return false;
    }
    m_criticalOperationsLockCount = 1;
    if (constraint->getType() == hkpConstraintInstance::TYPE_CHAIN)
    {
        hkpAction* action = *reinterpret_cast<hkpAction**>(reinterpret_cast<char*>(constraint) + 0x44); // m_action
        if (action->m_world == this)
        {
            removeActionImmediately(action);
        }
    }
    constraint->addReference();
    hkpWorldOperationUtil::removeConstraintImmediately(this, constraint, hkpWorldOperationUtil::DO_FIRE_CALLBACKS);
    constraint->setFixedRigidBodyPointersToZero(this);
    constraint->removeReference();
    hkWorld_unlock(this);
    return true;
}

// ---- actions

// 0x00DE48C0: the action goes to the island of its first non-fixed entity (whose islands merge), or the first
// entity's island when all are fixed
hkpAction* hkpWorld::addAction(hkpAction* action)
{
    if (m_criticalOperationsLockCount != 0)
    {
        hkWorldOperation::AddAction op;
        op.m_action = action;
        queueOperation(op);
        return nullptr;
    }
    action->addReference();
    m_criticalOperationsLockCount++;
    hkInplaceArray<hkpEntity*, 4, hkContainerHeapAllocator> entities;
    action->getEntities(entities);
    action->m_world = this;
    hkpEntity* firstMovableEntity = nullptr;
    for (int i = 0; i < entities.getSize(); i++)
    {
        hkpEntity* entity = entities[i];
        entity->m_actions.pushBack(action);
        hkpSimulationIsland* island = entity->m_simulationIsland;
        if (island->m_storageIndex == 0xFFFF) // the fixed island
        {
            continue;
        }
        if (firstMovableEntity == nullptr)
        {
            island->addAction(action);
            firstMovableEntity = entity;
        }
        else if (firstMovableEntity->m_simulationIsland != island)
        {
            hkpWorldOperationUtil::mergeIslands(this, firstMovableEntity, entity);
        }
    }
    if (firstMovableEntity == nullptr)
    {
        entities[0]->m_simulationIsland->addAction(action);
    }
    hkWorld_unlock(this);
    hkpWorldCallbackUtil::fireActionAdded(this, action);
    return action;
}

// 0x00DE6EB0
void hkpWorld::removeAction(hkpAction* action)
{
    if (m_criticalOperationsLockCount != 0)
    {
        hkWorldOperation::RemoveAction op;
        op.m_action = action;
        queueOperation(op);
        return;
    }
    removeActionImmediately(action);
}

// 0x00DE4A60: the action leaves its entities and its island; the island goes on the dirty list (action cleanup)
void hkpWorld::removeActionImmediately(hkpAction* action)
{
    action->addReference();
    m_criticalOperationsLockCount++;
    hkpWorldCallbackUtil::fireActionRemoved(this, action);
    {
        hkInplaceArray<hkpEntity*, 4, hkContainerHeapAllocator> entities;
        action->getEntities(entities);
        for (int i = 0; i < entities.getSize(); i++)
        {
            hkpEntity* entity = entities[i];
            entity->m_actions.removeAt(entity->m_actions.indexOf(action));
        }
        hkpSimulationIsland* island = action->m_island;
        island->removeAction(action);
        action->m_world = nullptr;
        action->removeReference();
        hkpWorldOperationUtil::putIslandOnDirtyList(island->m_world, island);
    }
    hkWorld_unlock(this);
    action->removeReference();
}

// ---- phantoms

// 0x00DE2E10
hkpPhantom* hkpWorld::addPhantom(hkpPhantom* phantom)
{
    if (m_criticalOperationsLockCountForPhantoms + m_criticalOperationsLockCount != 0)
    {
        hkWorldOperation::AddPhantom op;
        op.m_phantom = phantom;
        queueOperation(op);
        return nullptr;
    }
    m_criticalOperationsLockCount++;
    if (phantom->m_collidable.m_motion == nullptr)
    {
        phantom->m_collidable.m_motion = phantom->getMotionState();
    }
    phantom->m_world = this;
    phantom->addReference();
    m_phantoms.pushBack(phantom);
    hkpWorldOperationUtil::addPhantomBP(this, phantom);
    hkpWorldCallbackUtil::firePhantomAdded(this, phantom);
    phantom->firePhantomAdded();
    hkWorld_unlock(this);
    return phantom;
}

// 0x00DE2380
void hkpWorld::removePhantom(hkpPhantom* phantom)
{
    if (m_criticalOperationsLockCountForPhantoms + m_criticalOperationsLockCount != 0)
    {
        hkWorldOperation::RemovePhantom op;
        op.m_phantom = phantom;
        queueOperation(op);
        return;
    }
    m_criticalOperationsLockCount++;
    hkpWorldCallbackUtil::firePhantomRemoved(this, phantom);
    phantom->firePhantomRemoved();
    hkpWorldOperationUtil::removePhantomBP(this, phantom);
    m_phantoms.removeAt(m_phantoms.indexOf(phantom));
    phantom->m_world = nullptr;
    if ((phantom->getMemorySizeAndFlags()) == 0)
    {
        phantom->deallocateInternalArrays();
    }
    phantom->removeReference();
    hkWorld_unlock(this);
}

// 0x00DE4DB0
void hkpWorld::addPhantomBatch(hkpPhantom* const* phantomBatch, int numPhantoms)
{
    if (numPhantoms <= 0)
    {
        return;
    }
    if (m_criticalOperationsLockCountForPhantoms + m_criticalOperationsLockCount != 0)
    {
        hkWorldOperation::AddPhantomBatch op;
        op.m_phantoms = const_cast<hkpPhantom**>(phantomBatch);
        op.m_numPhantoms = (unsigned short)numPhantoms;
        queueOperation(op);
        return;
    }
    m_criticalOperationsLockCount++;
    hkLocalArray<hkpBroadPhaseHandle*> collList(numPhantoms);
    hkLocalArray<hkAabb> aabbList(numPhantoms);
    aabbList.setSizeUnchecked(numPhantoms);
    collList.setSizeUnchecked(numPhantoms);
    for (int i = 0; i < numPhantoms; i++)
    {
        hkpPhantom* phantom = phantomBatch[i];
        if (phantom->m_collidable.m_motion == nullptr)
        {
            phantom->m_collidable.m_motion = phantom->getMotionState();
        }
        phantom->m_world = this;
        collList[i] = &phantom->m_collidable.m_broadPhaseHandle;
        phantom->calcAabb(aabbList[i]);
        phantom->setBoundingVolumeData(aabbList[i]);
        phantom->addReference();
        m_phantoms.pushBack(phantom);
        hkpWorldCallbackUtil::firePhantomAdded(this, phantom);
        phantom->firePhantomAdded();
    }
    hkLocalArray<hkpBroadPhaseHandlePair> newPairs(m_broadPhaseQuerySize);
    m_broadPhase->addObjectBatch(collList, aabbList, newPairs);
    m_broadPhaseDispatcher->addPairs(static_cast<hkpTypedBroadPhaseHandlePair*>(newPairs.begin()), newPairs.getSize(), hkWorld_filter(this));
    hkWorld_unlock(this);
}

// 0x00DE5220
void hkpWorld::removePhantomBatch(hkpPhantom* const* phantomBatch, int numPhantoms)
{
    if (numPhantoms <= 0)
    {
        return;
    }
    if (m_criticalOperationsLockCountForPhantoms + m_criticalOperationsLockCount != 0)
    {
        hkWorldOperation::RemovePhantomBatch op;
        op.m_phantoms = const_cast<hkpPhantom**>(phantomBatch);
        op.m_numPhantoms = (unsigned short)numPhantoms;
        queueOperation(op);
        return;
    }
    m_criticalOperationsLockCount++;
    hkLocalArray<hkpBroadPhaseHandle*> collList(numPhantoms);
    for (int i = 0; i < numPhantoms; i++)
    {
        hkpPhantom* phantom = phantomBatch[i];
        collList.pushBack(&phantom->m_collidable.m_broadPhaseHandle);
        hkpWorldCallbackUtil::firePhantomRemoved(this, phantom);
        phantom->firePhantomRemoved();
    }
    if (collList.getSize() != 0)
    {
        hkLocalArray<hkpBroadPhaseHandlePair> removedPairs(m_broadPhaseQuerySize);
        m_broadPhase->removeObjectBatch(collList, removedPairs);
        m_broadPhaseDispatcher->removePairs(static_cast<hkpTypedBroadPhaseHandlePair*>(removedPairs.begin()), removedPairs.getSize());
    }
    for (int i = 0; i < numPhantoms; i++)
    {
        hkpPhantom* phantom = phantomBatch[i];
        phantom->m_world = nullptr;
        m_phantoms.removeAt(m_phantoms.indexOf(phantom));
        if ((phantom->getMemorySizeAndFlags()) == 0)
        {
            phantom->deallocateInternalArrays();
        }
        phantom->removeReference();
    }
    hkWorld_unlock(this);
}

// ---- listeners: added at the end; removal clears the slot (the callback utils skip and compact null entries).
// Original bug kept: removing a listener that is not there clears the slot before the list (index -1).

template<typename T>
static inline void hkWorld_removeListener(hkArray<T*, hkContainerHeapAllocator>& listeners, T* listener)
{
    listeners.begin()[listeners.indexOf(listener)] = nullptr;
}

// 0x00DE2F10
void hkpWorld::addActionListener(hkpActionListener* worldListener) { m_actionListeners.pushBack(worldListener); }
// 0x00DE1630
void hkpWorld::removeActionListener(hkpActionListener* worldListener) { hkWorld_removeListener(m_actionListeners, worldListener); }
// 0x00DE2F50
void hkpWorld::addConstraintListener(hkpConstraintListener* worldListener) { m_constraintListeners.pushBack(worldListener); }
// 0x00DE1680
void hkpWorld::removeConstraintListener(hkpConstraintListener* worldListener) { hkWorld_removeListener(m_constraintListeners, worldListener); }
// 0x00DE2F90
void hkpWorld::addEntityListener(hkpEntityListener* worldListener) { m_entityListeners.pushBack(worldListener); }
// 0x00DE16D0
void hkpWorld::removeEntityListener(hkpEntityListener* worldListener) { hkWorld_removeListener(m_entityListeners, worldListener); }
// 0x00DE2FD0
void hkpWorld::addPhantomListener(hkpPhantomListener* worldListener) { m_phantomListeners.pushBack(worldListener); }
// 0x00DE1720
void hkpWorld::removePhantomListener(hkpPhantomListener* worldListener) { hkWorld_removeListener(m_phantomListeners, worldListener); }
// 0x00DE3010
void hkpWorld::addIslandActivationListener(hkpIslandActivationListener* worldListener) { m_islandActivationListeners.pushBack(worldListener); }
// 0x00DE1770
void hkpWorld::removeIslandActivationListener(hkpIslandActivationListener* worldListener) { hkWorld_removeListener(m_islandActivationListeners, worldListener); }
// 0x00DE3050
void hkpWorld::addWorldPostCollideListener(hkpWorldPostCollideListener* worldListener) { m_worldPostCollideListeners.pushBack(worldListener); }
// 0x00DE17C0
void hkpWorld::removeWorldPostCollideListener(hkpWorldPostCollideListener* worldListener) { hkWorld_removeListener(m_worldPostCollideListeners, worldListener); }
// 0x00DE3090
void hkpWorld::addWorldPostSimulationListener(hkpWorldPostSimulationListener* worldListener) { m_worldPostSimulationListeners.pushBack(worldListener); }
// 0x00DE1810
void hkpWorld::removeWorldPostSimulationListener(hkpWorldPostSimulationListener* worldListener) { hkWorld_removeListener(m_worldPostSimulationListeners, worldListener); }
// 0x00DE30D0
void hkpWorld::addWorldPostIntegrateListener(hkpWorldPostIntegrateListener* worldListener) { m_worldPostIntegrateListeners.pushBack(worldListener); }
// 0x00DE1860
void hkpWorld::removeWorldPostIntegrateListener(hkpWorldPostIntegrateListener* worldListener) { hkWorld_removeListener(m_worldPostIntegrateListeners, worldListener); }
// 0x00DE3110
void hkpWorld::addIslandPostCollideListener(hkpIslandPostCollideListener* islandListener) { m_islandPostCollideListeners.pushBack(islandListener); }
// 0x00DE18B0
void hkpWorld::removeIslandPostCollideListener(hkpIslandPostCollideListener* islandListener) { hkWorld_removeListener(m_islandPostCollideListeners, islandListener); }
// 0x00DE3150
void hkpWorld::addIslandPostIntegrateListener(hkpIslandPostIntegrateListener* islandListener) { m_islandPostIntegrateListeners.pushBack(islandListener); }
// 0x00DE1900
void hkpWorld::removeIslandPostIntegrateListener(hkpIslandPostIntegrateListener* islandListener) { hkWorld_removeListener(m_islandPostIntegrateListeners, islandListener); }
// 0x00DE3190
void hkpWorld::addContactListener(hkpContactListener* collisionListener) { m_contactListeners.pushBack(collisionListener); }
// 0x00DE1950
void hkpWorld::removeContactListener(hkpContactListener* collisionListener) { hkWorld_removeListener(m_contactListeners, collisionListener); }
// 0x00DE31D0
void hkpWorld::addWorldDeletionListener(hkpWorldDeletionListener* worldListener) { m_worldDeletionListeners.pushBack(worldListener); }
// 0x00DE19A0
void hkpWorld::removeWorldDeletionListener(hkpWorldDeletionListener* worldListener) { hkWorld_removeListener(m_worldDeletionListeners, worldListener); }
// 0x00DE3210
void hkpWorld::addContactImpulseLimitBreachedListener(hkpContactImpulseLimitBreachedListener* listener) { m_contactImpulseLimitBreachedListeners.pushBack(listener); }
// 0x00DE19F0
void hkpWorld::removeContactImpulseLimitBreachedListener(hkpContactImpulseLimitBreachedListener* listener) { hkWorld_removeListener(m_contactImpulseLimitBreachedListeners, listener); }

// ---- contact managers

// 0x00DE10F0: the simple constraint manager between dynamic bodies (response 1), the report manager (response 2) and
// no manager (response 3) with everything
void hkWorld_setupContactMgrFactories(hkpWorld* world, hkpCollisionDispatcher* dis)
{
    hkpContactMgrFactory* simple = new hkpSimpleConstraintContactMgr::Factory(world);
    hkpContactMgrFactory* report = new hkpReportContactMgr::Factory(world);
    hkpContactMgrFactory* none = new hkpNullContactMgrFactory();
    dis->registerContactMgrFactoryWithAll(simple, 1); // hkpMaterial::RESPONSE_SIMPLE_CONTACT
    dis->registerContactMgrFactoryWithAll(report, 2); // RESPONSE_REPORTING
    dis->registerContactMgrFactoryWithAll(none, 3); // RESPONSE_NONE
    simple->removeReference();
    report->removeReference();
    none->removeReference();
}

// ---- queries

// 0x00DE1BF0
// Every hit along the ray goes to the collector (through the world's filter, no broadphase cache).
void hkpWorld::castRay(const hkpWorldRayCastInput& input, hkpRayHitCollector& collector) const
{
    hkpWorldRayCaster rayCaster;
    rayCaster.castRay(*m_broadPhase, input, m_collisionFilter, nullptr, collector);
}

// 0x00DE1CA0
// The closest hit along the ray goes to the output (through the world's filter, no broadphase cache).
void hkpWorld::castRay(const hkpWorldRayCastInput& input, hkpWorldRayCastOutput& output) const
{
    hkpSimpleWorldRayCaster rayCaster;
    rayCaster.castRay(*m_broadPhase, input, m_collisionFilter, nullptr, output);
}

// 0x00DE1D50
// The collidable's AABB swept along the path through the broadphase, then the dispatcher's linear cast per pair
// (through the world's filter, no broadphase cache).
void hkpWorld::linearCast(const hkpCollidable* collA, const hkpLinearCastInput& input, hkpCdPointCollector& castCollector, hkpCdPointCollector* startPointCollector) const
{
    hkpWorldLinearCaster caster;
    const hkpCollidableCollidableFilter* filter = m_collisionFilter ? static_cast<const hkpCollidableCollidableFilter*>(m_collisionFilter) : nullptr;
    caster.linearCast(*m_broadPhase, collA, input, filter, *m_collisionInput, m_collisionInput->m_config, nullptr, castCollector, startPointCollector);
}

// The broadphase pairs of the collidable's AABB (grown by the query tolerance less half the world's tolerance, which
// the broadphase boxes already have).
static void hkWorld_queryPairs(const hkpWorld* world, const hkpCollidable* collA, float tolerance, hkArray<hkpBroadPhaseHandlePair, hkContainerHeapAllocator>& pairs)
{
    hkAabb aabb;
    collA->getShape()->getAabb(collA->getTransform(), tolerance, aabb);
    world->m_broadPhase->querySingleAabb(aabb, pairs);
}

static inline const hkpCollidable* hkWorld_otherCollidable(const hkpBroadPhaseHandlePair& pair)
{
    return static_cast<const hkpCollidable*>(static_cast<const hkpTypedBroadPhaseHandle*>(pair.m_b)->getOwner());
}

// 0x00DE5970: every other collidable the filter allows and that has a shape
void hkpWorld::getClosestPoints(const hkpCollidable* collA, const hkpCollisionInput& input, hkpCdPointCollector& collector) const
{
    const float tolerance = input.m_tolerance.val() - m_collisionInput->m_tolerance.val() * 0.5f; // 0x012B9C10
    hkInplaceArray<hkpBroadPhaseHandlePair, 128, hkContainerHeapAllocator> pairs;
    hkWorld_queryPairs(this, collA, tolerance, pairs);
    const hkcdShapeType::ShapeTypeEnum typeA = static_cast<hkcdShapeType::ShapeTypeEnum>(collA->getShape()->m_type.m_storage);
    for (int i = 0; i < pairs.getSize(); i++)
    {
        const hkpCollidable* collB = hkWorld_otherCollidable(pairs[i]);
        if (collA == collB)
        {
            continue;
        }
        if (!static_cast<const hkpCollidableCollidableFilter*>(m_collisionFilter)->isCollisionEnabled(*collA, *collB).m_bool)
        {
            continue;
        }
        const hkpShape* shapeB = collB->getShape();
        if (shapeB == nullptr)
        {
            continue;
        }
        const hkpCollisionDispatcher* dispatcher = input.m_dispatcher;
        dispatcher->getGetClosestPointsFunc(typeA, static_cast<hkcdShapeType::ShapeTypeEnum>(shapeB->m_type.m_storage))(*collA, *collB, input, collector);
    }
}

// 0x00DE5B50: as getClosestPoints (the tolerance clamped at 0), stopping at the collector's early out
void hkpWorld::getPenetrations(const hkpCollidable* collA, const hkpCollisionInput& input, hkpCdBodyPairCollector& collector) const
{
    const float grown = input.m_tolerance.val() - m_collisionInput->m_tolerance.val() * 0.5f; // 0x012B9C10
    const float tolerance = _mm_cvtss_f32(_mm_max_ss(_mm_setzero_ps(), _mm_set_ss(grown)));
    hkInplaceArray<hkpBroadPhaseHandlePair, 128, hkContainerHeapAllocator> pairs;
    hkWorld_queryPairs(this, collA, tolerance, pairs);
    const hkcdShapeType::ShapeTypeEnum typeA = static_cast<hkcdShapeType::ShapeTypeEnum>(collA->getShape()->m_type.m_storage);
    for (int i = 0; i < pairs.getSize(); i++)
    {
        const hkpCollidable* collB = hkWorld_otherCollidable(pairs[i]);
        if (collA == collB)
        {
            continue;
        }
        if (!static_cast<const hkpCollidableCollidableFilter*>(m_collisionFilter)->isCollisionEnabled(*collA, *collB).m_bool)
        {
            continue;
        }
        const hkpShape* shapeB = collB->getShape();
        if (shapeB == nullptr)
        {
            continue;
        }
        const hkpCollisionDispatcher* dispatcher = input.m_dispatcher;
        dispatcher->getGetPenetrationsFunc(typeA, static_cast<hkcdShapeType::ShapeTypeEnum>(shapeB->m_type.m_storage))(*collA, *collB, input, collector);
        if (collector.m_earlyOut.m_bool)
        {
            break;
        }
    }
}

// 0x00DE22E0: the action now also acts on the entity, so their islands merge unless the entity is fixed
void hkpWorld::attachActionToEntity(hkpAction* action, hkpEntity* entity)
{
    entity->m_actions.pushBack(action);
    hkpSimulationIsland* island = action->getSimulationIsland();
    const bool entity_fixed = entity->getMotion()->m_type.m_storage == hkpMotion::MOTION_FIXED;
    if (island->m_storageIndex == 0xFFFF && !entity_fixed)
    {
        island->removeAction(action);
        entity->getSimulationIsland()->addAction(action);
    }
    else if (entity->getSimulationIsland() != island && !entity_fixed)
    {
        hkpWorldOperationUtil::mergeIslands(this, entity, island->m_entities[0]);
    }
}

// 0x00DE4CA0: the action moves to the island of another of its entities, preferably one that is not fixed
void hkpWorld::detachActionFromEntity(hkpAction* action, hkpEntity* entity)
{
    int index = -1;
    for (int i = 0; i < entity->m_actions.getSize(); i++)
    {
        if (entity->m_actions[i] == action)
        {
            index = i;
            break;
        }
    }
    entity->m_actions.removeAt(index);
    entity->getSimulationIsland()->m_splitCheckRequested = true;

    hkArray<hkpEntity*,hkContainerHeapAllocator> entities;
    action->getEntities(entities);
    hkpSimulationIsland* new_island = NULL;
    for (int i = 0; i < entities.getSize(); i++)
    {
        if (entities[i] != entity)
        {
            new_island = entities[i]->getSimulationIsland();
            if (new_island->m_storageIndex != 0xFFFF)
            {
                break;
            }
        }
    }
    if (new_island != action->getSimulationIsland())
    {
        action->getSimulationIsland()->removeAction(action);
        new_island->addAction(action);
    }
}
