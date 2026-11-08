// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/breakable/hkpbreakableconstraintdata.cpp
//
// Havok 2014.2.5: a constraint that breaks when its impulses grow too large (havok_constraint_create with a positive
// threshold), from rfg.exe. Its only atom is a bridge to buildJacobian, which builds the wrapped data's schemas (saving
// the bodies' velocities first) or, once broken, nothing. Before each build the solver setup calls
// buildJacobianCallback: the last step's impulses (the wrapped results' first floats, zeroed afterwards) above the limit
// break it (the constraint's and the world's listeners hear of it; with m_revertBackVelocityOnBreak the bodies'
// velocities go back towards the saved ones by limit / impulse).
//
// The broken and repaired events' m_eventSource would be &hkpBreakableConstraintDataClass (the reflection class); the
// classes are not ported, so it is null (no in-scope listener reads it). deepClone (hkpConstraintDataUtils::deepClone)
// has no in-scope caller and reports an error.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/breakable/hkpbreakableconstraintdata.h"

#include <stddef.h>

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/system/error/hkerror.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraintsolver/accumulator/hkpvelocityaccumulator.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraintsolver/constraint/hkpconstraintqueryin.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraintsolver/constraint/hkpconstraintqueryout.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraintsolver/solve/hkpsolverresults.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/atom/hkpsimplecontactconstraintatom.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/hkpconstraintinstance.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/hkpconstraintlistener.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/util/hkpconstraintcallbackutil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/hkpentity.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/hkpsimulationisland.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/hkpworld.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/util/hkpworldcallbackutil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/solver/atom/hkpbuildjacobianfromatoms.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/solver/contact/hkpsimplecontactconstraintinfo.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/solver/hkpsolversse.h"

using namespace hkSolverSse;

static_assert(sizeof(hkpBreakableConstraintData) == 0x30, "hkpBreakableConstraintData");
static_assert(offsetof(hkpBreakableConstraintData, m_solverResultLimit) == 0x24, "hkpBreakableConstraintData::m_solverResultLimit");
static_assert(sizeof(hkpBreakableConstraintData::Runtime) == 0x34, "hkpBreakableConstraintData::Runtime");

// 0x00DF5340 (stub_impl.md S4 SD): hkpBridgeAtoms(f) points the bridge at the loaded wrapped data first, then
// afterReflectNew points it at this data (the order of 2014.1 hkpBreakableConstraintData.cpp)
hkpBreakableConstraintData::hkpBreakableConstraintData(hkFinishLoadedObjectFlag f)
    : hkpWrappedConstraintData(f), m_atoms(f)
{
    if (f.m_finishing)
    {
        afterReflectNew();
    }
}

// 0x00DF5300 (stub_impl.md S4 SD, the type info's post-finish function): the child sizes are not serialized; they
// are taken from the loaded wrapped data (zero without one)
void hkpBreakableConstraintData::postFinish(void* data)
{
    hkpBreakableConstraintData* breakable = static_cast<hkpBreakableConstraintData*>(data);
    breakable->m_childRuntimeSize = 0;
    breakable->m_childNumSolverResults = 0;
    if (breakable->m_constraintData != nullptr)
    {
        hkpConstraintData::RuntimeInfo info;
        breakable->m_constraintData->getRuntimeInfo(true, info);
        breakable->m_childRuntimeSize = (unsigned short)info.m_sizeOfExternalRuntime;
        breakable->m_childNumSolverResults = (unsigned short)info.m_numSolverResults;
    }
}

// 0x00DF5190
void hkpBreakableConstraintData::afterReflectNew()
{
    m_atoms.m_bridgeAtom.init(this);
}

// 0x00DF53B0: a limit of 10, kept when broken, no velocity revert; the wrapped data's runtime (wanted) comes first in
// the instance's runtime, this data's after it
hkpBreakableConstraintData::hkpBreakableConstraintData(hkpConstraintData* constraintData)
    : hkpWrappedConstraintData(constraintData)
{
    m_atoms.m_bridgeAtom.m_constraintData = nullptr;
    m_solverResultLimit = 10.0f;
    m_removeWhenBroken = false;
    m_revertBackVelocityOnBreak = false;
    hkpConstraintData::RuntimeInfo info;
    m_constraintData->getRuntimeInfo(true, info);
    m_childNumSolverResults = (unsigned short)info.m_numSolverResults;
    m_childRuntimeSize = (unsigned short)info.m_sizeOfExternalRuntime;
    m_atoms.m_bridgeAtom.init(this);
}

// 0x00DF5380
hkpBreakableConstraintData::~hkpBreakableConstraintData()
{
}

