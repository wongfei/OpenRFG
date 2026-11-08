// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/motion/rigid/hkpboxmotion.cpp
//
// Havok 2014.2.5 (havok_impl.md HK7): the motion with a diagonal inertia tensor (xyz of m_inertiaAndMassInv hold
// the inverse inertia, w the inverse mass), from rfg.exe. The arithmetic follows the binary's SSE sequences
// (hkGskExact helpers), so results match bit for bit.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/motion/rigid/hkpboxmotion.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/hkmath.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/gjk/hkpgskexact.h"

using namespace hkGskExact;

// 0x00E12E70: unit mass and inertia, deactivation off (the rigid body enables it)
hkpBoxMotion::hkpBoxMotion(const hkVector4f& position, const hkQuaternionf& rotation)
    : hkpMotion(position, rotation, false)
{
    m_inertiaAndMassInv = hkVector4f::getConstant<HK_QUADREAL_1>();
    m_type.m_storage = MOTION_BOX_INERTIA;
}

// 0x00DDEF30 (hkpboxmotion.h)
hkpBoxMotion::hkpBoxMotion(hkFinishLoadedObjectFlag flag)
    : hkpMotion(flag)
{
    if (flag.m_finishing)
    {
        m_gravityFactor.setOne();
    }
}

// 0x00B64020
hkpBoxMotion::~hkpBoxMotion()
{
}

// 0x00E12FA0: the inverse of max(eps, inverse inertia) on the diagonal
void hkpBoxMotion::getInertiaLocal(hkMatrix3f& inertia) const
{
    const Q inv = _mm_max_ps(hkVector4f::getConstant<HK_QUADREAL_EPS>().m_quad, m_inertiaAndMassInv.m_quad);
    const Q diag = recip23(inv);
    inertia.getColumn<0>().m_quad = mul(hkVector4f::getConstant<HK_QUADREAL_1000>().m_quad, diag);
    inertia.getColumn<1>().m_quad = mul(hkVector4f::getConstant<HK_QUADREAL_0100>().m_quad, diag);
    inertia.getColumn<2>().m_quad = mul(hkVector4f::getConstant<HK_QUADREAL_0010>().m_quad, diag);
}

// 0x00E12F10: R * I * R^T
void hkpBoxMotion::getInertiaWorld(hkMatrix3f& inertia) const
{
    getInertiaLocal(inertia);
    inertia.changeBasis(m_motionState.getTransform().getRotation());
}

// 0x00E13000
void hkpBoxMotion::getInertiaInvLocal(hkMatrix3f& inertiaInv) const
{
    inertiaInv.getColumn<0>().m_quad = mul(m_inertiaAndMassInv.m_quad, hkVector4f::getConstant<HK_QUADREAL_1000>().m_quad);
    inertiaInv.getColumn<1>().m_quad = mul(m_inertiaAndMassInv.m_quad, hkVector4f::getConstant<HK_QUADREAL_0100>().m_quad);
    inertiaInv.getColumn<2>().m_quad = mul(m_inertiaAndMassInv.m_quad, hkVector4f::getConstant<HK_QUADREAL_0010>().m_quad);
}

// 0x00E12EB0: (R * diag(inverse inertia)) * R^T
void hkpBoxMotion::getInertiaInvWorld(hkMatrix3f& inertiaInv) const
{
    const hkRotationf& r = m_motionState.getTransform().getRotation();
    const Q d = m_inertiaAndMassInv.m_quad;
    hkMatrix3f scaled;
    scaled.getColumn<0>().m_quad = mul(bcast<0>(d), r.getColumn<0>().m_quad);
    scaled.getColumn<1>().m_quad = mul(bcast<1>(d), r.getColumn<1>().m_quad);
    scaled.getColumn<2>().m_quad = mul(bcast<2>(d), r.getColumn<2>().m_quad);
    inertiaInv.setMulInverse(scaled, r);
}

// 0x00E13590: the diagonal of the given tensor, inverted (23-bit, no zero check); w keeps the inverse mass
void hkpBoxMotion::setInertiaLocal(const hkMatrix3f& inertia)
{
    Q diag = selectQ(laneMask(2), inertia.getColumn<1>().m_quad, inertia.getColumn<0>().m_quad);
    diag = selectQ(laneMask(4), inertia.getColumn<2>().m_quad, diag);
    m_inertiaAndMassInv.m_quad = setXYZ_W(recip23(diag), m_inertiaAndMassInv.m_quad);
}

// 0x00E13600
void hkpBoxMotion::setInertiaInvLocal(const hkMatrix3f& inertiaInv)
{
    Q diag = selectQ(laneMask(2), inertiaInv.getColumn<1>().m_quad, inertiaInv.getColumn<0>().m_quad);
    diag = selectQ(laneMask(4), inertiaInv.getColumn<2>().m_quad, diag);
    m_inertiaAndMassInv.m_quad = setXYZ_W(diag, m_inertiaAndMassInv.m_quad);
}

