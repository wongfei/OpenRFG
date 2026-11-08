// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/motion/hkpmotion.cpp
//
// Havok 2014.2.5 (havok_impl.md HK7): the motion base, from rfg.exe. The out-of-line copies of hkpmotion.inl
// (setWorldSelectFlagsNeg, enableDeactivation and the small accessors) are defined here too.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/motion/hkpmotion.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/hkmath.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/swepttransform/hkswepttransformutil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkvector4util.h"

// 0x00DF0CD0: at the origin with the identity rotation, deactivation disabled
hkpMotion::hkpMotion()
{
    m_linearVelocity.setZero();
    m_angularVelocity.setZero();
    hkVector4f position;
    position.setZero();
    hkQuaternionf rotation;
    rotation.m_vec = hkVector4f::getConstant<HK_QUADREAL_0001>();
    m_motionState.initMotionState(position, rotation);
    m_deactivationRefOrientation[0] = 0;
    m_deactivationRefOrientation[1] = 0;
    m_gravityFactor.setOne();
    m_motionState.m_linearDamping.setZero();
    m_motionState.m_angularDamping.setZero();
    m_type.m_storage = MOTION_INVALID;
    m_deactivationIntegrateCounter = 0xFF; // deactivation disabled
    m_deactivationNumInactiveFrames[0] = 0;
    m_deactivationNumInactiveFrames[1] = 0;
    m_deactivationRefPosition[0].setZero();
    m_deactivationRefPosition[1].setZero();
}

// 0x00DF0D70: the deactivation counter starts at the truncated x position (mod 16), spreading the bodies' checks over
// the frames
hkpMotion::hkpMotion(const hkVector4f& position, const hkQuaternionf& rotation, bool wantDeactivation)
{
    m_linearVelocity.setZero();
    m_angularVelocity.setZero();
    m_motionState.initMotionState(position, rotation);
    m_motionState.m_linearDamping.setZero();
    m_motionState.m_angularDamping.setZero();
    m_type.m_storage = MOTION_INVALID;
    if (wantDeactivation)
    {
        m_deactivationIntegrateCounter = (unsigned char)((int)position.m_quad.m128_f32[0] & 0xF); // cvttss2si
    }
    else
    {
        m_deactivationIntegrateCounter = 0xFF;
    }
    m_deactivationNumInactiveFrames[0] = 0;
    m_deactivationNumInactiveFrames[1] = 0;
    m_deactivationRefOrientation[0] = 0;
    m_deactivationRefOrientation[1] = 0;
    m_deactivationRefPosition[0].setZero();
    m_deactivationRefPosition[1].setZero();
    m_gravityFactor.setOne();
}

// 0x00DDEB00: nothing to fix up (the derived classes set the gravity factor)
hkpMotion::hkpMotion(hkFinishLoadedObjectFlag flag)
    : hkReferencedObject(flag)
{
}

// 0x00DF0B40: through setMassInv (0 for a zero mass)
void hkpMotion::setMass(float mass)
{
    const float massInv = mass == 0.0f ? 0.0f : 1.0f / mass;
    setMassInv(massInv);
}

// 0x00DF0E10: through setMassInv (23-bit reciprocal, 0 for a zero mass)
void hkpMotion::setMass(const hkSimdFloat32& mass)
{
    hkSimdFloat32 massInv;
    massInv.setReciprocal<HK_ACC_23_BIT, HK_DIV_SET_ZERO>(mass);
    setMassInv(massInv);
}

// 0x00DF0C20
void hkpMotion::setMassInv(float massInv)
{
    m_inertiaAndMassInv.m_quad.m128_f32[3] = massInv;
}

// 0x00DF0C40
void hkpMotion::setMassInv(const hkSimdFloat32& massInv)
{
    m_inertiaAndMassInv.setW(massInv);
}

// 0x00DF0E70: 23-bit reciprocal of the inverse mass, 0 for an infinite mass
float hkpMotion::getMass() const
{
    hkSimdFloat32 massInv;
    massInv.m_real = _mm_shuffle_ps(m_inertiaAndMassInv.m_quad, m_inertiaAndMassInv.m_quad, _MM_SHUFFLE(3, 3, 3, 3));
    hkSimdFloat32 mass;
    mass.setReciprocal<HK_ACC_23_BIT, HK_DIV_SET_ZERO>(massInv);
    return mass.getReal();
}

