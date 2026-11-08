// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/setup/hkpconstraintsolversetup.cpp
//
// Havok 2014.2.5 (havok_impl.md HK8): the solve of an island with constraints, from rfg.exe. One buffer holds the
// velocity accumulators (a fixed one first, one per entity, an end marker), the Jacobian schemas of every
// constraint (built in the order of the entities' master constraint lists, the TOI priorities last) and the solver
// element temps. The solver integrates the accumulators; their velocities then go back to the motions.
// The TOI solver's incremental solve (initializeSolverState, internalAdd*, subSolve, oneStepIntegrate) splits a
// scratchpad the same way and adds accumulators and schemas as it activates entities.
// The "MiNumJacobians"/"MiNumEntities" monitor values are not reproduced.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/setup/hkpconstraintsolversetup.h"

#include <string.h>

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/router/hkmemoryrouter.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/allocator/lifo/hklifoallocator.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraint/data/hkpconstraintdata.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraintsolver/accumulator/hkpvelocityaccumulator.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraintsolver/constraint/hkpconstraintqueryin.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraintsolver/constraint/hkpconstraintqueryout.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraintsolver/jacobian/hkpjacobianschema.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraintsolver/solve/hkpsolve.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraintsolver/solve/hkpsolverinfo.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpprocesscollisioninput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/atom/hkpsimplecontactconstraintatom.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/contact/hkpcontactimpulselimitbreachedlistener.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/contact/hkpsimplecontactconstraintdata.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/hkpconstraintinstance.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/hkpentity.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/util/hkpentityaabbutil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/motion/util/hkprigidmotionutil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/hkpsimulationisland.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/hkpworld.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/util/hkpworldcallbackutil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/solver/atom/hkpbuildjacobianfromatoms.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/solver/contact/hkpsimplecontactconstraintinfo.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/solver/hkpsolversse.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/solver/solve/hkpsolverexport.h"

// the offset of hkpEntity::m_motion (the motion arrays of hkRigidMotionUtil are entity arrays)
static const int HK_SOLVER_MOTION_OFFSET = 0xE0;

// the fixed accumulator every constraint to a fixed or missing body uses (hkpVelocityAccumulator::setFixed, inlined)
static inline void hkConstraintSolverSetup_setFixed(hkpVelocityAccumulator* acc)
{
    acc->m_gravityFactor = 0.0f;
    acc->m_type.m_storage = hkpVelocityAccumulator::HK_KEYFRAMED_RIGID_BODY;
    acc->m_context.m_storage = hkpVelocityAccumulator::ACCUMULATOR_CONTEXT_BUILD_JACOBIANS;
    acc->m_invMasses.setZero();
    acc->m_linearVel.setZero();
    acc->m_angularVel.setZero();
    acc->m_scratch3.setZero();
    acc->m_scratch0.m_quad = g_vectorfConstants[HK_QUADREAL_1000];
    acc->m_scratch1.m_quad = g_vectorfConstants[HK_QUADREAL_0100];
    acc->m_scratch2.m_quad = g_vectorfConstants[HK_QUADREAL_0010];
}

// the schemas continue in the overflow buffer: a goto record at the current position (written once)
static inline void hkConstraintSolverSetup_gotoOverflow(hkpConstraintQueryOut& out, hkpJacobianSchema*& gotoLimit, hkpJacobianSchema* overflowSchemas)
{
    hkpJacobianSchema* current = out.m_jacobianSchemas;
    if (current >= gotoLimit)
    {
        reinterpret_cast<hkpJacobianGotoSchema*>(current)->initGoto(overflowSchemas);
        out.m_jacobianSchemas = overflowSchemas;
        gotoLimit = reinterpret_cast<hkpJacobianSchema*>(0xFFFFFFFF);
    }
}

