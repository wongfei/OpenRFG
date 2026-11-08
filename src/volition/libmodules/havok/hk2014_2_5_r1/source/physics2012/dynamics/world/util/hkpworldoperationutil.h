// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/util/hkpworldoperationutil.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/motion/hkpmotion.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/hkpworld.h"

class hkBool;
struct hkContainerHeapAllocator;
class hkpAction;
struct hkpAgentNnTrack;
class hkpConstraintInstance;
class hkpEntity;
class hkpPhantom;
class hkpRigidBody;
class hkpSimulationIsland;

// size 0x1
class hkpWorldOperationUtil
{
public:
    enum FireCallbacks
    {
        DO_NOT_FIRE_CALLBACKS_AND_SUPPRESS_EXECUTION_OF_PENDING_OPERATIONS = 0,
        DO_FIRE_CALLBACKS = 1,
    };

    static void addEntityBP(hkpWorld* world, hkpEntity* entity); // 0x00E08570
    static void removeEntityBP(hkpWorld* world, hkpEntity* entity); // 0x00E08880
    static void updateEntityBP(hkpWorld* world, hkpEntity* entity); // 0x00E081A0
    static void addPhantomBP(hkpWorld* world, hkpPhantom* phantom); // 0x00E08710
    static void removePhantomBP(hkpWorld* world, hkpPhantom* phantom); // 0x00E089E0
    static void addEntitySI(hkpWorld* world, hkpEntity* entity, hkpEntityActivation initialActivationState); // 0x00E06870
    static void removeEntitySI(hkpWorld* world, hkpEntity* entity); // 0x00E06980
    static void addConstraintToCriticalLockedIsland(hkpWorld* world, hkpConstraintInstance* constraint); // 0x00E062E0
    static void removeConstraintFromCriticalLockedIsland(hkpWorld* world, hkpConstraintInstance* constraint); // 0x00E06320
    static hkpConstraintInstance* addConstraintImmediately(hkpWorld* world, hkpConstraintInstance* constraint, FireCallbacks fireCallbacks); // 0x00E06360
    static void removeConstraintImmediately(hkpWorld* world, hkpConstraintInstance* constraint, FireCallbacks fireCallbacks); // 0x00E063F0
    static void internalActivateIsland(hkpWorld* world, hkpSimulationIsland* island, hkBool allowIslandMerges); // 0x00E07530
    static void internalDeactivateIsland(hkpWorld* world, hkpSimulationIsland* island); // 0x00E07810
    static void mergeIslandsIfNeeded(hkpEntity* entityA, hkpEntity* entityB); // 0x00E0B5F0
    static void mergeIslands(hkpWorld* world, hkpEntity* entityA, hkpEntity* entityB); // 0x00E07FB0
    static void internalMergeTwoIslands(hkpWorld* world, hkpSimulationIsland* islandA, hkpSimulationIsland* islandB); // 0x00E07020
    static void removeIsland(hkpWorld* world, hkpSimulationIsland* island); // 0x00E06570
    static void splitSimulationIsland(hkpWorld* world, hkpSimulationIsland* currentIsland); // 0x00E0A190
    static void splitSimulationIsland(hkpSimulationIsland* currentIsland, hkpWorld* world, hkArray<hkpSimulationIsland*,hkContainerHeapAllocator>& newIslandsOut, hkArray<hkpEntity*,hkContainerHeapAllocator>* oldEntitiesOut); // 0x00E08E60
    static void splitSimulationIslands(hkpWorld* world); // 0x00E0A250
    static void validateIsland(hkpWorld*, hkpSimulationIsland*);
    static void validateWorld(hkpWorld* world); // 0x00E064C0
    static void removeAttachedActionsFromFixedIsland(hkpWorld* world, hkpEntity* entity, hkArray<hkpAction*,hkContainerHeapAllocator>& actionsToBeMoved); // 0x00E069D0
    static void removeAttachedActionsFromDynamicIsland(hkpWorld* world, hkpEntity* entity, hkArray<hkpAction*,hkContainerHeapAllocator>& actionsToBeMoved); // 0x00E08B10
    static void addActionsToEntitysIsland(hkpWorld* world, hkpEntity* entity, hkArray<hkpAction*,hkContainerHeapAllocator>& actionsToBeMoved); // 0x00E08CC0
    static void setRigidBodyMotionType(hkpRigidBody* body, hkpMotion::MotionType newState, hkpEntityActivation initialActivationState, hkpUpdateCollisionFilterOnEntityMode collisionFilterUpdateMode); // 0x00E0A290
public:
    static void replaceMotionObject(hkpRigidBody* body, hkpMotion::MotionType newState, hkBool newStateNeedsInertia, hkBool oldStateNeedsInertia, hkpWorld* world); // 0x00E07AA0
    static void sortBigIslandToFront(hkpWorld* world, hkpSimulationIsland* island); // 0x00E0ACD0
    static void removeAttachedAgentsConnectingTheEntityAndAFixedPartnerEntityPlus(hkpAgentNnTrack& trackToScan, hkpEntity* entity, hkpAgentNnTrack& agentsRemoved, hkpMotion::MotionType newMotionType); // 0x00E067A0
    static void removeAttachedConstraints(hkpEntity* entity, hkArray<hkpConstraintInstance*,hkContainerHeapAllocator>& removedConstraints); // 0x00E073E0
public:
    static void cleanupDirtyIslands(hkpWorld* world); // 0x00E08010
    static void markIslandInactive(hkpWorld* world, hkpSimulationIsland* island); // 0x00E07930
    static void markIslandInactiveMt(hkpWorld* world, hkpSimulationIsland* island); // 0x00E079A0
    static void markIslandActive(hkpWorld* world, hkpSimulationIsland* island); // 0x00E07A10
    static void removeIslandFromDirtyList(hkpWorld* world, hkpSimulationIsland* island); // 0x00E06540
    static int estimateIslandSize(int numEntities, int numConstraints); // 0x00E0ACA0
    static bool canIslandBeSparse(hkpWorld* world, int size); // 0x00E0A860
    static void putIslandOnDirtyList(hkpWorld* world, hkpSimulationIsland* island); // 0x00DEC870

};
