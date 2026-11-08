// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/hkpworld.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/util/hkpworldoperationqueue.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/hkcontainerallocators.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/debugutil/multithreadcheck/hkmultithreadcheck.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkvector4f.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/object/hkreferencedobject.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkrefptr.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/physics/hkstepinfo.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraintsolver/solve/hkpsolverinfo.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/hkpworldcinfo.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/memory/hkpworldmemoryavailablewatchdog.h"

class hkAabb;
class hkClass;
class hkCriticalSection;
struct hkInternalClassMember;
class hkJobQueue;
class hkSemaphore;
class hkThreadPool;
class hkdWorld;
class hknpWorld;
class hkpAction;
class hkpActionListener;
class hkpBodyOperation;
class hkpBroadPhase;
class hkpBroadPhaseBorder;
class hkpBroadPhaseBorderListener;
class hkpCdBodyPairCollector;
class hkpCdPointCollector;
class hkpCollidable;
class hkpCollisionDispatcher;
class hkpCollisionFilter;
struct hkpCollisionInput;
class hkpConstraintData;
class hkpConstraintInstance;
class hkpConstraintListener;
class hkpContactImpulseLimitBreachedListener;
class hkpContactListener;
class hkpConvexListFilter;
struct hkpDebugInfoOnPendingOperationQueues;
class hkpEntity;
class hkpEntityEntityBroadPhaseListener;
class hkpEntityListener;
class hkpIslandActivationListener;
class hkpIslandPostCollideListener;
class hkpIslandPostIntegrateListener;
struct hkpLinearCastInput;
struct hkpMtThreadStructure;
struct hkpMultithreadConfig;
class hkpPhantom;
class hkpPhantomBroadPhaseListener;
class hkpPhantomListener;
class hkpPhysicsSystem;
struct hkpProcessCollisionInput;
class hkpRayHitCollector;
class hkpRigidBody;
class hkpSimulation;
class hkpSimulationIsland;
enum hkpStepResult;
class hkpTypedBroadPhaseDispatcher;
struct hkpViolatedConstraintArray;
class hkpWorldDeletionListener;
class hkpWorldExtension;
struct hkpWorldGetClosestPointsCommand;
struct hkpWorldLinearCastCommand;
class hkpWorldMaintenanceMgr;
class hkpWorldOperationQueue;
class hkpWorldPostCollideListener;
class hkpWorldPostIntegrateListener;
class hkpWorldPostSimulationListener;
struct hkpWorldRayCastCommand;
struct hkpWorldRayCastInput;
struct hkpWorldRayCastOutput;

enum hkpUpdateCollisionFilterOnWorldMode
{
    HK_UPDATE_FILTER_ON_WORLD_FULL_CHECK = 0,
    HK_UPDATE_FILTER_ON_WORLD_DISABLE_ENTITY_ENTITY_COLLISIONS_ONLY = 1,
};

enum hkpUpdateCollisionFilterOnEntityMode
{
    HK_UPDATE_FILTER_ON_ENTITY_FULL_CHECK = 0,
    HK_UPDATE_FILTER_ON_ENTITY_DISABLE_ENTITY_ENTITY_COLLISIONS_ONLY = 1,
};

enum hkpEntityActivation
{
    HK_ENTITY_ACTIVATION_DO_NOT_ACTIVATE = 0,
    HK_ENTITY_ACTIVATION_DO_ACTIVATE = 1,
};

enum hkpUpdateCollectionFilterMode
{
    HK_UPDATE_COLLECTION_FILTER_IGNORE_SHAPE_COLLECTIONS = 0,
    HK_UPDATE_COLLECTION_FILTER_PROCESS_SHAPE_COLLECTIONS = 1,
};

// size 0x160
struct alignas(16) hkpWorldDynamicsStepInfo
{
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpWorldDynamicsStepInfo);
    hkpWorldDynamicsStepInfo(); // 0x00DEAAB0

    hkStepInfo m_stepInfo; // 0x00
    hkpSolverInfo m_solverInfo; // 0x10
};

