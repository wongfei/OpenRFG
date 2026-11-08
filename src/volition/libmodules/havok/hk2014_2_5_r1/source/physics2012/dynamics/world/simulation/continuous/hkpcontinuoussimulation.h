// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/simulation/continuous/hkpcontinuoussimulation.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/hkcontainerallocators.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/localarray/hklocalarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/localarray/hklocalbuffer.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/pointermap/hkpointermap.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/hkpconstraintinstance.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/simulation/hkpsimulation.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/dynamics/world/simulation/continuous/hkptoievent.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/dynamics/world/simulation/continuous/toiresourcemgr/hkptoiresourcemgr.h"

class hkBool;
struct hkResult;
class hkStepInfo;
struct hkpAgentNnEntry;
class hkpBroadPhaseHandlePair;
class hkpConstraintQueryIn;
struct hkpConstraintSchemaInfo;
struct hkpConstraintSolverResources;
class hkpEntity;
struct hkpProcessCollisionInput;
struct hkpProcessCollisionOutput;
class hkpSimulationIsland;
struct hkpSolverInfo;
enum hkpStepResult;
struct hkpToiResources;
class hkpWorld;

// size 0x50
class hkpContinuousSimulation : public hkpSimulation
{
public:
    enum CollisionQualityOverride
    {
        PROCESS_NORMALLY = 0,
        DISABLE_TOIS = 1,
        DISABLE_TOIS_AND_EXPAND_MANIFOLD = 2,
    };

    // size 0x4
    class LockCriticalOperationsScope
    {
    public:
        LockCriticalOperationsScope(hkpWorld* world); // 0x00DFF3D0
        ~LockCriticalOperationsScope(); // 0x00DFF3F0

        hkpWorld* m_world; // 0x00
    };

    // size 0x1
    struct IsExportFinished
    {
        bool operator()(signed char actualVal) const; // 0x00DFF360

    };

    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int nbytes) 0x00DFFC70, delete(void* p, unsigned int nbytes) 0x00E00C70
    hkpContinuousSimulation(hkpWorld* world); // 0x00DFB940
    virtual ~hkpContinuousSimulation(); // vtbl+0x00 0x00DFB9E0
    static hkpSimulation* create(hkpWorld* world); // 0x00DFBA50
    virtual hkpStepResult advanceTime(); // vtbl+0x1C 0x00DFEB60
    virtual void resetCollisionInformationForEntities(hkpEntity** entities, int numEntities, hkpWorld* world, hkpSimulation::ResetCollisionInformation resetInfo); // vtbl+0x34 0x00DFAB60
    virtual void assertThereIsNoCollisionInformationForEntities(hkpEntity** entities, int numEntities, hkpWorld* world); // vtbl+0x38 0x00DFA300
    virtual void removeCollisionInformationForAgent(hkpAgentNnEntry* agent); // vtbl+0x3C 0x00DFA480
    virtual void assertThereIsNoCollisionInformationForAgent(hkpAgentNnEntry* agent); // vtbl+0x40 0x00DFA320
    virtual void reintegrateAndRecollideEntities(hkpEntity** entities, int numEntities, hkpWorld* world, int reintegrateRecollideMode); // vtbl+0x44 0x00DFD790
    virtual void warpTime(float warpDeltaTime); // vtbl+0x4C 0x00DFA330
    void collideEntitiesBroadPhaseContinuous(hkpEntity** entities, int numEntities, hkpWorld* world, signed char* exportFinished); // 0x00DFBAB0
    void collideEntitiesBroadPhaseContinuousFindPairs(hkpEntity** entities, int numEntities, hkpWorld* world, hkArray<hkpBroadPhaseHandlePair,hkContainerHeapAllocator>& newPairs, hkArray<hkpBroadPhaseHandlePair,hkContainerHeapAllocator>& delPairs); // 0x00DFABB0
    void collideIslandNarrowPhaseContinuous(hkpSimulationIsland* isle, const hkpProcessCollisionInput& input); // 0x00DFAF30
    void collideEntitiesNarrowPhaseContinuous(hkpEntity** entities, int numEntities, const hkpProcessCollisionInput& input); // 0x00DFBA90
