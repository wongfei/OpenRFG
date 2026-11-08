// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/response/hkpsimplecollisionresponse.cpp
//
// Havok 2014.2.5 (havok_impl.md HK8): one contact solved alone, from rfg.exe. solveSingleContact is the TOI response
// (motions at the time of impact); solveSingleContact2 is the bounce of hkSimpleContactConstraintData_fireCallbacks
// (velocity accumulators in the build-Jacobians context).

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/response/hkpsimplecollisionresponse.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/physics/contactpoint/hkcontactpoint.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraintsolver/accumulator/hkpvelocityaccumulator.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/collide/hkpdynamicscontactmgr.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/contact/hkpsimplecontactconstraintdata.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/hkprigidbody.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/motion/hkpmotion.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/solver/hkpsolversse.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/solver/simpleconstraints/hkpsimpleconstraintutil.h"

using namespace hkSolverSse;

namespace
{
    // the contact frame: the separating normal (w the distance), a perpendicular and their cross product
    // (hkVector4Util::calculatePerpendicularVector, normalized, inlined)
    void calcDirections(const hkVector4f& normal, hkMatrix3f& dirs)
    {
        const Q n = normal.m_quad;
        const Q nz = _mm_and_ps(n, _mm_castsi128_ps(_mm_set_epi32(0, -1, -1, -1)));
        const Q zx = _mm_shuffle_ps(nz, nz, 0xCE); // (z, 0, x, 0)
        const Q xzLen2 = hadd4(_mm_mul_ps(zx, zx));
        const Q n2 = _mm_mul_ps(n, n);
        const Q useZx = _mm_cmplt_ps(_mm_add_ps(yyyy(n2), xxxx(n2)), xzLen2);
        const Q signX = _mm_castsi128_ps(_mm_set_epi32(0, 0, 0, (int)0x80000000));
        const Q yx = _mm_xor_ps(_mm_shuffle_ps(nz, nz, 0xF1), signX); // (-y, x, 0, 0)
        const Q perp = _mm_or_ps(_mm_and_ps(useZx, _mm_xor_ps(signX, zx)), _mm_andnot_ps(useZx, yx)); // (-z, 0, x, 0)
        const Q t1 = _mm_mul_ps(invSqrt23(hadd3(_mm_mul_ps(perp, perp))), perp);
        dirs.m_col0.m_quad = n;
        dirs.m_col1.m_quad = t1;
        dirs.m_col2.m_quad = cross(n, t1);
    }

    // the swept transform's center of mass at time t: (t - t0) * invDeltaTime * (com1 - com0) + com0
    __forceinline Q centerOfMassAt(const hkSweptTransformf& st, const Q& t)
    {
        const Q com0 = st.m_centerOfMass0.m_quad;
        const Q com1 = st.m_centerOfMass1.m_quad;
        return _mm_add_ps(_mm_mul_ps(_mm_mul_ps(_mm_sub_ps(t, wwww(com0)), wwww(com1)), _mm_sub_ps(com1, com0)), com0);
    }

    // the velocity the swept transform moves with (the keyframed velocity over the step)
    __forceinline void sweptVelocity(const hkMotionState& ms, hkpBodyVelocity& vel)
    {
        const hkSweptTransformf& st = ms.m_sweptTransform;
        const Q invDt = wwww(st.m_centerOfMass1.m_quad);
        vel.m_angular.m_quad = _mm_mul_ps(ms.m_deltaAngle.m_quad, invDt);
        vel.m_linear.m_quad = _mm_mul_ps(_mm_sub_ps(st.m_centerOfMass1.m_quad, st.m_centerOfMass0.m_quad), invDt);
    }

    // a build-context accumulator's body: the world inverse inertia M^T * diag(invMasses) * M (M the core-from-world
    // matrix in scratch0..2) and the world angular velocity M^T * angular
    void initFromAccumulator(const hkContactPoint& cp, const hkpVelocityAccumulator* acc, hkpRigidBody* rb, hkpSimpleConstraintInfoInitInput& in, hkpBodyVelocity& vel)
    {
        in.m_massRelPos.m_quad = _mm_sub_ps(cp.m_position.m_quad, acc->m_scratch3.m_quad);
        in.m_invMasses = acc->m_invMasses;
        vel.m_linear = acc->m_linearVel;
        const hkMatrix3f& coreFromWorld = reinterpret_cast<const hkMatrix3f&>(acc->m_scratch0);
        hkVector4f r0 = acc->m_scratch0;
        hkVector4f r1 = acc->m_scratch1;
        hkVector4f r2 = acc->m_scratch2;
        HK_TRANSPOSE3f(r0, r1, r2);
        vel.m_angular.m_quad = rotate(r0.m_quad, r1.m_quad, r2.m_quad, acc->m_angularVel.m_quad);
        hkMatrix3f worldFromCore = coreFromWorld;
        worldFromCore.transpose();
        const Q invMasses = acc->m_invMasses.m_quad;
        hkMatrix3f scaled;
        scaled.m_col0.m_quad = _mm_mul_ps(xxxx(invMasses), worldFromCore.m_col0.m_quad);
        scaled.m_col1.m_quad = _mm_mul_ps(yyyy(invMasses), worldFromCore.m_col1.m_quad);
        scaled.m_col2.m_quad = _mm_mul_ps(zzzz(invMasses), worldFromCore.m_col2.m_quad);
        in.m_invInertia.setMul(scaled, coreFromWorld);
        in.m_transform = &rb->m_motion.m_motionState.m_transform;
    }

