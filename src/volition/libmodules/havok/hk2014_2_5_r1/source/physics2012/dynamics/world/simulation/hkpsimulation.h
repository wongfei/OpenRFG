// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/simulation/hkpsimulation.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/object/hkreferencedobject.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"

class hkClass;
struct hkContainerHeapAllocator;
struct hkInternalClassMember;
class hkJobQueue;
class hkStepInfo;
class hkThreadPool;
struct hkpAgentNnEntry;
class hkpConstraintQueryIn;
class hkpEntity;
struct hkpMultithreadConfig;
struct hkpProcessCollisionInput;
struct hkpProcessCollisionOutput;
class hkpSimulationIsland;
struct hkpToiEvent;
class hkpWorld;
struct hkpWorldDynamicsStepInfo;

// The result of a step (Havok 2014.1 hkpSimulation.h; values from rfg.pdb). Added by hand in HK7.
enum hkpStepResult
{
    HK_STEP_RESULT_SUCCESS = 0,
    HK_STEP_RESULT_MEMORY_FAILURE_BEFORE_INTEGRATION = 1,
    HK_STEP_RESULT_MEMORY_FAILURE_DURING_COLLIDE = 2,
    HK_STEP_RESULT_MEMORY_FAILURE_DURING_TOI_SOLVE = 3,
};

// size 0x2C
class hkpSimulation : public hkReferencedObject
{
public:
    enum FindContacts
    {
        FIND_CONTACTS_DEFAULT = 0,
        FIND_CONTACTS_EXTRA = 1,
    };

    enum ResetCollisionInformation
    {
        RESET_TOI = 1,
        RESET_TIM = 2,
        RESET_AABB = 4,
        RESET_ALL = 7,
    };

    enum LastProcessingStep
    {
        INTEGRATE = 0,
        COLLIDE = 1,
    };

    typedef hkpSimulation* (*CreateSimulationFunction)(hkpWorld*);
    typedef void (hkpSimulation::*AgentEntryProcessFunction)(hkpAgentNnEntry*, const hkpProcessCollisionInput&, hkpProcessCollisionOutput&);

    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int, void* p) 0x00DEF2B0, new(unsigned int nbytes) 0x00E039C0, delete(void* p, unsigned int nbytes) 0x00DEF3A0
    static const hkClass& staticClass(); // 0x00DEF2C0
    hkpSimulation(hkFinishLoadedObjectFlag flag); // 0x00DEF310
    hkpSimulation(hkpWorld* world); // 0x00E01390
    virtual ~hkpSimulation(); // vtbl+0x00 0x00E01200
    static hkpSimulation* create(hkpWorld* world); // 0x00E013E0
    virtual hkpStepResult stepDeltaTime(float physicsDeltaTime); // vtbl+0x10 0x00E012F0
    virtual hkpStepResult integrate(float physicsDeltaTime); // vtbl+0x14 0x00E02110
    virtual hkpStepResult collide(); // vtbl+0x18 0x00E01790
    virtual hkpStepResult advanceTime(); // vtbl+0x1C 0x00E015A0
    void setFrameTimeMarker(float frameDeltaTime); // 0x00E01210
    bool isSimulationAtMarker(); // 0x00E01230
    bool isSimulationAtPsi() const; // 0x00E01250
    virtual hkpStepResult stepBeginSt(hkJobQueue* queue, float physicsDeltaTime); // vtbl+0x20 0x00DEF320
    virtual hkpStepResult finishMtStep(hkJobQueue* queue, hkThreadPool* threadPool); // vtbl+0x24 0x00DEF330
    virtual void getMultithreadConfig(hkpMultithreadConfig& config); // vtbl+0x28 0x00DEF340
    virtual void setMultithreadConfig(const hkpMultithreadConfig& config, hkJobQueue* queue); // vtbl+0x2C 0x00DEF350
    float getCurrentTime(); // 0x00DE88B0
    float getCurrentPsiTime(); // 0x00DE88C0
    float getSimulateUntilTime(); // 0x00E1FC40
    float getPhysicsDeltaTime(); // 0x00DE88D0
    float getFrameMarkerPsiSnap();
    virtual void collideEntitiesDiscrete(hkpEntity** entities, int numEntities, hkpWorld* world, const hkStepInfo& stepInfo, FindContacts findExtraContacts); // vtbl+0x30 0x00E03730
    static void collideEntitiesBroadPhaseDiscrete(hkpEntity** entities, int numEntities, hkpWorld* world); // 0x00E02230
    void collideEntitiesNarrowPhaseDiscrete(hkpEntity** entities, int numEntities, const hkpProcessCollisionInput& input, FindContacts findExtraContacts); // 0x00E03540
    virtual void resetCollisionInformationForEntities(hkpEntity** entities, int numEntities, hkpWorld* world, ResetCollisionInformation resetInfo); // vtbl+0x34 0x00E03570
    virtual void assertThereIsNoCollisionInformationForEntities(hkpEntity** entities, int numEntities, hkpWorld* world); // vtbl+0x38 0x00DEF360
    virtual void removeCollisionInformationForAgent(hkpAgentNnEntry* agent); // vtbl+0x3C 0x00DEF370
    virtual void assertThereIsNoCollisionInformationForAgent(hkpAgentNnEntry* agent); // vtbl+0x40 0x00DEF380
