// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/physics/motionstate/hkmotionstate.h
//
// Havok 2014.2.5 (havok_impl.md HK2): ported from Havok 2014.1 (Common/Base/Types/Physics/MotionState/hkMotionState.h), checked against rfg.exe.
#ifndef HK_MATH_MOTION_STATE_H
#define HK_MATH_MOTION_STATE_H

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/hkmath.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/swepttransform/hkswepttransformf.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkufloat8.h"

extern const class hkClass hkMotionStateClass;

class hkMotionState
{
    // +version(3)
    public:

        HK_DECLARE_REFLECTION(); // staticClass 0x00CEB5F0
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkMotionState);
        inline hkMotionState(); // 0x00DC6BD0
        void initMotionState( const hkVector4& position, const hkQuaternion& rotation ); // 0x00FBEAB0
        inline hkTransform& getTransform(); // 0x0080CB00 (hkmotionstate.inl:15)
        inline const hkTransform& getTransform() const; // 0x0080CB10 (hkmotionstate.inl:20)
        inline const hkSweptTransform& getSweptTransform() const; // 0x0080CB20 (hkmotionstate.inl:26)
        inline hkSweptTransform& getSweptTransform(); // 0x00B62DB0 (hkmotionstate.inl:32)
    public: // protected in Havok; public as the PDB-generated headers had it (Volition and the tests read the members)

        hkTransform m_transform;

            // a swept transform
        class hkSweptTransform m_sweptTransform; //+overridetype(hkVector4[5])
    public:

        hkVector4 m_deltaAngle;
        hkReal m_objectRadius;
        hkHalf    m_linearDamping;
        hkHalf    m_angularDamping;
        hkHalf    m_timeFactor;
        hkUFloat8 m_maxLinearVelocity;
        hkUFloat8 m_maxAngularVelocity;

            // Note: the next variables actually belong to hkpMotion, however they can cheaply put here (padding!)
            // The deactivation class of the motion state as used internally
        hkUint8    m_deactivationClass;

};

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/physics/motionstate/hkmotionstate.inl"

#endif // HK_MATH_MOTION_STATE_H
