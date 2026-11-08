// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/motion/util/hkprigidmotionutil.cpp
//
// Havok 2014.2.5 (havok_impl.md HK7): rigid motion integration, from rfg.exe. Gravity and damping, the motion
// state step (hkSweptTransformUtil::_stepMotionState, inlined in every caller), the thin box's angular momentum
// conservation, velocity clipping and the deactivation counters (hkRigidMotionUtilCheckDeactivation, inlined too).
//
// The motions are reached as (char*)motions[i] + motionOffset (the callers pass entities and the offset of their
// motion). The arithmetic follows the binary operation by operation (hkpgskexact.h helpers): dot products are
// (y + x) + z, 4-component dots the SSE2 pair sums, inverse square roots one Newton step from rsqrtps.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/motion/util/hkprigidmotionutil.h"

#include <string.h>

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/hkmath.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/swepttransform/hkswepttransformutil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkvector4util.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/physics/hkstepinfo.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraintsolver/accumulator/hkpvelocityaccumulator.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraintsolver/solve/hkpsolverinfo.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/motion/hkpmotion.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/gjk/hkpgskexact.h"

using namespace hkGskExact;

namespace
{
// hkHalf to float: the 16 bits are the high half of the float
inline float hkRigidMotionUtil_half(const hkHalf& h)
{
    unsigned short s;
    memcpy(&s, &h, sizeof(s));
    const unsigned int u = (unsigned int)s << 16;
    float f;
    memcpy(&f, &u, sizeof(f));
    return f;
}

inline Q hkRigidMotionUtil_bitsOf(unsigned int u)
{
    return bits(u);
}

// hkVector4f::dot<4> (the SSE2 form): (xy + zw) pairs, then the pairs summed, in all lanes
inline Q hkRigidMotionUtil_dot4(Q a, Q b)
{
    const Q m = mul(a, b);
    const Q s0 = add(_mm_shuffle_ps(m, m, _MM_SHUFFLE(1, 0, 3, 2)), m);
    return add(_mm_shuffle_ps(s0, s0, _MM_SHUFFLE(2, 3, 0, 1)), s0);
}

inline hkpMotion* hkRigidMotionUtil_motion(hkpMotion* const* motions, int i, int motionOffset)
{
    return (hkpMotion*)((char*)motions[i] + motionOffset);
}

// max(0, 1 - damping * dt) * v
inline Q hkRigidMotionUtil_damp(Q v, const hkHalf& damping, Q dt)
{
    const Q f = sub(hkVector4f::getConstant<HK_QUADREAL_1>().m_quad, mul(splat(hkRigidMotionUtil_half(damping)), dt));
    return mul(_mm_max_ps(hkVector4f::getConstant<HK_QUADREAL_0>().m_quad, f), v);
}

// hkSweptTransformUtil::_stepMotionState (hkswepttransformfutil.inl), as inlined in rfg.exe. The NaN branches of the
// source compile to nothing under /fp:fast; the angular velocity clip writes angularVelocity back.
inline void hkRigidMotionUtil_stepMotionState(const hkStepInfo& info, Q& linearVelocity, Q& angularVelocity, hkMotionState& motionState)
{
    hkSweptTransformf& st = motionState.getSweptTransform();
    const Q deltaTime = splat(info.m_deltaTime.val() * hkRigidMotionUtil_half(motionState.m_timeFactor));

    // velocities that are NaN or beyond _stepMotionStateMaxVelf become (1, 0, 0, 0)
    {
        const Q maxVel = hkSweptTransformUtil::_stepMotionStateMaxVelf;
        Q isNormal = _mm_and_ps(_mm_cmplt_ps(abs(angularVelocity), maxVel), _mm_cmplt_ps(abs(linearVelocity), maxVel));
        isNormal = _mm_and_ps(isNormal, _mm_cmpeq_ps(linearVelocity, linearVelocity));
        isNormal = _mm_and_ps(isNormal, _mm_cmpeq_ps(angularVelocity, angularVelocity));
        if ((_mm_movemask_ps(isNormal) & 7) != 7)
        {
            linearVelocity = hkVector4f::getConstant<HK_QUADREAL_1000>().m_quad;
            angularVelocity = hkVector4f::getConstant<HK_QUADREAL_1000>().m_quad;
        }
    }

    st.m_centerOfMass0.m_quad = setXYZ_W(st.m_centerOfMass1.m_quad, splat(info.m_startTime.val()));

    {
        const Q maxLinear = splat((float)motionState.m_maxLinearVelocity);
        const Q linVelSq = hadd3(mul(linearVelocity, linearVelocity));
        if (greater(linVelSq, mul(maxLinear, maxLinear)))
        {
            linearVelocity = mul(mul(invSqrt23(linVelSq), maxLinear), linearVelocity);
        }
    }

    st.m_centerOfMass1.m_quad = add(mul(linearVelocity, deltaTime), st.m_centerOfMass1.m_quad);
    ((float*)&st.m_centerOfMass1)[3] = info.m_invDeltaTime.val();

    const Q rotation1 = st.m_rotation1.m_vec.m_quad;
    st.m_rotation0.m_vec.m_quad = rotation1;

    // the rotation over the step, its w from the series of cos(angle / 2) in numHalfRotations^2
    Q rotationRadians = mul(mul(deltaTime, hkVector4f::getConstant<HK_QUADREAL_INV_2>().m_quad), angularVelocity);
    Q numHalfRotations2 = mul(hadd3(mul(rotationRadians, rotationRadians)), hkRigidMotionUtil_bitsOf(0x3ECF817A)); // 4 / pi^2
    const Q a = hkRigidMotionUtil_bitsOf(0x3F52ACB8); // 0.822948
    const Q b = hkRigidMotionUtil_bitsOf(0x3E05A965); // 0.130529
    const Q c = hkRigidMotionUtil_bitsOf(0x3D35E52A); // 0.044408
    const Q maxAngular = splat((float)motionState.m_maxAngularVelocity);
    // min(0.9, dt * maxAngularVelocity / pi): clips the angular velocity to [0, pi * 0.9 / dt]
    const Q maxAngleHalfRotations = _mm_min_ps(splat(0.9f), mul(recip23(hkVector4f::getConstant<HK_QUADREAL_PI>().m_quad), mul(maxAngular, deltaTime)));
    const Q maxAngleHalfRotations2 = mul(maxAngleHalfRotations, maxAngleHalfRotations);
    if (greater(numHalfRotations2, maxAngleHalfRotations2))
    {
        const Q factor = mul(invSqrt23SetZero(numHalfRotations2), maxAngleHalfRotations);
        angularVelocity = mul(factor, angularVelocity);
        rotationRadians = mul(factor, rotationRadians);
        numHalfRotations2 = maxAngleHalfRotations2;
    }
    {
        const Q numHalfRotations4 = mul(numHalfRotations2, numHalfRotations2);
        Q w = sub(hkVector4f::getConstant<HK_QUADREAL_1>().m_quad, mul(a, numHalfRotations2));
        w = sub(w, mul(numHalfRotations4, b));
        w = sub(w, mul(mul(c, numHalfRotations2), numHalfRotations4));
        rotationRadians = setXYZ_W(rotationRadians, w);
    }

    // newRotation = rotationRadians * rotation1, normalized
    Q newRotation;
    {
        const Q rw = bcast<3>(rotationRadians);
        const Q qw = bcast<3>(rotation1);
        Q xyz = cross(rotationRadians, rotation1);
        xyz = add(xyz, mul(rw, rotation1));
        xyz = add(xyz, mul(qw, rotationRadians));
        const Q w = sub(mul(qw, rw), dot3(rotationRadians, rotation1));
        const Q q = setXYZ_W(xyz, w);
        newRotation = mul(invSqrt23(hkRigidMotionUtil_dot4(q, q)), q);
    }

    // deltaAngle: twice the rotation's vector part, w the angle (sqrt(numHalfRotations^2) * pi)
    {
        const Q angle = mul(_mm_andnot_ps(_mm_cmple_ps(numHalfRotations2, _mm_setzero_ps()), mul(invSqrt23(numHalfRotations2), numHalfRotations2)), hkVector4f::getConstant<HK_QUADREAL_PI>().m_quad);
        motionState.m_deltaAngle.m_quad = setXYZ_W(add(rotationRadians, rotationRadians), angle);
    }

    st.m_rotation1.m_vec.m_quad = newRotation;
    // calcTransAtT1
    hkTransformf& transform = motionState.getTransform();
    transform.getRotation().set(st.m_rotation1);
    transform.getTranslation().m_quad = sub(st.m_centerOfMass1.m_quad, rotate(transform.getRotation(), st.m_centerOfMassLocal.m_quad));
}

// hkSweptTransformUtil::_clipVelocities (hkswepttransformfutil.inl): the NaN resets compile to nothing (/fp:fast)
inline void hkRigidMotionUtil_clipVelocities(const hkMotionState& motionState, hkVector4f& linearVelocity, hkVector4f& angularVelocity)
{
    const Q linVelSq = hadd3(mul(linearVelocity.m_quad, linearVelocity.m_quad));
    const Q angVelSq = hadd3(mul(angularVelocity.m_quad, angularVelocity.m_quad));
    const Q maxLinear = splat((float)motionState.m_maxLinearVelocity);
    const Q maxAngular = splat((float)motionState.m_maxAngularVelocity);
    if (greater(linVelSq, mul(maxLinear, maxLinear)))
    {
        linearVelocity.m_quad = mul(mul(invSqrt23SetZero(linVelSq), maxLinear), linearVelocity.m_quad);
    }
    if (greater(angVelSq, mul(maxAngular, maxAngular)))
    {
        angularVelocity.m_quad = mul(mul(invSqrt23SetZero(angVelSq), maxAngular), angularVelocity.m_quad);
    }
}

// the interpolated center of mass at time t: com0 + ((t - com0.w) * com1.w) * (com1 - com0)
inline Q hkRigidMotionUtil_centerOfMassAt(const hkSweptTransformf& st, float t)
{
    const Q com0 = st.m_centerOfMass0.m_quad;
    const Q com1 = st.m_centerOfMass1.m_quad;
    return add(mul(mul(sub(splat(t), bcast<3>(com0)), bcast<3>(com1)), sub(com1, com0)), com0);
}
}

