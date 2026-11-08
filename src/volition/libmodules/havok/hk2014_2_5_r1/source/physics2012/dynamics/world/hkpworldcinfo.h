// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/hkpworldcinfo.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkvector4f.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/object/hkreferencedobject.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/geometry/aabb/hkaabb.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkrefptr.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/filter/hkpcollisionfilter.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/filter/hkpconvexlistfilter.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/memory/hkpworldmemoryavailablewatchdog.h"

class hkClass;
struct hkInternalClassMember;

// size 0xF0
class hkpWorldCinfo : public hkReferencedObject
{
public:
    enum SolverType
    {
        SOLVER_TYPE_INVALID = 0,
        SOLVER_TYPE_2ITERS_SOFT = 1,
        SOLVER_TYPE_2ITERS_MEDIUM = 2,
        SOLVER_TYPE_2ITERS_HARD = 3,
        SOLVER_TYPE_4ITERS_SOFT = 4,
        SOLVER_TYPE_4ITERS_MEDIUM = 5,
        SOLVER_TYPE_4ITERS_HARD = 6,
        SOLVER_TYPE_8ITERS_SOFT = 7,
        SOLVER_TYPE_8ITERS_MEDIUM = 8,
        SOLVER_TYPE_8ITERS_HARD = 9,
        SOLVER_TYPE_MAX_ID = 10,
    };

    enum SimulationType
    {
        SIMULATION_TYPE_INVALID = 0,
        SIMULATION_TYPE_DISCRETE = 1,
        SIMULATION_TYPE_CONTINUOUS = 2,
        SIMULATION_TYPE_MULTITHREADED = 3,
    };

    enum ContactPointGeneration
    {
        CONTACT_POINT_ACCEPT_ALWAYS = 0,
        CONTACT_POINT_REJECT_DUBIOUS = 1,
        CONTACT_POINT_REJECT_MANY = 2,
    };

    enum BroadPhaseType
    {
        BROADPHASE_TYPE_SAP = 0,
        BROADPHASE_TYPE_TREE = 1,
        BROADPHASE_TYPE_HYBRID = 2,
    };

    enum BroadPhaseBorderBehaviour
    {
        BROADPHASE_BORDER_ASSERT = 0,
        BROADPHASE_BORDER_FIX_ENTITY = 1,
        BROADPHASE_BORDER_REMOVE_ENTITY = 2,
        BROADPHASE_BORDER_DO_NOTHING = 3,
    };

    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int, void* p) 0x00DEF0A0, new(unsigned int nbytes) 0x00F25E10, delete(void* p, unsigned int nbytes) 0x00821DB0
    static const hkClass& staticClass(); // 0x00DEF0B0
    hkpWorldCinfo(hkFinishLoadedObjectFlag flag); // 0x00E041B0
    hkpWorldCinfo(); // 0x00E03F90
    void setupSolverInfo(SolverType st); // 0x00E03DC0
    void setBroadPhaseWorldSize(float sideLength); // 0x00E03F60
    virtual ~hkpWorldCinfo(); // vtbl+0x00 0x00821D60 (.h:586)
    void afterReflectNew(); // 0x00E03F10

    static const hkInternalClassMember Members[];
    alignas(16) hkVector4f m_gravity; // 0x10
    int m_broadPhaseQuerySize; // 0x20
    float m_contactRestingVelocity; // 0x24
    hkEnum<enum BroadPhaseType,signed char> m_broadPhaseType; // 0x28
    hkEnum<enum BroadPhaseBorderBehaviour,signed char> m_broadPhaseBorderBehaviour; // 0x29
    hkBool m_mtPostponeAndSortBroadPhaseBorderCallbacks; // 0x2A
    hkAabb m_broadPhaseWorldAabb; // 0x30
    float m_collisionTolerance; // 0x50
    hkRefPtr<hkpCollisionFilter> m_collisionFilter; // 0x54
    hkRefPtr<hkpConvexListFilter> m_convexListFilter; // 0x58
    float m_expectedMaxLinearVelocity; // 0x5C
    int m_sizeOfToiEventQueue; // 0x60
    float m_expectedMinPsiDeltaTime; // 0x64
    hkRefPtr<hkWorldMemoryAvailableWatchDog> m_memoryWatchDog; // 0x68
    int m_broadPhaseNumMarkers; // 0x6C
    hkEnum<enum ContactPointGeneration,signed char> m_contactPointGeneration; // 0x70
    hkBool m_allowToSkipConfirmedCallbacks; // 0x71
    float m_solverTau; // 0x74
    float m_solverDamp; // 0x78
    int m_solverIterations; // 0x7C
    int m_solverMicrosteps; // 0x80
    float m_maxConstraintViolation; // 0x84
    hkBool m_forceCoherentConstraintOrderingInSolver; // 0x88
    float m_snapCollisionToConvexEdgeThreshold; // 0x8C
    float m_snapCollisionToConcaveEdgeThreshold; // 0x90
    hkBool m_enableToiWeldRejection; // 0x94
    hkBool m_enableDeprecatedWelding; // 0x95
    float m_iterativeLinearCastEarlyOutDistance; // 0x98
    int m_iterativeLinearCastMaxIterations; // 0x9C
    unsigned char m_deactivationNumInactiveFramesSelectFlag0; // 0xA0
    unsigned char m_deactivationNumInactiveFramesSelectFlag1; // 0xA1
    unsigned char m_deactivationIntegrateCounter; // 0xA2
    hkBool m_shouldActivateOnRigidBodyTransformChange; // 0xA3
    float m_deactivationReferenceDistance; // 0xA4
    float m_toiCollisionResponseRotateNormal; // 0xA8
    hkBool m_useCompoundSpuElf; // 0xAC
    int m_maxSectorsPerMidphaseCollideTask; // 0xB0
    int m_maxSectorsPerNarrowphaseCollideTask; // 0xB4
    hkBool m_processToisMultithreaded; // 0xB8
    int m_maxEntriesPerToiMidphaseCollideTask; // 0xBC
    int m_maxEntriesPerToiNarrowphaseCollideTask; // 0xC0
    int m_maxNumToiCollisionPairsSinglethreaded; // 0xC4
    float m_numToisTillAllowedPenetrationSimplifiedToi; // 0xC8
    float m_numToisTillAllowedPenetrationToi; // 0xCC
    float m_numToisTillAllowedPenetrationToiHigher; // 0xD0
    float m_numToisTillAllowedPenetrationToiForced; // 0xD4
    hkBool m_enableDeactivation; // 0xD8
    hkEnum<enum SimulationType,signed char> m_simulationType; // 0xD9
    hkBool m_enableSimulationIslands; // 0xDA
    unsigned int m_minDesiredIslandSize; // 0xDC
    hkBool m_processActionsInSingleThread; // 0xE0
    hkBool m_allowIntegrationOfIslandsWithoutConstraintsInASeparateJob; // 0xE1
    float m_frameMarkerPsiSnap; // 0xE4
    hkBool m_fireCollisionCallbacks; // 0xE8
};