public:
    virtual void reintegrateAndRecollideEntities(hkpEntity** entities, int numEntities, hkpWorld* world, int reintegrateRecollideMode); // vtbl+0x44 0x00E035B0
    void checkDeltaTimeIsOk(float deltaTime); // 0x00E01420
    hkpStepResult reCollideAfterStepFailure(); // 0x00E01430
    virtual void collideInternal(const hkStepInfo& stepInfoIn); // vtbl+0x48 0x00E028A0
    hkpStepResult integrateInternal(const hkStepInfo& stepInfoIn); // 0x00E01960
    void applyActions(); // 0x00E01640
    static void integrateIsland(hkpSimulationIsland* island, const hkpWorldDynamicsStepInfo& stepInfo, hkpConstraintQueryIn& constraintQueryIn); // 0x00E03A80
    static void collideIslandNarrowPhaseDiscrete(hkpSimulationIsland* island, const hkpProcessCollisionInput& input); // 0x00E039E0
    float snapSimulateTimeAndGetTimeToAdvanceTo(); // 0x00E01270
    void processAgentsOfEntities(hkpEntity** entities, int numEntities, const hkpProcessCollisionInput& processInput, void (hkpSimulation::*processingFunction)(hkpAgentNnEntry*, const hkpProcessCollisionInput&, hkpProcessCollisionOutput&), FindContacts findExtraContacts); // 0x00E02C10
    void processAgentCollideDiscrete(hkpAgentNnEntry* entry, const hkpProcessCollisionInput& processInput, hkpProcessCollisionOutput& processOutput); // 0x00E016A0
    void processAgentResetCollisionInformation(hkpAgentNnEntry* entry, const hkpProcessCollisionInput& processInput, hkpProcessCollisionOutput& processOutput); // 0x00E01370
public:
    virtual void warpTime(float deltaTime); // vtbl+0x4C 0x00DEF390
    void setCurrentTime(float time); // 0x00E1FC50
    void setCurrentPsiTime(float time); // 0x00E1FC60
    void setSimulateUntilTime(float time); // 0x00E1FC70
    hkpWorld* getWorld(); // 0x00DF5C40

    static const hkInternalClassMember Members[]; // 0x016F7E48
    static hkpSimulation* (*createDiscrete)(hkpWorld*); // 0x03A2ADDC
    static hkpSimulation* (*createContinuous)(hkpWorld*); // 0x03A2ADE0
    static hkpSimulation* (*createMultithreaded)(hkpWorld*); // 0x03A2ADE4
    unsigned int m_determinismCheckFrameCounter; // 0x08
public:
    hkpWorld* m_world; // 0x0C
    hkEnum<enum LastProcessingStep,unsigned char> m_lastProcessingStep; // 0x10
    float m_currentTime; // 0x14
    float m_currentPsiTime; // 0x18
    float m_physicsDeltaTime; // 0x1C
    float m_simulateUntilTime; // 0x20
    float m_frameMarkerPsiSnap; // 0x24
    unsigned int m_previousStepResult; // 0x28
};

void hkLs_doSimpleCollisionResponse(hkpWorld* world, const hkpToiEvent& event, float rotateNormal, hkArray<hkpEntity*,hkContainerHeapAllocator>& toBeActivated); // 0x00E01EE0