// 0x00E125E0 (inlined in the callers): every 4th integration (counter 0) or 16th (counter 1) the motion's movement
// since the reference position and orientation is checked; moving resets the inactive frame counter, resting
// increments it (up to 64). Returns the larger of the two counters.
int hkRigidMotionUtilCheckDeactivation(const hkpSolverInfo& si, hkpMotion& motion)
{
    const int counter = motion.m_deactivationIntegrateCounter + 1;
    motion.m_deactivationIntegrateCounter = (unsigned char)counter;
    if ((counter & 3) == 0)
    {
        int select = -1;
        if (counter & 0xF)
        {
            select = 0;
        }
        else if (counter == 0x100)
        {
            motion.m_deactivationIntegrateCounter = 0xFF; // 0xFF is "deactivation disabled"
        }
        else
        {
            motion.m_deactivationIntegrateCounter = 0;
            select = 1;
        }

        if (select >= 0)
        {
            const hkMotionState& ms = motion.m_motionState;
            const Q radius = splat(lane0(_mm_min_ss(splat(1.0f), splat(ms.m_objectRadius))));
            const Q angular = mul(hadd3(mul(motion.m_angularVelocity.m_quad, motion.m_angularVelocity.m_quad)), mul(radius, radius));
            const Q velocity = add(angular, hadd3(mul(motion.m_linearVelocity.m_quad, motion.m_linearVelocity.m_quad)));
            hkVector4f& refPosition = motion.m_deactivationRefPosition[select];
            // w keeps the largest velocity measure seen
            refPosition.m_quad = setXYZ_W(refPosition.m_quad, _mm_max_ps(bcast<3>(refPosition.m_quad), velocity));

            const hkpSolverInfo::DeactivationInfo& di = si.m_deactivationInfo[ms.m_deactivationClass];
            const Q centerOfMass = ms.getSweptTransform().m_centerOfMass1.m_quad;
            const Q d = sub(refPosition.m_quad, centerOfMass);
            bool moved = greater(hadd3(mul(d, d)), splat(di.m_maxDistSqrd[select]));
            if (!moved)
            {
                // the reference orientation unpacked: (byte - 128) * (1.1 / 128) (0x3C0CCCCD)
                const __m128i packed = _mm_cvtsi32_si128((int)motion.m_deactivationRefOrientation[select]);
                const __m128i zero = _mm_setzero_si128();
                const __m128i ints = _mm_sub_epi32(_mm_unpacklo_epi16(_mm_unpacklo_epi8(packed, zero), zero), _mm_set1_epi32(0x80));
                const Q orientation = mul(_mm_cvtepi32_ps(ints), hkRigidMotionUtil_bitsOf(0x3C0CCCCD));
                const Q dq = sub(orientation, ms.getSweptTransform().m_rotation1.m_vec.m_quad);
                moved = greater(hkRigidMotionUtil_dot4(dq, dq), splat(di.m_maxRotSqrd[select]));
            }

            const unsigned short frames = motion.m_deactivationNumInactiveFrames[select];
            const unsigned short selectFlag = si.m_deactivationNumInactiveFramesSelectFlag[select];
            // bits 7..13 keep the previous counter, bit 14 the world's select flag
            const unsigned short high = (unsigned short)(((selectFlag << 7) | (frames & 0x7F)) << 7);
            if (moved)
            {
                motion.m_deactivationNumInactiveFrames[select] = high;
                refPosition.m_quad = _mm_castsi128_ps(_mm_srli_si128(_mm_slli_si128(_mm_castps_si128(centerOfMass), 4), 4));
                motion.m_deactivationRefOrientation[select] = hkVector4UtilImpl<float>::packQuaternionIntoInt32(ms.getSweptTransform().m_rotation1.m_vec);
            }
            else
            {
                const int f = frames & 0x7F;
                motion.m_deactivationNumInactiveFrames[select] = (unsigned short)((f - (f >> 6) + 1) | high);
            }
        }
    }
    const int frames0 = motion.m_deactivationNumInactiveFrames[0] & 0x7F;
    const int frames1 = motion.m_deactivationNumInactiveFrames[1] & 0x7F;
    return frames0 > frames1 ? frames0 : frames1;
}