// the query input of one constraint (the bodies' accumulators at their m_solverData offsets) and its Jacobians
static inline void hkConstraintSolverSetup_buildJacobian(const hkConstraintInternal* internal, hkpConstraintQueryIn& in, hkpConstraintQueryOut& out, char* accumulators)
{
    hkpEntity* entityA = internal->m_entities[0];
    hkpEntity* entityB = internal->m_entities[1];
    in.m_bodyA = reinterpret_cast<const hkpVelocityAccumulator*>(accumulators + entityA->m_solverData);
    in.m_bodyB = reinterpret_cast<const hkpVelocityAccumulator*>(accumulators + entityB->m_solverData);
    in.m_transformA = &entityA->m_motion.m_motionState.m_transform;
    in.m_transformB = &entityB->m_motion.m_motionState.m_transform;
    in.m_constraintInstance = internal->m_constraint;
    out.m_constraintRuntime = internal->m_runtime;
    out.m_instanceRuntime = internal->m_runtime;
    in.m_accumulatorAIndex = (entityA->m_solverData >> 7) & 0xFFFF;
    in.m_accumulatorBIndex = (entityB->m_solverData >> 7) & 0xFFFF;
    hkpConstraintAtom* atoms = internal->m_atoms;
    if (atoms->m_type.m_storage == hkpConstraintAtom::TYPE_CONTACT)
    {
        hkSimpleContactConstraintDataBuildJacobian(static_cast<hkpSimpleContactConstraintAtom*>(atoms), in, true, out);
    }
    else
    {
        hkSolverBuildJacobianFromAtomsNotContact(atoms, internal->m_atomsSize, in, out);
    }
}

