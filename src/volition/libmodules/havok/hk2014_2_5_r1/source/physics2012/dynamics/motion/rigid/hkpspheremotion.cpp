// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/motion/rigid/hkpspheremotion.cpp
//
// Havok 2014.2.5 (havok_impl.md HK7): the motion with a uniform inertia (x of m_inertiaAndMassInv; y and z hold the
// same value), from rfg.exe. The arithmetic follows the binary's SSE sequences (hkGskExact helpers).

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/motion/rigid/hkpspheremotion.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/hkmath.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/gjk/hkpgskexact.h"

using namespace hkGskExact;

// 0x00DF2B10 (hkpspheremotion.h): deactivation off (the rigid body enables it), the inertia is set by the body
hkpSphereMotion::hkpSphereMotion(const hkVector4f& position, const hkQuaternionf& rotation)
    : hkpMotion(position, rotation, false)
{
    m_type.m_storage = MOTION_SPHERE_INERTIA;
}

// 0x00DDEE60 (hkpspheremotion.h)
hkpSphereMotion::hkpSphereMotion(hkFinishLoadedObjectFlag flag)
    : hkpMotion(flag)
{
    if (flag.m_finishing)
    {
        m_gravityFactor.setOne();
    }
}

// 0x00DDEED0
hkpSphereMotion::~hkpSphereMotion()
{
}

// 0x00E15080: the inverse of max(eps, inverse inertia) on the diagonal
void hkpSphereMotion::getInertiaLocal(hkMatrix3f& inertia) const
{
    const Q inv = _mm_max_ps(hkVector4f::getConstant<HK_QUADREAL_EPS>().m_quad, bcast<0>(m_inertiaAndMassInv.m_quad));
    const Q diag = recip23(inv);
    inertia.getColumn<0>().m_quad = mul(hkVector4f::getConstant<HK_QUADREAL_1000>().m_quad, diag);
    inertia.getColumn<1>().m_quad = mul(hkVector4f::getConstant<HK_QUADREAL_0100>().m_quad, diag);
    inertia.getColumn<2>().m_quad = mul(hkVector4f::getConstant<HK_QUADREAL_0010>().m_quad, diag);
}

// 0x00E14FC0: a uniform tensor is the same in every frame
void hkpSphereMotion::getInertiaWorld(hkMatrix3f& inertia) const
{
    getInertiaLocal(inertia);
}

static inline void hkSphereMotion_inertiaInv(const hkpMotion& motion, hkMatrix3f& inertiaInv)
{
    const Q d = bcast<0>(motion.m_inertiaAndMassInv.m_quad);
    inertiaInv.getColumn<0>().m_quad = mul(hkVector4f::getConstant<HK_QUADREAL_1000>().m_quad, d);
    inertiaInv.getColumn<1>().m_quad = mul(hkVector4f::getConstant<HK_QUADREAL_0100>().m_quad, d);
    inertiaInv.getColumn<2>().m_quad = mul(hkVector4f::getConstant<HK_QUADREAL_0010>().m_quad, d);
}

// 0x00E150E0
void hkpSphereMotion::getInertiaInvLocal(hkMatrix3f& inertiaInv) const
{
    hkSphereMotion_inertiaInv(*this, inertiaInv);
}

// 0x00E15120
void hkpSphereMotion::getInertiaInvWorld(hkMatrix3f& inertiaInv) const
{
    hkSphereMotion_inertiaInv(*this, inertiaInv);
}

// The diagonal of a tensor (x of column 0, y of column 1, z of column 2).
static inline Q hkSphereMotion_diagonal(const hkMatrix3f& m)
{
    const Q diag = selectQ(laneMask(2), m.getColumn<1>().m_quad, m.getColumn<0>().m_quad);
    return selectQ(laneMask(4), m.getColumn<2>().m_quad, diag);
}

// 0x00E15370: the inverse of the diagonal's largest element (23-bit, 0 for 0); w keeps the inverse mass
void hkpSphereMotion::setInertiaLocal(const hkMatrix3f& inertia)
{
    const Q diag = hkSphereMotion_diagonal(inertia);
    const Q largest = _mm_max_ps(bcast<2>(diag), _mm_max_ps(bcast<1>(diag), bcast<0>(diag)));
    const Q inv = _mm_andnot_ps(_mm_cmpeq_ps(largest, _mm_setzero_ps()), recip23(largest));
    m_inertiaAndMassInv.m_quad = setXYZ_W(inv, m_inertiaAndMassInv.m_quad);
}

