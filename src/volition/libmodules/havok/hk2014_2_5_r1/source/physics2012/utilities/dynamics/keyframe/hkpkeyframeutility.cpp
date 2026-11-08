// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/utilities/dynamics/keyframe/hkpkeyframeutility.cpp
//
// Havok 2014.2.5 (havok_impl.md HK7): hard keyframing (the velocities that move a body to a target position and
// orientation in one step), from rfg.exe; multiplayer.md MP5 added the key frame info and acceleration info set-up the
// client's mover correction uses (its soft keyframe is RFG's own, havok_custom_soft_keyframe). applySoftKeyFrame and
// applyHardKeyFrameAsynchronously are not part of this port (fastSetUsingPositionOrientationPair: MP8).

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/utilities/dynamics/keyframe/hkpkeyframeutility.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/hkmath.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/hkprigidbody.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/gjk/hkpgskexact.h"

using namespace hkGskExact;

// hkVector4f::dot<4> (the SSE2 form): (xy + zw) pairs, then the pairs summed, in all lanes
static inline Q hkKeyFrameUtility_dot4(Q a, Q b)
{
    const Q m = mul(a, b);
    const Q s0 = add(_mm_shuffle_ps(m, m, _MM_SHUFFLE(1, 0, 3, 2)), m);
    return add(_mm_shuffle_ps(s0, s0, _MM_SHUFFLE(2, 3, 0, 1)), s0);
}

// x, y and z all within 1e-5 (0x3727C5AC) of each other
static inline bool hkKeyFrameUtility_equal3(Q a, Q b)
{
    const Q close = _mm_cmple_ps(abs(sub(a, b)), splat(1e-5f));
    return (_mm_movemask_ps(close) & 7) == 7;
}

// The angular velocity that turns q0 into q1 in 1 / invDt: the rotation still to go (q1 * conjugate(q0)), normalized;
// its axis * angle * invDt (the shorter way round), zero below HK_QUADREAL_EPS_SQRD (applyHardKeyFrame and
// KeyFrameInfo::setUsingPositionOrientationPair inline the same code).
static inline Q hkKeyFrameUtility_angularVelocity(Q q0, Q q1, Q invDt)
{
    Q xyz = cross(q0, q1);
    xyz = sub(xyz, mul(bcast<3>(q1), q0));
    xyz = add(xyz, mul(bcast<3>(q0), q1));
    const Q diff = setXYZ_W(xyz, hkKeyFrameUtility_dot4(q0, q1));
    hkQuaternionf rest;
    rest.m_vec.m_quad = mul(invSqrt23(hkKeyFrameUtility_dot4(diff, diff)), diff);
    if (!greater(hadd3(mul(rest.m_vec.m_quad, rest.m_vec.m_quad)), hkVector4f::getConstant<HK_QUADREAL_EPS_SQRD>().m_quad))
    {
        return _mm_setzero_ps();
    }
    const hkSimdFloat32 angle = rest.getAngleSr();
    const Q r = rest.m_vec.m_quad;
    const Q axis = mul(invSqrt23SetZero(hadd3(mul(r, r))), r);
    // the shorter way round: the axis flips with a negative w
    const Q wNegative = _mm_cmplt_ps(bcast<3>(r), _mm_setzero_ps());
    const Q sign = _mm_castsi128_ps(_mm_slli_epi32(_mm_srli_epi32(_mm_castps_si128(wNegative), 31), 31));
    return mul(_mm_xor_ps(sign, axis), mul(angle.m_real, invDt));
}