// 0x00E0FBA0: every motion's velocity measure ((angular^2 * min(1, radius)^2 + linear^2) / 4 - 0.1^2) stays at or
// below the w of the reference position of its larger inactive frame counter
bool hkRigidMotionUtilCanDeactivateFinal(const hkStepInfo& info, hkpMotion* const* motions, int numMotions, int motionOffset)
{
    const Q minVelocity = mul(splat(0.1f), splat(0.1f));
    for (int i = 0; i < numMotions; i++)
    {
        const hkpMotion* motion = hkRigidMotionUtil_motion(motions, i, motionOffset);
        const Q radius = splat(lane0(_mm_min_ss(splat(1.0f), splat(motion->m_motionState.m_objectRadius))));
        const Q angular = mul(hadd3(mul(motion->m_angularVelocity.m_quad, motion->m_angularVelocity.m_quad)), mul(radius, radius));
        Q velocity = add(angular, hadd3(mul(motion->m_linearVelocity.m_quad, motion->m_linearVelocity.m_quad)));
        velocity = sub(mul(velocity, hkVector4f::getConstant<HK_QUADREAL_INV_4>().m_quad), minVelocity);
        const unsigned char frames0 = (unsigned char)(motion->m_deactivationNumInactiveFrames[0] & 0x7F);
        const unsigned char frames1 = (unsigned char)(motion->m_deactivationNumInactiveFrames[1] & 0x7F);
        const Q reference = frames0 > frames1 ? motion->m_deactivationRefPosition[0].m_quad : motion->m_deactivationRefPosition[1].m_quad;
        if (greater(velocity, bcast<3>(reference)))
        {
            return false;
        }
    }
    return true;
}

