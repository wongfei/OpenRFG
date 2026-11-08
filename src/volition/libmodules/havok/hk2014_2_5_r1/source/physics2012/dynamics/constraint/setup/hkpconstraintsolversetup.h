// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/setup/hkpconstraintsolversetup.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0). The solver resources, the schema info and the
// Jacobian task structures from the PDB and Havok 2014.1 (HK8; the tasks belong to the multithreaded simulation, which
// rfg.exe does not use, but the buffer layout of calcBufferOffsetsForSolve is written into a task header).
// Hand-maintained.
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/hkmath.h"

class hkBool;
struct hkConstraintInternal;
struct hkContainerHeapAllocator;
class hkStepInfo;
struct hkpConstraintAtom;
class hkpConstraintInstance;
class hkpConstraintQueryIn;
class hkpConstraintQueryOut;
struct hkpConstraintRuntime;
class hkpEntity;
struct hkpImpulseLimitBreachedHeader;
class hkpJacobianSchema;
struct hkpSolveConstraintBatchTask;
class hkpSimulationIsland;
struct hkpSolverElemTemp;
struct hkpSolverInfo;
class hkpVelocityAccumulator;

// size 0xC
struct hkpConstraintSchemaInfo
{
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpConstraintSchemaInfo);

    hkpConstraintInstance* m_constraint; // 0x00
    hkpJacobianSchema* m_schema; // 0x04
    float m_allowedPenetrationDepth; // 0x08
};

// size 0x68: the buffers of an incremental solve (the TOI solver adds accumulators and schemas as it goes)
struct hkpConstraintSolverResources
{
    // size 0x30
    struct VelocityAccumTransformBackup
    {
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(VelocityAccumTransformBackup);

        hkRotationf m_coreTworldRotation; // 0x00
    };

    // size 0x10
    template<typename T>
    struct BufferState
    {
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(BufferState);

        T* m_begin; // 0x00
        T* m_end; // 0x04
        T* m_current; // 0x08
        T* m_lastProcessed; // 0x0C
    };

    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpConstraintSolverResources);

    static const int NUM_PRIORITY_CLASSES = 3; // 0x012C70B4

    hkStepInfo* m_stepInfo; // 0x00
    hkpSolverInfo* m_solverInfo; // 0x04
    hkpConstraintQueryIn* m_constraintQueryInput; // 0x08
    hkpVelocityAccumulator* m_accumulators; // 0x0C
    hkpVelocityAccumulator* m_accumulatorsEnd; // 0x10
    hkpVelocityAccumulator* m_accumulatorsCurrent; // 0x14
    VelocityAccumTransformBackup* m_accumulatorsBackup; // 0x18
    VelocityAccumTransformBackup* m_accumulatorsBackupEnd; // 0x1C
    BufferState<hkpJacobianSchema> m_schemas[3]; // 0x20
    unsigned char m_priorityClassMap[6]; // 0x50
    hkpSolverElemTemp* m_elemTemp; // 0x58
    hkpSolverElemTemp* m_elemTempEnd; // 0x5C
    hkpSolverElemTemp* m_elemTempCurrent; // 0x60
    hkpSolverElemTemp* m_elemTempLastProcessed; // 0x64
};

// size 0x1000
struct hkpBuildJacobianTask
{
    enum
    {
        MAX_NUM_ATOM_INFOS = 144,
    };

    // size 0x1C
    struct AtomInfo
    {
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(AtomInfo);

        hkpConstraintAtom* m_atoms; // 0x00
        hkpConstraintInstance* m_instance; // 0x04
        hkpConstraintRuntime* m_runtime; // 0x08
        const hkTransformf* m_transformA; // 0x0C
        const hkTransformf* m_transformB; // 0x10
        unsigned short m_atomsSize; // 0x14
        unsigned short m_runtimeSize; // 0x16
        unsigned short m_accumulatorIndexA; // 0x18
        unsigned short m_accumulatorIndexB; // 0x1A
    };

    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpBuildJacobianTask); // new(unsigned int nbytes) 0x00E2D050, delete(void* p, unsigned int nbytes) 0x00E29630
    hkpBuildJacobianTask(); // 0x00E2B500

    hkpBuildJacobianTask* m_next; // 0x00
    struct hkpBuildJacobianTaskHeader* m_taskHeader; // 0x04
    const hkpVelocityAccumulator* m_accumulators; // 0x08
    hkpJacobianSchema* m_schemas; // 0x0C
    hkpJacobianSchema* m_schemasOfNextTask; // 0x10
    int m_numAtomInfos; // 0x14
    AtomInfo m_atomInfos[144]; // 0x18
};

// size 0x10
struct hkpBuildJacobianTaskCollection
{
    // size 0x8
    struct CallbackPair
    {
        const hkConstraintInternal* m_callbackConstraints; // 0x00
        hkpBuildJacobianTask::AtomInfo* m_atomInfo; // 0x04
    };

    hkpBuildJacobianTaskCollection() : m_buildJacobianTasks(nullptr), m_numBuildJacobianTasks(0), m_callbackConstraints(nullptr), m_numCallbackConstraints(0) {} // 0x00E0D1B0

    hkpBuildJacobianTask* m_buildJacobianTasks; // 0x00
    int m_numBuildJacobianTasks; // 0x04
    CallbackPair* m_callbackConstraints; // 0x08
    int m_numCallbackConstraints; // 0x0C
};

// size 0x8
struct hkpSolveJacobiansTaskCollection
{
    hkpSolveJacobiansTaskCollection() : m_firstSolveJacobiansTask(nullptr), m_firstBatchSize(0) {} // 0x00E0D1C0