// 0x00E0C850: returns the number of frames the island's motions have been inactive (for deactivation).
// The buffer: the preallocated one, else the solver allocator's (bufAlloc may give more). When the elem temps do not
// fit, they come from the LIFO (only possible with a preallocated buffer; the end frees them through the temp
// allocator with size 0, an original quirk kept); when the schemas do not fit either, the rest goes to a temp
// allocation and a goto schema links the two parts; when not even the accumulators fit, everything is temp memory.
// A null solver allocation is not checked (the game does not step without enough solver memory).
int hkpConstraintSolverSetup::solve(const hkStepInfo& stepInfo, const hkpSolverInfo& solverInfo, hkpConstraintQueryIn& constraintQueryIn, hkpSimulationIsland& island, void* preallocatedBuffer, int preallocatedBufferSize, hkpEntity* const * bodies, int numBodies)
{
    const int sizeOfSchemas = island.m_constraintInfo.m_sizeOfSchemas + 0x10; // + the end schema
    const int sizeOfElemTemps = (island.m_constraintInfo.m_numSolverElemTemps * 4 + 0x17) & ~0xF;
    int sizeOfAccumulators = numBodies * 0x80 + 0x90; // + the fixed accumulator and the end marker
    int bufferSize = sizeOfElemTemps + sizeOfSchemas + sizeOfAccumulators;
    int numInactiveFrames = 0;
    void* tempBuffer = nullptr; // freed at the end through the temp allocator
    int tempBufferSize = 0;
    hkpJacobianSchema* overflowSchemas = nullptr;

    char* buffer;
    char* bufferEnd;
    char* solverBuffer;
    if (preallocatedBuffer == nullptr)
    {
        int size = bufferSize;
        buffer = static_cast<char*>(hkMemoryRouter::getInstance().solver().bufAlloc(size));
        bufferSize = size;
        solverBuffer = buffer;
        bufferEnd = buffer + size;
    }
    else
    {
        buffer = static_cast<char*>(preallocatedBuffer);
        bufferEnd = buffer + preallocatedBufferSize;
        solverBuffer = nullptr;
    }

    char* accumulators;
    char* schemas;
    char* elemTemps;
    char* gotoLimit;
    for (;;)
    {
        accumulators = buffer;
        schemas = accumulators + sizeOfAccumulators;
        elemTemps = schemas + sizeOfSchemas;
        gotoLimit = elemTemps;
    checkFit:
        if (elemTemps + sizeOfElemTemps <= bufferEnd)
        {
            break;
        }
        if (schemas >= bufferEnd)
        {
            // not even the accumulators fit: all of it from the temp allocator (the size from the solver
            // buffer's start: absolute with a preallocated buffer, an original quirk kept)
            tempBufferSize = int(elemTemps + sizeOfElemTemps - solverBuffer);
            tempBuffer = hkMemoryRouter::getInstance().temp().blockAlloc(tempBufferSize);
            buffer = static_cast<char*>(tempBuffer);
            bufferEnd = buffer + tempBufferSize;
            continue;
        }
        const int maxSchema = island.m_constraintInfo.m_maxSizeOfSchema + 0x10;
        if (elemTemps < bufferEnd)
        {
            // the elem temps from the LIFO
            hkLifoAllocator& lifo = hkMemoryRouter::getInstance().stack();
            elemTemps = static_cast<char*>(lifo.fastBlockAlloc((sizeOfElemTemps + 0x7F) & ~0x7F));
            tempBuffer = elemTemps;
            bufferEnd = elemTemps + sizeOfElemTemps;
            goto checkFit;
        }
        // the schemas that do not fit before the limit (one largest schema below the end) continue in a temp
        // allocation, followed by the elem temps
        gotoLimit = bufferEnd - maxSchema;
        const int room = int(gotoLimit - schemas);
        const int overflowSize = island.m_constraintInfo.m_sizeOfSchemas - (room > 0 ? room : 0) + maxSchema;
        tempBufferSize = overflowSize + sizeOfElemTemps;
        tempBuffer = hkMemoryRouter::getInstance().temp().blockAlloc(tempBufferSize);
        bufferEnd = static_cast<char*>(tempBuffer) + tempBufferSize;
        if (room < 0)
        {
            schemas = static_cast<char*>(tempBuffer);
            elemTemps = schemas + island.m_constraintInfo.m_sizeOfSchemas + 0x10;
            overflowSchemas = nullptr;
            gotoLimit = elemTemps;
        }
        else
        {
            overflowSchemas = static_cast<hkpJacobianSchema*>(tempBuffer);
            elemTemps = static_cast<char*>(tempBuffer) + overflowSize;
        }
        goto checkFit;
    }

    hkpVelocityAccumulator* fixedAccumulator = reinterpret_cast<hkpVelocityAccumulator*>(accumulators);
    hkConstraintSolverSetup_setFixed(fixedAccumulator);
    for (int i = 0; i < numBodies; i++)
    {
        const unsigned int offset = (i + 1) * 0x80;
        if (bodies[i]->m_solverData != offset)
        {
            bodies[i]->m_solverData = offset;
        }
    }
    hkpVelocityAccumulator* bodyAccumulators = fixedAccumulator + 1;
    hkpVelocityAccumulator* accEnd = hkRigidMotionUtilApplyForcesAndBuildAccumulators(stepInfo, reinterpret_cast<hkpMotion* const*>(bodies), numBodies, HK_SOLVER_MOTION_OFFSET, bodyAccumulators);
    accEnd->m_type.m_storage = hkpVelocityAccumulator::HK_END;
    memset(elemTemps, 0, island.m_constraintInfo.m_numSolverElemTemps * 4);

    hkpConstraintQueryOut out;
    out.m_jacobianSchemas = reinterpret_cast<hkpJacobianSchema*>(schemas);
    hkpJacobianSchema* limit = reinterpret_cast<hkpJacobianSchema*>(gotoLimit);
    hkInplaceArray<const hkConstraintInternal*, 256, hkContainerHeapAllocator> toiConstraints;
    for (hkpEntity* const* body = bodies; body < bodies + numBodies; body++)
    {
        const hkConstraintInternal* internal = (*body)->m_constraintsMaster.begin();
        const hkConstraintInternal* internalEnd = internal + (*body)->m_constraintsMaster.getSize();
        for (; internal < internalEnd; internal++)
        {
            // new contact points, contact point callbacks and Jacobian callbacks
            if ((internal->m_callbackRequest & (hkpConstraintAtom::CALLBACK_REQUEST_NEW_CONTACT_POINT | hkpConstraintAtom::CALLBACK_REQUEST_SETUP_CALLBACK | hkpConstraintAtom::CALLBACK_REQUEST_CONTACT_POINT_CALLBACK)) != 0)
            {
                hkpConstraintInstance* instance = internal->m_constraint;
                constraintQueryIn.m_constraintInstance = instance;
                out.m_constraintRuntime = internal->m_runtime;
                constraintQueryIn.m_bodyA = reinterpret_cast<const hkpVelocityAccumulator*>(accumulators + instance->m_entities[0]->m_solverData);
                constraintQueryIn.m_bodyB = reinterpret_cast<const hkpVelocityAccumulator*>(accumulators + instance->m_entities[1]->m_solverData);
                constraintQueryIn.m_transformA = static_cast<const hkTransformf*>(instance->m_entities[0]->m_collidable.m_motion);
                constraintQueryIn.m_transformB = static_cast<const hkTransformf*>(instance->m_entities[1]->m_collidable.m_motion);
                if ((internal->m_callbackRequest & (hkpConstraintAtom::CALLBACK_REQUEST_NEW_CONTACT_POINT | hkpConstraintAtom::CALLBACK_REQUEST_CONTACT_POINT_CALLBACK)) != 0)
                {
                    hkpSimpleContactConstraintData* data = static_cast<hkpSimpleContactConstraintData*>(instance->m_data);
                    hkSimpleContactConstraintData_fireCallbacks(data, &constraintQueryIn, data->m_atom, hkpContactPointEvent::TYPE_MANIFOLD);
                }
                if ((internal->m_callbackRequest & hkpConstraintAtom::CALLBACK_REQUEST_SETUP_CALLBACK) != 0)
                {
                    internal->m_constraint->m_data->buildJacobianCallback(constraintQueryIn, out);
                }
            }
            if (internal->m_priority.m_storage >= hkpConstraintInstance::PRIORITY_TOI_HIGHER)
            {
                toiConstraints.pushBack(internal);
                continue;
            }
            hkConstraintSolverSetup_gotoOverflow(out, limit, overflowSchemas);
            hkConstraintSolverSetup_buildJacobian(internal, constraintQueryIn, out, accumulators);
        }
    }
    for (int i = 0; i < toiConstraints.getSize(); i++)
    {
        hkConstraintSolverSetup_gotoOverflow(out, limit, overflowSchemas);
        hkConstraintSolverSetup_buildJacobian(toiConstraints[i], constraintQueryIn, out, accumulators);
    }
    *reinterpret_cast<unsigned int*>(out.m_jacobianSchemas.val()) = 0; // the end schema
    toiConstraints.clearAndDeallocate();

    hkpJacobianSchema* schemasBase = reinterpret_cast<hkpJacobianSchema*>(schemas);
    hkpSolverElemTemp* temps = reinterpret_cast<hkpSolverElemTemp*>(elemTemps);
    const bool solved = hkSolveConstraints(solverInfo, schemasBase, fixedAccumulator, temps);
    if (solved)
    {
        hkExportImpulsesAndRhs(solverInfo, temps, schemasBase, fixedAccumulator);
        numInactiveFrames = hkRigidMotionUtilApplyAccumulators(solverInfo, stepInfo, bodyAccumulators, reinterpret_cast<hkpMotion* const*>(bodies), numBodies, HK_SOLVER_MOTION_OFFSET);
        hkpEntityAabbUtil::entityBatchRecalcAabb(bodies[0]->m_world->m_collisionInput, bodies, numBodies);
        // the export wrote the breached contacts at the start of the schema buffer
        const hkpImpulseLimitBreachedHeader* breached = reinterpret_cast<const hkpImpulseLimitBreachedHeader*>(schemasBase);
        if (breached->m_numBreached != 0)
        {
            hkpWorldCallbackUtil::fireContactImpulseLimitBreached(island.m_world, reinterpret_cast<const hkpContactImpulseLimitBreachedListenerInfo*>(schemas + sizeof(hkpImpulseLimitBreachedHeader)), breached->m_numBreached);
        }
    }
    if (tempBuffer != nullptr)
    {
        hkMemoryRouter::getInstance().temp().blockFree(tempBuffer, tempBufferSize);
    }
    if (solverBuffer != nullptr)
    {
        hkMemoryRouter::getInstance().solver().bufFree(solverBuffer, bufferSize);
    }
    return numInactiveFrames;
}