public:
    void processAgentCollideContinuous(hkpAgentNnEntry* entry, const hkpProcessCollisionInput& processInput, hkpProcessCollisionOutput& processOutput); // 0x00DFB160
    virtual void collideInternal(const hkStepInfo& stepInfoIn); // vtbl+0x48 0x00DFD940
    hkpStepResult advanceTimeInternal(); // 0x00DFD590
public:
    void addToiEvent(const hkpProcessCollisionOutput& processOutput, const hkpAgentNnEntry& entry); // 0x00DFA4E0
public:
    void removeToiEventsOfEntities(hkpEntity** entities, int numEntities); // 0x00DFA640
    void removeToiEventsOfEntity(hkpEntity* entity); // 0x00DFA780
    void fireToiEventRemoved(hkpToiEvent& event); // 0x00E006C0
    hkpStepResult handleAllToisTill(float minTime); // 0x00DFD240
    void collideEntitiesOfOneIslandContinuous_toiOnly(hkpEntity** entities, int numEntities, hkpWorld* world, const hkpProcessCollisionInput& input, hkPointerMap<unsigned int,hkpEntity*,hkContainerHeapAllocator>& entitiesNeedingPsiCollisionDetection); // 0x00E00FE0
    virtual void collideEntitiesOfOneIslandNarrowPhaseContinuous_toiOnly(hkpEntity** entities, int numEntities, const hkpProcessCollisionInput& input, hkPointerMap<unsigned int,hkpEntity*,hkContainerHeapAllocator>& entitiesNeedingPsiCollisionDetection); // vtbl+0x50 0x00DFC190
    virtual void collideEntitiesNeedingPsiCollisionDetectionNarrowPhase_toiOnly(const hkpProcessCollisionInput& input, hkPointerMap<unsigned int,hkpEntity*,hkContainerHeapAllocator>& entitiesNeedingPsiCollisionDetection); // vtbl+0x54 0x00DFC560
    void deleteToiEventsForActiveEntitiesInIsland(const hkpSimulationIsland* island, const hkFixedArray<unsigned char>& entityState, hkArray<hkpToiEvent,hkContainerHeapAllocator>& toiEvents); // 0x00E006F0
public:
    void waitForSolverExport(signed char* exportFinished); // 0x00E00610
    void removeAndAddPairs(hkpWorld* world, hkpEntity** entities, hkArray<hkpBroadPhaseHandlePair,hkContainerHeapAllocator>& delPairs, hkArray<hkpBroadPhaseHandlePair,hkContainerHeapAllocator>& newPairs); // 0x00DFFC90
public:
    void handleSimpleToi(hkpWorld* world, hkpToiEvent& event, float physicsDeltaTime, float rotateNormal); // 0x00DFC920
    virtual void simulateToi(hkpWorld* world, hkpToiEvent& event, float physicsDeltaTime, float rotateNormal); // vtbl+0x58 0x00DFEC20
public:
    void processAgentNnEntries(hkpAgentNnEntry* const * entries, int numEntries, const hkpProcessCollisionInput& collisionInput, hkpSimulationIsland* island, CollisionQualityOverride collisionQualityOverride); // 0x00DFB210

    alignas(16) hkArray<hkpToiEvent,hkContainerHeapAllocator> m_toiEvents; // 0x30
    hkPointerMap<unsigned int,hkpEntity*,hkContainerHeapAllocator> m_entitiesNeedingPsiCollisionDetection; // 0x3C
    hkpToiResourceMgr* m_toiResourceMgr; // 0x48
    int m_toiCounter; // 0x4C
};

