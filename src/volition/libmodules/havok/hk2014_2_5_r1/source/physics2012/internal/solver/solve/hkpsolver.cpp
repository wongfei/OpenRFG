// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/solver/solve/hkpsolver.cpp
//
// Havok 2014.2.5 (havok_impl.md HK8): the constraint solver, from rfg.exe (written from the disassembly in its SSE
// operation order). hkSolveConstraints turns the accumulators' core-from-world matrices into quaternions (rigid
// bodies get the step's gravity), then per step and micro step runs every schema once (sequential impulses on the
// accumulator velocities, impulses accumulated in the solver elem temps), and per step integrates the velocities
// (damping slow bodies, summing the velocities for the export). hkSolveStepJacobians (the TOI solver) is one such
// pass without integrating. Ported are the schemas of contact constraints and their
// response modifiers, since MP7 (multiplayer.md) those of the ragdoll joints (1d bilateral, limited too, angular,
// angular limits, angular friction), the linear limits and friction of the prismatic constraint (stuck projectiles)
// and the hinge's angular motor (havok_constraint_add_motor); the user tau, linear motor, stable, deformable, chain
// and wheel friction schemas belong to constraint types no in-scope code creates (havok_impl.md section 2) and report
// an error.
// The license check (hkpCheckKeycode/hkpProcessFlyingColors, S phase) always passes.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraintsolver/solve/hkpsolve.h"

#include <string.h>

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/system/error/hkerror.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraintsolver/accumulator/hkpvelocityaccumulator.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraintsolver/jacobian/hkpjacobianschema.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraintsolver/solve/hkpsolver.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraintsolver/solve/hkpsolverinfo.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/solver/hkpsolversse.h"

using namespace hkSolverSse;

namespace hkpAtomSolverFunctions
{
template<typename ACCUM, typename INFO, typename TEMP>
void solve2dRollingFriction(const INFO& info, const TEMP& temp, const hkpJacobian2dRollingFrictionSchema* schema, ACCUM* mA, ACCUM* mB, hkpSolverElemTemp* temps);
}