// 0x00E0B7D0: the layout solve uses for this island (the task header's accumulators, schemas with room for the
// multithreaded goto records, elem temps); with a buffer, the fixed accumulator and the end marker are written.
// Returns the size of the whole buffer.
int hkpConstraintSolverSetup::calcBufferOffsetsForSolve(const hkpSimulationIsland& island, char* buffer, int bufferSize, hkpBuildJacobianTaskHeader& taskHeader)
{
    taskHeader.m_buffer = buffer;
    const int numEntities = island.m_entities.getSize();
    taskHeader.m_accumulatorsBase = reinterpret_cast<hkpVelocityAccumulator*>(buffer);
    char* schemas = buffer + numEntities * 0x80 + 0x90;
    const int numGotos = (island.m_numConstraints / hkpBuildJacobianTask::MAX_NUM_ATOM_INFOS) * 2 + 8;
    taskHeader.m_schemasBase = reinterpret_cast<hkpJacobianSchema*>(schemas);
    char* elemTemps = schemas + numGotos * 0x10 + island.m_constraintInfo.m_sizeOfSchemas + 0x10;
    taskHeader.m_solverTempBase = reinterpret_cast<hkpSolverElemTemp*>(elemTemps);
    const int size = int(elemTemps + (((numGotos * 3 + island.m_constraintInfo.m_numSolverElemTemps) * 4 + 0x17) & ~0xF) - static_cast<char*>(taskHeader.m_buffer));
    taskHeader.m_bufferSize = size;
    if (taskHeader.m_buffer != nullptr)
    {
        hkConstraintSolverSetup_setFixed(taskHeader.m_accumulatorsBase);
        taskHeader.m_accumulatorsBase[numEntities + 1].m_type.m_storage = hkpVelocityAccumulator::HK_END;
    }
    return size;
}

