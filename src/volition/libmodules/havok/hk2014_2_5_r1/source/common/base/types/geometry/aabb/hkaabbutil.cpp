// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/geometry/aabb/hkaabbutil.cpp
//
// Havok 2014.2.5 (havok_impl.md HK2): the out-of-line hkAabbUtil functions of the implementation closure, from rfg.exe
// (the inline ones are in hkaabbutil.inl).
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/geometry/aabb/hkaabbutil.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/geometry/aabb/hkaabb.h"

// 0x01A35500: the largest value convertAabbToUint32 produces, 2^31 - 2^17 (0x4EFFFC00) in every lane.
hkQuadReal hkAabbUtil::hkAabbUint32MaxVal = { 2147352576.0f, 2147352576.0f, 2147352576.0f, 2147352576.0f };

// 0x00FB91B0
// striding is in bytes; w of both corners is cleared, except for an empty array (corners +-HK_QUADREAL_MAX in all
// four components).
void HK_CALL hkAabbUtil::calcAabb(const hkReal* vertexArray, int numVertices, int striding, hkAabb& aabbOut)
{
    aabbOut.m_min = hkVector4f::getConstant<HK_QUADREAL_MAX>();
    aabbOut.m_max.setNeg<4>(aabbOut.m_min);
    if (numVertices <= 0)
    {
        return;
    }
    const char* p = reinterpret_cast<const char*>(vertexArray);
    for (int i = 0; i < numVertices; i++)
    {
        // x, y, z, 0 (movsd + movss)
        const hkReal* f = reinterpret_cast<const hkReal*>(p);
        hkVector4f v;
        v.m_quad = _mm_movelh_ps(_mm_castpd_ps(_mm_load_sd(reinterpret_cast<const double*>(f))), _mm_load_ss(f + 2));
        aabbOut.m_min.setMin(aabbOut.m_min, v);
        aabbOut.m_max.setMax(aabbOut.m_max, v);
        p += striding;
    }
    aabbOut.m_min.zeroComponent<3>();
    aabbOut.m_max.zeroComponent<3>();
}

// 0x00FB8F20
// The box of a transformed box: center transformed, half extents through |rotation| (each column's product made
// positive, then summed).
void HK_CALL hkAabbUtil::calcAabb(const hkTransform& BvToWorld, const hkAabb& aabb, hkAabb& aabbOut)
{
    hkVector4f center;
    center.setAdd(aabb.m_min, aabb.m_max);
    center.mul(hkSimdFloat32_Inv2);
    hkVector4f halfExtents;
    halfExtents.setSub(aabb.m_max, aabb.m_min);
    halfExtents.mul(hkSimdFloat32_Inv2);

    const hkRotationf& r = BvToWorld.getRotation();
    hkVector4f ex, ey, ez;
    ex.setMul(halfExtents.getComponent<0>(), r.getColumn<0>());
    ey.setMul(halfExtents.getComponent<1>(), r.getColumn<1>());
    ez.setMul(halfExtents.getComponent<2>(), r.getColumn<2>());
    ex.setAbs(ex);
    ey.setAbs(ey);
    ez.setAbs(ez);
    hkVector4f worldHalfExtents;
    worldHalfExtents.setAdd(ex, ey);
    worldHalfExtents.add(ez);

    hkVector4f worldCenter;
    worldCenter._setTransformedPos(BvToWorld, center);
    aabbOut.m_max.setAdd(worldCenter, worldHalfExtents);
    hkVector4f negHalfExtents;
    negHalfExtents.setNeg<4>(worldHalfExtents);
    aabbOut.m_min.setAdd(negHalfExtents, worldCenter);
}

// 0x00FB9240
// The transforms a continuously moving body's AABB is swept through (sweepOffsetAabb): its start, the inverse of its
// end, and intermediate transforms whose translation w is the factor that covers the arc between them (1 / cos of
// the angle covered). A small rotation (deltaAngle.w <= pi/8) uses the start transform with 1 / (1 - a^2 / 2), one up
// to pi/4 the middle of the step with 1 / (1 - a^2 / 8); larger ones every other step of pi/8 with 1 / cos(pi/8).
void HK_CALL hkAabbUtil::initOffsetAabbInput(const hkMotionState* motionState, OffsetAabbInput& input)
{
    input.m_motionState = motionState;
    input.m_endTransformInv.setInverse(motionState->getTransform());

    const hkSweptTransformf& swept = motionState->getSweptTransform();
    swept._approxTransformAt(swept.getBaseTimeSr(), input.m_startTransform);

    const hkSimdFloat32 invDeltaTime = swept.getInvDeltaTimeSr();
    if (!(invDeltaTime.getReal() < 0.0f || invDeltaTime.getReal() > 0.0f)) // zero (or NaN): not integrated
    {
        input.m_numTransforms = 0;
        return;
    }
    hkSimdFloat32 deltaTime;
    deltaTime.setReciprocal(invDeltaTime);

    const hkSimdFloat32 deltaAngle = motionState->m_deltaAngle.getW();
    hkSimdFloat32 piOver8;
    piOver8.setFromFloat(0.39269909f); // 0x3EC90FDB
    if (!(deltaAngle.getReal() > piOver8.getReal()))
    {
        input.m_transforms[0] = input.m_startTransform;
        const hkSimdFloat32 c = hkSimdFloat32_1 - (deltaAngle * deltaAngle) * hkSimdFloat32_Inv2;
        hkSimdFloat32 f;
        f.setReciprocal(c);
        input.m_transforms[0].getTranslation().setW(f);
        input.m_numTransforms = 1;
        return;
    }

    if (!(deltaAngle.getReal() > hkSimdFloat32_PiOver4.getReal()))
    {
        const hkSimdFloat32 halfDeltaTime = deltaTime * hkSimdFloat32_Inv2;
        swept._approxTransformAt(swept.getBaseTimeSr() + halfDeltaTime, input.m_transforms[0]);
        const hkSimdFloat32 c = hkSimdFloat32_1 - ((deltaAngle * deltaAngle) * hkSimdFloat32_Inv4) * hkSimdFloat32_Inv2;
        hkSimdFloat32 f;
        f.setReciprocal(c);
        input.m_transforms[0].getTranslation().setW(f);
        input.m_numTransforms = 1;
        return;
    }

    hkSimdFloat32 invPiOver8;
    invPiOver8.setReciprocal(piOver8);
    const hkSimdFloat32 numSteps = invPiOver8 * (piOver8 + deltaAngle);
    hkSimdFloat32 invNumSteps;
    invNumSteps.setReciprocal(numSteps);
    input.m_numTransforms = 0;

    hkSimdFloat32 invCosPiOver8;
    invCosPiOver8.setFromFloat(1.0824f); // 0x3F8A8C15: 1 / cos(pi/8) = 1.0823922, rounded up
    hkSimdFloat32 i = hkSimdFloat32_1;
    if (i.getReal() >= numSteps.getReal())
    {
        input.m_numTransforms = 0;
        return;
    }
    do
    {
        const hkSimdFloat32 time = ((i * invNumSteps) * deltaTime) + swept.getBaseTimeSr();
        const int n = input.m_numTransforms;
        input.m_numTransforms = n + 1;
        hkTransformf& t = input.m_transforms[n];
        swept._approxTransformAt(time, t);
        t.getTranslation().setW(invCosPiOver8);
        i = i + hkSimdFloat32_2;
    } while (!(i.getReal() >= numSteps.getReal()));
}
