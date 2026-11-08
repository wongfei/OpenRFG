// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraintsolver/solve/hkpsolve.h
//
// Havok 2014.2.5 (havok_impl.md HK8): the solver entry points (defined in physics2012/internal/solver/solve/
// hkpsolver.cpp, hkpsolverexport.cpp and hkpsolvergettoivelocities.cpp) and the impulse-limit breach records, from
// rfg.pdb and the Havok 2014.1 header. Hand-maintained.
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraintsolver/solve/hkpsolverelemtemp.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraintsolver/solve/hkpsolverinfo.h"

class hkpConstraintInstance;
class hkpJacobianSchema;
class hkpSolverResults;
class hkpVelocityAccumulator;

// size 0x10
struct alignas(16) hkpImpulseLimitBreachedElem
{
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpImpulseLimitBreachedElem);

    unsigned short m_type; // 0x00: 0
    unsigned short m_isContact; // 0x02
    hkpConstraintInstance* m_constraintInstance; // 0x04: the query input's m_constraintInstance
    hkpSolverResults* m_solverResult; // 0x08
};

// size 0x10: followed by m_numBreached hkpImpulseLimitBreachedElem
struct alignas(16) hkpImpulseLimitBreachedHeader
{
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpImpulseLimitBreachedHeader);
    // 0x00E0D370
    hkpImpulseLimitBreachedElem& getElem(int index)
    {
        return *reinterpret_cast<hkpImpulseLimitBreachedElem*>(reinterpret_cast<char*>(this) + index * sizeof(hkpImpulseLimitBreachedElem) + sizeof(hkpImpulseLimitBreachedHeader));
    }

    int m_numBreached; // 0x00
    hkpImpulseLimitBreachedHeader* m_next; // 0x04: the next array (multithreaded simulation)
};

// size 0x20
struct alignas(16) hkSolverStepTemp
{
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkSolverStepTemp);
    hkSolverStepTemp() {} // 0x00EA9440

    hkPadSpu<int> m_currentStep; // 0x00
    hkSimdFloat32 m_intregratePositionFactor; // 0x10
};

bool hkSolveConstraints(const hkpSolverInfo& info, hkpJacobianSchema* schemas, hkpVelocityAccumulator* accumulators, hkpSolverElemTemp* temp); // 0x00EA43E0
void hkSolveApplyGravityByTheSteps(const hkpSolverInfo& info, hkpVelocityAccumulator* accumulators, hkpVelocityAccumulator* accumulatorsEnd); // 0x00EA04E0
bool hkSolveConstraintsByTheSteps(const hkpSolverInfo& info, int currentSolverStep, int currentSolverMicroStep, const hkpJacobianSchema* schemas, hkpVelocityAccumulator* accumulators, hkpSolverElemTemp* elemTemp); // 0x00EA6840
int hkSolveIntegrateVelocitiesByTheSteps(const hkpSolverInfo& info, int currentSolverStep, hkpVelocityAccumulator* accumulators, hkpVelocityAccumulator* accumulatorsEnd, hkBool& outSolveFinished); // 0x00E9DB60
void hkSolveStepJacobians(const hkpSolverInfo& info, const hkpJacobianSchema* schemas, hkpVelocityAccumulator* accumulators, hkpSolverElemTemp* tmp); // 0x00EA2460