namespace
{
__forceinline Q loadQ(const void* p) { return _mm_load_ps(static_cast<const float*>(p)); }
__forceinline void storeQ(void* p, const Q& v) { _mm_store_ps(static_cast<float*>(p), v); }

// a 1 linear 2 angular row (linear, angular A, angular B; 16-byte aligned floats)
struct Row
{
    const float* m_jac;
    Q lin() const { return loadQ(m_jac); }
    Q ang0() const { return loadQ(m_jac + 4); }
    Q ang1() const { return loadQ(m_jac + 8); }
};

// the velocity along a row: hadd3(((B.ang * ang1) + (A.ang * ang0)) + (A.lin - B.lin) * lin)
__forceinline Q rowVelocity(const Row& row, const Q& dLin, const Q& angA, const Q& angB)
{
    return hadd3(_mm_add_ps(_mm_add_ps(_mm_mul_ps(angB, row.ang1()), _mm_mul_ps(angA, row.ang0())), _mm_mul_ps(dLin, row.lin())));
}

// an impulse along a row: A gains it, B loses it (through the inverse masses; the linear part by the mass in w)
__forceinline void applyRow(const Row& row, const Q& impulse, hkpVelocityAccumulator* mA, hkpVelocityAccumulator* mB)
{
    const Q a = _mm_mul_ps(mA->m_invMasses.m_quad, impulse);
    const Q b = _mm_mul_ps(mB->m_invMasses.m_quad, impulse);
    mA->m_linearVel.m_quad = _mm_add_ps(_mm_mul_ps(wwww(a), row.lin()), mA->m_linearVel.m_quad);
    mB->m_linearVel.m_quad = _mm_sub_ps(mB->m_linearVel.m_quad, _mm_mul_ps(wwww(b), row.lin()));
    mA->m_angularVel.m_quad = _mm_add_ps(mA->m_angularVel.m_quad, _mm_mul_ps(a, row.ang0()));
    mB->m_angularVel.m_quad = _mm_add_ps(_mm_mul_ps(b, row.ang1()), mB->m_angularVel.m_quad);
}

// the angular part of an impulse along a 2 angular row
__forceinline void applyAngular(const float* jac, const Q& impulse, hkpVelocityAccumulator* mA, hkpVelocityAccumulator* mB)
{
    mA->m_angularVel.m_quad = _mm_add_ps(mA->m_angularVel.m_quad, _mm_mul_ps(loadQ(jac), _mm_mul_ps(impulse, mA->m_invMasses.m_quad)));
    mB->m_angularVel.m_quad = _mm_add_ps(mB->m_angularVel.m_quad, _mm_mul_ps(loadQ(jac + 4), _mm_mul_ps(impulse, mB->m_invMasses.m_quad)));
}

__forceinline void addTemp(hkpSolverElemTemp* temp, const Q& impulse)
{
    temp->m_impulseApplied = lane0(_mm_add_ps(splat(temp->m_impulseApplied), impulse));
}

// the friction cone: impulses longer than the max impulse are scaled down (the scale is kept in a temp)
__forceinline Q frictionScale(const Q& lenSq, float maxImpulse, hkpSolverElemTemp* scaleTemp)
{
    const Q max = splat(maxImpulse);
    if (lane0(lenSq) > lane0(_mm_mul_ps(max, max)))
    {
        const Q r = _mm_rsqrt_ps(lenSq);
        const Q inv = _mm_mul_ps(_mm_sub_ps(g_vectorfConstants[HK_QUADREAL_3], _mm_mul_ps(_mm_mul_ps(r, lenSq), r)), _mm_mul_ps(r, _mm_set1_ps(0.5f)));
        const Q scale = _mm_mul_ps(inv, max);
        scaleTemp->m_impulseApplied = lane0(scale);
        return scale;
    }
    scaleTemp->m_impulseApplied = 1.0f;
    return _mm_set_ss(1.0f);
}

// SCHEMA_TYPE_SINGLE_CONTACT: the impulse keeps the accumulated impulse non-negative
__forceinline void solveSingleContact(const float* jac, hkpVelocityAccumulator* mA, hkpVelocityAccumulator* mB, hkpSolverElemTemp* temp)
{
    const Row row = { jac };
    const Q v = rowVelocity(row, _mm_sub_ps(mA->m_linearVel.m_quad, mB->m_linearVel.m_quad), mA->m_angularVel.m_quad, mB->m_angularVel.m_quad);
    const Q dImpulse = _mm_mul_ps(wwww(row.ang0()), _mm_sub_ps(wwww(row.lin()), v));
    const Q impulse = _mm_max_ps(_mm_sub_ps(_mm_setzero_ps(), splat(temp->m_impulseApplied)), dImpulse);
    applyRow(row, impulse, mA, mB);
    addTemp(temp, impulse);
}

// SCHEMA_TYPE_SINGLE_LIMIT_CONTACT: also at most the max impulse (in ang1.w) in all
__forceinline void solveSingleLimitContact(const float* jac, hkpVelocityAccumulator* mA, hkpVelocityAccumulator* mB, hkpSolverElemTemp* temp)
{
    const Row row = { jac };
    const Q v = rowVelocity(row, _mm_sub_ps(mA->m_linearVel.m_quad, mB->m_linearVel.m_quad), mA->m_angularVel.m_quad, mB->m_angularVel.m_quad);
    const Q dImpulse = _mm_mul_ps(wwww(row.ang0()), _mm_sub_ps(wwww(row.lin()), v));
    const Q accumulated = splat(temp->m_impulseApplied);
    const Q lower = _mm_max_ps(_mm_sub_ps(_mm_setzero_ps(), accumulated), dImpulse);
    const Q impulse = _mm_min_ps(_mm_sub_ps(wwww(row.ang1()), accumulated), lower);
    applyRow(row, impulse, mA, mB);
    addTemp(temp, impulse);
}

// SCHEMA_TYPE_PAIR_CONTACT: both rows from the same velocities through the 2x2 inverse (the coupled effective masses
// in ang1.w, the coupling in m_invJac01); when one impulse would pull, the other row is solved alone
__forceinline void solvePairContact(const hkpJacobianPairContactSchema* schema, hkpVelocityAccumulator* mA, hkpVelocityAccumulator* mB, hkpSolverElemTemp* temps)
{
    const Row row0 = { reinterpret_cast<const float*>(&schema->m_jac.m_jac0) };
    const Row row1 = { reinterpret_cast<const float*>(&schema->m_jac.m_jac1) };
    const Q dLin = _mm_sub_ps(mA->m_linearVel.m_quad, mB->m_linearVel.m_quad);
    const Q r0 = _mm_sub_ps(wwww(row0.lin()), rowVelocity(row0, dLin, mA->m_angularVel.m_quad, mB->m_angularVel.m_quad));
    const Q r1 = _mm_sub_ps(wwww(row1.lin()), rowVelocity(row1, dLin, mA->m_angularVel.m_quad, mB->m_angularVel.m_quad));
    const Q invJac01 = splat(schema->m_invJac01);
    const Q impulse0 = _mm_add_ps(_mm_mul_ps(wwww(row0.ang1()), r0), _mm_mul_ps(invJac01, r1));
    const Q impulse1 = _mm_add_ps(_mm_mul_ps(wwww(row1.ang1()), r1), _mm_mul_ps(invJac01, r0));
    const Q negT0 = splat(-temps[0].m_impulseApplied);
    const Q negT1 = splat(-temps[1].m_impulseApplied);
    if (lane0(impulse0) > lane0(negT0))
    {
        if (lane0(impulse1) > lane0(negT1))
        {
            applyRow(row0, impulse0, mA, mB);
            addTemp(&temps[0], impulse0);
            applyRow(row1, impulse1, mA, mB);
            addTemp(&temps[1], impulse1);
            return;
        }
    }
    else
    {
        const Q single1 = _mm_mul_ps(wwww(row1.ang0()), r1);
        if (lane0(single1) > lane0(negT1))
        {
            applyRow(row1, single1, mA, mB);
            addTemp(&temps[1], single1);
            return;
        }
    }
    const Q single0 = _mm_max_ps(negT0, _mm_mul_ps(wwww(row0.ang0()), r0));
    applyRow(row0, single0, mA, mB);
    addTemp(&temps[0], single0);
}

// SCHEMA_TYPE_2D_FRICTION: the two friction rows through their 2x2 inverse, within the friction cone
__forceinline void solveFriction2d(const hkpJacobian2dFrictionSchema* schema, hkpVelocityAccumulator* mA, hkpVelocityAccumulator* mB, hkpSolverElemTemp* temps)
{
    const Row row0 = { reinterpret_cast<const float*>(&schema->m_jac.m_jac0) };
    const Row row1 = { reinterpret_cast<const float*>(&schema->m_jac.m_jac1) };
    const Q dLin = _mm_sub_ps(mA->m_linearVel.m_quad, mB->m_linearVel.m_quad);
    const Q r0 = _mm_sub_ps(wwww(row0.lin()), rowVelocity(row0, dLin, mA->m_angularVel.m_quad, mB->m_angularVel.m_quad));
    const Q r1 = _mm_sub_ps(wwww(row1.lin()), rowVelocity(row1, dLin, mA->m_angularVel.m_quad, mB->m_angularVel.m_quad));
    const Q invJac01 = splat(schema->m_invJac01);
    Q impulse0 = _mm_add_ps(_mm_mul_ps(wwww(row0.ang1()), r0), _mm_mul_ps(invJac01, r1));
    Q impulse1 = _mm_add_ps(_mm_mul_ps(wwww(row1.ang1()), r1), _mm_mul_ps(r0, invJac01));
    const Q lenSq = _mm_add_ps(_mm_mul_ps(impulse1, impulse1), _mm_mul_ps(impulse0, impulse0));
    const Q scale = frictionScale(lenSq, schema->m_maxImpulse, &temps[2]);
    if (lane0(lenSq) > lane0(_mm_mul_ps(splat(schema->m_maxImpulse), splat(schema->m_maxImpulse))))
    {
        impulse0 = _mm_mul_ps(impulse0, scale);
        impulse1 = _mm_mul_ps(impulse1, scale);
    }
    applyRow(row0, impulse0, mA, mB);
    addTemp(&temps[0], impulse0);
    applyRow(row1, impulse1, mA, mB);
    addTemp(&temps[1], impulse1);
}

// SCHEMA_TYPE_3D_FRICTION: 2d friction plus the angular row around the normal (its impulse per unit contact radius)
__forceinline void solveFriction3d(const hkpJacobian3dFrictionSchema* schema, hkpVelocityAccumulator* mA, hkpVelocityAccumulator* mB, hkpSolverElemTemp* temps)
{
    const Row row0 = { reinterpret_cast<const float*>(&schema->m_jac.m_jac0) };
    const Row row1 = { reinterpret_cast<const float*>(&schema->m_jac.m_jac1) };
    const float* jac2 = reinterpret_cast<const float*>(&schema->m_jac.m_jac2);
    const Q dLin = _mm_sub_ps(mA->m_linearVel.m_quad, mB->m_linearVel.m_quad);
    const Q r0 = _mm_sub_ps(wwww(row0.lin()), rowVelocity(row0, dLin, mA->m_angularVel.m_quad, mB->m_angularVel.m_quad));
    const Q r1 = _mm_sub_ps(wwww(row1.lin()), rowVelocity(row1, dLin, mA->m_angularVel.m_quad, mB->m_angularVel.m_quad));
    const Q v2 = hadd3(_mm_add_ps(_mm_mul_ps(loadQ(jac2), mA->m_angularVel.m_quad), _mm_mul_ps(mB->m_angularVel.m_quad, loadQ(jac2 + 4))));
    const Q r2 = _mm_sub_ps(wwww(loadQ(jac2 + 4)), v2);
    const Q invJac01 = splat(schema->m_invJac01);
    Q impulse2 = _mm_mul_ps(wwww(loadQ(jac2)), r2);
    Q impulse0 = _mm_add_ps(_mm_mul_ps(wwww(row0.ang1()), r0), _mm_mul_ps(r1, invJac01));
    Q impulse1 = _mm_add_ps(_mm_mul_ps(wwww(row1.ang1()), r1), _mm_mul_ps(invJac01, r0));
    const Q lenSq = _mm_add_ps(_mm_add_ps(_mm_mul_ps(impulse1, impulse1), _mm_mul_ps(impulse0, impulse0)), _mm_mul_ps(impulse2, impulse2));
    const Q scale = frictionScale(lenSq, schema->m_maxImpulse, &temps[3]);
    if (lane0(lenSq) > lane0(_mm_mul_ps(splat(schema->m_maxImpulse), splat(schema->m_maxImpulse))))
    {
        impulse0 = _mm_mul_ps(impulse0, scale);
        impulse1 = _mm_mul_ps(impulse1, scale);
        impulse2 = _mm_mul_ps(impulse2, scale);
    }
    const Q angularImpulse = _mm_mul_ps(splat(schema->m_contactRadius), impulse2);
    applyRow(row0, impulse0, mA, mB);
    addTemp(&temps[0], impulse0);
    applyRow(row1, impulse1, mA, mB);
    addTemp(&temps[1], impulse1);
    applyAngular(jac2, angularImpulse, mA, mB);
    addTemp(&temps[2], angularImpulse);
}
}

