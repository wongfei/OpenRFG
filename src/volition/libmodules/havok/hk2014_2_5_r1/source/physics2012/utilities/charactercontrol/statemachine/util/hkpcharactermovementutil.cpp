// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/utilities/charactercontrol/statemachine/util/hkpcharactermovementutil.cpp
//
// Havok 2014.2.5 (player_minimal.md P1): the character's velocity change towards a desired velocity, from rfg.exe.
// The work is done in the surface frame (forward along the surface, right, surface normal), relative to the
// surface velocity; the change is the gain share of the difference, limited to m_maxVelocityDelta.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/utilities/charactercontrol/statemachine/util/hkpcharactermovementutil.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/matrix/hkmatrix3f.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hksimdfloat32.h"

// 0x00814BE0 (hkpcharactermovementutil.h:32)
hkpCharacterMovementUtil::hkpMovementUtilInput::hkpMovementUtilInput()
{
    m_desiredVelocitySpace.m_storage = hkpMovementUtilInput::SURFACE_SPACE;
}

// 0x00F20890: a forward vector parallel to up gives no surface frame, and the velocity is left as it is
void hkpCharacterMovementUtil::calculateMovement(const hkpMovementUtilInput& input, hkVector4f& velocityOut)
{
    hkVector4f perpToUp;
    perpToUp.setCross(input.m_forward, input.m_up);
    // ucomiss/jc: NaN returns
    if (!(perpToUp.lengthSquared<3>().getReal() >= hkSimdFloat32::getConstant<HK_QUADREAL_EPS>().getReal()))
    {
        return;
    }
    perpToUp.normalize<3, HK_ACC_23_BIT, HK_SQRT_SET_ZERO>();

    hkVector4f surfaceForward;
    surfaceForward.setCross(perpToUp, input.m_surfaceNormal);
    surfaceForward.normalize<3, HK_ACC_23_BIT, HK_SQRT_SET_ZERO>();
    hkVector4f surfaceRight;
    surfaceRight.setCross(surfaceForward, input.m_surfaceNormal);
    surfaceRight.normalize<3, HK_ACC_23_BIT, HK_SQRT_SET_ZERO>();
    hkMatrix3f surfaceFrame;
    surfaceFrame.setCols(surfaceForward, surfaceRight, input.m_surfaceNormal);

    // the current velocity relative to the surface, in the surface frame
    hkVector4f relVelocitySF;
    relVelocitySF.setSub(input.m_currentVelocity, input.m_surfaceVelocity);
    relVelocitySF.setRotatedInverseDir(surfaceFrame, relVelocitySF);

    const hkSimdFloat32 zero = hkSimdFloat32::getConstant<HK_QUADREAL_0>();
    hkVector4f desiredVelocitySF;
    if (input.m_desiredVelocitySpace.m_storage != hkpMovementUtilInput::SURFACE_SPACE)
    {
        // world space: the desired velocity in the character's (forward, right) frame, put on the surface with its
        // length kept
        hkVector4f right;
        right.setCross(input.m_up, input.m_forward);
        const hkSimdFloat32 forwardVel = input.m_desiredVelocity.dot<3>(input.m_forward);
        const hkSimdFloat32 rightVel = input.m_desiredVelocity.dot<3>(right);
        const hkSimdFloat32 desiredSpeed = input.m_desiredVelocity.length<3, HK_ACC_23_BIT, HK_SQRT_SET_ZERO>();
        desiredVelocitySF.set(zero - forwardVel, rightVel, zero, zero);
        desiredVelocitySF.normalize<3, HK_ACC_23_BIT, HK_SQRT_SET_ZERO>();
        desiredVelocitySF.mul(desiredSpeed);
    }
    else
    {
        desiredVelocitySF = input.m_desiredVelocity;
    }

    hkVector4f diffSF;
    diffSF.setSub(desiredVelocitySF, relVelocitySF);
    const hkSimdFloat32 maxVelocityDelta = hkSimdFloat32::fromFloat(input.m_maxVelocityDelta);
    const hkSimdFloat32 gain = hkSimdFloat32::fromFloat(input.m_gain);
    const hkSimdFloat32 diffLen2 = diffSF.lengthSquared<3>();
    hkSimdFloat32 scale;
    if ((diffLen2 * gain * gain).getReal() > (maxVelocityDelta * maxVelocityDelta).getReal())
    {
        scale = diffLen2.sqrtInverse<HK_ACC_23_BIT, HK_SQRT_SET_ZERO>() * maxVelocityDelta;
    }
    else
    {
        scale = gain;
    }
    relVelocitySF.addMul(scale, diffSF);

    velocityOut.setRotatedDir(surfaceFrame, relVelocitySF);
    velocityOut.add(input.m_surfaceVelocity);
}