// 0x00E0BCA0
int hkpConstraintSolverSetup::calcBufferSize(hkpSimulationIsland& island)
{
    hkpBuildJacobianTaskHeader taskHeader;
    return calcBufferOffsetsForSolve(island, nullptr, 0, taskHeader);
}

// ---- the incremental solve of the TOI solver: accumulators and schemas are added as the solver goes ----

// 0x00E0B8C0: splits the buffer (16-byte aligned, its size rounded down to 16) into 10/65 accumulators, 5/65 transform
// backups, the three priority classes' schemas (their ratios of 40/65 of the buffer, less 30 bytes) and the elem
// temps. The schema sizes are computed on the x87: (ratio * size) * (1/65 as a float), truncated (the double product
// here, with MSVC's default 53-bit x87 precision).
void hkpConstraintSolverSetup::initializeSolverState(hkStepInfo& stepInfo, hkpSolverInfo& solverInfo, hkpConstraintQueryIn& constraintQueryIn, char* buffer, int bufferSize, const unsigned char* priorityClassMap, const float* priorityClassRatios, hkpConstraintSolverResources& s)
{
    s.m_stepInfo = &stepInfo;
    s.m_solverInfo = &solverInfo;
    s.m_constraintQueryInput = &constraintQueryIn;
    memcpy(s.m_priorityClassMap, priorityClassMap, 6);
    const int size = bufferSize - bufferSize % 16;
    char* accumulators = reinterpret_cast<char*>(reinterpret_cast<hkUlong>(buffer + 0xF) & ~0xF);
    s.m_accumulators = reinterpret_cast<hkpVelocityAccumulator*>(accumulators);
    char* accumulatorsEnd = reinterpret_cast<char*>(reinterpret_cast<hkUlong>(accumulators + (size * 10) / 65) & ~0xF);
    s.m_accumulatorsEnd = reinterpret_cast<hkpVelocityAccumulator*>(accumulatorsEnd);
    s.m_accumulatorsBackup = reinterpret_cast<hkpConstraintSolverResources::VelocityAccumTransformBackup*>(accumulatorsEnd);
    char* schemas = reinterpret_cast<char*>(reinterpret_cast<hkUlong>(accumulatorsEnd + (size * 5) / 65) & ~0xF);
    s.m_accumulatorsBackupEnd = reinterpret_cast<hkpConstraintSolverResources::VelocityAccumTransformBackup*>(schemas);
    const double schemaBytes = (double)(float)((size * 5 - 240) * 8);
    const double inv65 = (double)0.0153846154f; // 0x3C7C0FC1
    for (int i = 0; i < hkpConstraintSolverResources::NUM_PRIORITY_CLASSES; i++)
    {
        hkpConstraintSolverResources::BufferState<hkpJacobianSchema>& state = s.m_schemas[i];
        state.m_begin = reinterpret_cast<hkpJacobianSchema*>(schemas);
        const int classBytes = (int)(long long)(((double)priorityClassRatios[i] * schemaBytes) * inv65);
        schemas = reinterpret_cast<char*>(reinterpret_cast<hkUlong>(schemas + classBytes + 0x10) & ~0xF);
        state.m_end = reinterpret_cast<hkpJacobianSchema*>(schemas);
        state.m_current = state.m_begin;
        state.m_lastProcessed = state.m_begin;
    }
    s.m_elemTempEnd = reinterpret_cast<hkpSolverElemTemp*>(buffer + size);
    s.m_elemTemp = reinterpret_cast<hkpSolverElemTemp*>(schemas);
    s.m_accumulatorsCurrent = s.m_accumulators;
    s.m_elemTempCurrent = reinterpret_cast<hkpSolverElemTemp*>(schemas);
    s.m_elemTempLastProcessed = reinterpret_cast<hkpSolverElemTemp*>(schemas);
}

// 0x00E0B760
void hkpConstraintSolverSetup::shutdownSolver(hkpConstraintSolverResources& s)
{
}

// 0x00E0B770: room for the entities' accumulators (and the fixed one first), keeping 16 bytes for the end marker
hkBool hkpConstraintSolverSetup::internalIsMemoryOkForNewAccumulators(hkpConstraintSolverResources& s, hkpEntity** entities, int numEntities)
{
    if (s.m_accumulatorsCurrent == s.m_accumulators)
    {
        numEntities++;
    }
    return numEntities * 0x80 <= int(reinterpret_cast<char*>(s.m_accumulatorsEnd) - reinterpret_cast<char*>(s.m_accumulatorsCurrent)) - 0x10;
}