void hkLs_buildStepInfos(hkpWorld* world, const hkpToiEvent& event, float physicsDeltaTime, hkStepInfo& stepInfoOut, hkpSolverInfo& solverInfoOut, hkpConstraintQueryIn& constraintQueryInOut); // 0x00DFFDE0
void hkLs_updateTransformIfNeeded(hkpEntity* entity, float time, hkFixedArray<unsigned char>& entityState); // 0x00E007D0
void hkLs_updateTransform(hkpEntity* entity, float time); // 0x00DFFF90
hkBool hkLs_isConstraintInteresting(hkpConstraintInstance* constraint, hkpConstraintInstance::ConstraintPriority minPriorityToProcess); // 0x00E00810
hkBool hkLs_isAgentInteresting(hkpAgentNnEntry* entry, hkpConstraintInstance::ConstraintPriority minPriorityToProcess); // 0x00DFF680
void hkLs_collectAgentEntriesToProcess(const hkpEntity* entity, hkpConstraintInstance::ConstraintPriority minPriorityToProcess, float time, hkFixedArray<unsigned char>& entityState, hkLocalArray<hkpAgentNnEntry*>& entriesOut); // 0x00DFCBC0
int hkLs_areVelocitiesOk(float psiTimeLeft, const hkpConstraintSchemaInfo& constraintStatus, const float* velocities, int numVelocities, const hkpProcessCollisionInput& processInput); // 0x00DFA810
void hkLs_toiCheckValidityOfConstraints(hkpConstraintSolverResources& solverResources, hkpProcessCollisionInput& processInput, hkArray<hkpConstraintSchemaInfo,hkContainerHeapAllocator>& constraintStatus, int& firstNonActiveConstraintStatus, hkpConstraintInstance::ConstraintPriority minPriorityToProcess, hkFixedArray<unsigned char>& entityState, const hkArray<hkpEntity*,hkContainerHeapAllocator>& touchedEntities, hkArray<hkpEntity*,hkContainerHeapAllocator>& toBeActivated); // 0x00DFB3A0
void hkLs_toiActivateEntitiesAndCheckConstraints(hkpProcessCollisionInput& collisionInput, hkpConstraintInstance::ConstraintPriority minPriorityToProcess, float time, const hkArray<hkpEntity*,hkContainerHeapAllocator>& toBeActivated, hkFixedArray<unsigned char>& entityState, hkArray<hkpEntity*,hkContainerHeapAllocator>& newTouchedEntities, hkArray<hkpConstraintInstance*,hkContainerHeapAllocator>& newTouchedConstraints); // 0x00DFCCF0
void hkLs_toiActivateConstraintsLinkingToFixedAndKeyframedEntities(hkArray<hkpConstraintSchemaInfo,hkContainerHeapAllocator>& constraintStatus, int& firstNonActiveConstraintStatus, int numOfConstraintsToCheckAtTheEndOfTheList); // 0x00DFA8A0
void hkLs_toiActivateConstraintsLinkingActivatedEntities(hkArray<hkpConstraintSchemaInfo,hkContainerHeapAllocator>& constraintStatus, int& firstNonActiveConstraintStatus, hkFixedArray<unsigned char>& entityState); // 0x00DFA930
void hkLs_toiResetVelocityAccumulatorsForEntities(hkpConstraintSolverResources& solverResources, const hkArray<hkpEntity*,hkContainerHeapAllocator>& entities); // 0x00DFA400
void hkLs_backstepAndFreezeEntireIsland(float time, hkpSimulationIsland* island, hkFixedArray<unsigned char>& entityState, hkArray<hkpEntity*,hkContainerHeapAllocator>& activeEntities); // 0x00DFB6F0
void hkLs_restoreTransformOnBodiesWithUpdatedTransform(hkpSimulationIsland* island, hkFixedArray<unsigned char>& entityState, hkpConstraintSolverResources& solverResources); // 0x00DFA9D0
hkBool hkLs_toiCheckFinalValidityOfCriticalConstraints(hkpConstraintSolverResources& solverResources, const hkArray<hkpConstraintSchemaInfo,hkContainerHeapAllocator>& constraintStatus, hkpProcessCollisionInput& processInput, hkArray<hkpToiResourceMgr::ConstraintViolationInfo,hkContainerHeapAllocator>* violatedConstraints); // 0x00DFB7D0
hkResult hkLs_localizedSolveToi(const hkpToiResources& toiResources, hkpConstraintSolverResources& solverResources, hkpToiEvent& event, hkpToiResourceMgr& toiResourceMgr, hkpWorld* world, hkArray<hkpEntity*,hkContainerHeapAllocator>& activeEntities, hkFixedArray<unsigned char>& entityState, float rotateNormal); // 0x00DFDCD0