// 0x0080DAC0 (hkpbreakableconstraintdata.inl:13)
void hkpBreakableConstraintData::setThreshold(float thresh)
{
    m_solverResultLimit = thresh;
}

// 0x0080DAD0 (hkpbreakableconstraintdata.inl:23)
void hkpBreakableConstraintData::setRemoveWhenBroken(hkBool tf)
{
    m_removeWhenBroken = tf;
}

// 0x00DF50B0
float hkpBreakableConstraintData::getThreshold() const
{
    return m_solverResultLimit;
}

// 0x00DF5840
hkpBreakableConstraintData::Runtime* hkpBreakableConstraintData::getRuntime(hkpConstraintRuntime* runtime)
{
    return reinterpret_cast<Runtime*>(reinterpret_cast<char*>(runtime) + m_childRuntimeSize);
}

// 0x00DF51F0: a change of state fires the event (on the constraint's listeners when it has any, then the world's);
// a broken constraint needs no more Jacobian callbacks, a repaired one again
void hkpBreakableConstraintData::setBroken(hkpConstraintInstance* instance, hkBool broken, float currentForce)
{
    unsigned char* isBroken = reinterpret_cast<unsigned char*>(instance->m_internal->m_runtime) + m_childRuntimeSize;
    if ((broken.m_bool != 0) == (*isBroken != 0))
    {
        return;
    }
    *isBroken = broken.m_bool;
    if (broken.m_bool == 0)
    {
        instance->m_internal->m_callbackRequest |= hkpConstraintAtom::CALLBACK_REQUEST_SETUP_CALLBACK;
    }
    else
    {
        instance->m_internal->m_callbackRequest &= ~hkpConstraintAtom::CALLBACK_REQUEST_SETUP_CALLBACK;
    }
    hkpWorld* world = static_cast<hkpSimulationIsland*>(instance->m_owner)->m_world;
    hkpConstraintBrokenEvent event;
    event.m_world = world;
    event.m_constraintInstance = instance;
    event.m_eventSource = nullptr; // &hkpBreakableConstraintDataClass, see the file comment
    event.m_eventSourceDetails = nullptr;
    if (broken.m_bool != 0)
    {
        event.m_actualImpulse = currentForce;
        event.m_impulseLimit = m_solverResultLimit;
        if (instance->m_listeners.getSize() != 0)
        {
            hkpConstraintCallbackUtil::fireConstraintBroken(event);
        }
        hkpWorldCallbackUtil::fireConstraintBroken(world, event);
        return;
    }
    const hkpConstraintRepairedEvent& repaired = reinterpret_cast<const hkpConstraintRepairedEvent&>(event);
    if (instance->m_listeners.getSize() != 0)
    {
        hkpConstraintCallbackUtil::fireConstraintRepaired(repaired);
    }
    hkpWorldCallbackUtil::fireConstraintRepaired(world, repaired);
}

// 0x00DF51E0
int hkpBreakableConstraintData::getType() const
{
    return CONSTRAINT_TYPE_BREAKABLE;
}

// 0x00DF52D0: the wrapped data's sizes, this data's bridge atom
void hkpBreakableConstraintData::getConstraintInfo(hkpConstraintData::ConstraintInfo& info) const
{
    m_constraintData->getConstraintInfo(info);
    info.m_atoms = const_cast<hkpConstraintAtom*>(static_cast<const hkpConstraintAtom*>(&m_atoms.m_bridgeAtom));
    info.m_sizeOfAllAtoms = sizeof(hkpBridgeConstraintAtom);
}

// 0x00DF51C0
void hkpBreakableConstraintData::getRuntimeInfo(hkBool wantRuntime, hkpConstraintData::RuntimeInfo& infoOut) const
{
    infoOut.m_numSolverResults = m_childNumSolverResults;
    infoOut.m_sizeOfExternalRuntime = m_childRuntimeSize + sizeof(Runtime);
}

// 0x00DF51A0: a header only (no solver results)
void hkpBreakableConstraintData::buildNopJacobian(const hkpConstraintQueryIn& in, hkpConstraintQueryOut& out)
{
    in.m_beginConstraints(in, out, nullptr, sizeof(hkpSolverResults));
}

