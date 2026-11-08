// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/swepttransform/hkswepttransformutil.h
//
// Havok 2014.2.5 (havok_impl.md HK2): ported from Havok 2014.1 (Common/Base/Math/SweptTransform/hkSweptTransformfUtil.h), checked against rfg.exe.
#ifndef HK_MATH_SWEPT_TRANSFORMf_UTIL_H
#define HK_MATH_SWEPT_TRANSFORMf_UTIL_H

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/hkmath.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/physics/motionstate/hkmotionstate.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/physics/hkstepinfo.h"

namespace hkSweptTransformUtil
{
        HK_FORCE_INLINE void HK_CALL _lerp2( const hkSweptTransformf& sweptTrans, hkFloat32 t, hkQuaternionf& quatOut );
        HK_FORCE_INLINE void HK_CALL _lerp2( const hkSweptTransformf& sweptTrans, hkSimdFloat32Parameter t, hkQuaternionf& quatOut ); // 0x00CFCB20
        HK_FORCE_INLINE void HK_CALL _lerp2( const hkSweptTransformf& sweptTrans, hkFloat32 t, hkTransformf& transformOut );
        HK_FORCE_INLINE void HK_CALL _lerp2( const hkSweptTransformf& sweptTrans, hkSimdFloat32Parameter t, hkTransformf& transformOut ); // 0x00CFCC40
        void HK_CALL lerp2( const hkSweptTransformf& sweptTrans, hkTime time, hkTransformf& transformOut ); // 0x00CFBE90
        void HK_CALL lerp2( const hkSweptTransformf& sweptTrans, hkSimdFloat32Parameter time, hkTransformf& transformOut ); // 0x00CFBD00

            // internal function: approximate a transform given an interpolation time with a higher accuracy time input
        void HK_CALL lerp2Ha( const hkSweptTransformf& sweptTrans, hkTime t, hkFloat32 tAddOn, hkTransformf& transformOut ); // 0x00CFC050
        void HK_CALL lerp2Ha( const hkSweptTransformf& sweptTrans, hkSimdFloat32Parameter t, hkSimdFloat32Parameter tAddOn, hkTransformf& transformOut ); // 0x00CFBEC0
        void HK_CALL lerp2Rel( const hkSweptTransformf& sweptTrans, hkFloat32 r, hkTransformf& transformOut ); // 0x00CFC210
        void HK_CALL lerp2Rel( const hkSweptTransformf& sweptTrans, hkSimdFloat32Parameter r, hkTransformf& transformOut ); // 0x00CFC0A0
        HK_FORCE_INLINE void HK_CALL calcTransAtT0( const hkSweptTransformf& sweptTrans, hkTransformf& transformOut );
        HK_FORCE_INLINE void HK_CALL calcTransAtT1( const hkSweptTransformf& sweptTrans, hkTransformf& transformOut ); // 0x00CFCA90

#    if defined(HK_REAL_IS_FLOAT)
        void HK_CALL setCentreOfRotationLocal( hkVector4fParameter newCenterOfRotation, hkMotionState& sweptTrans); // 0x00CFBBB0
        HK_FORCE_INLINE void HK_CALL _stepMotionState( const hkStepInfo& stepInfo, hkVector4f& linearVelocity, hkVector4f& angularVelocity, hkMotionState& motionState ); // 0x00E129B0
        HK_FORCE_INLINE void HK_CALL _clipVelocities( const hkMotionState& motionState, hkVector4f& linearVelocity, hkVector4f& angularVelocity ); // 0x00E12810
        void HK_CALL keyframeMotionState( const hkStepInfo& stepInfo, hkVector4fParameter pos1, hkQuaternionfParameter rot1, hkMotionState& motionState ); // 0x00CFC7A0
        HK_FORCE_INLINE void HK_CALL calcCenterOfMassAt( const hkMotionState& ms, hkTime t, hkVector4f& centerOut ); // 0x00E125A0
        HK_FORCE_INLINE void HK_CALL calcCenterOfMassAt( const hkMotionState& ms, hkSimdFloat32Parameter t, hkVector4f& centerOut ); // 0x00E12560
        HK_FORCE_INLINE void HK_CALL getVelocity( const hkMotionState& ms, hkVector4f& linearVelOut, hkVector4f& angularVelOut ); // 0x00B73810 (hkswepttransformfutil.inl:309)

            // interpolates the information of t1 to be at time t.
        void HK_CALL backStepMotionState( hkTime t, hkMotionState& motionState ); // 0x00CFC570
        void HK_CALL backStepMotionState( hkSimdFloat32Parameter t, hkMotionState& motionState ); // 0x00CFC390

            // Internal function. Only used upon deactivation of entities
        void HK_CALL freezeMotionState( hkTime time, hkMotionState& motionState ); // 0x00CFC770
        void HK_CALL freezeMotionState( hkSimdFloat32Parameter time, hkMotionState& motionState ); // 0x00CFC5A0

            // Call this if you deactivate a body (otherwise numerical problems can result in strange extrapolation of body movement)
        HK_FORCE_INLINE void HK_CALL deactivate( hkMotionState& ms );

            // Internal function. Only used upon activation of entities
        void HK_CALL setTimeInformation( hkTime startTime, hkFloat32 invDeltaTime, hkMotionState& motionState); // 0x00CFBA00
        void HK_CALL setTimeInformation( hkSimdFloat32Parameter startTime, hkSimdFloat32Parameter invDeltaTime, hkMotionState& motionState); // 0x00CFBA20
        void HK_CALL warpToPosition( hkVector4fParameter position, hkMotionState& ms ); // 0x00CFBB00
        void HK_CALL warpToRotation( hkQuaternionfParameter rotation, hkMotionState& ms ); // 0x00CFBB90
        void HK_CALL warpTo( hkVector4fParameter position, hkQuaternionfParameter rotation, hkMotionState& ms ); // 0x00CFBA60
        void HK_CALL warpTo( const hkTransformf& transform, hkMotionState& ms ); // 0x00CFBC40

            // For a given delta time: Calc the relative linear movement (xyz) and the worst case angular movment (w)
        HK_FORCE_INLINE void HK_CALL calcTimInfo( const hkMotionState& ms0, const hkMotionState& ms1, hkFloat32 deltaTime, hkVector4f& timOut); // 0x00D78A50

            // calc the relative angular movement for a reduced step
        HK_FORCE_INLINE void HK_CALL calcAngularTimInfo( const hkMotionState& ms0, const hkMotionState& ms1, hkFloat32 deltaTime, hkVector4f* deltaAngleOut0, hkVector4f* deltaAngleOut1 ); // 0x00E87F30
#endif // defined(HK_REAL_IS_FLOAT)

        extern const hkQuadFloat32 _stepMotionStateMaxVelf;
}

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/swepttransform/hkswepttransformfutil.inl"

#endif // HK_MATH_SWEPT_TRANSFORMf_UTIL_H