    hkpSolveConstraintBatchTask* m_firstSolveJacobiansTask; // 0x00
    unsigned short m_firstBatchSize; // 0x04
};

// size 0x80: an island's solve (multithreaded simulation); calcBufferOffsetsForSolve fills its buffer layout
struct alignas(16) hkpBuildJacobianTaskHeader
{
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpBuildJacobianTaskHeader); // new(unsigned int nbytes) 0x00E2D070, delete(void* p, unsigned int nbytes) 0x00E29660
    // 0x00E0D9A0
    hkpBuildJacobianTaskHeader()
        : m_referenceCount(1)
        , m_numIslandsAfterSplit(1)
        , m_buffer(nullptr)
        , m_bufferSize(0)
        , m_bufferCapacity(0)
    {
    }
    ~hkpBuildJacobianTaskHeader() {} // 0x00E0D9E0 (the split island array)

    int m_openJobs; // 0x00
    int m_numUnfinishedJobsForBroadphase; // 0x04
    int m_islandShouldBeDeactivated; // 0x08
    long m_referenceCount; // 0x0C
    int m_numIslandsAfterSplit; // 0x10
    void* m_buffer; // 0x14
    int m_bufferSize; // 0x18
    int m_bufferCapacity; // 0x1C
    signed char m_exportFinished; // 0x20
    signed char m_splitCheckRequested; // 0x21
    hkpImpulseLimitBreachedHeader* m_impulseLimitsBreached; // 0x24
    alignas(16) hkpVelocityAccumulator* m_accumulatorsBase; // 0x30
    hkpVelocityAccumulator* m_accumulatorsEnd; // 0x34
    unsigned short m_numApplyGravityJobs; // 0x38
    unsigned short m_numIntegrateVelocitiesJobs; // 0x3A
    hkpJacobianSchema* m_schemasBase; // 0x3C
    hkpSolverElemTemp* m_solverTempBase; // 0x40
    int m_numSolverResults; // 0x44
    int m_numSolverElemTemps; // 0x48
    const hkpConstraintQueryIn* m_constraintQueryIn; // 0x4C
    hkpEntity* const* m_allEntities; // 0x50
    unsigned short m_numAllEntities; // 0x54
    unsigned short m_entitiesCapacity; // 0x56
    hkBool m_solveInSingleThread; // 0x58
    hkBool m_solveInSingleThreadOnPpuOnly; // 0x59
    hkArray<hkpSimulationIsland*, hkContainerHeapAllocator> m_newSplitIslands; // 0x5C
    hkpBuildJacobianTaskCollection m_tasks; // 0x68
    hkpSolveJacobiansTaskCollection m_solveTasks; // 0x78
};

// size 0x1
class hkpConstraintSolverSetup
{
public:
    enum SolverMode
    {
        SOLVER_MODE_PROCESS_ALL = 0,
        SOLVER_MODE_INCREMENTAL_CONTINUE = 1,
    };

    static int solve(const hkStepInfo& stepInfo, const hkpSolverInfo& solverInfo, hkpConstraintQueryIn& constraintQueryIn, hkpSimulationIsland& island, void* preallocatedBuffer, int preallocatedBufferSize, hkpEntity* const * bodies, int numBodies); // 0x00E0C850
    static int calcBufferSize(hkpSimulationIsland& island); // 0x00E0BCA0
    static int calcBufferOffsetsForSolve(const hkpSimulationIsland& island, char* buffer, int bufferSize, hkpBuildJacobianTaskHeader& taskHeader); // 0x00E0B7D0
public:
    static void _buildAccumulators(const hkStepInfo& info, hkpEntity* const * bodiesIn, int numEntities, hkpVelocityAccumulator* accumOut); // 0x00E0D3A0
    static void _buildJacobianElement(const hkConstraintInternal* c, hkpConstraintQueryIn& in, hkpVelocityAccumulator* baseAccum, hkpConstraintQueryOut& out); // 0x00E0D440
    static void _buildJacobianElements(hkpConstraintQueryIn& in, hkpEntity* const * bodies, int numBodies, hkpVelocityAccumulator* baseAccum, hkpJacobianSchema* schemaOut, hkpJacobianSchema* schemaOutEnd, hkpJacobianSchema* schemaOutB); // 0x00E0DA60
public:
    static void initializeSolverState(hkStepInfo& stepInfo, hkpSolverInfo& solverInfo, hkpConstraintQueryIn& constraintQueryIn, char* buffer, int bufferSize, const unsigned char* priorityClassMap, const float* priorityClassRatios, hkpConstraintSolverResources& s); // 0x00E0B8C0
    static void shutdownSolver(hkpConstraintSolverResources& s); // 0x00E0B760
    static void internalAddAccumulators(hkpConstraintSolverResources& s, hkpEntity* const * entities, int numEntities); // 0x00E0BE00
    static void internalAddJacobianSchemas(hkpConstraintSolverResources& s, hkpConstraintInstance** constraints, int numConstraints, hkArray<hkpConstraintSchemaInfo,hkContainerHeapAllocator>& constraintStatus); // 0x00E0C270
    static void subSolve(hkpConstraintSolverResources& s, SolverMode mode); // 0x00E0B9F0
    static void oneStepIntegrate(const hkpSolverInfo& si, const hkStepInfo& info, const hkpVelocityAccumulator* accumulatorsBase, hkpEntity* const * entities, int numEntities); // 0x00E0BA60
    static hkBool internalIsMemoryOkForNewAccumulators(hkpConstraintSolverResources& s, hkpEntity** entities, int numEntities); // 0x00E0B770
    static hkBool internalIsMemoryOkForNewJacobianSchemas(hkpConstraintSolverResources& s, hkpConstraintInstance** constraints, int numConstraints); // 0x00E0BB60
};
