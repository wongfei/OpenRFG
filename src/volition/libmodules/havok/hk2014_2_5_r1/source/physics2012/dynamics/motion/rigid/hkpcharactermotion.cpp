// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/motion/rigid/hkpcharactermotion.cpp
//
// Havok 2014.2.5 (havok_impl.md HK7): the character motion (a sphere inertia whose projected point velocity ignores
// the rotation's virtual mass), from rfg.exe. The arithmetic follows the binary's SSE sequences (hkGskExact helpers).

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/motion/rigid/hkpcharactermotion.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/hkmath.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/gjk/hkpgskexact.h"

using namespace hkGskExact;

// 0x00DF2AE0 (hkpcharactermotion.h)
hkpCharacterMotion::hkpCharacterMotion(const hkVector4f& position, const hkQuaternionf& rotation)
    : hkpMotion(position, rotation, false)
{
    m_type.m_storage = MOTION_CHARACTER;
}

// 0x00DDF030 (hkpcharactermotion.h)
hkpCharacterMotion::hkpCharacterMotion(hkFinishLoadedObjectFlag flag)
    : hkpMotion(flag)
{
    if (flag.m_finishing)
    {
        m_gravityFactor.setOne();
    }
}

// 0x00DDF0A0
hkpCharacterMotion::~hkpCharacterMotion()
{
}

static inline void hkCharacterMotion_diagonalMatrix(Q diag, hkMatrix3f& m)
{
    m.getColumn<0>().m_quad = mul(hkVector4f::getConstant<HK_QUADREAL_1000>().m_quad, diag);
    m.getColumn<1>().m_quad = mul(hkVector4f::getConstant<HK_QUADREAL_0100>().m_quad, diag);
    m.getColumn<2>().m_quad = mul(hkVector4f::getConstant<HK_QUADREAL_0010>().m_quad, diag);
}

// 0x00E15520: HK_REAL_HIGH (0x5F7FFFF0, 1.8446743e19) on the diagonal for a zero inverse inertia
void hkpCharacterMotion::getInertiaLocal(hkMatrix3f& inertia) const
{
    const Q inv = bcast<0>(m_inertiaAndMassInv.m_quad);
    if (!(lane0(inv) != 0.0f)) // jnz: NaN counts as zero
    {
        const float high = _mm_cvtss_f32(bits(0x5F7FFFF0));
        inertia.getColumn<0>().setZero();
        inertia.getColumn<1>().setZero();
        inertia.getColumn<2>().setZero();
        inertia.getColumn<0>().m_quad.m128_f32[0] = high;
        inertia.getColumn<1>().m_quad.m128_f32[1] = high;
        inertia.getColumn<2>().m_quad.m128_f32[2] = high;
        return;
    }
    hkCharacterMotion_diagonalMatrix(recip23(inv), inertia);
}

// 0x00E155A0: no zero check here
void hkpCharacterMotion::getInertiaWorld(hkMatrix3f& inertia) const
{
    hkCharacterMotion_diagonalMatrix(recip23(bcast<0>(m_inertiaAndMassInv.m_quad)), inertia);
}

// 0x00E15600
void hkpCharacterMotion::getInertiaInvLocal(hkMatrix3f& inertiaInv) const
{
    hkCharacterMotion_diagonalMatrix(bcast<0>(m_inertiaAndMassInv.m_quad), inertiaInv);
}

// 0x00E15640
void hkpCharacterMotion::getInertiaInvWorld(hkMatrix3f& inertiaInv) const
{
    hkCharacterMotion_diagonalMatrix(bcast<0>(m_inertiaAndMassInv.m_quad), inertiaInv);
}

static inline Q hkCharacterMotion_diagonal(const hkMatrix3f& m)
{
    const Q diag = selectQ(laneMask(2), m.getColumn<1>().m_quad, m.getColumn<0>().m_quad);
    return selectQ(laneMask(4), m.getColumn<2>().m_quad, diag);
}