// 0x00DF0B70
void hkpMotion::setCenterOfMassInLocal(const hkVector4f& centerOfMass)
{
    hkSweptTransformUtil::setCentreOfRotationLocal(centerOfMass, m_motionState);
}

// 0x00DF0B90
void hkpMotion::setPosition(const hkVector4f& position)
{
    hkSweptTransformUtil::warpToPosition(position, m_motionState);
}

// 0x00DF0BB0
void hkpMotion::setRotation(const hkQuaternionf& rotation)
{
    hkSweptTransformUtil::warpToRotation(rotation, m_motionState);
}

// 0x00DF0BD0
void hkpMotion::setPositionAndRotation(const hkVector4f& position, const hkQuaternionf& rotation)
{
    hkSweptTransformUtil::warpTo(position, rotation, m_motionState);
}

// 0x00DF0BF0
void hkpMotion::setTransform(const hkTransformf& transform)
{
    hkSweptTransformUtil::warpTo(transform, m_motionState);
}

// 0x00DF0C10
void hkpMotion::setDeactivationClass(int deactivationClass)
{
    m_motionState.m_deactivationClass = (unsigned char)deactivationClass;
}

// 0x00DF0C90
void hkpMotion::setLinearVelocity(const hkVector4f& newVel)
{
    m_linearVelocity = newVel;
}

// 0x00DF0CB0
void hkpMotion::setAngularVelocity(const hkVector4f& newVel)
{
    m_angularVelocity = newVel;
}

// 0x00DF0EB0: v += imp * invMass
void hkpMotion::applyLinearImpulse(const hkVector4f& imp)
{
    const __m128 massInv = _mm_shuffle_ps(m_inertiaAndMassInv.m_quad, m_inertiaAndMassInv.m_quad, _MM_SHUFFLE(3, 3, 3, 3));
    m_linearVelocity.m_quad = _mm_add_ps(_mm_mul_ps(massInv, imp.m_quad), m_linearVelocity.m_quad);
}

// 0x00DF0EE0: both reference positions (w = HK_REAL_MAX) and orientations become the current ones, and both
// inactive frame counters jump to 6 (past NUM_INACTIVE_FRAMES_TO_DEACTIVATE), keeping the counters' select bits
void hkpMotion::requestDeactivation()
{
    m_deactivationRefPosition[0] = m_motionState.getTransform().getTranslation();
    m_deactivationRefPosition[0].setW(hkSimdFloat32::getConstant<HK_QUADREAL_MAX>());
    m_deactivationRefPosition[1] = m_deactivationRefPosition[0];
    const unsigned int orientation = hkVector4UtilImpl<float>::packQuaternionIntoInt32(m_motionState.getSweptTransform().m_rotation1.m_vec);
    m_deactivationRefOrientation[0] = orientation;
    m_deactivationRefOrientation[1] = orientation;
    m_deactivationNumInactiveFrames[0] = (unsigned short)((m_deactivationNumInactiveFrames[0] & 0xFF86) | 6);
    m_deactivationNumInactiveFrames[1] = (unsigned short)((m_deactivationNumInactiveFrames[1] & 0xFF86) | 6);
}

// 0x00DF0F60
void hkpMotion::getMotionStateAndVelocitiesAndDeactivationType(hkpMotion* motionOut)
{
    motionOut->m_motionState = m_motionState;
    motionOut->m_linearVelocity = m_linearVelocity;
    motionOut->m_angularVelocity = m_angularVelocity;
    motionOut->m_deactivationIntegrateCounter = m_deactivationIntegrateCounter;
}

// 0x00DDE680 (hkpmotion.inl): bit 14 of each inactive frame counter is the world's select flag, inverted when the
// motion's integrate counter is past the world's (mod 4 for counter 0, mod 16 for counter 1)
void hkpMotion::setWorldSelectFlagsNeg(int worldFlag0, int worldFlag1, int worldDeactivationIntegrateCounter)
{
    if ((unsigned)(worldDeactivationIntegrateCounter & 3) < (unsigned)(m_deactivationIntegrateCounter & 3))
    {
        worldFlag0 = ~worldFlag0;
    }
    if (worldDeactivationIntegrateCounter < (int)m_deactivationIntegrateCounter)
    {
        worldFlag1 = ~worldFlag1;
    }
    m_deactivationNumInactiveFrames[0] = (unsigned short)((m_deactivationNumInactiveFrames[0] & 0x3FFF) | (worldFlag0 << 14));
    m_deactivationNumInactiveFrames[1] = (unsigned short)((m_deactivationNumInactiveFrames[1] & 0x3FFF) | (worldFlag1 << 14));
}

