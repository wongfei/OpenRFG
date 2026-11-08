// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/hkpworldcinfo.cpp
//
// Havok 2014.2.5 (havok_impl.md HK7): the world construction info and its defaults, from rfg.exe.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/hkpworldcinfo.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/filter/hkpcollisionfilter.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/filter/hkpconvexlistfilter.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/memory/hkpworldmemoryavailablewatchdog.h"

// The hkRefPtr members are seams without constructors: the cinfo sets and releases their pointers itself.
template<typename T>
static inline void hkWorldCinfo_release(hkRefPtr<T>& p)
{
    hkReferencedObject* object = (hkReferencedObject*)p.val();
    if (object != nullptr)
    {
        object->removeReference();
    }
    *(T**)&p = nullptr;
}

// 0x00E03F90
hkpWorldCinfo::hkpWorldCinfo()
{
    *(void**)&m_collisionFilter = nullptr;
    *(void**)&m_convexListFilter = nullptr;
    *(void**)&m_memoryWatchDog = nullptr;
    m_gravity.set(0.0f, -9.8f, 0.0f, 0.0f); // 0xC11CCCCD
    m_enableSimulationIslands.m_bool = 1;
    m_broadPhaseQuerySize = 1024;
    m_mtPostponeAndSortBroadPhaseBorderCallbacks.m_bool = 0;
    m_broadPhaseWorldAabb.m_min.set(-500.0f, -500.0f, -500.0f, 0.0f);
    m_broadPhaseWorldAabb.m_max.set(500.0f, 500.0f, 500.0f, 0.0f);
    hkWorldCinfo_release(m_collisionFilter);
    hkWorldCinfo_release(m_convexListFilter);
    m_solverTau = 0.6f;
    m_solverDamp = 1.0f;
    m_contactRestingVelocity = 1.0f;
    m_maxConstraintViolation = 1.8446726e19f; // HK_REAL_HIGH (0x5F7FFFF0)
    m_broadPhaseNumMarkers = 0;
    m_sizeOfToiEventQueue = 250;
    m_solverMicrosteps = 1;
    m_solverIterations = 4;
    m_forceCoherentConstraintOrderingInSolver.m_bool = 0;
    m_snapCollisionToConvexEdgeThreshold = 0.524f; // 0x3F0624DD
    m_snapCollisionToConcaveEdgeThreshold = 0.698f; // 0x3F32B021
    m_enableToiWeldRejection.m_bool = 0;
    m_collisionTolerance = 0.1f;
    m_toiCollisionResponseRotateNormal = 0.2f;
    m_broadPhaseType.m_storage = BROADPHASE_TYPE_SAP;
    m_broadPhaseBorderBehaviour.m_storage = BROADPHASE_BORDER_ASSERT;
    m_useCompoundSpuElf.m_bool = 0;
    m_processToisMultithreaded.m_bool = 1;
    m_numToisTillAllowedPenetrationSimplifiedToi = 3.0f;
    m_numToisTillAllowedPenetrationToi = 3.0f;
    m_numToisTillAllowedPenetrationToiHigher = 4.0f;
    m_numToisTillAllowedPenetrationToiForced = 20.0f;
    m_deactivationReferenceDistance = 0.02f;
    m_expectedMaxLinearVelocity = 200.0f;
    m_expectedMinPsiDeltaTime = 1.0f / 30.0f; // 0x3D088889
    m_iterativeLinearCastEarlyOutDistance = 0.01f;
    m_maxSectorsPerMidphaseCollideTask = 2;
    m_maxSectorsPerNarrowphaseCollideTask = 4;
    m_maxEntriesPerToiMidphaseCollideTask = 4;
    m_maxEntriesPerToiNarrowphaseCollideTask = 12;
    m_maxNumToiCollisionPairsSinglethreaded = 0;
    m_enableDeprecatedWelding.m_bool = 0;
    m_iterativeLinearCastMaxIterations = 20;
    m_enableDeactivation.m_bool = 1;
    m_shouldActivateOnRigidBodyTransformChange.m_bool = 1;
    m_minDesiredIslandSize = 64;
    m_deactivationNumInactiveFramesSelectFlag0 = 0;
    m_deactivationNumInactiveFramesSelectFlag1 = 0;
    m_deactivationIntegrateCounter = 0;
    m_contactPointGeneration.m_storage = CONTACT_POINT_REJECT_MANY;
    m_allowToSkipConfirmedCallbacks.m_bool = 0;
    m_simulationType.m_storage = SIMULATION_TYPE_CONTINUOUS;
    m_frameMarkerPsiSnap = 0.0001f; // 0x38D1B717
    hkWorldCinfo_release(m_memoryWatchDog);
    m_processActionsInSingleThread.m_bool = 1;
    m_allowIntegrationOfIslandsWithoutConstraintsInASeparateJob.m_bool = 0;
    m_fireCollisionCallbacks.m_bool = 0;
}