// 0x00EAAE70: the two rolling rows (angular only) within the max impulse
template<typename ACCUM, typename INFO, typename TEMP>
void hkpAtomSolverFunctions::solve2dRollingFriction(const INFO& info, const TEMP& temp, const hkpJacobian2dRollingFrictionSchema* schema, ACCUM* mA, ACCUM* mB, hkpSolverElemTemp* temps)
{
    const float* jac0 = reinterpret_cast<const float*>(&schema->m_jac0);
    const float* jac1 = reinterpret_cast<const float*>(&schema->m_jac1);
    const Q j0a0 = loadQ(jac0);
    const Q j0a1 = loadQ(jac0 + 4);
    const Q j1a0 = loadQ(jac1);
    const Q j1a1 = loadQ(jac1 + 4);
    const Q v0 = hadd3(_mm_add_ps(_mm_mul_ps(j0a0, mA->m_angularVel.m_quad), _mm_mul_ps(j0a1, mB->m_angularVel.m_quad)));
    Q impulse0 = _mm_mul_ps(_mm_sub_ps(wwww(j0a1), v0), wwww(j0a0));
    const Q v1 = hadd3(_mm_add_ps(_mm_mul_ps(j1a0, mA->m_angularVel.m_quad), _mm_mul_ps(j1a1, mB->m_angularVel.m_quad)));
    Q impulse1 = _mm_mul_ps(_mm_sub_ps(wwww(j1a1), v1), wwww(j1a0));
    const Q max = splat(schema->m_maxImpulse);
    const Q lenSq = _mm_add_ps(_mm_mul_ps(impulse1, impulse1), _mm_mul_ps(impulse0, impulse0));
    if (lane0(lenSq) > lane0(_mm_mul_ps(max, max)))
    {
        const Q scale = _mm_mul_ps(invSqrt23(lenSq), max);
        temps[2].m_impulseApplied = lane0(scale);
        impulse0 = _mm_mul_ps(impulse0, scale);
        impulse1 = _mm_mul_ps(impulse1, scale);
    }
    else
    {
        temps[2].m_impulseApplied = 1.0f;
    }
    mA->m_angularVel.m_quad = _mm_add_ps(_mm_mul_ps(_mm_mul_ps(mA->m_invMasses.m_quad, impulse0), j0a0), mA->m_angularVel.m_quad);
    mB->m_angularVel.m_quad = _mm_add_ps(_mm_mul_ps(_mm_mul_ps(mB->m_invMasses.m_quad, impulse0), j0a1), mB->m_angularVel.m_quad);
    const Q t0 = _mm_add_ps(splat(temps[0].m_impulseApplied), impulse0);
    mA->m_angularVel.m_quad = _mm_add_ps(_mm_mul_ps(_mm_mul_ps(mA->m_invMasses.m_quad, impulse1), j1a0), mA->m_angularVel.m_quad);
    mB->m_angularVel.m_quad = _mm_add_ps(_mm_mul_ps(_mm_mul_ps(mB->m_invMasses.m_quad, impulse1), j1a1), mB->m_angularVel.m_quad);
    temps[0].m_impulseApplied = lane0(t0);
    temps[1].m_impulseApplied = lane0(_mm_add_ps(splat(temps[1].m_impulseApplied), impulse1));
}

// 0x00EABBE0 (<1>, the last step) and 0x00EABEA0 (<0>): per accumulator up to the end marker (accEnd null: no
// limit). Dynamic bodies (types 0 and 2) with an invalid velocity (not finite or over 1e6) get (1, 0, 0, 0); slow
// bodies (their deactivation class's weighted speed at most 1) are damped by the slow-object multiplier, or stopped
// below the relative sleep threshold. The sum velocities gather the step's velocities (times the integrate factor);
// on the last step they are scaled to the step's average and the velocities become the step's change, otherwise
// the velocities restart from the sums plus the step's gravity (rigid bodies only).
template<int LAST_STEP>
void hkSolver::integrateVelocities(const hkpSolverInfo& info, const hkVector4f& velocityField, const hkSimdFloat32& sumVelFactor, hkpVelocityAccumulator* acc, hkpVelocityAccumulator* accEnd)
{
    if (accEnd == nullptr)
    {
        accEnd = reinterpret_cast<hkpVelocityAccumulator*>(0xFFFFFFFF);
    }
    for (; acc < accEnd; acc++)
    {
        const unsigned char type = acc->m_type.m_storage;
        if (type == hkpVelocityAccumulator::HK_END)
        {
            return;
        }
        if (type != hkpVelocityAccumulator::HK_KEYFRAMED_RIGID_BODY)
        {
            const hkpSolverInfo::DeactivationInfo& deactivation = info.m_deactivationInfo[acc->m_deactivationClass];
            const Q lin = acc->m_linearVel.m_quad;
            const Q ang = acc->m_angularVel.m_quad;
            const Q absLin = _mm_castsi128_ps(_mm_srli_epi32(_mm_slli_epi32(_mm_castps_si128(lin), 1), 1));
            const Q absAng = _mm_castsi128_ps(_mm_srli_epi32(_mm_slli_epi32(_mm_castps_si128(ang), 1), 1));
            const Q speed = _mm_add_ps(_mm_mul_ps(splat(deactivation.m_angularVelocityThresholdInv), absAng), _mm_mul_ps(splat(deactivation.m_linearVelocityThresholdInv), absLin));
            const Q maxVel = splat(1000000.0f); // maxVelf (0x013657B0)
            Q valid = _mm_and_ps(_mm_cmplt_ps(absAng, maxVel), _mm_cmplt_ps(absLin, maxVel));
            valid = _mm_and_ps(valid, _mm_cmpeq_ps(lin, lin));
            valid = _mm_and_ps(valid, _mm_cmpeq_ps(ang, ang));
            if ((_mm_movemask_ps(valid) & 7) != 7)
            {
                acc->m_linearVel.m_quad = g_vectorfConstants[HK_QUADREAL_1000];
                acc->m_angularVel.m_quad = g_vectorfConstants[HK_QUADREAL_1000];
            }
            if ((_mm_movemask_ps(_mm_cmple_ps(speed, g_vectorfConstants[HK_QUADREAL_1])) & 7) == 7)
            {
                if ((_mm_movemask_ps(_mm_cmple_ps(speed, splat(deactivation.m_relativeSleepVelocityThreshold))) & 7) == 7)
                {
                    acc->m_angularVel.setZero();
                    acc->m_linearVel.setZero();
                }
                else
                {
                    const Q factor = splat(deactivation.m_slowObjectVelocityMultiplier);
                    acc->m_angularVel.m_quad = _mm_mul_ps(acc->m_angularVel.m_quad, factor);
                    acc->m_linearVel.m_quad = _mm_mul_ps(factor, acc->m_linearVel.m_quad);
                }
            }
        }
        const Q dLin = _mm_sub_ps(acc->m_linearVel.m_quad, acc->m_scratch0.m_quad);
        const Q dAng = _mm_sub_ps(acc->m_angularVel.m_quad, acc->m_scratch1.m_quad);
        acc->m_scratch0.m_quad = _mm_add_ps(_mm_mul_ps(info.m_integrateVelocityFactor.m_quad, dLin), acc->m_scratch0.m_quad);
        acc->m_scratch1.m_quad = _mm_add_ps(_mm_mul_ps(info.m_integrateVelocityFactor.m_quad, dAng), acc->m_scratch1.m_quad);
        if (LAST_STEP)
        {
            acc->m_scratch0.m_quad = _mm_mul_ps(acc->m_scratch0.m_quad, sumVelFactor.m_real);
            acc->m_scratch1.m_quad = _mm_mul_ps(acc->m_scratch1.m_quad, sumVelFactor.m_real);
            acc->m_linearVel.m_quad = dLin;
            acc->m_angularVel.m_quad = dAng;
        }
        else
        {
            // the step's gravity for rigid bodies; keyframed and no-gravity bodies get it times 0 (kept from the
            // binary, which has a copy of the loop per type)
            const Q field = type == hkpVelocityAccumulator::HK_RIGID_BODY ? velocityField.m_quad : g_vectorfConstants[HK_QUADREAL_0];
            const Q gravity = _mm_mul_ps(splat(acc->m_gravityFactor), field);
            acc->m_linearVel.m_quad = _mm_add_ps(gravity, _mm_add_ps(acc->m_scratch0.m_quad, dLin));
            acc->m_angularVel.m_quad = _mm_add_ps(acc->m_scratch1.m_quad, dAng);
        }
    }
}

