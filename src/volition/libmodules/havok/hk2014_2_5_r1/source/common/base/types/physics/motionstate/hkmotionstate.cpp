// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/physics/motionstate/hkmotionstate.cpp
//
// Havok 2014.2.5 (havok_impl.md HK2): hkMotionState::initMotionState from rfg.exe. The copy assignment (0x00DF1140)
// is the implicit one.
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/physics/motionstate/hkmotionstate.h"

// 0x00FBEAB0
// The damping and time factor halves are set to 0 (storage 0, not 1), the velocity limits to 0 (hkUFloat8).
void hkMotionState::initMotionState(const hkVector4& position, const hkQuaternion& rotation)
{
    m_sweptTransform.initSweptTransform(position, rotation);
    m_transform.setRotation(rotation);
    m_transform.setTranslation(position);
    m_deltaAngle.setZero();
    m_objectRadius = 1.0f;
    m_linearDamping.setZero();
    m_angularDamping.setZero();
    m_timeFactor.setZero();
    m_maxLinearVelocity = 0.0f;
    m_maxAngularVelocity = 0.0f;
    m_deactivationClass = 0;
}