// 0x00E10010: the solver's accumulators (damping applied first). Box and thin box motions solve in their principal
// frame (the transposed rotation); dynamic motions (never created) get nothing written, keyframed ones no
// deactivation class.
hkpVelocityAccumulator* hkRigidMotionUtilApplyForcesAndBuildAccumulators(const hkStepInfo& info, hkpMotion* const* motions, int numMotions, int motionOffset, hkpVelocityAccumulator* accumulatorsOut)
{
    const Q identity0 = hkVector4f::getConstant<HK_QUADREAL_1000>().m_quad;
    const Q identity1 = hkVector4f::getConstant<HK_QUADREAL_0100>().m_quad;
    const Q identity2 = hkVector4f::getConstant<HK_QUADREAL_0010>().m_quad;
    hkpVelocityAccumulator* acc = accumulatorsOut;
    for (int i = 0; i < numMotions; i++, acc++)
    {
        hkpMotion* motion = hkRigidMotionUtil_motion(motions, i, motionOffset);
        const unsigned int type = motion->m_type.m_storage;
        if (type > hkpMotion::MOTION_CHARACTER)
        {
            continue;
        }
        const Q dt = splat(info.m_deltaTime.val());
        switch (type)
        {
        case hkpMotion::MOTION_INVALID:
        case hkpMotion::MOTION_BOX_INERTIA:
        case hkpMotion::MOTION_THIN_BOX_INERTIA:
        {
            motion->m_angularVelocity.m_quad = hkRigidMotionUtil_damp(motion->m_angularVelocity.m_quad, motion->m_motionState.m_angularDamping, dt);
            motion->m_linearVelocity.m_quad = hkRigidMotionUtil_damp(motion->m_linearVelocity.m_quad, motion->m_motionState.m_linearDamping, dt);
            acc->m_type.m_storage = hkpVelocityAccumulator::HK_RIGID_BODY;
            acc->m_context.m_storage = hkpVelocityAccumulator::ACCUMULATOR_CONTEXT_BUILD_JACOBIANS;
            // the core from world matrix: the rotation transposed (w lanes as the unpack shuffles leave them)
            const hkRotationf& r = motion->m_motionState.getTransform().getRotation();
            const Q c0 = r.getColumn<0>().m_quad;
            const Q c1 = r.getColumn<1>().m_quad;
            const Q c2 = r.getColumn<2>().m_quad;
            const Q lo = _mm_unpacklo_ps(c0, c1);
            const Q hi = _mm_unpackhi_ps(c0, c1);
            const Q row0 = _mm_movelh_ps(lo, c2);
            const Q row1 = _mm_shuffle_ps(_mm_movehl_ps(row0, lo), c2, 0xD4);
            const Q row2 = _mm_shuffle_ps(hi, c2, 0xE4);
            acc->m_scratch0.m_quad = row0;
            acc->m_scratch1.m_quad = row1;
            acc->m_scratch2.m_quad = row2;
            acc->m_invMasses = motion->m_inertiaAndMassInv;
            acc->m_linearVel = motion->m_linearVelocity;
            acc->m_scratch3.m_quad = hkRigidMotionUtil_centerOfMassAt(motion->m_motionState.getSweptTransform(), info.m_startTime.val());
            const Q w = motion->m_angularVelocity.m_quad;
            acc->m_angularVel.m_quad = add(add(mul(bcast<1>(w), row1), mul(bcast<0>(w), row0)), mul(bcast<2>(w), row2));
            acc->m_deactivationClass = motion->m_motionState.m_deactivationClass;
            acc->m_gravityFactor = hkRigidMotionUtil_half(motion->m_motionState.m_timeFactor) * hkRigidMotionUtil_half(motion->m_gravityFactor);
            break;
        }
        case hkpMotion::MOTION_SPHERE_INERTIA:
        case hkpMotion::MOTION_CHARACTER:
        {
            // characters are not affected by gravity in the solver
            acc->m_type.m_storage = (hkUint8)(type == hkpMotion::MOTION_CHARACTER ? hkpVelocityAccumulator::HK_NO_GRAVITY_RIGID_BODY : hkpVelocityAccumulator::HK_RIGID_BODY);
            acc->m_context.m_storage = hkpVelocityAccumulator::ACCUMULATOR_CONTEXT_BUILD_JACOBIANS;
            motion->m_angularVelocity.m_quad = hkRigidMotionUtil_damp(motion->m_angularVelocity.m_quad, motion->m_motionState.m_angularDamping, dt);
            motion->m_linearVelocity.m_quad = hkRigidMotionUtil_damp(motion->m_linearVelocity.m_quad, motion->m_motionState.m_linearDamping, dt);
            acc->m_invMasses = motion->m_inertiaAndMassInv;
            acc->m_scratch3.m_quad = hkRigidMotionUtil_centerOfMassAt(motion->m_motionState.getSweptTransform(), info.m_startTime.val());
            acc->m_angularVel = motion->m_angularVelocity;
            acc->m_linearVel = motion->m_linearVelocity;
            acc->m_scratch0.m_quad = identity0;
            acc->m_scratch1.m_quad = identity1;
            acc->m_scratch2.m_quad = identity2;
            acc->m_deactivationClass = motion->m_motionState.m_deactivationClass;
            acc->m_gravityFactor = hkRigidMotionUtil_half(motion->m_motionState.m_timeFactor) * hkRigidMotionUtil_half(motion->m_gravityFactor);
            break;
        }
        case hkpMotion::MOTION_KEYFRAMED:
            acc->m_type.m_storage = hkpVelocityAccumulator::HK_KEYFRAMED_RIGID_BODY;
            acc->m_context.m_storage = hkpVelocityAccumulator::ACCUMULATOR_CONTEXT_BUILD_JACOBIANS;
            acc->m_invMasses.m_quad = _mm_setzero_ps();
            acc->m_scratch3 = motion->m_motionState.getSweptTransform().m_centerOfMass1;
            acc->m_angularVel = motion->m_angularVelocity;
            acc->m_linearVel = motion->m_linearVelocity;
            acc->m_scratch0.m_quad = identity0;
            acc->m_scratch1.m_quad = identity1;
            acc->m_scratch2.m_quad = identity2;
            acc->m_gravityFactor = hkRigidMotionUtil_half(motion->m_motionState.m_timeFactor) * hkRigidMotionUtil_half(motion->m_gravityFactor);
            break;
        case hkpMotion::MOTION_FIXED:
            acc->m_context.m_storage = hkpVelocityAccumulator::ACCUMULATOR_CONTEXT_BUILD_JACOBIANS;
            acc->m_type.m_storage = hkpVelocityAccumulator::HK_KEYFRAMED_RIGID_BODY;
            acc->m_gravityFactor = 0.0f;
            acc->m_invMasses.m_quad = _mm_setzero_ps();
            acc->m_linearVel.m_quad = _mm_setzero_ps();
            acc->m_angularVel.m_quad = _mm_setzero_ps();
            acc->m_scratch3.m_quad = _mm_setzero_ps();
            acc->m_scratch0.m_quad = identity0;
            acc->m_scratch1.m_quad = identity1;
            acc->m_scratch2.m_quad = identity2;
            break;
        default: // MOTION_DYNAMIC
            break;
        }
    }
    return acc;
}

