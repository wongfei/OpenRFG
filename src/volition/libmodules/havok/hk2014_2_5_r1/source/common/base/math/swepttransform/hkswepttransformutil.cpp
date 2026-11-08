// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/swepttransform/hkswepttransformutil.cpp
//
// Havok 2014.2.5 (havok_impl.md HK2): the out-of-line hkSweptTransformUtil functions of the implementation closure,
// from rfg.exe (the inline ones are in hkswepttransformfutil.inl). A swept transform keeps the start time in
// m_centerOfMass0.w and 1 / step length in m_centerOfMass1.w; zero there means not integrated.
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/swepttransform/hkswepttransformutil.h"

// 0x00CFBD00
void HK_CALL hkSweptTransformUtil::lerp2(const hkSweptTransformf& sweptTrans, hkSimdFloat32Parameter time, hkTransformf& transformOut)
{
    const hkSimdFloat32 t = sweptTrans.getInterpolationValue(time);
    _lerp2(sweptTrans, t, transformOut);
}

// 0x00CFBE90
void HK_CALL hkSweptTransformUtil::lerp2(const hkSweptTransformf& sweptTrans, hkTime time, hkTransformf& transformOut)
{
    lerp2(sweptTrans, hkSimdFloat32::fromFloat(time), transformOut);
}

// 0x00CFBEC0
void HK_CALL hkSweptTransformUtil::lerp2Ha(const hkSweptTransformf& sweptTrans, hkSimdFloat32Parameter t, hkSimdFloat32Parameter tAddOn, hkTransformf& transformOut)
{
    const hkSimdFloat32 ti = sweptTrans.getInterpolationValueHiAccuracy(t, tAddOn);
    _lerp2(sweptTrans, ti, transformOut);
}

// 0x00CFC050
void HK_CALL hkSweptTransformUtil::lerp2Ha(const hkSweptTransformf& sweptTrans, hkTime t, hkFloat32 tAddOn, hkTransformf& transformOut)
{
    lerp2Ha(sweptTrans, hkSimdFloat32::fromFloat(t), hkSimdFloat32::fromFloat(tAddOn), transformOut);
}

// 0x00CFC390
// Moves the end of the step back to time (relative position no less than HK_QUADREAL_EPS); the step gets
// correspondingly shorter (1 / t through the 23-bit reciprocal).
void HK_CALL hkSweptTransformUtil::backStepMotionState(hkSimdFloat32Parameter time, hkMotionState& motionState)
{
    hkSweptTransformf& st = motionState.getSweptTransform();
    hkSimdFloat32 t;
    t.setMax(st.getInterpolationValue(time), hkSimdFloat32_Eps);

    _lerp2(st, t, st.m_rotation1);

    hkSimdFloat32 invT;
    invT.setReciprocal(t);
    const hkSimdFloat32 invDeltaTime = invT * st.getInvDeltaTimeSr();
    st.m_centerOfMass1.setInterpolate(st.m_centerOfMass0, st.m_centerOfMass1, t);
    st.m_centerOfMass1.setW(invDeltaTime);

    motionState.m_deltaAngle.mul(t);
    calcTransAtT1(st, motionState.getTransform());
}

// 0x00CFC570
void HK_CALL hkSweptTransformUtil::backStepMotionState(hkTime time, hkMotionState& motionState)
{
    backStepMotionState(hkSimdFloat32::fromFloat(time), motionState);
}

// 0x00CFC5A0
// Ends the motion at time (not before the start of the step): both ends of the swept transform become the state at
// that time, and 1 / step length becomes zero.
void HK_CALL hkSweptTransformUtil::freezeMotionState(hkSimdFloat32Parameter time, hkMotionState& motionState)
{
    hkSweptTransformf& st = motionState.getSweptTransform();
    hkSimdFloat32 freezeTime;
    freezeTime.setMax(time, st.getBaseTimeSr());
    const hkSimdFloat32 t = st.getInterpolationValue(freezeTime);

    _lerp2(st, t, st.m_rotation1);
    st.m_rotation0 = st.m_rotation1;

    st.m_centerOfMass1.setInterpolate(st.m_centerOfMass0, st.m_centerOfMass1, t);
    st.m_centerOfMass0.setXYZ_W(st.m_centerOfMass1, freezeTime);
    st.m_centerOfMass1.zeroComponent<3>();

    calcTransAtT1(st, motionState.getTransform());
}

// 0x00CFC770
void HK_CALL hkSweptTransformUtil::freezeMotionState(hkTime time, hkMotionState& motionState)
{
    freezeMotionState(hkSimdFloat32::fromFloat(time), motionState);
}

// 0x00CFBA20
void HK_CALL hkSweptTransformUtil::setTimeInformation(hkSimdFloat32Parameter startTime, hkSimdFloat32Parameter invDeltaTime, hkMotionState& motionState)
{
    motionState.getSweptTransform().m_centerOfMass0.setW(startTime);
    motionState.getSweptTransform().m_centerOfMass1.setW(invDeltaTime);
}

