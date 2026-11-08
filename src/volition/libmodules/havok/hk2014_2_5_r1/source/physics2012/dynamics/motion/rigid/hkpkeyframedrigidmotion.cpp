// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/motion/rigid/hkpkeyframedrigidmotion.cpp
//
// Havok 2014.2.5 (havok_impl.md HK7): the keyframed motion (infinite mass: the mass, inertia and impulse setters do
// nothing) and hkpMaxSizeMotion (the storage for a dynamic motion while the body is keyframed or fixed), from rfg.exe.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/motion/rigid/hkpkeyframedrigidmotion.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/hkmath.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/gjk/hkpgskexact.h"

using namespace hkGskExact;

// 0x00DDE710 (hkpkeyframedrigidmotion.h)
hkpKeyframedRigidMotion::hkpKeyframedRigidMotion()
    : hkpMotion()
{
    m_savedMotion = nullptr;
}

// 0x00E14CF0: zero inverse mass and inertia, deactivation off
hkpKeyframedRigidMotion::hkpKeyframedRigidMotion(const hkVector4f& position, const hkQuaternionf& rotation)
    : hkpMotion(position, rotation, false)
{
    m_savedMotion = nullptr;
    m_savedQualityTypeIndex = 0;
    m_inertiaAndMassInv.setZero();
    m_type.m_storage = MOTION_KEYFRAMED;
}

// 0x00DDEB60 (hkpkeyframedrigidmotion.h)
hkpKeyframedRigidMotion::hkpKeyframedRigidMotion(hkFinishLoadedObjectFlag flag)
    : hkpMotion(flag)
{
    if (flag.m_finishing)
    {
        m_gravityFactor.setOne();
    }
}

// 0x00E14DF0
hkpKeyframedRigidMotion::~hkpKeyframedRigidMotion()
{
    if (m_savedMotion != nullptr)
    {
        m_savedMotion->removeReference();
    }
}

// 0x00E14C20, 0x00E14C30, 0x00E14C40, 0x00E14C50, 0x00E14C60, 0x00E14C70: the mass and inertia stay infinite
void hkpKeyframedRigidMotion::setMass(float m)
{
}

void hkpKeyframedRigidMotion::setMass(const hkSimdFloat32& m)
{
}

void hkpKeyframedRigidMotion::setMassInv(float mInv)
{
}

void hkpKeyframedRigidMotion::setMassInv(const hkSimdFloat32& mInv)
{
}

void hkpKeyframedRigidMotion::setInertiaLocal(const hkMatrix3f& inertia)
{
}

void hkpKeyframedRigidMotion::setInertiaInvLocal(const hkMatrix3f& inertiaInv)
{
}

// 0x00E14C80, 0x00E14C90, 0x00E14CA0, 0x00E14CB0, 0x00E14CC0, 0x00E14CD0: impulses and forces are ignored
void hkpKeyframedRigidMotion::applyLinearImpulse(const hkVector4f& imp)
{
}

void hkpKeyframedRigidMotion::applyPointImpulse(const hkVector4f& imp, const hkVector4f& p)
{
}

void hkpKeyframedRigidMotion::applyAngularImpulse(const hkVector4f& imp)
{
}

void hkpKeyframedRigidMotion::applyForce(const float deltaTime, const hkVector4f& force)
{
}

void hkpKeyframedRigidMotion::applyForce(const float deltaTime, const hkVector4f& force, const hkVector4f& p)
{
}

void hkpKeyframedRigidMotion::applyTorque(const float deltaTime, const hkVector4f& torque)
{
}

// 0x00E14CE0
void hkpKeyframedRigidMotion::setStepPosition(float position, float timestep)
{
}

// 0x00E14D40, 0x00E14D60, 0x00E14D80, 0x00E14DA0: zero tensors
void hkpKeyframedRigidMotion::getInertiaLocal(hkMatrix3f& inertia) const
{
    inertia.setZero();
}

void hkpKeyframedRigidMotion::getInertiaWorld(hkMatrix3f& inertia) const
{
    inertia.setZero();
}

void hkpKeyframedRigidMotion::getInertiaInvLocal(hkMatrix3f& inertiaInv) const
{
    inertiaInv.setZero();
}

void hkpKeyframedRigidMotion::getInertiaInvWorld(hkMatrix3f& inertiaInvOut) const
{
    inertiaInvOut.setZero();
}

// 0x00E14DC0
void hkpKeyframedRigidMotion::setStoredMotion(hkpMaxSizeMotion* savedMotion)
{
    if (savedMotion != nullptr)
    {
        savedMotion->addReference();
    }
    if (m_savedMotion != nullptr)
    {
        m_savedMotion->removeReference();
    }
    m_savedMotion = savedMotion;
}

// The angular term uses normal x arm, as hkpSphereMotion (original quirk kept).
static inline Q hkKeyframedRigidMotion_projectedPointVelocity(const hkpMotion& motion, const hkVector4f& pos, const hkVector4f& normal)
{
    const Q arm = sub(pos.m_quad, motion.m_motionState.getSweptTransform().m_centerOfMass1.m_quad);
    const Q linear = mul(motion.m_linearVelocity.m_quad, normal.m_quad);
    const Q angular = mul(cross(normal.m_quad, arm), motion.m_angularVelocity.m_quad);
    return add(hadd3(angular), hadd3(linear));
}

// 0x00E14E10: infinite mass (zero inverse virtual mass)
void hkpKeyframedRigidMotion::getProjectedPointVelocity(const hkVector4f& pos, const hkVector4f& normal, float& velOut, float& invVirtMassOut) const
{
    velOut = lane0(hkKeyframedRigidMotion_projectedPointVelocity(*this, pos, normal));
    invVirtMassOut = 0.0f;
}

// 0x00E14EA0
void hkpKeyframedRigidMotion::getProjectedPointVelocitySimd(const hkVector4f& pos, const hkVector4f& normal, hkSimdFloat32& velOut, hkSimdFloat32& invVirtMassOut) const
{
    velOut.m_real = hkKeyframedRigidMotion_projectedPointVelocity(*this, pos, normal);
    invVirtMassOut.m_real = _mm_setzero_ps();
}

// 0x00DDE730 (hkpkeyframedrigidmotion.h)
hkpMaxSizeMotion::hkpMaxSizeMotion()
    : hkpKeyframedRigidMotion()
{
}

// 0x00DDEC20 (hkpkeyframedrigidmotion.h)
hkpMaxSizeMotion::hkpMaxSizeMotion(hkFinishLoadedObjectFlag flag)
    : hkpKeyframedRigidMotion(flag)
{
}

// 0x00DDE750: the keyframed motion's destructor (jmp)
hkpMaxSizeMotion::~hkpMaxSizeMotion()
{
}