// 0x00E041B0
hkpWorldCinfo::hkpWorldCinfo(hkFinishLoadedObjectFlag flag)
    : hkReferencedObject(flag)
{
    if (flag.m_finishing)
    {
        afterReflectNew();
    }
}

// 0x00821D60 (hkpworldcinfo.h): the references on the watchdog and filters go
hkpWorldCinfo::~hkpWorldCinfo()
{
    hkWorldCinfo_release(m_memoryWatchDog);
    hkWorldCinfo_release(m_convexListFilter);
    hkWorldCinfo_release(m_collisionFilter);
}

// 0x00E03F10: defaults for fields older versions did not have
void hkpWorldCinfo::afterReflectNew()
{
    if (m_contactRestingVelocity == 0.0f)
    {
        m_contactRestingVelocity = 1.0f;
    }
    if (m_maxEntriesPerToiMidphaseCollideTask == -1)
    {
        m_maxEntriesPerToiMidphaseCollideTask = 4;
    }
    if (m_maxEntriesPerToiNarrowphaseCollideTask == -1)
    {
        m_maxEntriesPerToiNarrowphaseCollideTask = 12;
    }
}

// 0x00E03DC0: soft (tau 0.3, damping 0.9), medium (0.6, 1.0) or hard (0.9, 1.1) with 2, 4 or 8 iterations
void hkpWorldCinfo::setupSolverInfo(SolverType st)
{
    switch (st)
    {
    case SOLVER_TYPE_2ITERS_SOFT:
        m_solverTau = 0.3f;
        m_solverIterations = 2;
        m_solverDamp = 0.9f;
        break;
    case SOLVER_TYPE_2ITERS_MEDIUM:
        m_solverTau = 0.6f;
        m_solverIterations = 2;
        m_solverDamp = 1.0f;
        break;
    case SOLVER_TYPE_2ITERS_HARD:
        m_solverIterations = 2;
        m_solverTau = 0.9f;
        m_solverDamp = 1.1f;
        break;
    case SOLVER_TYPE_4ITERS_SOFT:
        m_solverTau = 0.3f;
        m_solverIterations = 4;
        m_solverDamp = 0.9f;
        break;
    case SOLVER_TYPE_4ITERS_MEDIUM:
        m_solverTau = 0.6f;
        m_solverIterations = 4;
        m_solverDamp = 1.0f;
        break;
    case SOLVER_TYPE_4ITERS_HARD:
        m_solverIterations = 4;
        m_solverTau = 0.9f;
        m_solverDamp = 1.1f;
        break;
    case SOLVER_TYPE_8ITERS_SOFT:
        m_solverTau = 0.3f;
        m_solverIterations = 8;
        m_solverDamp = 0.9f;
        break;
    case SOLVER_TYPE_8ITERS_MEDIUM:
        m_solverTau = 0.6f;
        m_solverIterations = 8;
        m_solverDamp = 1.0f;
        break;
    case SOLVER_TYPE_8ITERS_HARD:
        m_solverIterations = 8;
        m_solverTau = 0.9f;
        m_solverDamp = 1.1f;
        break;
    default:
        break;
    }
}