// size 0x380
class alignas(16) hkpWorld : public hkReferencedObject
{
public:
    enum ReintegrationRecollideMode
    {
        RR_MODE_REINTEGRATE = 1,
        RR_MODE_RECOLLIDE_BROADPHASE = 2,
        RR_MODE_RECOLLIDE_NARROWPHASE = 4,
        RR_MODE_ALL = 7,
    };

    enum MtAccessChecking
    {
        MT_ACCESS_CHECKING_ENABLED = 0,
        MT_ACCESS_CHECKING_DISABLED = 1,
    };

    enum CachedAabbUpdate
    {
        SHIFT_BROADPHASE_UPDATE_ENTITY_AABBS = 0,
        SHIFT_BROADPHASE_IGNORE_ENTITY_AABBS = 1,
    };

    // size 0x1
    class IgnoreForceMultithreadedSimulation
    {
    public:
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(IgnoreForceMultithreadedSimulation);
        IgnoreForceMultithreadedSimulation();
        ~IgnoreForceMultithreadedSimulation();

        hkBool m_originalForceMultithreadedSimulationValue; // 0x00
    };

    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int, void* p) 0x00DEF110, new(unsigned int nbytes) 0x00814610, delete(void* p, unsigned int nbytes) 0x00DED700
    static const hkClass& staticClass(); // 0x00DEF120
    hkpWorld(hkFinishLoadedObjectFlag flag); // 0x00DE34E0
    hkpWorld(const hkpWorldCinfo& infoBase, unsigned int sdkversion); // 0x00DE7220
    void getCinfo(hkpWorldCinfo& info) const; // 0x00DE2530
    hkpEntity* addEntity(hkpEntity* entity, hkpEntityActivation initialActivationState); // 0x00DE13B0
    hkBool removeEntity(hkpEntity* entity); // 0x00DE1550
    void addEntityBatch(hkpEntity* const * entityBatch, int numEntities, hkpEntityActivation initialActivationState); // 0x00DE3E30
    void removeEntityBatch(hkpEntity* const * entityBatch, int numEntities); // 0x00DE44D0
    hkpConstraintInstance* addConstraint(hkpConstraintInstance* constraint); // 0x00DE6C70
    hkBool removeConstraint(hkpConstraintInstance* constraint); // 0x00DE6DE0
    hkpConstraintInstance* createAndAddConstraintInstance(hkpRigidBody* bodyA, hkpRigidBody* bodyB, hkpConstraintData* constraintData); // 0x00DE6D80
    hkpAction* addAction(hkpAction* action); // 0x00DE48C0
    void removeAction(hkpAction* action); // 0x00DE6EB0
    void removeActionImmediately(hkpAction* action); // 0x00DE4A60
    hkpPhantom* addPhantom(hkpPhantom* phantom); // 0x00DE2E10
    void addPhantomBatch(hkpPhantom* const * phantomBatch, int numPhantoms); // 0x00DE4DB0
    void removePhantom(hkpPhantom* phantom); // 0x00DE2380
    void removePhantomBatch(hkpPhantom* const * phantomBatch, int numPhantoms); // 0x00DE5220
    void activateRegion(const hkAabb& aabb); // 0x00DE33F0
    void addPhysicsSystem(const hkpPhysicsSystem* sys); // 0x00DE6EE0
    void removePhysicsSystem(const hkpPhysicsSystem* sys); // 0x00DE7030
    const hkpRigidBody* getFixedRigidBody() const;
    hkpRigidBody* getFixedRigidBody(); // 0x00E05A10
    void updateCollisionFilterOnEntity(hkpEntity* entity, hkpUpdateCollisionFilterOnEntityMode updateMode, hkpUpdateCollectionFilterMode updateShapeCollectionFilter); // 0x00DE38D0
    void reenableCollisionBetweenEntityPair(hkpEntity* entityA, hkpEntity* entityB); // 0x00DE1290
    void updateCollisionFilterOnPhantom(hkpPhantom* phantom, hkpUpdateCollectionFilterMode updateShapeCollectionFilter); // 0x00DE3630
    void updateCollisionFilterOnWorld(hkpUpdateCollisionFilterOnWorldMode updateMode, hkpUpdateCollectionFilterMode updateShapeCollectionFilter); // 0x00DE68A0
    void reintegrateAndRecollideEntities(hkpEntity** entityBatch, int numEntities, ReintegrationRecollideMode mode); // 0x00DE1350
    const hkArray<hkpPhantom*,hkContainerHeapAllocator>& getPhantoms() const; // 0x00DE8710
    void setGravity(const hkVector4f& gravity); // 0x00DE0E20
    const hkVector4f& getGravity() const; // 0x0080D040 (hkpworld.inl:50)
    void findInitialContactPoints(hkpEntity** entities, int numEntities); // 0x00DE20F0
    void findInitialContactPointsOfAllEntities(); // 0x00DE2160
    void calcRequiredSolverBufferSize(hkWorldMemoryAvailableWatchDog::MemUsageInfo& infoOut); // 0x00DE21B0
    void checkDeterminism(); // 0x00DE0F40
    void checkDeterminismOfIslandBroadPhase(const hkpSimulationIsland* island); // 0x00DE0F30
    hkWorldMemoryAvailableWatchDog* getMemoryWatchDog() const; // 0x00DE2830
    void setMemoryWatchDog(hkWorldMemoryAvailableWatchDog* watchDog); // 0x00DE2840
    void lock(); // 0x00DE1020
    void lockReadOnly(); // 0x00DE1040
    void unlock(); // 0x00DE1030
    void unlockReadOnly(); // 0x00DE1050
    bool checkUnmarked(); // 0x00DE1060
    void lockIslandForConstraintUpdate(hkpSimulationIsland* island); // 0x00DE2260
    void lockForIslandSplit(hkpSimulationIsland* island); // 0x00DE1070
    void unlockIslandForConstraintUpdate(hkpSimulationIsland* island); // 0x00DE2280
    void unlockForIslandSplit(hkpSimulationIsland* island); // 0x00DE22A0
    void setMultithreadedAccessChecking(MtAccessChecking accessCheckState); // 0x00DE1090
    MtAccessChecking getMultithreadedAccessChecking() const; // 0x00DE22C0
    void markForRead() const; // 0x0080D080 (hkpworld.inl:209)
    void markForWrite(); // 0x0080D090 (hkpworld.inl:213)
    void unmarkForRead() const; // 0x0080D0A0 (hkpworld.inl:217)
    void unmarkForWrite(); // 0x0080D0B0 (hkpworld.inl:221)
    void addActionListener(hkpActionListener* worldListener); // 0x00DE2F10
    void removeActionListener(hkpActionListener* worldListener); // 0x00DE1630
    void addConstraintListener(hkpConstraintListener* worldListener); // 0x00DE2F50
    void removeConstraintListener(hkpConstraintListener* worldListener); // 0x00DE1680
    void addEntityListener(hkpEntityListener* worldListener); // 0x00DE2F90
    void removeEntityListener(hkpEntityListener* worldListener); // 0x00DE16D0
    void addPhantomListener(hkpPhantomListener* worldListener); // 0x00DE2FD0
    void removePhantomListener(hkpPhantomListener* worldListener); // 0x00DE1720
    void addIslandActivationListener(hkpIslandActivationListener* worldListener); // 0x00DE3010
    void removeIslandActivationListener(hkpIslandActivationListener* worldListener); // 0x00DE1770
    void addWorldPostCollideListener(hkpWorldPostCollideListener* worldListener); // 0x00DE3050
    void removeWorldPostCollideListener(hkpWorldPostCollideListener* worldListener); // 0x00DE17C0
    void addWorldPostSimulationListener(hkpWorldPostSimulationListener* worldListener); // 0x00DE3090
    void removeWorldPostSimulationListener(hkpWorldPostSimulationListener* worldListener); // 0x00DE1810
    void addWorldPostIntegrateListener(hkpWorldPostIntegrateListener* worldListener); // 0x00DE30D0
    void removeWorldPostIntegrateListener(hkpWorldPostIntegrateListener* worldListener); // 0x00DE1860
    void addIslandPostCollideListener(hkpIslandPostCollideListener* islandListener); // 0x00DE3110
    void removeIslandPostCollideListener(hkpIslandPostCollideListener* islandListener); // 0x00DE18B0
    void addIslandPostIntegrateListener(hkpIslandPostIntegrateListener* islandListener); // 0x00DE3150
    void removeIslandPostIntegrateListener(hkpIslandPostIntegrateListener* islandListener); // 0x00DE1900
    void addContactListener(hkpContactListener* collisionListener); // 0x00DE3190
    void removeContactListener(hkpContactListener* collisionListener); // 0x00DE1950
    const hkArray<hkpContactListener*,hkContainerHeapAllocator>& getContactListeners() const;
    void addWorldDeletionListener(hkpWorldDeletionListener* worldListener); // 0x00DE31D0
    void removeWorldDeletionListener(hkpWorldDeletionListener* worldListener); // 0x00DE19A0
    void addContactImpulseLimitBreachedListener(hkpContactImpulseLimitBreachedListener* listener); // 0x00DE3210
    void removeContactImpulseLimitBreachedListener(hkpContactImpulseLimitBreachedListener* listener); // 0x00DE19F0
    void addWorldExtension(hkpWorldExtension* extension); // 0x00DE3250
    void removeWorldExtension(hkpWorldExtension* extension); // 0x00DE2480
    hkpWorldExtension* findWorldExtension(int id) const; // 0x00DE1A40
    const hkpBroadPhase* getBroadPhase() const; // 0x00DE86F0
    hkpBroadPhase* getBroadPhase(); // 0x0080D020 (hkpworld.inl:31)
    const hkpProcessCollisionInput* getCollisionInput() const; // 0x00742380 (hkpworld.inl:11)
    hkpProcessCollisionInput* getCollisionInputRw(); // 0x00DFF320
    hkpSolverInfo* getSolverInfo();
    hkpCollisionDispatcher* getCollisionDispatcher() const; // 0x0080D030 (hkpworld.inl:44)
    const hkpCollisionFilter* getCollisionFilter() const; // 0x0080D070 (hkpworld.inl:96)
    void setCollisionFilter(hkpCollisionFilter* filter, hkBool runUpdateCollisionFilterOnWorld, hkpUpdateCollisionFilterOnWorldMode updateMode, hkpUpdateCollectionFilterMode updateShapeCollectionFilter); // 0x00DE7170
    void castRay(const hkpWorldRayCastInput& input, hkpRayHitCollector& collector) const; // 0x00DE1BF0
    void castRay(const hkpWorldRayCastInput& input, hkpWorldRayCastOutput& output) const; // 0x00DE1CA0
    void castRayMt(hkpWorldRayCastCommand* commandArray, int numCommands, hkJobQueue* jobQueue, hkThreadPool* threadPool, hkSemaphore* semaphore, int numCommandsPerJob) const; // 0x00DE2870
    void linearCast(const hkpCollidable* collA, const hkpLinearCastInput& input, hkpCdPointCollector& castCollector, hkpCdPointCollector* startPointCollector) const; // 0x00DE1D50
    void linearCastMt(hkpWorldLinearCastCommand* commandArray, int numCommands, hkJobQueue* jobQueue, hkThreadPool* threadPool, hkSemaphore* semaphore, int numCommandsPerJob) const; // 0x00DE2B80
    void getClosestPoints(const hkpCollidable* collA, const hkpCollisionInput& input, hkpCdPointCollector& collector) const; // 0x00DE5970
    void getClosestPointsMt(hkpWorldGetClosestPointsCommand* commandArray, int numCommands, hkJobQueue* jobQueue, hkThreadPool* threadPool, hkSemaphore* semaphore, int numCommandsPerJob) const; // 0x00DE29E0
    void getPenetrations(const hkpCollidable* collA, const hkpCollisionInput& input, hkpCdBodyPairCollector& collector) const; // 0x00DE5B50
    hkpBroadPhaseBorder* getBroadPhaseBorder() const; // 0x00DE0F50
    void setBroadPhaseBorder(hkpBroadPhaseBorder* b); // 0x00DE1BB0
    void shiftBroadPhase(const hkVector4f& shiftDistance, hkVector4f& effectiveShiftDistanceOut, CachedAabbUpdate updateAabbs); // 0x00DE55B0
    const hkArray<hkpSimulationIsland*,hkContainerHeapAllocator>& getActiveSimulationIslands() const; // 0x0080D060 (hkpworld.inl:82)
    const hkArray<hkpSimulationIsland*,hkContainerHeapAllocator>& getInactiveSimulationIslands() const; // 0x0080D050 (hkpworld.inl:76)
    const hkpSimulationIsland* getFixedIsland() const; // 0x00DE8700
    hkpStepResult stepDeltaTime(float physicsDeltaTime); // 0x00DE1A70
    hkpStepResult integrate(float physicsDeltaTime); // 0x00DE1B10
    hkpStepResult collide(); // 0x00DE1B40
    hkpStepResult advanceTime(); // 0x00DE1B70
    void setFrameTimeMarker(float frameDeltaTime); // 0x00DE0E50
    bool isSimulationAtMarker() const; // 0x00DE0E70
    bool isSimulationAtPsi() const; // 0x00DE0E80
    static void registerWithJobQueue(hkJobQueue*);
    hkpStepResult stepMultithreaded(hkJobQueue* jobQueue, hkThreadPool* threadPool, float physicsDeltaTime); // 0x00DE32A0
    hkpStepResult initMtStep(hkJobQueue* jobQueue, float physicsDeltaTime); // 0x00DE24F0
    hkpStepResult finishMtStep(hkJobQueue* jobQueue, hkThreadPool* threadPool); // 0x00DE0E90
    float getCurrentTime() const; // 0x00DE0E30
    float getCurrentPsiTime() const; // 0x00DE0E40
    void getMultithreadConfig(hkpMultithreadConfig& config); // 0x00DE0F10
    void setMultithreadConfig(const hkpMultithreadConfig& config, hkJobQueue* queue); // 0x00DE0F20
    hkMultiThreadCheck& getMultiThreadCheck() const;
    void checkAccessGetActiveSimulationIslands() const; // 0x00DE1BA0
    hkpPhysicsSystem* getWorldAsOneSystem() const; // 0x00DE2DB0
    void getWorldAsSystems(hkArray<hkpPhysicsSystem*,hkContainerHeapAllocator>& systemsInOut) const; // 0x00DE32F0
    virtual ~hkpWorld(); // vtbl+0x00 0x00DE5FA0
    void removeAll(); // 0x00DE5D60
    void attachActionToEntity(hkpAction* action, hkpEntity* entity); // 0x00DE22E0
    void detachActionFromEntity(hkpAction* action, hkpEntity* entity); // 0x00DE4CA0
    void lockCriticalOperations(); // 0x00DE8720
    void unlockCriticalOperations(); // 0x00DE8730
    int areCriticalOperationsLocked() const; // 0x00710FD0 (hkpworld.inl:127)
    int areCriticalOperationsLockedUnchecked() const; // 0x00E0A840
    void unlockCriticalOperationsForPhantoms(); // 0x00DF5C10
    void lockCriticalOperationsForPhantoms(); // 0x00DF5C00
    int areCriticalOperationsLockedForPhantoms() const; // 0x00DE8740
    void blockExecutingPendingOperations(hkBool block); // 0x00DE8750
    void unlockAndAttemptToExecutePendingOperations(); // 0x00DEA580
    void attemptToExecutePendingOperations(bool executeBodyOperations); // 0x00DEA530
    void queueOperation(const hkWorldOperation::BaseOperation& operation); // 0x00DE1000
    hkWorldOperation::UserCallback* queueCallback(hkWorldOperation::UserCallback* callback, unsigned long userData); // 0x00DE20A0
    void addBodyOperation(hkpRigidBody* breakingBody, hkpBodyOperation* operation, int priority, int hint); // 0x00DE1010
    void checkConstraintsViolated(); // 0x00DE0FA0
