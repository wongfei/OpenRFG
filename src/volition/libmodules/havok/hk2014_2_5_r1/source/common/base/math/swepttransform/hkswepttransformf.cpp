// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/swepttransform/hkswepttransformf.cpp
//
// Havok 2014.2.5 (havok_impl.md HK2): the out-of-line hkSweptTransformf functions of the implementation closure,
// from rfg.exe.
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/swepttransform/hkswepttransformf.h"

// 0x00FBEB70
// Both ends at position (time information zero), the same rotation, the center of mass at the origin.
void hkSweptTransformf::initSweptTransform(hkVector4fParameter position, hkQuaternionfParameter rotation)
{
    m_centerOfMass0 = position;
    m_centerOfMass0.zeroComponent<3>();
    m_centerOfMass1 = position;
    m_centerOfMass1.zeroComponent<3>();
    m_rotation0 = rotation;
    m_rotation1 = rotation;
    m_centerOfMassLocal.setZero();
}