template void hkSolver::integrateVelocities<1>(const hkpSolverInfo&, const hkVector4f&, const hkSimdFloat32&, hkpVelocityAccumulator*, hkpVelocityAccumulator*);
template void hkSolver::integrateVelocities<0>(const hkpSolverInfo&, const hkVector4f&, const hkSimdFloat32&, hkpVelocityAccumulator*, hkpVelocityAccumulator*);

namespace
{
// SCHEMA_TYPE_1D_BILATERAL (a ball socket's axis, MP7): a contact row without the clamp
__forceinline void solve1dBilateral(const float* jac, hkpVelocityAccumulator* mA, hkpVelocityAccumulator* mB, hkpSolverElemTemp* temp)
{
    const Row row = { jac };
    const Q v = rowVelocity(row, _mm_sub_ps(mA->m_linearVel.m_quad, mB->m_linearVel.m_quad), mA->m_angularVel.m_quad, mB->m_angularVel.m_quad);
    const Q impulse = _mm_mul_ps(wwww(row.ang0()), _mm_sub_ps(wwww(row.lin()), v));
    applyRow(row, impulse, mA, mB);
    addTemp(temp, impulse);
}

// 0x00EAB3E0 SCHEMA_TYPE_1D_BILATERAL_WITH_IMPULSE_LIMIT (a ball socket with a breach impulse, MP7): the accumulated
// impulse stays within +-limit (in ang1.w)
__forceinline void solve1dBilateralWithImpulseLimit(const float* jac, hkpVelocityAccumulator* mA, hkpVelocityAccumulator* mB, hkpSolverElemTemp* temp)
{
    const Row row = { jac };
    const Q v = rowVelocity(row, _mm_sub_ps(mA->m_linearVel.m_quad, mB->m_linearVel.m_quad), mA->m_angularVel.m_quad, mB->m_angularVel.m_quad);
    const Q impulse = _mm_mul_ps(_mm_sub_ps(wwww(row.lin()), v), wwww(row.ang0()));
    const Q limit = wwww(row.ang1());
    const Q accumulated = splat(temp->m_impulseApplied);
    const Q clamped = _mm_min_ps(_mm_sub_ps(limit, accumulated), _mm_max_ps(_mm_sub_ps(_mm_sub_ps(_mm_setzero_ps(), limit), accumulated), impulse));
    applyRow(row, clamped, mA, mB);
    temp->m_impulseApplied = lane0(_mm_add_ps(accumulated, clamped));
}

// 0x00EAB780 SCHEMA_TYPE_1D_LINEAR_LIMITS (a prismatic constraint's range): below the minimum the impulse pushes (when
// the accumulated impulse stays above zero); then, above the maximum, it pulls. Original: the second test compares
// with the accumulated impulse from before the first impulse (the temp is updated in between).
__forceinline void solve1dLinLimits(const hkpJacobian1dLinearLimitsSchema* schema, hkpVelocityAccumulator* mA, hkpVelocityAccumulator* mB, hkpSolverElemTemp* temp)
{
    const Row row = { reinterpret_cast<const float*>(&schema->m_jac) };
    const Q v = rowVelocity(row, _mm_sub_ps(mA->m_linearVel.m_quad, mB->m_linearVel.m_quad), mA->m_angularVel.m_quad, mB->m_angularVel.m_quad);
    const Q effMass = wwww(row.ang0());
    const Q a = _mm_sub_ps(wwww(row.lin()), v);
    const Q accumulated = splat(temp->m_impulseApplied);
    const Q negAccumulated = _mm_sub_ps(_mm_setzero_ps(), accumulated);
    const Q impulseMin = _mm_mul_ps(_mm_sub_ps(a, splat(schema->m_minRhs)), effMass);
    if (lane0(impulseMin) > lane0(negAccumulated))
    {
        applyRow(row, impulseMin, mA, mB);
        temp->m_impulseApplied = lane0(_mm_add_ps(accumulated, impulseMin));
    }
    const Q impulseMax = _mm_mul_ps(_mm_sub_ps(a, splat(schema->m_maxRhs)), effMass);
    // JNC: an unordered compare applies the impulse
    if (!(lane0(impulseMax) >= lane0(negAccumulated)))
    {
        applyRow(row, impulseMax, mA, mB);
        temp->m_impulseApplied = lane0(_mm_add_ps(splat(temp->m_impulseApplied), impulseMax));
    }
}

// 0x00EAB660 SCHEMA_TYPE_1D_FRICTION (a prismatic constraint's friction): the impulse within +-m_maxFrictionImpulse;
// the scale applied goes to the second temp
__forceinline void solve1dFriction(const hkpJacobian1dFrictionSchema* schema, hkpVelocityAccumulator* mA, hkpVelocityAccumulator* mB, hkpSolverElemTemp* temps)
{
    const Row row = { reinterpret_cast<const float*>(&schema->m_jac) };
    const Q v = rowVelocity(row, _mm_sub_ps(mA->m_linearVel.m_quad, mB->m_linearVel.m_quad), mA->m_angularVel.m_quad, mB->m_angularVel.m_quad);
    Q impulse = _mm_mul_ps(_mm_sub_ps(wwww(row.lin()), v), wwww(row.ang0()));
    const Q max = splat(schema->m_maxFrictionImpulse);
    const Q absImpulse = _mm_castsi128_ps(_mm_srli_epi32(_mm_slli_epi32(_mm_castps_si128(impulse), 1), 1));
    float scale;
    if (lane0(absImpulse) > lane0(max))
    {
        scale = lane0(_mm_mul_ps(rcp23(absImpulse), max));
        // the impulse becomes +-max (its sign bit on max)
        impulse = _mm_xor_ps(_mm_castsi128_ps(_mm_slli_epi32(_mm_srli_epi32(_mm_castps_si128(impulse), 31), 31)), max);
    }
    else
    {
        scale = 1.0f;
    }
    temps[1].m_impulseApplied = scale;
    applyRow(row, impulse, mA, mB);
    addTemp(&temps[0], impulse);
}

// 0x00EAB0F0 SCHEMA_TYPE_1D_ANGULAR_MOTOR (a hinge's motor): the velocity towards the target velocity (by the step's
// position factor) plus the right hand side, against the summed velocities (by invIntegrateVelocityFactor * tau) and
// the velocities' change (by damping), all by tau; an impulse beyond the range is clamped to it, the second temp
// keeping the scale
__forceinline void solve1dAngularMotor(const hkpSolverInfo& info, const hkSolverStepTemp& stepTemp, const hkpJacobian1dAngularMotorSchema* schema, hkpVelocityAccumulator* mA, hkpVelocityAccumulator* mB, hkpSolverElemTemp* temps)
{
    const float* jac = reinterpret_cast<const float*>(&schema->m_jac);
    const Q ang0 = loadQ(jac);
    const Q ang1 = loadQ(jac + 4);
    const Q sumA = mA->m_scratch1.m_quad;
    const Q sumB = mB->m_scratch1.m_quad;
    const Q tau = splat(schema->m_tau);
    const Q changes = _mm_mul_ps(_mm_add_ps(_mm_mul_ps(_mm_sub_ps(mA->m_angularVel.m_quad, sumA), ang0), _mm_mul_ps(_mm_sub_ps(mB->m_angularVel.m_quad, sumB), ang1)), splat(schema->m_damping));
    const Q sumFactor = _mm_mul_ps(xxxx(info.m_invIntegrateVelocityFactor.m_quad), tau);
    const Q sums = _mm_mul_ps(_mm_add_ps(_mm_mul_ps(ang0, sumA), _mm_mul_ps(ang1, sumB)), sumFactor);
    const Q velocity = hadd3(_mm_add_ps(changes, sums));
    const Q target = _mm_mul_ps(_mm_add_ps(_mm_mul_ps(splat(schema->m_velocity), stepTemp.m_intregratePositionFactor.m_real), wwww(ang1)), tau);
    Q impulse = _mm_mul_ps(_mm_sub_ps(target, velocity), wwww(ang0));
    const Q max = splat(schema->m_maxImpulsePerSubstep);
    const Q min = splat(schema->m_minImpulsePerSubstep);
    if (lane0(impulse) > lane0(max))
    {
        temps[1].m_impulseApplied = lane0(_mm_mul_ps(rcp23(impulse), max));
        impulse = max;
    }
    // JNC: an unordered compare clamps to the minimum
    else if (!(lane0(impulse) >= lane0(min)))
    {
        temps[1].m_impulseApplied = lane0(_mm_mul_ps(rcp23(impulse), min));
        impulse = min;
    }
    else
    {
        temps[1].m_impulseApplied = 1.0f;
    }
    mA->m_angularVel.m_quad = _mm_add_ps(_mm_mul_ps(_mm_mul_ps(mA->m_invMasses.m_quad, impulse), ang0), mA->m_angularVel.m_quad);
    mB->m_angularVel.m_quad = _mm_add_ps(_mm_mul_ps(_mm_mul_ps(mB->m_invMasses.m_quad, impulse), ang1), mB->m_angularVel.m_quad);
    addTemp(&temps[0], impulse);
}

// the velocity along a 2 angular row: hadd3((B.ang * ang1) + (ang0 * A.ang))
__forceinline Q angularVelocity(const float* jac, const hkpVelocityAccumulator* mA, const hkpVelocityAccumulator* mB)
{
    return hadd3(_mm_add_ps(_mm_mul_ps(mB->m_angularVel.m_quad, loadQ(jac + 4)), _mm_mul_ps(loadQ(jac), mA->m_angularVel.m_quad)));
}

// SCHEMA_TYPE_1D_ANGULAR (a hinge's axes, MP7): the effective mass times the right hand side minus the velocity
__forceinline void solve1dAngular(const float* jac, hkpVelocityAccumulator* mA, hkpVelocityAccumulator* mB, hkpSolverElemTemp* temp)
{
    const Q ang0 = loadQ(jac);
    const Q ang1 = loadQ(jac + 4);
    const Q impulse = _mm_mul_ps(wwww(ang0), _mm_sub_ps(wwww(ang1), angularVelocity(jac, mA, mB)));
    mA->m_angularVel.m_quad = _mm_add_ps(_mm_mul_ps(_mm_mul_ps(mA->m_invMasses.m_quad, impulse), ang0), mA->m_angularVel.m_quad);
    mB->m_angularVel.m_quad = _mm_add_ps(_mm_mul_ps(_mm_mul_ps(mB->m_invMasses.m_quad, impulse), ang1), mB->m_angularVel.m_quad);
    addTemp(temp, impulse);
}

// SCHEMA_TYPE_1D_ANGULAR_FRICTION (MP7): the impulse within +-m_maxImpulse; the scale applied goes to the second temp
__forceinline void solve1dAngularFriction(const hkpJacobian1dAngularFrictionSchema* schema, hkpVelocityAccumulator* mA, hkpVelocityAccumulator* mB, hkpSolverElemTemp* temps)
{
    const float* jac = reinterpret_cast<const float*>(&schema->m_jac);
    const Q ang0 = loadQ(jac);
    const Q ang1 = loadQ(jac + 4);
    const Q v = hadd3(_mm_add_ps(_mm_mul_ps(mB->m_angularVel.m_quad, ang1), _mm_mul_ps(mA->m_angularVel.m_quad, ang0)));
    Q impulse = _mm_mul_ps(wwww(ang0), _mm_sub_ps(wwww(ang1), v));
    const Q max = splat(schema->m_maxImpulse);
    const Q absImpulse = _mm_castsi128_ps(_mm_srli_epi32(_mm_slli_epi32(_mm_castps_si128(impulse), 1), 1));
    float scale;
    if (lane0(absImpulse) > lane0(max))
    {
        scale = lane0(_mm_mul_ps(rcp23(absImpulse), max));
        // the impulse becomes +-max (its sign bit on max)
        impulse = _mm_xor_ps(_mm_castsi128_ps(_mm_slli_epi32(_mm_srli_epi32(_mm_castps_si128(impulse), 31), 31)), max);
    }
    else
    {
        scale = 1.0f;
    }
    temps[1].m_impulseApplied = scale;
    mA->m_angularVel.m_quad = _mm_add_ps(_mm_mul_ps(_mm_mul_ps(impulse, mA->m_invMasses.m_quad), ang0), mA->m_angularVel.m_quad);
    mB->m_angularVel.m_quad = _mm_add_ps(_mm_mul_ps(_mm_mul_ps(impulse, mB->m_invMasses.m_quad), ang1), mB->m_angularVel.m_quad);
    addTemp(&temps[0], impulse);
}

// rotates v by the unit quaternion q: 2 * ((v * (w^2 - 1/2) + q * (q . v)) + w * (q x v))
__forceinline Q rotateByQuaternion(const Q& q, const Q& v)
{
    const Q w = wwww(q);
    const Q scaled = _mm_mul_ps(v, _mm_sub_ps(_mm_mul_ps(w, w), g_vectorfConstants[HK_QUADREAL_INV_2]));
    const Q projected = _mm_add_ps(_mm_mul_ps(hadd3(_mm_mul_ps(q, v)), q), scaled);
    const Q r = _mm_add_ps(_mm_mul_ps(cross(q, v), w), projected);
    return _mm_add_ps(r, r);
}

// SCHEMA_TYPE_1D_ANGULAR_LIMITS (MP7): the rows are turned by the rotation the bodies will have made by the end of
// the step (half-angle quaternions of their summed and current angular velocities, (dAngle, 2) normalized); the angle
// is predicted from the summed velocities (scaled by invIntegrateVelocityFactor * tau) and the velocities (by damping).
// Below the minimum the impulse pushes (when the accumulated impulse stays above zero), above the maximum it pulls.
__forceinline void solve1dAngularLimits(const hkpSolverInfo& info, const hkpJacobian1dAngularLimitsSchema* schema, hkpVelocityAccumulator* mA, hkpVelocityAccumulator* mB, hkpSolverElemTemp* temp)
{
    const float* jac = reinterpret_cast<const float*>(&schema->m_jac);
    const Q factor = _mm_sub_ps(info.m_invIntegrateVelocityFactor.m_quad, g_vectorfConstants[HK_QUADREAL_1]);
    const Q dt = splat(info.m_deltaTime);
    const Q sumA = mA->m_scratch1.m_quad;
    const Q sumB = mB->m_scratch1.m_quad;
    const Q angA = mA->m_angularVel.m_quad;
    const Q angB = mB->m_angularVel.m_quad;
    const Q dA = _mm_mul_ps(_mm_add_ps(_mm_mul_ps(factor, sumA), angA), dt);
    const Q dB = _mm_mul_ps(_mm_add_ps(angB, _mm_mul_ps(factor, sumB)), dt);
    const Q qA4 = setW(dA, g_vectorfConstants[HK_QUADREAL_2]);
    const Q qB4 = setW(dB, g_vectorfConstants[HK_QUADREAL_2]);
    // normalize<4> without the zero test
    const Q lenA = hadd4(_mm_mul_ps(qA4, qA4));
    const Q rA = _mm_rsqrt_ps(lenA);
    const Q qA = _mm_mul_ps(_mm_mul_ps(_mm_sub_ps(g_vectorfConstants[HK_QUADREAL_3], _mm_mul_ps(_mm_mul_ps(rA, lenA), rA)), _mm_mul_ps(rA, _mm_set1_ps(0.5f))), qA4);
    const Q lenB = hadd4(_mm_mul_ps(qB4, qB4));
    const Q rB = _mm_rsqrt_ps(lenB);
    const Q qB = _mm_mul_ps(qB4, _mm_mul_ps(_mm_sub_ps(g_vectorfConstants[HK_QUADREAL_3], _mm_mul_ps(_mm_mul_ps(rB, lenB), rB)), _mm_mul_ps(rB, _mm_set1_ps(0.5f))));
    const Q ang0 = loadQ(jac);
    const Q ang1 = loadQ(jac + 4);
    const Q jacA = rotateByQuaternion(qA, ang0);
    const Q jacB = rotateByQuaternion(qB, ang1);
    const Q tau = splat(schema->m_tau);
    const Q damping = splat(schema->m_damping);
    const Q velocity = _mm_add_ps(_mm_mul_ps(_mm_sub_ps(angA, sumA), ang0), _mm_mul_ps(ang1, _mm_sub_ps(angB, sumB)));
    const Q sum = _mm_add_ps(_mm_mul_ps(sumB, ang1), _mm_mul_ps(ang0, sumA));
    const Q predicted = hadd3(_mm_add_ps(_mm_mul_ps(_mm_mul_ps(xxxx(info.m_invIntegrateVelocityFactor.m_quad), tau), sum), _mm_mul_ps(velocity, damping)));
    const Q accumulated = splat(temp->m_impulseApplied);
    const Q a = _mm_sub_ps(_mm_mul_ps(wwww(ang1), tau), predicted);
    const Q negAccumulated = _mm_sub_ps(_mm_setzero_ps(), accumulated);
    const Q effMass = wwww(ang0);
    Q impulse = _mm_mul_ps(effMass, _mm_sub_ps(a, splat(schema->m_minRhs)));
    if (!(lane0(impulse) > lane0(negAccumulated)))
    {
        impulse = _mm_mul_ps(effMass, _mm_sub_ps(a, splat(schema->m_maxRhs)));
        if (lane0(impulse) >= lane0(negAccumulated))
        {
            return;
        }
    }
    mA->m_angularVel.m_quad = _mm_add_ps(mA->m_angularVel.m_quad, _mm_mul_ps(_mm_mul_ps(mA->m_invMasses.m_quad, impulse), jacA));
    mB->m_angularVel.m_quad = _mm_add_ps(mB->m_angularVel.m_quad, _mm_mul_ps(_mm_mul_ps(mB->m_invMasses.m_quad, impulse), jacB));
    temp->m_impulseApplied = lane0(_mm_add_ps(accumulated, impulse));
}

// SCHEMA_TYPE_SET_CENTER_OF_MASS: the angular velocities (and their sums) moved to the linear ones through the
// modifier's matrices: rotate(matrix, ang) + lin
__forceinline void solveSetCenterOfMass(const hkpJacobianSetCenterOfMassSchema* schema, hkpVelocityAccumulator* mA, hkpVelocityAccumulator* mB)
{
    const float* a = reinterpret_cast<const float*>(&schema->m_angToLinVelA);
    const float* b = reinterpret_cast<const float*>(&schema->m_angToLinVelB);
    const Q linA = rotate(loadQ(a), loadQ(a + 4), loadQ(a + 8), mA->m_angularVel.m_quad);
    const Q linB = rotate(loadQ(b), loadQ(b + 4), loadQ(b + 8), mB->m_angularVel.m_quad);
    const Q sumA = rotate(loadQ(a), loadQ(a + 4), loadQ(a + 8), mA->m_scratch1.m_quad);
    const Q sumB = rotate(loadQ(b), loadQ(b + 4), loadQ(b + 8), mB->m_scratch1.m_quad);
    mA->m_linearVel.m_quad = _mm_add_ps(linA, mA->m_linearVel.m_quad);
    mA->m_scratch0.m_quad = _mm_add_ps(sumA, mA->m_scratch0.m_quad);
    mB->m_linearVel.m_quad = _mm_add_ps(mB->m_linearVel.m_quad, linB);
    mB->m_scratch0.m_quad = _mm_add_ps(sumB, mB->m_scratch0.m_quad);
}

// one pass over the schemas (hkSolveConstraints' micro step and hkSolveStepJacobians, the same inlined loop in both).
// Until the first header the bodies are the two zeroed accumulators (used when the schemas start with a goto); any
// other first schema is taken as a header. Returns false on a schema type that is not ported.
__forceinline bool solveSchemaPass(const hkpSolverInfo& info, const hkSolverStepTemp& stepTemp, const hkpJacobianSchema* schemas, hkpVelocityAccumulator* accumulators, hkpSolverElemTemp* temp, hkpVelocityAccumulator* zeroAccumulators)
{
    const char* schema = reinterpret_cast<const char*>(schemas);
    hkpSolverElemTemp* temps = temp;
    hkpVelocityAccumulator* mA = nullptr;
    hkpVelocityAccumulator* mB = nullptr;
    unsigned char type = *reinterpret_cast<const unsigned char*>(schema);
    if (type == hkpJacobianSchema::SCHEMA_TYPE_GOTO)
    {
        memset(zeroAccumulators, 0, 2 * sizeof(hkpVelocityAccumulator));
        mA = &zeroAccumulators[1];
        mB = &zeroAccumulators[0];
    }
    else if (type != hkpJacobianSchema::SCHEMA_TYPE_END)
    {
        type = hkpJacobianSchema::SCHEMA_TYPE_HEADER; // the first schema is taken as a header
    }
    for (;;)
    {
        if (type == hkpJacobianSchema::SCHEMA_TYPE_END)
        {
            return true;
        }
        switch (type)
        {
        case hkpJacobianSchema::SCHEMA_TYPE_HEADER:
        {
            const hkpJacobianHeaderSchema* header = reinterpret_cast<const hkpJacobianHeaderSchema*>(schema);
            mA = accumulators + header->m_bodyAIndex;
            mB = accumulators + header->m_bodyBIndex;
            schema += sizeof(hkpJacobianHeaderSchema);
            break;
        }
        case hkpJacobianSchema::SCHEMA_TYPE_GOTO:
            schema += reinterpret_cast<const hkpJacobianGotoSchema*>(schema)->m_offset;
            break;
        case hkpJacobianSchema::SCHEMA_TYPE_SHIFT_SOLVER_RESULTS_POINTER:
            schema += sizeof(hkpJacobianShiftSolverResultsPointerSchema);
            break;
        case hkpJacobianSchema::SCHEMA_TYPE_SINGLE_CONTACT:
        case hkpJacobianSchema::SCHEMA_TYPE_SINGLE_CONTACT_WITH_ACCESSOR_CHECK:
            solveSingleContact(reinterpret_cast<const float*>(schema), mA, mB, temps);
            temps += hkpJacobianSchema::getNumSolverElemTemps(reinterpret_cast<const hkpJacobianSchema*>(schema));
            schema += hkpJacobianSchemaInfo::SingleContact::Sizeof;
            break;
        case hkpJacobianSchema::SCHEMA_TYPE_SINGLE_LIMIT_CONTACT:
        case hkpJacobianSchema::SCHEMA_TYPE_SINGLE_LIMIT_CONTACT_WITH_ACCESSOR_CHECK:
            solveSingleLimitContact(reinterpret_cast<const float*>(schema), mA, mB, temps);
            temps += hkpJacobianSchema::getNumSolverElemTemps(reinterpret_cast<const hkpJacobianSchema*>(schema));
            schema += hkpJacobianSchemaInfo::SingleContact::Sizeof;
            break;
        case hkpJacobianSchema::SCHEMA_TYPE_PAIR_CONTACT:
        case hkpJacobianSchema::SCHEMA_TYPE_PAIR_CONTACT_WITH_ACCESSOR_CHECK:
            solvePairContact(reinterpret_cast<const hkpJacobianPairContactSchema*>(schema), mA, mB, temps);
            temps += 2;
            schema += hkpJacobianSchemaInfo::PairContact::Sizeof;
            break;
        case hkpJacobianSchema::SCHEMA_TYPE_2D_FRICTION:
            solveFriction2d(reinterpret_cast<const hkpJacobian2dFrictionSchema*>(schema), mA, mB, temps);
            temps += 3;
            schema += hkpJacobianSchemaInfo::Friction2D::Sizeof;
            break;
        case hkpJacobianSchema::SCHEMA_TYPE_3D_FRICTION:
            solveFriction3d(reinterpret_cast<const hkpJacobian3dFrictionSchema*>(schema), mA, mB, temps);
            temps += 4;
            schema += hkpJacobianSchemaInfo::Friction3D::Sizeof;
            break;
        case hkpJacobianSchema::SCHEMA_TYPE_2D_ROLLING_FRICTION:
            hkpAtomSolverFunctions::solve2dRollingFriction(info, stepTemp, reinterpret_cast<const hkpJacobian2dRollingFrictionSchema*>(schema), mA, mB, temps);
            temps += hkpJacobianSchema::getNumSolverElemTemps(reinterpret_cast<const hkpJacobianSchema*>(schema));
            schema += hkpJacobianSchemaInfo::RollingFriction2D::Sizeof;
            break;
        case hkpJacobianSchema::SCHEMA_TYPE_1D_BILATERAL:
            solve1dBilateral(reinterpret_cast<const float*>(schema), mA, mB, temps);
            temps += hkpJacobianSchemaInfo::Bilateral1D::Temps;
            schema += hkpJacobianSchemaInfo::Bilateral1D::Sizeof;
            break;
        case hkpJacobianSchema::SCHEMA_TYPE_1D_BILATERAL_WITH_IMPULSE_LIMIT:
            solve1dBilateralWithImpulseLimit(reinterpret_cast<const float*>(schema), mA, mB, temps);
            temps += hkpJacobianSchemaInfo::Bilateral1D::Temps;
            schema += hkpJacobianSchemaInfo::Bilateral1D::Sizeof;
            break;
        case hkpJacobianSchema::SCHEMA_TYPE_1D_LINEAR_LIMITS:
            solve1dLinLimits(reinterpret_cast<const hkpJacobian1dLinearLimitsSchema*>(schema), mA, mB, temps);
            temps += hkpJacobianSchemaInfo::LinearLimits1D::Temps;
            schema += hkpJacobianSchemaInfo::LinearLimits1D::Sizeof;
            break;
        case hkpJacobianSchema::SCHEMA_TYPE_1D_FRICTION:
            solve1dFriction(reinterpret_cast<const hkpJacobian1dFrictionSchema*>(schema), mA, mB, temps);
            temps += hkpJacobianSchemaInfo::Friction1D::Temps;
            schema += hkpJacobianSchemaInfo::Friction1D::Sizeof;
            break;
        case hkpJacobianSchema::SCHEMA_TYPE_1D_ANGULAR_MOTOR:
            solve1dAngularMotor(info, stepTemp, reinterpret_cast<const hkpJacobian1dAngularMotorSchema*>(schema), mA, mB, temps);
            temps += hkpJacobianSchemaInfo::AngularMotor1D::Temps;
            schema += hkpJacobianSchemaInfo::AngularMotor1D::Sizeof;
            break;
        case hkpJacobianSchema::SCHEMA_TYPE_1D_ANGULAR:
            solve1dAngular(reinterpret_cast<const float*>(schema), mA, mB, temps);
            temps += hkpJacobianSchemaInfo::Angular1D::Temps;
            schema += hkpJacobianSchemaInfo::Angular1D::Sizeof;
            break;
        case hkpJacobianSchema::SCHEMA_TYPE_1D_ANGULAR_LIMITS:
            solve1dAngularLimits(info, reinterpret_cast<const hkpJacobian1dAngularLimitsSchema*>(schema), mA, mB, temps);
            temps += hkpJacobianSchemaInfo::AngularLimits1D::Temps;
            schema += hkpJacobianSchemaInfo::AngularLimits1D::Sizeof;
            break;
        case hkpJacobianSchema::SCHEMA_TYPE_1D_ANGULAR_FRICTION:
            solve1dAngularFriction(reinterpret_cast<const hkpJacobian1dAngularFrictionSchema*>(schema), mA, mB, temps);
            temps += hkpJacobianSchemaInfo::AngularFriction1D::Temps;
            schema += hkpJacobianSchemaInfo::AngularFriction1D::Sizeof;
            break;
        case hkpJacobianSchema::SCHEMA_TYPE_SET_MASS:
        {
            // the inverse masses of the modifier (A's x loses its low byte, the schema type)
            const hkpJacobianSetMassSchema* setMass = reinterpret_cast<const hkpJacobianSetMassSchema*>(schema);
            mA->m_invMasses = setMass->m_invMass0;
            mB->m_invMasses = setMass->m_invMass1;
            *reinterpret_cast<unsigned char*>(&mA->m_invMasses) = 0;
            schema += hkpJacobianSchemaInfo::SetMass::Sizeof;
            break;
        }
        case hkpJacobianSchema::SCHEMA_TYPE_ADD_VELOCITY:
        {
            const hkpJacobianAddVelocitySchema* addVelocity = reinterpret_cast<const hkpJacobianAddVelocitySchema*>(schema);
            hkpVelocityAccumulator* acc = addVelocity->m_bodyIndex != 0 ? mB : mA;
            const Q sumPart = _mm_mul_ps(_mm_mul_ps(xxxx(info.m_integrateVelocityFactor.m_quad), splat((float)stepTemp.m_currentStep.val())), addVelocity->m_deltaVel.m_quad);
            const Q lin = _mm_add_ps(addVelocity->m_deltaVel.m_quad, acc->m_linearVel.m_quad);
            acc->m_linearVel.m_quad = _mm_add_ps(sumPart, lin);
            acc->m_scratch0.m_quad = _mm_add_ps(sumPart, acc->m_scratch0.m_quad);
            schema += hkpJacobianSchemaInfo::AddVelocity::Sizeof;
            break;
        }
        case hkpJacobianSchema::SCHEMA_TYPE_SET_CENTER_OF_MASS:
            solveSetCenterOfMass(reinterpret_cast<const hkpJacobianSetCenterOfMassSchema*>(schema), mA, mB);
            schema += hkpJacobianSchemaInfo::SetCenterOfMass::Sizeof;
            break;
        default:
            hkErrorFwd::messageError(0x4F70454F, "OpenRFG: hkpsolver: schema type not ported", "hkpsolver.cpp", __LINE__);
            return false;
        }
        type = *reinterpret_cast<const unsigned char*>(schema);
    }
}
}