public:
    void internal_executePendingOperations(); // 0x00DE0F60
    void internal_executePendingBodyOperations(); // 0x00DE0F80
public:
    void allowCriticalOperations(hkBool allow); // 0x00DE8760

    static const hkInternalClassMember Members[];
    hkpSimulation* m_simulation; // 0x08
    hkVector4f m_gravity; // 0x10
    hkpSimulationIsland* m_fixedIsland; // 0x20
    hkpRigidBody* m_fixedRigidBody; // 0x24
    hkArray<hkpSimulationIsland*,hkContainerHeapAllocator> m_activeSimulationIslands; // 0x28
    hkArray<hkpSimulationIsland*,hkContainerHeapAllocator> m_inactiveSimulationIslands; // 0x34
    hkArray<hkpSimulationIsland*,hkContainerHeapAllocator> m_dirtySimulationIslands; // 0x40
    hkpWorldMaintenanceMgr* m_maintenanceMgr; // 0x4C
    hkRefPtr<hkWorldMemoryAvailableWatchDog> m_memoryWatchDog; // 0x50
    hkBool m_assertOnRunningOutOfSolverMemory; // 0x54
    hkEnum<enum hkpWorldCinfo::BroadPhaseType,signed char> m_broadPhaseType; // 0x55
    hkpBroadPhase* m_broadPhase; // 0x58
    hkpTypedBroadPhaseDispatcher* m_broadPhaseDispatcher; // 0x5C
    hkpPhantomBroadPhaseListener* m_phantomBroadPhaseListener; // 0x60
    hkpEntityEntityBroadPhaseListener* m_entityEntityBroadPhaseListener; // 0x64
    hkpBroadPhaseBorderListener* m_broadPhaseBorderListener; // 0x68
    hkpMtThreadStructure* m_multithreadedSimulationJobData; // 0x6C
    hkpProcessCollisionInput* m_collisionInput; // 0x70
    hkpCollisionFilter* m_collisionFilter; // 0x74
    hkpCollisionDispatcher* m_collisionDispatcher; // 0x78
    hkpConvexListFilter* m_convexListFilter; // 0x7C
    hkpWorldOperationQueue* m_pendingOperations; // 0x80
    int m_pendingOperationsCount; // 0x84
    int m_pendingBodyOperationsCount; // 0x88
    int m_criticalOperationsLockCount; // 0x8C
    int m_criticalOperationsLockCountForPhantoms; // 0x90
    hkBool m_blockExecutingPendingOperations; // 0x94
    hkBool m_criticalOperationsAllowed; // 0x95
    hkpDebugInfoOnPendingOperationQueues* m_pendingOperationQueues; // 0x98
    int m_pendingOperationQueueCount; // 0x9C
    hkMultiThreadCheck m_multiThreadCheck; // 0xA0
    hkBool m_processActionsInSingleThread; // 0xAC
    hkBool m_allowIntegrationOfIslandsWithoutConstraintsInASeparateJob; // 0xAD
    unsigned int m_minDesiredIslandSize; // 0xB0
    hkCriticalSection* m_modifyConstraintCriticalSection; // 0xB4
    int m_isLocked; // 0xB8
    hkCriticalSection* m_islandDirtyListCriticalSection; // 0xBC
    hkCriticalSection* m_propertyMasterLock; // 0xC0
    hkBool m_wantSimulationIslands; // 0xC4
    float m_snapCollisionToConvexEdgeThreshold; // 0xC8
    float m_snapCollisionToConcaveEdgeThreshold; // 0xCC
    hkBool m_enableToiWeldRejection; // 0xD0
    hkBool m_wantDeactivation; // 0xD1
    hkBool m_shouldActivateOnRigidBodyTransformChange; // 0xD2
    float m_deactivationReferenceDistance; // 0xD4
    float m_toiCollisionResponseRotateNormal; // 0xD8
    int m_maxSectorsPerMidphaseCollideTask; // 0xDC
    int m_maxSectorsPerNarrowphaseCollideTask; // 0xE0
    hkBool m_processToisMultithreaded; // 0xE4
    int m_maxEntriesPerToiMidphaseCollideTask; // 0xE8
    int m_maxEntriesPerToiNarrowphaseCollideTask; // 0xEC
    int m_maxNumToiCollisionPairsSinglethreaded; // 0xF0
    hkEnum<enum hkpWorldCinfo::SimulationType,int> m_simulationType; // 0xF4
    float m_numToisTillAllowedPenetrationSimplifiedToi; // 0xF8
    float m_numToisTillAllowedPenetrationToi; // 0xFC
    float m_numToisTillAllowedPenetrationToiHigher; // 0x100
    float m_numToisTillAllowedPenetrationToiForced; // 0x104
    static hkBool m_forceMultithreadedSimulation; // 0x03A29D72
    unsigned int m_lastEntityUid; // 0x108
    unsigned int m_lastIslandUid; // 0x10C
    unsigned int m_lastConstraintUid; // 0x110
    hkArray<hkpPhantom*,hkContainerHeapAllocator> m_phantoms; // 0x114
    hkArray<hkpActionListener*,hkContainerHeapAllocator> m_actionListeners; // 0x120
    hkArray<hkpEntityListener*,hkContainerHeapAllocator> m_entityListeners; // 0x12C
    hkArray<hkpPhantomListener*,hkContainerHeapAllocator> m_phantomListeners; // 0x138
    hkArray<hkpConstraintListener*,hkContainerHeapAllocator> m_constraintListeners; // 0x144
    hkArray<hkpWorldDeletionListener*,hkContainerHeapAllocator> m_worldDeletionListeners; // 0x150
    hkArray<hkpIslandActivationListener*,hkContainerHeapAllocator> m_islandActivationListeners; // 0x15C
    hkArray<hkpWorldPostSimulationListener*,hkContainerHeapAllocator> m_worldPostSimulationListeners; // 0x168
    hkArray<hkpWorldPostIntegrateListener*,hkContainerHeapAllocator> m_worldPostIntegrateListeners; // 0x174
    hkArray<hkpWorldPostCollideListener*,hkContainerHeapAllocator> m_worldPostCollideListeners; // 0x180
    hkArray<hkpIslandPostIntegrateListener*,hkContainerHeapAllocator> m_islandPostIntegrateListeners; // 0x18C
    hkArray<hkpIslandPostCollideListener*,hkContainerHeapAllocator> m_islandPostCollideListeners; // 0x198
    hkArray<hkpContactListener*,hkContainerHeapAllocator> m_contactListeners; // 0x1A4
    hkArray<hkpContactImpulseLimitBreachedListener*,hkContainerHeapAllocator> m_contactImpulseLimitBreachedListeners; // 0x1B0
    hkArray<hkpWorldExtension*,hkContainerHeapAllocator> m_worldExtensions; // 0x1BC
    hkpViolatedConstraintArray* m_violatedConstraintArray; // 0x1C8
    hkpBroadPhaseBorder* m_broadPhaseBorder; // 0x1CC
    hkdWorld* m_destructionWorld; // 0x1D0
    hknpWorld* m_npWorld; // 0x1D4
    hkpWorldDynamicsStepInfo m_dynamicsStepInfo; // 0x1E0
    hkVector4f m_broadPhaseExtents[2]; // 0x340
    int m_broadPhaseNumMarkers; // 0x360
    int m_sizeOfToiEventQueue; // 0x364
    int m_broadPhaseQuerySize; // 0x368
    int m_broadPhaseUpdateSize; // 0x36C
    hkEnum<enum hkpWorldCinfo::ContactPointGeneration,signed char> m_contactPointGeneration; // 0x370
    hkBool m_useCompoundSpuElf; // 0x371
};