// 0x00E15400: the diagonal's smallest element
void hkpSphereMotion::setInertiaInvLocal(const hkMatrix3f& inertiaInv)
{
    const Q diag = hkSphereMotion_diagonal(inertiaInv);
    const Q smallest = _mm_min_ps(bcast<2>(diag), _mm_min_ps(bcast<1>(diag), bcast<0>(diag)));
    m_inertiaAndMassInv.m_quad = setXYZ_W(smallest, m_inertiaAndMassInv.m_quad);
}

// 0x00E15160
void hkpSphereMotion::applyPointImpulse(const hkVector4f& imp, const hkVector4f& p)
{
    m_linearVelocity.m_quad = add(mul(bcast<3>(m_inertiaAndMassInv.m_quad), imp.m_quad), m_linearVelocity.m_quad);
    const Q arm = sub(p.m_quad, m_motionState.getSweptTransform().m_centerOfMass1.m_quad);
    m_angularVelocity.m_quad = add(mul(cross(arm, imp.m_quad), m_inertiaAndMassInv.m_quad), m_angularVelocity.m_quad);
}

// 0x00E14FF0
void hkpSphereMotion::applyAngularImpulse(const hkVector4f& imp)
{
    m_angularVelocity.m_quad = add(mul(m_inertiaAndMassInv.m_quad, imp.m_quad), m_angularVelocity.m_quad);
}

// 0x00E15010: through applyPointImpulse
void hkpSphereMotion::applyForce(const float deltaTime, const hkVector4f& force, const hkVector4f& p)
{
    hkVector4f impulse;
    impulse.m_quad = mul(splat(deltaTime), force.m_quad);
    applyPointImpulse(impulse, p);
}

// 0x00E151C0
void hkpSphereMotion::applyForce(const float deltaTime, const hkVector4f& force)
{
    const Q impulse = mul(splat(deltaTime), force.m_quad);
    m_linearVelocity.m_quad = add(mul(impulse, bcast<3>(m_inertiaAndMassInv.m_quad)), m_linearVelocity.m_quad);
}

// 0x00E15050: through applyAngularImpulse
void hkpSphereMotion::applyTorque(const float deltaTime, const hkVector4f& torque)
{
    hkVector4f impulse;
    impulse.m_quad = mul(splat(deltaTime), torque.m_quad);
    applyAngularImpulse(impulse);
}

// Original quirk kept: unlike hkpBoxMotion, the angular term uses normal x arm (the binary's operand order), so the
// angular velocity counts with the opposite sign.
static inline void hkSphereMotion_projectedPointVelocity(const hkpMotion& motion, const hkVector4f& pos, const hkVector4f& normal, Q& velOut, Q& invVirtMassOut)
{
    const Q arm = sub(pos.m_quad, motion.m_motionState.getSweptTransform().m_centerOfMass1.m_quad);
    const Q crs = cross(normal.m_quad, arm);
    const Q angular = mul(motion.m_angularVelocity.m_quad, crs);
    const Q linear = mul(motion.m_linearVelocity.m_quad, normal.m_quad);
    velOut = add(hadd3(angular), hadd3(linear));
    const Q inertia = mul(hadd3(mul(crs, crs)), bcast<0>(motion.m_inertiaAndMassInv.m_quad));
    invVirtMassOut = add(inertia, bcast<3>(motion.m_inertiaAndMassInv.m_quad));
}

// 0x00E151F0
void hkpSphereMotion::getProjectedPointVelocity(const hkVector4f& pos, const hkVector4f& normal, float& velOut, float& invVirtMassOut) const
{
    Q vel;
    Q invVirtMass;
    hkSphereMotion_projectedPointVelocity(*this, pos, normal, vel, invVirtMass);
    velOut = lane0(vel);
    invVirtMassOut = lane0(invVirtMass);
}

// 0x00E152B0
void hkpSphereMotion::getProjectedPointVelocitySimd(const hkVector4f& pos, const hkVector4f& normal, hkSimdFloat32& velOut, hkSimdFloat32& invVirtMassOut) const
{
    hkSphereMotion_projectedPointVelocity(*this, pos, normal, velOut.m_real, invVirtMassOut.m_real);
}
