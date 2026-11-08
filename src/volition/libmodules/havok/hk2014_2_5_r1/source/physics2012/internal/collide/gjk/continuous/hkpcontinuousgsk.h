// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/gjk/continuous/hkpcontinuousgsk.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0); the structs (layouts from rfg.pdb) and the
// helpers of hkpcontinuousgsk.cpp added by hand (HK6). Hand-maintained.
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/hkmath.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"

class hkMotionState;
class hkStepInfo;
class hkpConvexShape;
class hkpGsk;
class hkpGskCache;
struct hkpAgent3ProcessInput;
struct hkpProcessCollisionOutput;

// size 0x14: the tolerances of the continuous GSK, in Rt units (the fraction of the step) where noted
struct hkp4dGskTolerances
{
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkp4dGskTolerances);

    hkPadSpu<float> m_toiSeparation; // 0x00
    hkPadSpu<float> m_minSeparation; // 0x04
    hkPadSpu<float> m_minSafeDeltaRt; // 0x08
    hkPadSpu<float> m_minToiDeltaTime; // 0x0C
    hkPadSpu<float> m_toiAccuracy; // 0x10
};

// size 0x90: the vertices of A (collision spheres) moving against B's separating plane
struct alignas(16) hkp4dGskVertexCollidePointsInput
{
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkp4dGskVertexCollidePointsInput);

    hkPadSpu<const hkMotionState*> m_motionA; // 0x00
    hkPadSpu<const hkMotionState*> m_motionB; // 0x04
    hkPadSpu<hkVector4f*> m_verticesA; // 0x08
    hkPadSpu<int> m_numVertices; // 0x0C
    hkPadSpu<int> m_allocatedNumVertices; // 0x10
    hkPadSpu<float> m_radiusSum; // 0x14
    hkPadSpu<float> m_maxAccel; // 0x18
    hkPadSpu<float> m_invMaxAccel; // 0x1C
    hkVector4f m_planeB; // 0x20
    hkVector4f m_pointOnPlaneB; // 0x30
    hkPadSpu<const hkStepInfo*> m_stepInfo; // 0x40
    hkPadSpu<float> m_worstCaseApproachingDelta; // 0x44
    hkVector4f m_linearTimInfo; // 0x50
    hkVector4f m_deltaAngles[2]; // 0x60
    hkPadSpu<float> m_startRt; // 0x80
};

// size 0x20
struct alignas(16) hkp4dGskVertexCollidePointsOutput
{
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkp4dGskVertexCollidePointsOutput);

    hkPadSpu<float> m_Rtoi; // 0x00
    hkVector4f m_collidingPoint; // 0x10
};

// size 0x130: the transforms and velocities at one Rt, in A's space
struct alignas(16) hk4dGskCollideMatrices
{
    hkTransformf m_wTa; // 0x00
    hkTransformf m_wTb; // 0x40
    hkTransformf m_aTb; // 0x80
    hkVector4f m_plane; // 0xC0
    hkVector4f m_pointOnPlane; // 0xD0
    hkVector4f m_linearVel; // 0xE0
    hkVector4f m_angularVel[2]; // 0xF0
    hkVector4f m_massCenterAinA; // 0x110
    hkVector4f m_massCenterBinA; // 0x120
};

void hk4dGskCollidePointsWithPlane(const hkp4dGskVertexCollidePointsInput& input, const hkp4dGskTolerances& tol, hkp4dGskVertexCollidePointsOutput& out); // 0x00E84D80
void hk4dGskCollideCalcToi(const hkpAgent3ProcessInput& in3, const hkSimdFloat32& allowedPenetrationDepth, const hkSimdFloat32& minSeparation, const hkSimdFloat32& toiSeparation, hkpGskCache& gskCache, hkVector4f& separatingNormal, hkpProcessCollisionOutput& out); // 0x00E86570

// The helpers (file statics in Havok; out-of-line in the binary, most with LTCG register arguments). Only
// hk4dGskCollideInitMatrices, hk4dGskCollidePointWithPlaneFinal and hk4dGskCalcPointVelocityAssumingAngularVelError
// are called; the others are copies of code inlined into hk4dGskCollideCalcToi.
void hk4dGskCollideInitMatrices(const hkp4dGskVertexCollidePointsInput& input, const hkSimdFloat32& Rt, hk4dGskCollideMatrices& out); // 0x00E84960
void hk4dGskCollidePointWithPlaneFinal(const hkp4dGskVertexCollidePointsInput& input, const hkp4dGskTolerances& tol, const hkVector4f& vertexA, const hkSimdFloat32& startRtIn, const hkSimdFloat32& endRtIn, const hkSimdFloat32& distance0In, const hkSimdFloat32& distance1In, hkp4dGskVertexCollidePointsOutput& out, hkSimdFloat32& toiOut); // 0x00E84B40
void hk4dGskCalcSeparatingNormal(const hkTransformf& tA, const hkTransformf& tB, const hkpConvexShape* shapeA, const hkpConvexShape* shapeB, hkpGsk& gsk, hkVector4f& separatingNormalOut, hkVector4f& pointOnBInB); // 0x00E85100
void hk4dGskCalcPointVelocity(const hkMotionState& msA, const hkMotionState& msB, const hkVector4f& separatingPoint, const hkVector4f& separatingNormal, const hkVector4f& linearTimInfo, const hkVector4f* deltaAngles, const hkSimdFloat32& t, hkSimdFloat32& separatingVelocity); // 0x00E85230
void hk4dGskCalcPointVelocityAssumingAngularVelError(const hkMotionState& msA, const hkMotionState& msB, const hkVector4f& separatingPoint, const hkVector4f& separatingNormal, const hkVector4f& linearTimInfo, const hkVector4f* deltaAngles, const hkSimdFloat32& t, hkSimdFloat32& separatingVelocity); // 0x00E85320
void hk4dGskCollPInputInitialize(const hkpAgent3ProcessInput& in3, hkp4dGskVertexCollidePointsInput& collPInput); // 0x00E85490
void hk4dGskCalcWorstCaseProjectedVelocity(const hkMotionState& msA, const hkMotionState& msB, const hkVector4f& separatingNormal, const hkVector4f& linearTimInfo, const hkVector4f* deltaAngles, hkSimdFloat32& realProjectedLinearDelta, hkSimdFloat32& projectedAngularDelta); // 0x00E85590
void hk4dGskCalcToiFinal(const hkpAgent3ProcessInput& in3, const hkp4dGskTolerances& tol, hkpGsk& gsk, int featureChange, const hkVector4f& oldMasterNormal, const hkVector4f* deltaAngles, const hkSimdFloat32& startRtIn, const hkSimdFloat32& endRtIn, const hkSimdFloat32& distance0In, const hkSimdFloat32& distance1In, hkpProcessCollisionOutput& out); // 0x00E856D0