// 0x00E0BB60: room for the constraints' schemas in their priority classes (4 bytes for each class's end marker) and
// their elem temps
hkBool hkpConstraintSolverSetup::internalIsMemoryOkForNewJacobianSchemas(hkpConstraintSolverResources& s, hkpConstraintInstance** constraints, int numConstraints)
{
    char* current[hkpConstraintSolverResources::NUM_PRIORITY_CLASSES];
    for (int i = 0; i < hkpConstraintSolverResources::NUM_PRIORITY_CLASSES; i++)
    {
        current[i] = reinterpret_cast<char*>(s.m_schemas[i].m_current);
    }
    int elemTempBytes = 0;
    for (hkpConstraintInstance** c = constraints; c < constraints + numConstraints; c++)
    {
        const hkConstraintInternal* internal = (*c)->m_internal;
        current[s.m_priorityClassMap[(*c)->m_priority.m_storage]] += internal->m_sizeOfSchemas;
        elemTempBytes += internal->m_numSolverElemTemps * 4;
    }
    const char* elemTemps = reinterpret_cast<const char*>(s.m_elemTempCurrent) + elemTempBytes;
    return elemTemps <= reinterpret_cast<const char*>(s.m_elemTempEnd) && current[0] + 4 <= reinterpret_cast<char*>(s.m_schemas[0].m_end) && current[1] + 4 <= reinterpret_cast<char*>(s.m_schemas[1].m_end) && current[2] + 4 <= reinterpret_cast<char*>(s.m_schemas[2].m_end);
}

// 0x00E0BE00: the first call adds the fixed accumulator (keyframed, identity rotation, backed up). The entities'
// accumulators follow (m_solverData their offsets); their core-from-world rotations go to the transform backups
// (schemas are built in the build-Jacobians context) before they become solver quaternions. An end marker follows.
void hkpConstraintSolverSetup::internalAddAccumulators(hkpConstraintSolverResources& s, hkpEntity* const * entities, int numEntities)
{
    if (numEntities == 0)
    {
        return;
    }
    if (s.m_accumulatorsCurrent == s.m_accumulators)
    {
        hkpVelocityAccumulator* fixed = s.m_accumulatorsCurrent;
        fixed->m_type.m_storage = hkpVelocityAccumulator::HK_KEYFRAMED_RIGID_BODY;
        fixed->m_context.m_storage = hkpVelocityAccumulator::ACCUMULATOR_CONTEXT_BUILD_JACOBIANS;
        fixed->m_gravityFactor = 0.0f;
        fixed->m_invMasses.setZero();
        fixed->m_linearVel.setZero();
        fixed->m_angularVel.setZero();
        fixed->m_scratch3.setZero();
        fixed->m_scratch0.m_quad = g_vectorfConstants[HK_QUADREAL_1000];
        fixed->m_scratch1.m_quad = g_vectorfConstants[HK_QUADREAL_0100];
        fixed->m_scratch2.m_quad = g_vectorfConstants[HK_QUADREAL_0010];
        hkSolverSse::convertToSolverType(fixed);
        s.m_accumulatorsCurrent = fixed + 1;
        hkpConstraintSolverResources::VelocityAccumTransformBackup* backup = s.m_accumulatorsBackup;
        backup->m_coreTworldRotation.m_col0.m_quad = g_vectorfConstants[HK_QUADREAL_1000];
        backup->m_coreTworldRotation.m_col1.m_quad = g_vectorfConstants[HK_QUADREAL_0100];
        backup->m_coreTworldRotation.m_col2.m_quad = g_vectorfConstants[HK_QUADREAL_0010];
    }
    for (unsigned int i = 0; i < (unsigned int)numEntities; i++)
    {
        entities[i]->m_solverData = i * 0x80 - reinterpret_cast<hkUlong>(s.m_accumulators) + reinterpret_cast<hkUlong>(s.m_accumulatorsCurrent);
    }
    hkpVelocityAccumulator* acc = s.m_accumulatorsCurrent;
    hkpConstraintSolverResources::VelocityAccumTransformBackup* backup = s.m_accumulatorsBackup + (entities[0]->m_solverData >> 7);
    s.m_accumulatorsCurrent = hkRigidMotionUtilApplyForcesAndBuildAccumulators(*s.m_stepInfo, reinterpret_cast<hkpMotion* const*>(entities), numEntities, HK_SOLVER_MOTION_OFFSET, acc);
    for (int i = 0; i < numEntities; i++, acc++, backup++)
    {
        backup->m_coreTworldRotation.m_col0 = acc->m_scratch0;
        backup->m_coreTworldRotation.m_col1 = acc->m_scratch1;
        backup->m_coreTworldRotation.m_col2 = acc->m_scratch2;
        hkSolverSse::convertToSolverType(acc);
    }
    s.m_accumulatorsCurrent->m_type.m_storage = hkpVelocityAccumulator::HK_END;
}