// 0x00E10430: the motion states of all non-fixed motions step with their current velocities
void hkRigidMotionUtilStep(const hkStepInfo& info, hkpMotion* const* motions, int numMotions, int motionOffset)
{
    for (int i = 0; i < numMotions; i++)
    {
        hkpMotion* motion = hkRigidMotionUtil_motion(motions, i, motionOffset);
        if (motion->m_type.m_storage == hkpMotion::MOTION_FIXED)
        {
            continue;
        }
        hkRigidMotionUtil_stepMotionState(info, motion->m_linearVelocity.m_quad, motion->m_angularVelocity.m_quad, motion->m_motionState);
    }
}

// 0x00E10940: islands without constraints integrate here instead of in the solver. Gravity (deltaVel * gravity
// factor * time factor) for dynamic motions, damping for those and characters, nothing for keyframed ones; fixed
// motions are skipped. Returns the smallest inactive frame count (0x7FFFFFFF without non-fixed motions).
int hkRigidMotionUtilApplyForcesAndStep(const hkpSolverInfo& solverInfo, const hkStepInfo& info, const hkVector4f& deltaVel, hkpMotion* const* motions, int numMotions, int motionOffset)
{
    int numInactiveFrames = 0x7FFFFFFF;
    for (int i = 0; i < numMotions; i++)
    {
        hkpMotion* motion = hkRigidMotionUtil_motion(motions, i, motionOffset);
        hkMotionState& ms = motion->m_motionState;
        const unsigned int type = motion->m_type.m_storage;
        if (type == hkpMotion::MOTION_FIXED)
        {
            continue;
        }
        const Q gravityFactor = mul(splat(hkRigidMotionUtil_half(motion->m_gravityFactor)), splat(hkRigidMotionUtil_half(ms.m_timeFactor)));
        if (type != hkpMotion::MOTION_KEYFRAMED)
        {
            if (type != hkpMotion::MOTION_CHARACTER)
            {
                motion->m_linearVelocity.m_quad = add(mul(deltaVel.m_quad, gravityFactor), motion->m_linearVelocity.m_quad);
            }
            const Q dt = splat(info.m_deltaTime.val());
            motion->m_linearVelocity.m_quad = hkRigidMotionUtil_damp(motion->m_linearVelocity.m_quad, ms.m_linearDamping, dt);
            motion->m_angularVelocity.m_quad = hkRigidMotionUtil_damp(motion->m_angularVelocity.m_quad, ms.m_angularDamping, dt);
        }

        if (type == hkpMotion::MOTION_THIN_BOX_INERTIA)
        {
            // the angular momentum (inertia * angular velocity, in the principal frame) is kept over the step
            hkVector4f localAngVel;
            localAngVel.setRotatedInverseDir(ms.getTransform().getRotation(), motion->m_angularVelocity);
            localAngVel.m_quad = mul(recip23(motion->m_inertiaAndMassInv.m_quad), localAngVel.m_quad);
            hkVector4f angularMomentum;
            angularMomentum.setRotatedDir(ms.getTransform().getRotation(), localAngVel);

            hkRigidMotionUtil_stepMotionState(info, motion->m_linearVelocity.m_quad, motion->m_angularVelocity.m_quad, ms);

            hkVector4f local;
            local.setRotatedInverseDir(ms.getTransform().getRotation(), angularMomentum);
            local.m_quad = mul(local.m_quad, motion->m_inertiaAndMassInv.m_quad);
            motion->m_angularVelocity.setRotatedDir(ms.getTransform().getRotation(), local);
            hkRigidMotionUtil_clipVelocities(ms, motion->m_linearVelocity, motion->m_angularVelocity);
        }
        else
        {
            hkRigidMotionUtil_stepMotionState(info, motion->m_linearVelocity.m_quad, motion->m_angularVelocity.m_quad, ms);
        }

        const int frames = hkRigidMotionUtilCheckDeactivation(solverInfo, *motion);
        if (numInactiveFrames >= frames)
        {
            numInactiveFrames = frames;
        }
    }
    return numInactiveFrames;
}