// 0x00F1FDD0: the velocities that bring the center of mass to nextPosition + nextOrientation * centerOfMassLocal and
// the rotation to nextOrientation in 1 / invDeltaTime; the body is only activated and its velocities set when they
// differ from the current ones
void hkpKeyFrameUtility::applyHardKeyFrame(const hkVector4f& nextPosition, const hkQuaternionf& nextOrientation, float invDeltaTime, hkpRigidBody* body)
{
    const Q invDt = splat(invDeltaTime);
    const hkMotionState& motionState = body->m_motion.m_motionState;

    // hkVector4f::setRotatedDir(quaternion): 2 * ((q.v . v) q.v + (w^2 - 0.5) v + w (q.v x v))
    {
        const Q q = nextOrientation.m_vec.m_quad;
        const Q v = motionState.getSweptTransform().m_centerOfMassLocal.m_quad;
        const Q qw = bcast<3>(q);
        Q r = mul(dot3(v, q), q);
        r = add(r, mul(sub(mul(qw, qw), hkVector4f::getConstant<HK_QUADREAL_INV_2>().m_quad), v));
        r = add(r, mul(cross(q, v), qw));
        const Q target = add(add(r, r), nextPosition.m_quad);
        hkVector4f linearVelocity;
        linearVelocity.m_quad = mul(sub(target, motionState.getSweptTransform().m_centerOfMass1.m_quad), invDt);
        if (!hkKeyFrameUtility_equal3(body->m_motion.m_linearVelocity.m_quad, linearVelocity.m_quad))
        {
            body->setLinearVelocity(linearVelocity);
        }
    }

    // the rotation still to go (nextOrientation * conjugate(rotation1)), normalized; its axis * angle / dt
    {
        hkVector4f angularVelocity;
        angularVelocity.m_quad = hkKeyFrameUtility_angularVelocity(motionState.getSweptTransform().m_rotation1.m_vec.m_quad, nextOrientation.m_vec.m_quad, invDt);
        if (!hkKeyFrameUtility_equal3(body->m_motion.m_angularVelocity.m_quad, angularVelocity.m_quad))
        {
            body->setAngularVelocity(angularVelocity);
        }
    }
}

// 0x00F1F870
// Beyond the allowed distance the body is put at the key frame at once; then its velocities move towards the key
// frame's (velocity factors * deltaTime) and towards its pose (position factors: the position difference, twice the
// rotation still to go, the shorter way round), each change limited to the maximum acceleration * deltaTime; a velocity
// is only set when a component changes by more than 1e-5. invDeltaTime is unused.
void hkpKeyFrameUtility::applySoftKeyFrame(const KeyFrameInfo& keyFrameInfo, AccelerationInfo& accelInfo, float deltaTime, float invDeltaTime, hkpRigidBody* body)
{
    const Q dt = splat(deltaTime);
    Q d = sub(keyFrameInfo.m_position.m_quad, body->getPosition().m_quad);
    const Q maxDistance = splat(accelInfo.m_maxAllowedDistance);
    // (the maximum linear and angular velocities are decoded here and not used)
    if (_mm_comigt_ss(hadd3(mul(d, d)), mul(maxDistance, maxDistance)))
    {
        d = _mm_setzero_ps();
        body->setPositionAndRotation(keyFrameInfo.m_position, keyFrameInfo.m_orientation);
        body->setAngularVelocity(keyFrameInfo.m_angularVelocity);
        body->setLinearVelocity(keyFrameInfo.m_linearVelocity);
    }
    const Q q0 = body->getRotation().m_vec.m_quad;
    const Q q1 = keyFrameInfo.m_orientation.m_vec.m_quad;
    Q xyz = cross(q0, q1);
    xyz = sub(xyz, mul(bcast<3>(q1), q0));
    xyz = add(xyz, mul(bcast<3>(q0), q1));
    const Q diff = setXYZ_W(xyz, hkKeyFrameUtility_dot4(q0, q1));

    const Q linearVelocity = body->getLinearVelocity().m_quad;
    Q linearChange = mul(sub(keyFrameInfo.m_linearVelocity.m_quad, linearVelocity), mul(accelInfo.m_linearVelocityFactor.m_quad, dt));
    linearChange = add(linearChange, mul(accelInfo.m_linearPositionFactor.m_quad, d));
    const Q angularVelocity = body->getAngularVelocity().m_quad;
    // 2 * diff with the sign of its w flipped away (the shorter way round)
    const Q wNegative = _mm_cmplt_ps(bcast<3>(diff), _mm_setzero_ps());
    const Q sign = _mm_castsi128_ps(_mm_slli_epi32(_mm_srli_epi32(_mm_castps_si128(wNegative), 31), 31));
    Q angularChange = mul(_mm_xor_ps(sign, add(diff, diff)), accelInfo.m_angularPositionFactor.m_quad);
    angularChange = add(angularChange, mul(sub(keyFrameInfo.m_angularVelocity.m_quad, angularVelocity), mul(accelInfo.m_angularVelocityFactor.m_quad, dt)));

    const Q maxLinear = mul(splat(accelInfo.m_maxLinearAcceleration), dt);
    const Q linear2 = hadd3(mul(linearChange, linearChange));
    if (_mm_comigt_ss(linear2, mul(maxLinear, maxLinear)))
    {
        linearChange = mul(linearChange, mul(invSqrt23SetZero(linear2), maxLinear));
    }
    const Q maxAngular = mul(splat(accelInfo.m_maxAngularAcceleration), dt);
    const Q angular2 = hadd3(mul(angularChange, angularChange));
    if (_mm_comigt_ss(angular2, mul(maxAngular, maxAngular)))
    {
        angularChange = mul(angularChange, mul(invSqrt23SetZero(angular2), maxAngular));
    }

    hkVector4f newAngularVelocity;
    newAngularVelocity.m_quad = add(angularVelocity, angularChange);
    hkVector4f newLinearVelocity;
    newLinearVelocity.m_quad = add(linearVelocity, linearChange);
    if (!hkKeyFrameUtility_equal3(angularVelocity, newAngularVelocity.m_quad))
    {
        body->setAngularVelocity(newAngularVelocity);
    }
    if (!hkKeyFrameUtility_equal3(body->getLinearVelocity().m_quad, newLinearVelocity.m_quad))
    {
        body->setLinearVelocity(newLinearVelocity);
    }
}