// 0x00CFBA60
void HK_CALL hkSweptTransformUtil::warpTo(hkVector4fParameter position, hkQuaternionfParameter rotation, hkMotionState& ms)
{
    hkSweptTransformf& st = ms.getSweptTransform();
    ms.m_deltaAngle.setZero();
    st.m_rotation0 = rotation;
    st.m_rotation1 = rotation;
    ms.getTransform().setRotation(rotation);
    ms.getTransform().setTranslation(position);

    const hkSimdFloat32 baseTime = st.getBaseTimeSr();
    hkVector4f centerOfMass;
    centerOfMass._setTransformedPos(ms.getTransform(), st.m_centerOfMassLocal);
    st.m_centerOfMass0 = centerOfMass;
    st.m_centerOfMass1 = centerOfMass;
    st.m_centerOfMass0.setW(baseTime);
    st.m_centerOfMass1.zeroComponent<3>();
}

// 0x00CFBC40
void HK_CALL hkSweptTransformUtil::warpTo(const hkTransformf& transform, hkMotionState& ms)
{
    hkSweptTransformf& st = ms.getSweptTransform();
    ms.m_deltaAngle.setZero();
    hkQuaternionf rotation;
    rotation.set(transform.getRotation());
    ms.getTransform() = transform;
    st.m_rotation0 = rotation;
    st.m_rotation1 = rotation;

    const hkSimdFloat32 baseTime = st.getBaseTimeSr();
    hkVector4f centerOfMass;
    centerOfMass._setTransformedPos(transform, st.m_centerOfMassLocal);
    st.m_centerOfMass0 = centerOfMass;
    st.m_centerOfMass1 = centerOfMass;
    st.m_centerOfMass0.setW(baseTime);
    st.m_centerOfMass1.zeroComponent<3>();
}

// 0x00CFBB00
void HK_CALL hkSweptTransformUtil::warpToPosition(hkVector4fParameter position, hkMotionState& ms)
{
    hkSweptTransformf& st = ms.getSweptTransform();
    ms.m_deltaAngle.setZero();
    ms.getTransform().setTranslation(position);

    const hkSimdFloat32 baseTime = st.getBaseTimeSr();
    hkVector4f centerOfMass;
    centerOfMass._setRotatedDir(ms.getTransform().getRotation(), st.m_centerOfMassLocal);
    centerOfMass.add(position);
    st.m_centerOfMass0 = centerOfMass;
    st.m_centerOfMass1 = centerOfMass;
    st.m_rotation0 = st.m_rotation1;
    st.m_centerOfMass0.setW(baseTime);
    st.m_centerOfMass1.zeroComponent<3>();
}

// 0x00CFBB90
void HK_CALL hkSweptTransformUtil::warpToRotation(hkQuaternionfParameter rotation, hkMotionState& ms)
{
    warpTo(ms.getTransform().getTranslation(), rotation, ms);
}

// 0x00CFBBB0
// Moves the center of mass in local space; the world centers move with it and keep their time information.
void HK_CALL hkSweptTransformUtil::setCentreOfRotationLocal(hkVector4fParameter newCenterOfRotation, hkMotionState& motionState)
{
    hkSweptTransformf& st = motionState.getSweptTransform();
    hkVector4f offset;
    offset.setSub(newCenterOfRotation, st.m_centerOfMassLocal);
    st.m_centerOfMassLocal = newCenterOfRotation;

    hkVector4f offsetWs;
    offsetWs._setRotatedDir(motionState.getTransform().getRotation(), offset);

    const hkSimdFloat32 baseTime = st.getBaseTimeSr();
    const hkSimdFloat32 invDeltaTime = st.getInvDeltaTimeSr();
    st.m_centerOfMass0.add(offsetWs);
    st.m_centerOfMass1.add(offsetWs);
    st.m_centerOfMass0.setW(baseTime);
    st.m_centerOfMass1.setW(invDeltaTime);
}

// 0x0134D0E0: the largest velocity a motion state step keeps, 1e6 (0x49742400) in every lane
HK_ALIGN16(const hkQuadFloat32 hkSweptTransformUtil::_stepMotionStateMaxVelf) = { 1000000.0f, 1000000.0f, 1000000.0f, 1000000.0f };

// 0x00CFBA00
void HK_CALL hkSweptTransformUtil::setTimeInformation(hkTime startTime, hkFloat32 invDeltaTime, hkMotionState& motionState)
{
    motionState.getSweptTransform().m_centerOfMass0.setW(hkSimdFloat32::fromFloat(startTime));
    motionState.getSweptTransform().m_centerOfMass1.setW(hkSimdFloat32::fromFloat(invDeltaTime));
}