    // the solved velocity back to the accumulator and, unless fixed, the body
    __forceinline void storeVelocity(const hkpBodyVelocity& vel, hkpRigidBody* rb, hkpVelocityAccumulator* acc)
    {
        acc->m_linearVel = vel.m_linear;
        if (rb->m_motion.m_type.m_storage != hkpMotion::MOTION_FIXED)
        {
            rb->m_motion.m_linearVelocity = vel.m_linear;
            rb->m_motion.m_angularVelocity = vel.m_angular;
        }
    }
}

// 0x00E2EAD0
void hkpSimpleCollisionResponse::solveSingleContact(const hkContactPoint& cp, float time, hkpSimpleConstraintUtilCollideParams& params, hkpMotion* bodyA, hkpMotion* bodyB, hkpDynamicsContactMgr* contactMgr, SolveSingleOutput& output)
{
    const Q t = splat(time);
    hkpSimpleConstraintInfoInitInput inA;
    hkpSimpleConstraintInfoInitInput inB;
    inA.m_invMasses = bodyA->m_inertiaAndMassInv;
    inA.m_massRelPos.m_quad = _mm_sub_ps(cp.m_position.m_quad, centerOfMassAt(bodyA->m_motionState.m_sweptTransform, t));
    bodyA->getInertiaInvWorld(inA.m_invInertia);
    inB.m_invMasses = bodyB->m_inertiaAndMassInv;
    inB.m_massRelPos.m_quad = _mm_sub_ps(cp.m_position.m_quad, centerOfMassAt(bodyB->m_motionState.m_sweptTransform, t));
    inA.m_transform = &bodyA->m_motionState.m_transform;
    bodyB->getInertiaInvWorld(inB.m_invInertia);
    inB.m_transform = &bodyB->m_motionState.m_transform;

    hkpBodyVelocity velA;
    hkpBodyVelocity velB;
    hkpBodyVelocity sweptA;
    hkpBodyVelocity sweptB;
    velA.m_linear = bodyA->m_linearVelocity;
    velA.m_angular = bodyA->m_angularVelocity;
    velB.m_linear = bodyB->m_linearVelocity;
    velB.m_angular = bodyB->m_angularVelocity;
    sweptVelocity(bodyA->m_motionState, sweptA);
    sweptVelocity(bodyB->m_motionState, sweptB);

    hkMatrix3f dirs;
    calcDirections(cp.m_separatingNormal, dirs);
    contactMgr->toiCollisionResponseBeginCallback(cp, inA, velA, inB, velB);
    hkpSimpleConstraintInfo info;
    hkSimpleConstraintUtil_InitInfo(inA, inB, dirs, info);
    const float impulse = hkSimpleConstraintUtil_Collide(info, params, velA, velB);
    output.m_impulse = impulse;
    contactMgr->toiCollisionResponseEndCallback(cp, impulse, inA, velA, inB, velB);

    // the normal velocity of each body's swept motion against the other's new velocity
    hkVector4f pointVelA;
    hkVector4f pointVelB;
    hkSimpleConstraintUtil_getPointVelocity(info, sweptA, velB, pointVelA);
    hkSimpleConstraintUtil_getPointVelocity(info, velA, sweptB, pointVelB);
    output.m_velocityKeyframedA = pointVelA.m_quad.m128_f32[0];
    output.m_velocityKeyframedB = pointVelB.m_quad.m128_f32[0];

    if (bodyA->m_type.m_storage != hkpMotion::MOTION_FIXED)
    {
        bodyA->setLinearVelocity(velA.m_linear);
        bodyA->setAngularVelocity(velA.m_angular);
    }
    if (bodyB->m_type.m_storage != hkpMotion::MOTION_FIXED)
    {
        bodyB->setLinearVelocity(velB.m_linear);
        bodyB->setAngularVelocity(velB.m_angular);
    }
}

// 0x00E2EE80
void hkpSimpleCollisionResponse::solveSingleContact2(hkpSimpleContactConstraintData* constraintData, const hkContactPoint& cp, hkpSimpleConstraintUtilCollideParams& params, hkpRigidBody* rigidBodyA, hkpRigidBody* rigidBodyB, hkpVelocityAccumulator* bodyA, hkpVelocityAccumulator* bodyB, SolveSingleOutput2& output)
{
    hkpSimpleConstraintInfoInitInput inA;
    hkpSimpleConstraintInfoInitInput inB;
    hkpBodyVelocity velA;
    hkpBodyVelocity velB;
    initFromAccumulator(cp, bodyA, rigidBodyA, inA, velA);
    initFromAccumulator(cp, bodyB, rigidBodyB, inB, velB);

    hkMatrix3f dirs;
    calcDirections(cp.m_separatingNormal, dirs);
    constraintData->collisionResponseBeginCallback(cp, inA, velA, inB, velB);
    hkpSimpleConstraintInfo info;
    hkSimpleConstraintUtil_InitInfo(inA, inB, dirs, info);
    const float impulse = hkSimpleConstraintUtil_Collide(info, params, velA, velB);
    output.m_impulse = impulse;
    constraintData->collisionResponseEndCallback(cp, impulse, inA, velA, inB, velB);

    storeVelocity(velA, rigidBodyA, bodyA);
    storeVelocity(velB, rigidBodyB, bodyB);

    // the angular velocities back into core space
    bodyA->m_angularVel.m_quad = rotate(bodyA->m_scratch0.m_quad, bodyA->m_scratch1.m_quad, bodyA->m_scratch2.m_quad, velA.m_angular.m_quad);
    bodyB->m_angularVel.m_quad = rotate(bodyB->m_scratch0.m_quad, bodyB->m_scratch1.m_quad, bodyB->m_scratch2.m_quad, velB.m_angular.m_quad);
}