// 0x00F1FBF0: the current pose and the velocities that reach the next pose in 1 / invDeltaTime (no center of mass
// offset, unlike applyHardKeyFrame)
void hkpKeyFrameUtility::KeyFrameInfo::setUsingPositionOrientationPair(const hkVector4f& currentPosition, const hkQuaternionf& currentOrientation, const hkVector4f& nextPosition, const hkQuaternionf& nextOrientation, float invDeltaTime)
{
    const Q invDt = splat(invDeltaTime);
    m_position = currentPosition;
    m_linearVelocity.m_quad = mul(sub(nextPosition.m_quad, currentPosition.m_quad), invDt);
    m_orientation = currentOrientation;
    m_angularVelocity.m_quad = hkKeyFrameUtility_angularVelocity(currentOrientation.m_vec.m_quad, nextOrientation.m_vec.m_quad, invDt);
}

// 0x00F1F730: as setUsingPositionOrientationPair with the small-angle angular velocity: twice the vector part of
// next * conjugate(current) times invDeltaTime (w: twice the dot product), negated when the dot product is negative
// (the shorter way round); no normalization (multiplayer.md MP8, the client's item correction).
void hkpKeyFrameUtility::KeyFrameInfo::fastSetUsingPositionOrientationPair(const hkVector4f& currentPosition, const hkQuaternionf& currentOrientation, const hkVector4f& nextPosition, const hkQuaternionf& nextOrientation, float invDeltaTime)
{
    m_position = currentPosition;
    const float* cur = currentPosition.m_quad.m128_f32;
    const float* next = nextPosition.m_quad.m128_f32;
    m_linearVelocity.m_quad = _mm_setr_ps((next[0] - cur[0]) * invDeltaTime, (next[1] - cur[1]) * invDeltaTime, (next[2] - cur[2]) * invDeltaTime, (next[3] - cur[3]) * invDeltaTime);
    m_orientation = currentOrientation;
    const float* q0 = currentOrientation.m_vec.m_quad.m128_f32;
    const float* q1 = nextOrientation.m_vec.m_quad.m128_f32;
    const float dot = q0[0] * q1[0] + q0[2] * q1[2] + q0[1] * q1[1] + q0[3] * q1[3];
    const float twice_inv = invDeltaTime + invDeltaTime;
    float lanes[4] = {
        twice_inv * (((q1[2] * q0[1] - q0[2] * q1[1]) - q1[3] * q0[0]) + q0[3] * q1[0]),
        twice_inv * (((q1[0] * q0[2] - q0[0] * q1[2]) - q1[3] * q0[1]) + q0[3] * q1[1]),
        twice_inv * (((q1[1] * q0[0] - q0[1] * q1[0]) - q1[3] * q0[2]) + q0[3] * q1[2]),
        twice_inv * dot,
    };
    if (dot < 0.0f)
    {
        // (the sign bit flipped, as the original's xor)
        for (int i = 0; i < 4; i++)
        {
            lanes[i] = -lanes[i];
        }
    }
    m_angularVelocity.m_quad = _mm_loadu_ps(lanes);
}

// 0x00F1F830: the factors 1 (all four lanes; hkdiff_multi), the limits HK_REAL_MAX
hkpKeyFrameUtility::AccelerationInfo::AccelerationInfo()
{
    m_linearPositionFactor.m_quad = splat(1.0f);
    m_angularPositionFactor.m_quad = splat(1.0f);
    m_linearVelocityFactor.m_quad = splat(1.0f);
    m_angularVelocityFactor.m_quad = splat(1.0f);
    m_maxLinearAcceleration = 3.40282e+38f; // HK_REAL_MAX (0x7F7FFFEE)
    m_maxAngularAcceleration = 3.40282e+38f;
    m_maxAllowedDistance = 3.40282e+38f;
}