// 0x00DF2900 (hkpmotion.inl)
void hkpMotion::enableDeactivation(bool value, int randomNumber, int worldFlag0, int worldFlag1, int worldDeactivationIntegrateCounter)
{
    if (value)
    {
        m_deactivationIntegrateCounter = (unsigned char)(randomNumber & 0xF);
        setWorldSelectFlagsNeg(worldFlag0, worldFlag1, worldDeactivationIntegrateCounter);
    }
    else
    {
        m_deactivationIntegrateCounter = 0xFF;
        m_deactivationNumInactiveFrames[0] = 0;
        m_deactivationNumInactiveFrames[1] = 0;
    }
}

// 0x00B711F0 (hkpmotion.inl)
hkSimdFloat32 hkpMotion::getMassInv() const
{
    return m_inertiaAndMassInv.getComponent<3>();
}

// 0x0080CB30, 0x0080CB40 (hkpmotion.inl)
const hkVector4f& hkpMotion::getCenterOfMassLocal() const
{
    return m_motionState.getSweptTransform().m_centerOfMassLocal;
}

const hkVector4f& hkpMotion::getCenterOfMassInWorld() const
{
    return m_motionState.getSweptTransform().m_centerOfMass1;
}

// 0x00E0D360, 0x00B62DD0 (hkpmotion.inl)
const hkMotionState* hkpMotion::getMotionState() const
{
    return &m_motionState;
}

hkMotionState* hkpMotion::getMotionState()
{
    return &m_motionState;
}

// 0x0080CB50, 0x0080CB60, 0x0080CB70 (hkpmotion.inl)
const hkVector4f& hkpMotion::getPosition() const
{
    return m_motionState.getTransform().getTranslation();
}

const hkQuaternionf& hkpMotion::getRotation() const
{
    return m_motionState.getSweptTransform().m_rotation1;
}

const hkTransformf& hkpMotion::getTransform() const
{
    return m_motionState.getTransform();
}

// 0x0080CB80, 0x0080CB90 (hkpmotion.inl)
const hkVector4f& hkpMotion::getLinearVelocity() const
{
    return m_linearVelocity;
}

const hkVector4f& hkpMotion::getAngularVelocity() const
{
    return m_angularVelocity;
}

// 0x0080CBA0 (hkpmotion.inl): v + w x (p - centerOfMass)
void hkpMotion::getPointVelocity(const hkVector4f& p, hkVector4f& pointVelOut) const
{
    const __m128 relPos = _mm_sub_ps(p.m_quad, getCenterOfMassInWorld().m_quad);
    hkVector4f cross;
    cross.setCross(m_angularVelocity, *(const hkVector4f*)&relPos);
    pointVelOut.m_quad = _mm_add_ps(cross.m_quad, m_linearVelocity.m_quad);
}

// 0x00DF28F0 (hkpmotion.inl)
int hkpMotion::getDeactivationClass()
{
    return m_motionState.m_deactivationClass;
}

// 0x00DDE6E0 (hkpmotion.inl): 0xFF is "deactivation disabled"
bool hkpMotion::isDeactivationEnabled() const
{
    return m_deactivationIntegrateCounter != 0xFF;
}

// 0x00DDE6F0 (hkpmotion.inl): the low 7 bits are the counter of the current select flag
int hkpMotion::getNumInactiveFrames(int select)
{
    return m_deactivationNumInactiveFrames[select] & 0x7F;
}

// 0x00E0AC30 (hkpmotion.inl): bits 0..6 and 7..13 hold two counters; bit 14 says which one belongs to the world's
// current select flag
int hkpMotion::getNumInactiveFramesMt(int select, int worldDeactivationNumInactiveFramesSelectFlag)
{
    const int dc = m_deactivationNumInactiveFrames[select];
    const int motionDeactivationFlag = dc >> 14;
    int numInactiveFrames;
    if (worldDeactivationNumInactiveFramesSelectFlag != motionDeactivationFlag)
    {
        numInactiveFrames = dc & 0x7F;
    }
    else
    {
        numInactiveFrames = (dc >> 7) & 0x7F;
    }
    return numInactiveFrames & 0x7F;
}

// 0x00B62DC0
hkpMotion::~hkpMotion()
{
}