// 0x00E0C270: the constraints' Jacobians, each into its priority class's schemas, with its status (the schema
// position and the smaller allowed penetration depth of the bodies). The bodies' accumulators get their backed up
// rotations for the build and become solver quaternions again after it. Contact constraints fire their callbacks as
// an expanded manifold first. Each class's schemas end with an end marker (not advanced over).
void hkpConstraintSolverSetup::internalAddJacobianSchemas(hkpConstraintSolverResources& s, hkpConstraintInstance** constraints, int numConstraints, hkArray<hkpConstraintSchemaInfo, hkContainerHeapAllocator>& constraintStatus)
{
    hkpConstraintQueryIn& in = *s.m_constraintQueryInput;
    char* accumulators = reinterpret_cast<char*>(s.m_accumulators);
    for (hkpConstraintInstance** c = constraints; c < constraints + numConstraints; c++)
    {
        hkpConstraintInstance* instance = *c;
        const hkConstraintInternal* internal = instance->m_internal;
        const unsigned char priorityClass = s.m_priorityClassMap[instance->m_priority.m_storage];
        hkpEntity* entityA = internal->m_entities[0];
        hkpEntity* entityB = internal->m_entities[1];
        hkpVelocityAccumulator* accA = reinterpret_cast<hkpVelocityAccumulator*>(accumulators + entityA->m_solverData);
        hkpVelocityAccumulator* accB = reinterpret_cast<hkpVelocityAccumulator*>(accumulators + entityB->m_solverData);
        in.m_bodyA = accA;
        in.m_bodyB = accB;
        const hkRotationf& rotationA = s.m_accumulatorsBackup[internal->m_entities[0]->m_solverData >> 7].m_coreTworldRotation;
        const hkRotationf& rotationB = s.m_accumulatorsBackup[internal->m_entities[1]->m_solverData >> 7].m_coreTworldRotation;
        accA->m_context.m_storage = hkpVelocityAccumulator::ACCUMULATOR_CONTEXT_BUILD_JACOBIANS;
        accA->m_scratch0 = rotationA.m_col0;
        accA->m_scratch1 = rotationA.m_col1;
        accA->m_scratch2 = rotationA.m_col2;
        accB->m_context.m_storage = hkpVelocityAccumulator::ACCUMULATOR_CONTEXT_BUILD_JACOBIANS;
        accB->m_scratch0 = rotationB.m_col0;
        accB->m_scratch1 = rotationB.m_col1;
        accB->m_scratch2 = rotationB.m_col2;
        in.m_transformA = &entityA->m_motion.m_motionState.m_transform;
        in.m_constraintInstance = instance;
        in.m_transformB = &entityB->m_motion.m_motionState.m_transform;
        hkpConstraintQueryOut out;
        out.m_constraintRuntime = instance->m_internal->m_runtime;
        out.m_instanceRuntime = out.m_constraintRuntime;
        in.m_accumulatorAIndex = (internal->m_entities[0]->m_solverData >> 7) & 0xFFFF;
        in.m_accumulatorBIndex = (internal->m_entities[1]->m_solverData >> 7) & 0xFFFF;

        hkpJacobianSchema*& schemas = s.m_schemas[priorityClass].m_current;
        hkpConstraintSchemaInfo& status = constraintStatus.expandOne();
        status.m_constraint = instance;
        status.m_schema = schemas;
        const float depthA = internal->m_entities[0]->m_collidable.m_allowedPenetrationDepth;
        const float depthB = internal->m_entities[1]->m_collidable.m_allowedPenetrationDepth;
        status.m_allowedPenetrationDepth = hkSolverSse::lane0(_mm_min_ss(_mm_set_ss(depthA), _mm_set_ss(depthB)));
        out.m_jacobianSchemas = schemas;
        if (instance->m_data->getType() == hkpConstraintData::CONSTRAINT_TYPE_CONTACT)
        {
            hkpSimpleContactConstraintData* data = static_cast<hkpSimpleContactConstraintData*>(instance->m_data);
            hkSimpleContactConstraintData_fireCallbacks(data, &in, data->m_atom, hkpContactPointEvent::TYPE_EXPAND_MANIFOLD);
        }
        if ((instance->m_internal->m_callbackRequest & hkpConstraintAtom::CALLBACK_REQUEST_SETUP_CALLBACK) != 0)
        {
            instance->m_data->buildJacobianCallback(in, out);
        }
        hkpConstraintAtom* atoms = instance->m_internal->m_atoms;
        if (atoms->m_type.m_storage == hkpConstraintAtom::TYPE_CONTACT)
        {
            hkSimpleContactConstraintDataBuildJacobian(static_cast<hkpSimpleContactConstraintAtom*>(atoms), in, true, out);
        }
        else
        {
            hkSolverBuildJacobianFromAtomsNotContact(atoms, instance->m_internal->m_atomsSize, in, out);
        }
        schemas = out.m_jacobianSchemas;
        s.m_elemTempCurrent += instance->m_internal->m_numSolverElemTemps;
        hkSolverSse::convertToSolverType(accA);
        hkSolverSse::convertToSolverType(accB);
    }
    *reinterpret_cast<unsigned int*>(s.m_schemas[0].m_current) = 0;
    *reinterpret_cast<unsigned int*>(s.m_schemas[1].m_current) = 0;
    *reinterpret_cast<unsigned int*>(s.m_schemas[2].m_current) = 0;
}