// 0x00EA43E0: returns false only when the license check fails (never here)
bool hkSolveConstraints(const hkpSolverInfo& info, hkpJacobianSchema* schemas, hkpVelocityAccumulator* accumulators, hkpSolverElemTemp* temp)
{
    hkVector4f velocityField = info.m_globalAccelerationPerSubStep;
    for (hkpVelocityAccumulator* acc = accumulators; acc->m_type.m_storage != hkpVelocityAccumulator::HK_END; acc++)
    {
        if (acc->m_type.m_storage == hkpVelocityAccumulator::HK_RIGID_BODY)
        {
            const Q gravityFactor = splat(acc->m_gravityFactor);
            convertToSolverType(acc);
            acc->m_linearVel.m_quad = _mm_add_ps(acc->m_linearVel.m_quad, _mm_mul_ps(velocityField.m_quad, gravityFactor));
        }
        else
        {
            convertToSolverType(acc);
        }
    }

    hkSimdFloat32 sumVelFactor;
    sumVelFactor.m_real = _mm_mul_ps(xxxx(info.m_invIntegrateVelocityFactor.m_quad), splat(info.m_invNumSteps));
    // the solver's own zeroed accumulators, used until the first header (when the schemas start with a goto)
    HK_ALIGN16(hkpVelocityAccumulator zeroAccumulators[2]);
    for (int step = 0; step < info.m_numSteps; step++)
    {
        hkSolverStepTemp stepTemp;
        stepTemp.m_currentStep = step;
        stepTemp.m_intregratePositionFactor.m_real = splat((float)(step + 1));
        for (int microStep = info.m_numMicroSteps; ; )
        {
            if (!solveSchemaPass(info, stepTemp, schemas, accumulators, temp, zeroAccumulators))
            {
                return true;
            }
            if (--microStep <= 0)
            {
                break;
            }
        }
        if (step == info.m_numSteps - 1)
        {
            hkSolver::integrateVelocities<1>(info, velocityField, sumVelFactor, accumulators, nullptr);
        }
        else
        {
            hkSolver::integrateVelocities<0>(info, velocityField, sumVelFactor, accumulators, nullptr);
        }
    }
    return true;
}

// 0x00EA2460: one pass of the solver over the schemas, as a micro step of the first solver step (step 0, position
// factor 0), without integrating; the TOI solver (hkpConstraintSolverSetup::subSolve) runs it on the schemas added
// since its last pass
void hkSolveStepJacobians(const hkpSolverInfo& info, const hkpJacobianSchema* schemas, hkpVelocityAccumulator* accumulators, hkpSolverElemTemp* tmp)
{
    hkSolverStepTemp stepTemp;
    stepTemp.m_intregratePositionFactor.m_real = _mm_setzero_ps();
    stepTemp.m_currentStep = 0;
    HK_ALIGN16(hkpVelocityAccumulator zeroAccumulators[2]);
    solveSchemaPass(info, stepTemp, schemas, accumulators, tmp, zeroAccumulators);
}

