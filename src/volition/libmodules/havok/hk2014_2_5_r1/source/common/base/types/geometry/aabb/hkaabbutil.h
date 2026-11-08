// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/geometry/aabb/hkaabbutil.h
//
// Havok 2014.2.5 (havok_impl.md HK2): ported from Havok 2014.1 (Common/Base/Types/Geometry/Aabb/hkAabbUtil.h), checked against rfg.exe.
#ifndef HK_COLLIDE2_AABB_UTIL_H
#define HK_COLLIDE2_AABB_UTIL_H

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/hkmath.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/physics/motionstate/hkmotionstate.h"    // needed for .inl file
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkvector4util.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/geometry/aabb/hkaabb.h"

struct hkAabbUint32;

namespace hkAabbUtil
{
        //
        // Non-continuous AABB calculations
        //

        void HK_CALL                calcAabb( const hkReal* vertexArray, int numVertices, int striding, hkAabb& aabbOut ); // 0x00FB91B0
        void HK_CALL                calcAabb( const hkVector4* vertexArray, int numVertices, hkAabb& aabbOut ); // 0x00FB8D70
        void HK_CALL                calcAabb( const hkVector4*const* vertexArray, int numVertices, hkAabb& aabbOut ); // 0x00FB8DE0
        HK_FORCE_INLINE void HK_CALL        calcAabb( const hkTransform& BvToWorld, const hkVector4& halfExtents, const hkVector4& center, hkSimdRealParameter extraRadius, hkAabb& aabbOut); // 0x0071B7E0 (hkaabbutil.inl:41)
        HK_FORCE_INLINE void HK_CALL        calcAabb( const hkTransform& BvToWorld, const hkVector4& halfExtents, const hkVector4& center, hkAabb& aabbOut); // 0x00FB96F0
        HK_FORCE_INLINE void HK_CALL        calcAabb( const hkTransform& BvToWorld, const hkVector4& halfExtents, hkSimdRealParameter extraRadius, hkAabb& aabbOut); // 0x00D825A0
        void HK_CALL    calcAabb(const hkTransform& BvToWorld, const hkAabb& aabb, hkSimdRealParameter extraRadius, hkAabb& aabbOut); // 0x00FB8E60
        void HK_CALL    calcAabb(const hkTransform& BvToWorld, const hkAabb& aabb, hkAabb& aabbOut); // 0x00FB8F20
        void HK_CALL                calcAabb(const hkQsTransform& bvToWorld, const hkAabb& aabb, hkAabb& aabbOut); // 0x00FB8FE0
        void HK_CALL                calcAabb(const hkQTransform& bvToWorld, const hkAabb& aabb, hkAabb& aabbOut); // 0x00FB90D0
        HK_FORCE_INLINE hkSimdReal HK_CALL calcMinGap( const hkAabb& innerAabb, const hkAabb& outerAabb );
        HK_FORCE_INLINE void HK_CALL        calcAabb( const hkVector4& center, hkSimdRealParameter innerRadius, hkAabb& aabbOut); // 0x00FF4040
        HK_FORCE_INLINE void HK_CALL        transformAabbIntoLocalSpace( const hkTransform& localToWorld, const hkAabb& aabb, hkAabb& aabbOut );
        HK_FORCE_INLINE hkSimdReal HK_CALL    getOuterRadius(const hkAabb& aabb); // 0x01014DB0
        HK_FORCE_INLINE hkSimdReal            distanceSquared(const hkAabb& a, hkVector4Parameter b);
        HK_FORCE_INLINE void                getVertex(const hkAabb& aabb, int index, hkVector4& vertexOut);
        HK_FORCE_INLINE void                get8Vertices(const hkAabb& aabb, hkVector4* verticesOut);
        HK_FORCE_INLINE void        scaleAabb(const hkVector4& scale, const hkAabb& aabbIn, hkAabb& aabbOut); // 0x00E72B30
        HK_FORCE_INLINE void        expandAabbByMotion(const hkAabb& aabbIn, const hkVector4& motion, hkSimdRealParameter anyDirectionExpansion, hkAabb& aabbOut);
        HK_FORCE_INLINE void        expandAabbByMotion(const hkAabb& aabbIn, const hkVector4& motion, hkAabb& aabbOut); // 0x00E3AA30
        HK_FORCE_INLINE void        expandAabbByMotion(const hkAabb& aabbIn, hkVector4Parameter motionA, hkVector4Parameter motionB, hkSimdRealParameter anyDirectionExpansion, hkAabb& aabbOut);
        HK_FORCE_INLINE void        expandAabbByMotionCircle(const hkAabb& aabbIn, hkVector4Parameter motionA, hkVector4Parameter motionB, hkSimdRealParameter anyDirectionExpansion, hkAabb& aabbOut);
        HK_FORCE_INLINE void        projectAabbOnAxis(const hkVector4& axis, const hkAabb& aabb, hkSimdReal& minOut, hkSimdReal& maxOut); // 0x00E3BEF0
        HK_FORCE_INLINE void        projectAabbMinOnAxis(const hkVector4& axis, const hkAabb& aabb, hkSimdReal& prjOut); // 0x00E3BF50
        HK_FORCE_INLINE void        projectAabbMaxOnAxis(const hkVector4& axis, const hkAabb& aabb, hkSimdReal& prjOut);
        HK_FORCE_INLINE void        computeAabbPlaneSpan(const hkVector4& plane, const hkAabb& aabb, hkSimdReal& minOut, hkSimdReal& maxOut); // 0x00E3BF90
        HK_FORCE_INLINE void HK_CALL convertAabbToUint32( const hkAabb& aabb, hkVector4fParameter offsetLow, hkVector4fParameter offsetHigh, hkVector4fParameter scale, hkAabbUint32& aabbOut ); // 0x00D8EED0
        HK_FORCE_INLINE void HK_CALL convertAabbFromUint32( const hkAabbUint32& aabbIn, hkVector4fParameter offsetLow, hkVector4fParameter scale, hkAabb& aabbOut);
        HK_FORCE_INLINE void HK_CALL compressExpandedAabbUint32(const hkAabbUint32& expandedAabbInt, hkAabbUint32& unexpandedAabbInt_InOut); // 0x00E05850
        HK_FORCE_INLINE void HK_CALL uncompressExpandedAabbUint32(const hkAabbUint32& unexpandedAabbInt, hkAabbUint32& expandedAabbOut); // 0x00DB9970
        enum { AABB_UINT32_MAX_FVALUE = 0x7ffe0000 };
        extern hkQuadReal hkAabbUint32MaxVal;

        //
        // Continuous AABB calculations
        //

        struct OffsetAabbInput
        {
            HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkAabbUtil::OffsetAabbInput);
            OffsetAabbInput() {} // 0x00DB93A0
            hkPadSpu<const hkMotionState*> m_motionState;

            // intermediate transforms
            hkTransform m_transforms[4];
            hkPadSpu<int> m_numTransforms;

            // start-end transforms
            hkTransform m_startTransform;
            hkTransform m_endTransformInv;
        };
        HK_FORCE_INLINE void HK_CALL sweepAabb(const hkMotionState* motionstate, hkReal tolerance, const hkAabb& aabbIn, hkAabb& aabbOut); // 0x00E149D0
        void HK_CALL initOffsetAabbInput(const hkMotionState* motionstate, OffsetAabbInput& input); // 0x00FB9240
        void HK_CALL sweepOffsetAabb(const OffsetAabbInput& input, const hkAabb& aabbIn, hkAabb& aabbOut); // 0x00DB97A0
}

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/geometry/aabb/hkaabbutil.inl"

#endif // HK_COLLIDE2_AABB_UTIL_H