// 0x00E0B9F0: zeroes the elem temps added since the last pass and runs one solver pass over each priority class's
// schemas added since then (all of them with SOLVER_MODE_PROCESS_ALL). Each pass gets the start of all elem temps,
// not the temps of its first schema (kept from the binary: an incremental pass after the first one solves its new
// schemas against the temps of the old ones).
void hkpConstraintSolverSetup::subSolve(hkpConstraintSolverResources& s, SolverMode mode)
{
    if (mode == SOLVER_MODE_PROCESS_ALL)
    {
        for (int i = 0; i < hkpConstraintSolverResources::NUM_PRIORITY_CLASSES; i++)
        {
            s.m_schemas[i].m_lastProcessed = s.m_schemas[i].m_begin;
        }
    }
    const int numTemps = int((reinterpret_cast<char*>(s.m_elemTempCurrent) - reinterpret_cast<char*>(s.m_elemTempLastProcessed)) >> 2);
    if (numTemps - 1 >= 0)
    {
        memset(s.m_elemTempLastProcessed, 0, numTemps * 4);
    }
    s.m_elemTempLastProcessed = s.m_elemTempCurrent;
    for (int i = 0; i < hkpConstraintSolverResources::NUM_PRIORITY_CLASSES; i++)
    {
        hkpConstraintSolverResources::BufferState<hkpJacobianSchema>& state = s.m_schemas[i];
        if (state.m_lastProcessed != state.m_current)
        {
            hkSolveStepJacobians(*s.m_solverInfo, state.m_lastProcessed, s.m_accumulators, s.m_elemTemp);
        }
        state.m_lastProcessed = state.m_current;
    }
}

// 0x00E0BA60: each entity's accumulator velocities applied to its motion as the step's (a copy whose sum velocities
// are the velocities), with its deactivation counter held at 0xFF during the apply; its AABB is recalculated
void hkpConstraintSolverSetup::oneStepIntegrate(const hkpSolverInfo& si, const hkStepInfo& info, const hkpVelocityAccumulator* accumulatorsBase, hkpEntity* const * entities, int numEntities)
{
    for (int i = 0; i < numEntities; i++)
    {
        hkpEntity* entity = entities[i];
        hkpMotion* motion = &entity->m_motion;
        hkpVelocityAccumulator acc = *reinterpret_cast<const hkpVelocityAccumulator*>(reinterpret_cast<const char*>(accumulatorsBase) + entity->m_solverData);
        acc.m_scratch0 = acc.m_linearVel;
        acc.m_scratch1 = acc.m_angularVel;
        const unsigned char counter = entity->m_motion.m_deactivationIntegrateCounter;
        entity->m_motion.m_deactivationIntegrateCounter = 0xFF;
        hkRigidMotionUtilApplyAccumulators(si, info, &acc, &motion, 1, 0);
        hkpEntityAabbUtil::entityBatchRecalcAabb(entity->m_world->m_collisionInput, &entity, 1);
        motion->m_deactivationIntegrateCounter = counter;
    }
}