// 0x00E15830: as hkpSphereMotion::setInertiaLocal
void hkpCharacterMotion::setInertiaLocal(const hkMatrix3f& inertia)
{
    const Q diag = hkCharacterMotion_diagonal(inertia);
    const Q largest = _mm_max_ps(bcast<2>(diag), _mm_max_ps(bcast<1>(diag), bcast<0>(diag)));
    const Q inv = _mm_andnot_ps(_mm_cmpeq_ps(largest, _mm_setzero_ps()), recip23(largest));
    m_inertiaAndMassInv.m_quad = setXYZ_W(inv, m_inertiaAndMassInv.m_quad);
}

// 0x00E158C0: as hkpSphereMotion::setInertiaInvLocal
void hkpCharacterMotion::setInertiaInvLocal(const hkMatrix3f& inertiaInv)
{
    const Q diag = hkCharacterMotion_diagonal(inertiaInv);
    const Q smallest = _mm_min_ps(bcast<2>(diag), _mm_min_ps(bcast<1>(diag), bcast<0>(diag)));
    m_inertiaAndMassInv.m_quad = setXYZ_W(smallest, m_inertiaAndMassInv.m_quad);
}

// 0x00E15680
void hkpCharacterMotion::applyPointImpulse(const hkVector4f& imp, const hkVector4f& p)
{
    m_linearVelocity.m_quad = add(mul(bcast<3>(m_inertiaAndMassInv.m_quad), imp.m_quad), m_linearVelocity.m_quad);
    const Q arm = sub(p.m_quad, m_motionState.getSweptTransform().m_centerOfMass1.m_quad);
    m_angularVelocity.m_quad = add(mul(cross(arm, imp.m_quad), m_inertiaAndMassInv.m_quad), m_angularVelocity.m_quad);
}

// 0x00E15490
void hkpCharacterMotion::applyAngularImpulse(const hkVector4f& imp)
{
    m_angularVelocity.m_quad = add(mul(m_inertiaAndMassInv.m_quad, imp.m_quad), m_angularVelocity.m_quad);
}

// 0x00E154B0: through applyPointImpulse
void hkpCharacterMotion::applyForce(const float deltaTime, const hkVector4f& force, const hkVector4f& p)
{
    hkVector4f impulse;
    impulse.m_quad = mul(splat(deltaTime), force.m_quad);
    applyPointImpulse(impulse, p);
}

// 0x00E156E0
void hkpCharacterMotion::applyForce(const float deltaTime, const hkVector4f& force)
{
    const Q impulse = mul(splat(deltaTime), force.m_quad);
    m_linearVelocity.m_quad = add(mul(impulse, bcast<3>(m_inertiaAndMassInv.m_quad)), m_linearVelocity.m_quad);
}

// 0x00E154F0: through applyAngularImpulse
void hkpCharacterMotion::applyTorque(const float deltaTime, const hkVector4f& torque)
{
    hkVector4f impulse;
    impulse.m_quad = mul(splat(deltaTime), torque.m_quad);
    applyAngularImpulse(impulse);
}

// The angular term uses normal x arm, as hkpSphereMotion (original quirk kept); the inverse virtual mass is the
// inverse mass only.
static inline Q hkCharacterMotion_projectedPointVelocity(const hkpMotion& motion, const hkVector4f& pos, const hkVector4f& normal)
{
    const Q arm = sub(pos.m_quad, motion.m_motionState.getSweptTransform().m_centerOfMass1.m_quad);
    const Q linear = mul(motion.m_linearVelocity.m_quad, normal.m_quad);
    const Q angular = mul(cross(normal.m_quad, arm), motion.m_angularVelocity.m_quad);
    return add(hadd3(angular), hadd3(linear));
}

// 0x00E15710
void hkpCharacterMotion::getProjectedPointVelocity(const hkVector4f& pos, const hkVector4f& normal, float& velOut, float& invVirtMassOut) const
{
    velOut = lane0(hkCharacterMotion_projectedPointVelocity(*this, pos, normal));
    invVirtMassOut = lane0(bcast<3>(m_inertiaAndMassInv.m_quad));
}

// 0x00E157A0
void hkpCharacterMotion::getProjectedPointVelocitySimd(const hkVector4f& pos, const hkVector4f& normal, hkSimdFloat32& velOut, hkSimdFloat32& invVirtMassOut) const
{
    velOut.m_real = hkCharacterMotion_projectedPointVelocity(*this, pos, normal);
    invVirtMassOut.m_real = bcast<3>(m_inertiaAndMassInv.m_quad);
}