// 0x00DF55C0: broken, only a header (and, if so set, the constraint leaves the world); else the bodies' velocities are
// saved for a revert and the wrapped data's atoms are built
void hkpBreakableConstraintData::buildJacobian(const hkpConstraintQueryIn& in, hkpConstraintQueryOut& out)
{
    Runtime* runtime = getRuntime(out.m_constraintRuntime.val());
    if (runtime->m_isBroken.m_bool != 0)
    {
        buildNopJacobian(in, out);
        if (m_removeWhenBroken.m_bool != 0)
        {
            hkpConstraintInstance* instance = in.m_constraintInstance.val();
            instance->m_entities[0]->m_world->removeConstraint(instance);
        }
        return;
    }
    const hkpVelocityAccumulator* bodyA = in.m_bodyA.val();
    const hkpVelocityAccumulator* bodyB = in.m_bodyB.val();
    for (int i = 0; i < 3; i++)
    {
        runtime->m_linearVelcityA[i] = bodyA->m_linearVel.m_quad.m128_f32[i];
        runtime->m_linearVelcityB[i] = bodyB->m_linearVel.m_quad.m128_f32[i];
        runtime->m_angularVelcityA[i] = bodyA->m_angularVel.m_quad.m128_f32[i];
        runtime->m_angularVelcityB[i] = bodyB->m_angularVel.m_quad.m128_f32[i];
    }
    hkpConstraintData::ConstraintInfo info;
    m_constraintData->getConstraintInfo(info);
    if (info.m_atoms->m_type.m_storage == hkpConstraintAtom::TYPE_CONTACT)
    {
        hkSimpleContactConstraintDataBuildJacobian(static_cast<hkpSimpleContactConstraintAtom*>(info.m_atoms), in, true, out);
        return;
    }
    hkSolverBuildJacobianFromAtomsNotContact(info.m_atoms, info.m_sizeOfAllAtoms, in, out);
}

// 0x00DF5920
hkBool hkpBreakableConstraintData::isBuildJacobianCallbackRequired() const
{
    return true;
}

namespace
{
// v back towards the saved (x, y, z, 0): (v - saved) * factor + saved
__forceinline void revertVelocity(hkVector4f& v, const float* saved, const Q& factor)
{
    const Q s = _mm_setr_ps(saved[0], saved[1], saved[2], 0.0f);
    v.m_quad = _mm_add_ps(_mm_mul_ps(_mm_sub_ps(v.m_quad, s), factor), s);
}
}

// 0x00DF5420: the last step's impulses (summed squares, in result order) above the limit squared break it with their
// length as the force; the impulses restart at zero either way
void hkpBreakableConstraintData::buildJacobianCallback(const hkpConstraintQueryIn& in, const hkpConstraintQueryOut& out)
{
    hkpSolverResults* results = reinterpret_cast<hkpSolverResults*>(out.m_constraintRuntime.val());
    const int numResults = m_childNumSolverResults;
    Runtime* runtime = reinterpret_cast<Runtime*>(reinterpret_cast<char*>(results) + m_childRuntimeSize);
    if (runtime->m_isBroken.m_bool == 0)
    {
        Q sum = _mm_setzero_ps();
        for (int i = 0; i < numResults; i++)
        {
            const Q impulse = splat(results[i].m_impulseApplied);
            sum = _mm_add_ps(sum, _mm_mul_ps(impulse, impulse));
        }
        const Q limit = splat(m_solverResultLimit);
        if (lane0(sum) > lane0(_mm_mul_ps(limit, limit)))
        {
            const Q force = sqrt23(sum);
            setBroken(in.m_constraintInstance.val(), true, lane0(force));
            if (m_revertBackVelocityOnBreak.m_bool != 0)
            {
                const Q factor = _mm_mul_ps(rcp23(force), limit);
                hkpVelocityAccumulator* bodyA = const_cast<hkpVelocityAccumulator*>(in.m_bodyA.val());
                hkpVelocityAccumulator* bodyB = const_cast<hkpVelocityAccumulator*>(in.m_bodyB.val());
                revertVelocity(bodyA->m_linearVel, runtime->m_linearVelcityA, factor);
                revertVelocity(bodyB->m_linearVel, runtime->m_linearVelcityB, factor);
                revertVelocity(bodyA->m_angularVel, runtime->m_angularVelcityA, factor);
                revertVelocity(bodyB->m_angularVel, runtime->m_angularVelcityB, factor);
            }
        }
    }
    for (int i = 0; i < numResults; i++)
    {
        results[i].m_impulseApplied = 0.0f;
    }
}

// 0x00DF56C0
hkpWrappedConstraintData* hkpBreakableConstraintData::deepClone() const
{
    hkErrorFwd::messageError(0x4F70455B, "OpenRFG: hkpBreakableConstraintData::deepClone not ported", "hkpbreakableconstraintdata.cpp", __LINE__);
    return nullptr;
}