// 0x00E11950: the solver's velocities go back to the motions (box motions from the principal frame) and the motion
// states step with the accumulators' summed velocities (scratch0/scratch1). Every motion, fixed ones included, is
// clipped and counted for deactivation; dynamic and fixed motions do not step.
int hkRigidMotionUtilApplyAccumulators(const hkpSolverInfo& solverInfo, const hkStepInfo& info, const hkpVelocityAccumulator* accumulators, hkpMotion* const* motions, int numMotions, int motionOffset)
{
    int numInactiveFrames = 0x7FFFFFFF;
    hkVector4f angularMomentum;
    angularMomentum.m_quad = _mm_setzero_ps();
    for (int i = 0; i < numMotions; i++)
    {
        const hkpVelocityAccumulator* acc = accumulators + i;
        hkpMotion* motion = hkRigidMotionUtil_motion(motions, i, motionOffset);
        hkMotionState& ms = motion->m_motionState;
        const unsigned int type = motion->m_type.m_storage;
        if (type <= hkpMotion::MOTION_CHARACTER && type != hkpMotion::MOTION_DYNAMIC && type != hkpMotion::MOTION_FIXED)
        {
            Q stepLinVel;
            Q stepAngVel;
            const hkRotationf& r = ms.getTransform().getRotation();
            switch (type)
            {
            case hkpMotion::MOTION_INVALID:
            case hkpMotion::MOTION_THIN_BOX_INERTIA:
                angularMomentum.m_quad = rotate(r, mul(recip23(motion->m_inertiaAndMassInv.m_quad), acc->m_angularVel.m_quad));
                // fall through
            case hkpMotion::MOTION_BOX_INERTIA:
                motion->m_linearVelocity = acc->m_linearVel;
                motion->m_angularVelocity.m_quad = rotate(r, acc->m_angularVel.m_quad);
                stepAngVel = rotate(r, acc->m_scratch1.m_quad);
                stepLinVel = acc->m_scratch0.m_quad;
                break;
            case hkpMotion::MOTION_KEYFRAMED:
                stepLinVel = motion->m_linearVelocity.m_quad;
                stepAngVel = motion->m_angularVelocity.m_quad;
                break;
            default: // MOTION_SPHERE_INERTIA, MOTION_CHARACTER
                motion->m_linearVelocity = acc->m_linearVel;
                motion->m_angularVelocity = acc->m_angularVel;
                stepAngVel = acc->m_scratch1.m_quad;
                stepLinVel = acc->m_scratch0.m_quad;
                break;
            }
            hkRigidMotionUtil_stepMotionState(info, stepLinVel, stepAngVel, ms);
        }

        if (type == hkpMotion::MOTION_THIN_BOX_INERTIA)
        {
            motion->m_linearVelocity = acc->m_linearVel;
            hkVector4f local;
            local.setRotatedInverseDir(ms.getTransform().getRotation(), angularMomentum);
            local.m_quad = mul(motion->m_inertiaAndMassInv.m_quad, local.m_quad);
            motion->m_angularVelocity.setRotatedDir(ms.getTransform().getRotation(), local);
        }
        hkRigidMotionUtil_clipVelocities(ms, motion->m_linearVelocity, motion->m_angularVelocity);

        const int frames = hkRigidMotionUtilCheckDeactivation(solverInfo, *motion);
        if (numInactiveFrames >= frames)
        {
            numInactiveFrames = frames;
        }
    }
    return numInactiveFrames;
}