// 0x00E13040: 23-bit reciprocal without a zero check
void hkpBoxMotion::setMass(float mass)
{
    m_inertiaAndMassInv.m_quad = setXYZ_W(m_inertiaAndMassInv.m_quad, recip23(splat(mass)));
}

// 0x00E13080
void hkpBoxMotion::setMass(const hkSimdFloat32& mass)
{
    m_inertiaAndMassInv.m_quad = setXYZ_W(m_inertiaAndMassInv.m_quad, recip23(mass.m_real));
}

// The world space angular velocity change of a world space angular impulse: R * (invI * (R^T * imp)).
static inline Q hkBoxMotion_angularDelta(const hkpMotion& motion, Q imp)
{
    const hkRotationf& r = motion.m_motionState.getTransform().getRotation();
    const Q local = mul(rotateInverse(r, imp), motion.m_inertiaAndMassInv.m_quad);
    return rotate(r, local);
}

// 0x00E130C0
void hkpBoxMotion::applyPointImpulse(const hkVector4f& imp, const hkVector4f& p)
{
    m_linearVelocity.m_quad = add(mul(bcast<3>(m_inertiaAndMassInv.m_quad), imp.m_quad), m_linearVelocity.m_quad);
    const Q arm = sub(p.m_quad, m_motionState.getSweptTransform().m_centerOfMass1.m_quad);
    const Q angular = cross(arm, imp.m_quad);
    m_angularVelocity.m_quad = add(hkBoxMotion_angularDelta(*this, angular), m_angularVelocity.m_quad);
}

// 0x00E131E0
void hkpBoxMotion::applyAngularImpulse(const hkVector4f& imp)
{
    m_angularVelocity.m_quad = add(hkBoxMotion_angularDelta(*this, imp.m_quad), m_angularVelocity.m_quad);
}

// 0x00E12F30: through applyPointImpulse
void hkpBoxMotion::applyForce(const float deltaTime, const hkVector4f& force, const hkVector4f& p)
{
    hkVector4f impulse;
    impulse.m_quad = mul(splat(deltaTime), force.m_quad);
    applyPointImpulse(impulse, p);
}

// 0x00E132C0: v += (dt * force) * invMass
void hkpBoxMotion::applyForce(const float deltaTime, const hkVector4f& force)
{
    const Q impulse = mul(splat(deltaTime), force.m_quad);
    m_linearVelocity.m_quad = add(mul(impulse, bcast<3>(m_inertiaAndMassInv.m_quad)), m_linearVelocity.m_quad);
}

// 0x00E12F70: through applyAngularImpulse
void hkpBoxMotion::applyTorque(const float deltaTime, const hkVector4f& torque)
{
    hkVector4f impulse;
    impulse.m_quad = mul(splat(deltaTime), torque.m_quad);
    applyAngularImpulse(impulse);
}

// The velocity of the point along the normal, and the inverse virtual mass for an impulse there along the normal.
static inline void hkBoxMotion_projectedPointVelocity(const hkpMotion& motion, const hkVector4f& pos, const hkVector4f& normal, Q& velOut, Q& invVirtMassOut)
{
    const Q arm = sub(pos.m_quad, motion.m_motionState.getSweptTransform().m_centerOfMass1.m_quad);
    const Q crs = cross(arm, normal.m_quad);
    const Q linear = mul(motion.m_linearVelocity.m_quad, normal.m_quad);
    const Q angular = mul(motion.m_angularVelocity.m_quad, crs);
    const Q local = rotateInverse(motion.m_motionState.getTransform().getRotation(), crs);
    const Q weighted = mul(mul(motion.m_inertiaAndMassInv.m_quad, local), local);
    invVirtMassOut = add(hadd3(weighted), bcast<3>(motion.m_inertiaAndMassInv.m_quad));
    velOut = add(hadd3(angular), hadd3(linear));
}

// 0x00E132F0
void hkpBoxMotion::getProjectedPointVelocity(const hkVector4f& pos, const hkVector4f& normal, float& velOut, float& invVirtMassOut) const
{
    Q vel;
    Q invVirtMass;
    hkBoxMotion_projectedPointVelocity(*this, pos, normal, vel, invVirtMass);
    velOut = lane0(vel);
    invVirtMassOut = lane0(invVirtMass);
}

// 0x00E13440
void hkpBoxMotion::getProjectedPointVelocitySimd(const hkVector4f& pos, const hkVector4f& normal, hkSimdFloat32& velOut, hkSimdFloat32& invVirtMassOut) const
{
    hkBoxMotion_projectedPointVelocity(*this, pos, normal, velOut.m_real, invVirtMassOut.m_real);
}
