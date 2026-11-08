// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/utilities/dynamics/keyframe/hkpkeyframeutility.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/quaternion/hkquaternionf.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkvector4f.h"

class hkpRigidBody;

// size 0x1
class hkpKeyFrameUtility
{
public:
    // size 0x40
    struct KeyFrameInfo
    {
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(KeyFrameInfo);
        void setUsingPositionOrientationPair(const hkVector4f& currentPosition, const hkQuaternionf& currentOrientation, const hkVector4f& nextPosition, const hkQuaternionf& nextOrientation, float invDeltaTime); // 0x00F1FBF0
        void fastSetUsingPositionOrientationPair(const hkVector4f& currentPosition, const hkQuaternionf& currentOrientation, const hkVector4f& nextPosition, const hkQuaternionf& nextOrientation, float invDeltaTime); // 0x00F1F730

        hkVector4f m_position; // 0x00
        hkQuaternionf m_orientation; // 0x10
        hkVector4f m_linearVelocity; // 0x20
        hkVector4f m_angularVelocity; // 0x30
    };

    // size 0x50
    struct AccelerationInfo
    {
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(AccelerationInfo);
        AccelerationInfo(); // 0x00F1F830

        hkVector4f m_linearPositionFactor; // 0x00
        hkVector4f m_angularPositionFactor; // 0x10
        hkVector4f m_linearVelocityFactor; // 0x20
        hkVector4f m_angularVelocityFactor; // 0x30
        float m_maxLinearAcceleration; // 0x40
        float m_maxAngularAcceleration; // 0x44
        float m_maxAllowedDistance; // 0x48
    };

    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpKeyFrameUtility);
    static void applySoftKeyFrame(const KeyFrameInfo& keyFrameInfo, AccelerationInfo& accelInfo, float deltaTime, float invDeltaTime, hkpRigidBody* body); // 0x00F1F870
    static void applyHardKeyFrame(const hkVector4f& nextPosition, const hkQuaternionf& nextOrientation, float invDeltaTime, hkpRigidBody* body); // 0x00F1FDD0
    static void applyHardKeyFrameAsynchronously(const hkVector4f& nextPosition, const hkQuaternionf& nextOrientation, float invDeltaTime, hkpRigidBody* body); // 0x00F200A0

};
