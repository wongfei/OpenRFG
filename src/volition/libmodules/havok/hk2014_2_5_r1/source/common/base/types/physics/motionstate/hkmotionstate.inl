// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/physics/motionstate/hkmotionstate.inl
//
// Havok 2014.2.5 (havok_impl.md HK2): ported from Havok 2014.1 (Common/Base/Types/Physics/MotionState/hkMotionState.inl), checked against rfg.exe.
hkMotionState::hkMotionState()
{
}

hkTransform& hkMotionState::getTransform()
{
    return m_transform;
}

const hkTransform& hkMotionState::getTransform() const
{
    return m_transform;
}

const hkSweptTransform& hkMotionState::getSweptTransform() const
{
    return m_sweptTransform;
}

hkSweptTransform& hkMotionState::getSweptTransform()
{
    return m_sweptTransform;
}