// 0x00E0FCA0: as hkRigidMotionUtilApplyForcesAndBuildAccumulators, leaving the motions alone: the damping goes to the
// accumulators of sphere and character motions only, and the velocities of box motions are not moved to their frame.
hkpVelocityAccumulator* hkRigidMotionUtilBuildAccumulators(const hkStepInfo& info, hkpMotion* const* motions, int numMotions, int motionOffset, hkpVelocityAccumulator* accumulatorsOut)
{
    const Q identity0 = hkVector4f::getConstant<HK_QUADREAL_1000>().m_quad;
    const Q identity1 = hkVector4f::getConstant<HK_QUADREAL_0100>().m_quad;
    const Q identity2 = hkVector4f::getConstant<HK_QUADREAL_0010>().m_quad;
    hkpVelocityAccumulator* acc = accumulatorsOut;
    for (int i = 0; i < numMotions; i++, acc++)
    {
        const hkpMotion* motion = hkRigidMotionUtil_motion(motions, i, motionOffset);
        const unsigned int type = motion->m_type.m_storage;
        switch (type)
        {
        case hkpMotion::MOTION_INVALID:
        case hkpMotion::MOTION_BOX_INERTIA:
        case hkpMotion::MOTION_THIN_BOX_INERTIA:
        {
            acc->m_type.m_storage = hkpVelocityAccumulator::HK_RIGID_BODY;
            acc->m_context.m_storage = hkpVelocityAccumulator::ACCUMULATOR_CONTEXT_BUILD_JACOBIANS;
            const hkRotationf& r = motion->m_motionState.getTransform().getRotation();
            const Q c0 = r.getColumn<0>().m_quad;
            const Q c1 = r.getColumn<1>().m_quad;
            const Q c2 = r.getColumn<2>().m_quad;
            const Q lo = _mm_unpacklo_ps(c0, c1);
            const Q hi = _mm_unpackhi_ps(c0, c1);
            const Q row0 = _mm_movelh_ps(lo, c2);
            acc->m_scratch0.m_quad = row0;
            acc->m_scratch1.m_quad = _mm_shuffle_ps(_mm_movehl_ps(row0, lo), c2, 0xD4);
            acc->m_scratch2.m_quad = _mm_shuffle_ps(hi, c2, 0xE4);
            acc->m_invMasses = motion->m_inertiaAndMassInv;
            acc->m_linearVel = motion->m_linearVelocity;
            acc->m_scratch3.m_quad = hkRigidMotionUtil_centerOfMassAt(motion->m_motionState.getSweptTransform(), info.m_startTime.val());
            acc->m_angularVel = motion->m_angularVelocity;
            acc->m_deactivationClass = motion->m_motionState.m_deactivationClass;
            acc->m_gravityFactor = hkRigidMotionUtil_half(motion->m_motionState.m_timeFactor) * hkRigidMotionUtil_half(motion->m_gravityFactor);
            break;
        }
        case hkpMotion::MOTION_SPHERE_INERTIA:
        case hkpMotion::MOTION_CHARACTER:
        {
            const Q dt = splat(info.m_deltaTime.val());
            acc->m_type.m_storage = (hkUint8)(type == hkpMotion::MOTION_CHARACTER ? hkpVelocityAccumulator::HK_NO_GRAVITY_RIGID_BODY : hkpVelocityAccumulator::HK_RIGID_BODY);
            acc->m_context.m_storage = hkpVelocityAccumulator::ACCUMULATOR_CONTEXT_BUILD_JACOBIANS;
            acc->m_invMasses = motion->m_inertiaAndMassInv;
            acc->m_scratch3.m_quad = hkRigidMotionUtil_centerOfMassAt(motion->m_motionState.getSweptTransform(), info.m_startTime.val());
            acc->m_angularVel.m_quad = hkRigidMotionUtil_damp(motion->m_angularVelocity.m_quad, motion->m_motionState.m_angularDamping, dt);
            acc->m_linearVel.m_quad = hkRigidMotionUtil_damp(motion->m_linearVelocity.m_quad, motion->m_motionState.m_linearDamping, dt);
            acc->m_scratch0.m_quad = identity0;
            acc->m_scratch1.m_quad = identity1;
            acc->m_scratch2.m_quad = identity2;
            acc->m_deactivationClass = motion->m_motionState.m_deactivationClass;
            acc->m_gravityFactor = hkRigidMotionUtil_half(motion->m_motionState.m_timeFactor) * hkRigidMotionUtil_half(motion->m_gravityFactor);
            break;
        }
        case hkpMotion::MOTION_KEYFRAMED:
            acc->m_type.m_storage = hkpVelocityAccumulator::HK_KEYFRAMED_RIGID_BODY;
            acc->m_context.m_storage = hkpVelocityAccumulator::ACCUMULATOR_CONTEXT_BUILD_JACOBIANS;
            acc->m_invMasses.m_quad = _mm_setzero_ps();
            acc->m_scratch3 = motion->m_motionState.getSweptTransform().m_centerOfMass1;
            acc->m_angularVel = motion->m_angularVelocity;
            acc->m_linearVel = motion->m_linearVelocity;
            acc->m_scratch0.m_quad = identity0;
            acc->m_scratch1.m_quad = identity1;
            acc->m_scratch2.m_quad = identity2;
            acc->m_gravityFactor = hkRigidMotionUtil_half(motion->m_motionState.m_timeFactor) * hkRigidMotionUtil_half(motion->m_gravityFactor);
            break;
        case hkpMotion::MOTION_FIXED:
            acc->m_type.m_storage = hkpVelocityAccumulator::HK_KEYFRAMED_RIGID_BODY;
            acc->m_context.m_storage = hkpVelocityAccumulator::ACCUMULATOR_CONTEXT_BUILD_JACOBIANS;
            acc->m_gravityFactor = 0.0f;
            acc->m_invMasses.m_quad = _mm_setzero_ps();
            acc->m_linearVel.m_quad = _mm_setzero_ps();
            acc->m_angularVel.m_quad = _mm_setzero_ps();
            acc->m_scratch3.m_quad = _mm_setzero_ps();
            acc->m_scratch0.m_quad = identity0;
            acc->m_scratch1.m_quad = identity1;
            acc->m_scratch2.m_quad = identity2;
            break;
        default: // MOTION_DYNAMIC
            break;
        }
    }
    return acc;
}
