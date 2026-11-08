// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/gjk/continuous/hkpcontinuousgsk.cpp
//
// Havok 2014.2.5 (havok_impl.md HK6): the continuous GSK, from rfg.exe. hk4dGskCollideCalcToi advances the pair
// through the step by conservative advancement (the GSK distance against a worst case approaching velocity), then
// bisects the time of impact at the TOI separation and reports it to the contact manager. When the linear motion
// cannot explain the approach (rotation dominates), the collision spheres of A are moved against B's separating plane
// (hk4dGskCollidePointsWithPlane). Times are Rt, the fraction of the step (0 at the step start, 1 at its end).
//
// No SDK reference exists; ported from the binary. hk4dGskCalcSeparatingNormal, hk4dGskCalcPointVelocity,
// hk4dGskCollPInputInitialize, hk4dGskCalcWorstCaseProjectedVelocity and hk4dGskCalcToiFinal have no callers in
// rfg.exe: their out-of-line copies match the code inlined into hk4dGskCollideCalcToi, which calls them here.
//
// The shipped build records no monitor timers here.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/gjk/continuous/hkpcontinuousgsk.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/swepttransform/hkswepttransformutil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/allocator/lifo/hklifoallocator.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/router/hkmemoryrouter.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/physics/contactpoint/hkcontactpoint.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/physics/hkstepinfo.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/physics/motionstate/hkmotionstate.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcdbody.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/contactmgr/hkpcontactmgr.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpcollisionqualityinfo.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpprocesscollisioninput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpprocesscollisionoutput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent3/hkpagent3.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/convex/hkpconvexshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/gjk/hkpgsk.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/gjk/hkpgskcache.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/gjk/hkpgskexact.h"

#include <string.h>

using namespace hkGskExact;

// The interpolation fraction of the even bisection steps is clamped to [0.1, 0.9] (the odd ones are not).
static const float HK_4DGSK_MIN_FRACTION = 0.1f; // 0x3DCCCCCD
static const float HK_4DGSK_MAX_FRACTION = 0.9f; // 0x3F666666
// The assumed relative error of the angular velocity terms (hk4dGskCalcPointVelocityAssumingAngularVelError) and the
// slack allowed between the measured and the projected separating velocity.
static const float HK_4DGSK_ANGULAR_VEL_ERROR = 1.2f; // 0x3F99999A
// Written into m_internalDataA of the TOI's contact point properties (a marker; the solver overwrites the field).
static const float HK_4DGSK_TOI_PROPERTIES_MARKER = 321.0f; // 0x43A08000
// The bisections stop after this many steps.
static const int HK_4DGSK_MAX_BISECTION_STEPS = 10;

static inline Q hk4dGsk_zero() { return _mm_setzero_ps(); }

// The ucomiss "equal" branch (ZF set): equal or unordered.
static inline bool hk4dGsk_equalOrNaN(Q a, Q b) { return !(lane0(a) < lane0(b)) && !(lane0(a) > lane0(b)); }

// x + y + z + w as ((y + w) + (x + z)) in all lanes (hkVector4f::horizontalAdd<4>)
static inline Q hk4dGsk_hadd4(Q v)
{
    const Q s1 = add(_mm_shuffle_ps(v, v, 0x4E), v);
    return add(_mm_shuffle_ps(s1, s1, 0xB1), s1);
}

// The signed distance of a point to a plane given by its normal (offset in w) and a point on it:
// dot4 of ((point - pointOnPlane) * plane) with the plane's w in w.
static inline Q hk4dGsk_planeDistance(Q point, Q plane, Q pointOnPlane)
{
    return hk4dGsk_hadd4(setXYZ_W(mul(sub(point, pointOnPlane), plane), plane));
}

// sqrt(x) as x * invSqrt(x) (HK_ACC_23_BIT), 0 where x <= 0 (masked after the multiplication)
static inline Q hk4dGsk_sqrt23SetZero(Q len2)
{
    return _mm_andnot_ps(_mm_cmple_ps(len2, hk4dGsk_zero()), mul(invSqrt23(len2), len2));
}

// The swept transform's center of mass at time t: ((t - t0) * invDeltaTime) * (c1 - c0) + c0
static inline Q hk4dGsk_centerOfMassAt(const hkMotionState& ms, Q t)
{
    const Q c0 = ms.m_sweptTransform.m_centerOfMass0.m_quad;
    const Q c1 = ms.m_sweptTransform.m_centerOfMass1.m_quad;
    return add(mul(mul(sub(t, bcast<3>(c0)), bcast<3>(c1)), sub(c1, c0)), c0);
}

// The motion's rotation over the step: (invDeltaTime of the swept transform * step delta time) * m_deltaAngle
static inline Q hk4dGsk_deltaAngle(const hkMotionState& ms, Q deltaTime)
{
    return mul(mul(bcast<3>(ms.m_sweptTransform.m_centerOfMass1.m_quad), deltaTime), ms.m_deltaAngle.m_quad);
}

static inline hkSimdFloat32 hk4dGsk_simd(Q q)
{
    hkSimdFloat32 s;
    s.m_real = q;
    return s;
}

// hkAllocateStack / hkDeallocateStack of n vertices (128-byte multiples)
static inline int hk4dGsk_stackBytes(int numVertices) { return (numVertices * (int)sizeof(hkVector4f) + 127) & ~127; }

// 0x00E84960 (input in EAX, Rt in ECX, out in ESI)
// The transforms at Rt, B's plane and point and the motion of the bodies, in A's space.
void hk4dGskCollideInitMatrices(const hkp4dGskVertexCollidePointsInput& input, const hkSimdFloat32& Rt, hk4dGskCollideMatrices& out)
{
    const hkStepInfo& stepInfo = *input.m_stepInfo;
    const hkSimdFloat32 tAddOn = hk4dGsk_simd(mul(splat(stepInfo.m_deltaTime), Rt.m_real));
    const hkSimdFloat32 t = hk4dGsk_simd(splat(stepInfo.m_startTime));
    const Q radiusSum = splat(input.m_radiusSum);
    const hkMotionState& msA = *input.m_motionA;
    const hkMotionState& msB = *input.m_motionB;
    hkSweptTransformUtil::lerp2Ha(msA.m_sweptTransform, t, tAddOn, out.m_wTa);
    hkSweptTransformUtil::lerp2Ha(msB.m_sweptTransform, t, tAddOn, out.m_wTb);
    out.m_aTb.setMulInverseMul(out.m_wTa, out.m_wTb);
    out.m_plane.m_quad = rotate(out.m_aTb.getRotation(), input.m_planeB.m_quad);
    out.m_pointOnPlane.m_quad = transformPos(out.m_aTb, input.m_pointOnPlaneB.m_quad);
    out.m_plane.m_quad = setXYZ_W(out.m_plane.m_quad, sub(hk4dGsk_zero(), radiusSum));
    out.m_massCenterAinA = msA.m_sweptTransform.m_centerOfMassLocal;
    for (int i = 0; i < 2; i++)
    {
        out.m_angularVel[i].m_quad = rotateInverse(out.m_wTa.getRotation(), input.m_deltaAngles[i].m_quad);
    }
    out.m_massCenterBinA.m_quad = transformPos(out.m_aTb, msB.m_sweptTransform.m_centerOfMassLocal.m_quad);
    out.m_linearVel.m_quad = rotateInverse(out.m_wTa.getRotation(), input.m_linearTimInfo.m_quad);
}

// 0x00E84B40 (vertexA in EDI, distance0In in EDX, distance1In in ECX; the velocity argument of the PDB signature is
// unused and dropped)
// Bisects the time the vertex reaches the TOI separation between startRtIn (above it) and endRtIn (below it). The
// output keeps the earliest time; toiOut gets the new upper bound of the search.
void hk4dGskCollidePointWithPlaneFinal(const hkp4dGskVertexCollidePointsInput& input, const hkp4dGskTolerances& tol, const hkVector4f& vertexA, const hkSimdFloat32& startRtIn, const hkSimdFloat32& endRtIn, const hkSimdFloat32& distance0In, const hkSimdFloat32& distance1In, hkp4dGskVertexCollidePointsOutput& out, hkSimdFloat32& toiOut)
{
    Q Rt0 = startRtIn.m_real;
    Q Rt1 = endRtIn.m_real;
    Q d0 = distance0In.m_real;
    Q d1 = distance1In.m_real;
    const Q toiSeparation = splat(tol.m_toiSeparation);
    const Q accuracy = mul(g_vectorfConstants[HK_QUADREAL_INV_4], splat(tol.m_toiAccuracy));
    Q Rt = Rt0;
    for (int i = 0; i < HK_4DGSK_MAX_BISECTION_STEPS; i++)
    {
        if (lessEqualOrNaN(d0, toiSeparation))
        {
            break;
        }
        const Q delta = sub(d1, d0);
        if (greater(delta, sub(hk4dGsk_zero(), accuracy)))
        {
            break; // not approaching
        }
        Q fraction = _mm_div_ps(sub(toiSeparation, d0), delta);
        if ((i & 1) == 0)
        {
            const Q maxFraction = splat(HK_4DGSK_MAX_FRACTION);
            fraction = _mm_max_ps(splat(HK_4DGSK_MIN_FRACTION), selectQ(_mm_cmplt_ps(fraction, maxFraction), fraction, maxFraction));
        }
        Rt = add(mul(sub(Rt1, Rt0), fraction), Rt0);

        hk4dGskCollideMatrices matrices;
        hk4dGskCollideInitMatrices(input, hk4dGsk_simd(Rt), matrices);
        const Q dist = hk4dGsk_planeDistance(vertexA.m_quad, matrices.m_plane.m_quad, matrices.m_pointOnPlane.m_quad);
        if (lessOrNaN(abs(sub(dist, toiSeparation)), accuracy))
        {
            break;
        }
        const Q below = _mm_cmplt_ps(dist, toiSeparation);
        Rt0 = selectQ(below, Rt0, Rt);
        Rt1 = selectQ(below, Rt, Rt1);
        d0 = selectQ(below, d0, dist);
        d1 = selectQ(below, dist, d1);
    }

    const Q Rtoi = splat(out.m_Rtoi);
    if (lessOrNaN(Rt, Rtoi))
    {
        out.m_Rtoi = lane0(Rt);
        out.m_collidingPoint = vertexA;
        toiOut.m_real = Rt;
    }
    else
    {
        toiOut.m_real = Rtoi;
    }
}

// 0x00E84D80
// Moves the vertices of A against B's separating plane from input.m_startRt on. Each pass drops the vertices that
// cannot reach the plane before the current end of the search (the plane distance plus the worst case approach, then
// the vertex's own velocity with the maximum acceleration), keeps the others at the front of the array with their
// distance in w and advances to the earliest time one of them could reach the TOI separation. A vertex already
// closer than the TOI separation ends the search (at the start time) or is bisected (hk4dGskCollidePointWithPlaneFinal).
void hk4dGskCollidePointsWithPlane(const hkp4dGskVertexCollidePointsInput& input, const hkp4dGskTolerances& tol, hkp4dGskVertexCollidePointsOutput& out)
{
    Q Rt1 = g_vectorfConstants[HK_QUADREAL_1]; // the end of the search
    const Q startRt = splat(input.m_startRt);
    Q Rt = startRt;
    Q Rt0 = startRt; // the previous pass's Rt
    const Q toiSeparation = splat(tol.m_toiSeparation);
    const Q minSeparation = splat(tol.m_minSeparation);
    const Q invMaxAccel = splat(input.m_invMaxAccel);
    int numVertices = input.m_numVertices;
    if (!numVertices)
    {
        return;
    }
    int numKept;
    do
    {
        hk4dGskCollideMatrices matrices;
        hk4dGskCollideInitMatrices(input, hk4dGsk_simd(Rt), matrices);
        const Q deltaRt = sub(Rt1, Rt);
        Q nextRt = Rt1;
        const Q worstCaseDistance = add(mul(splat(input.m_worstCaseApproachingDelta), deltaRt), toiSeparation);
        numKept = 0;
        hkVector4f* vertex = input.m_verticesA;
        hkVector4f* kept = vertex;
        for (int i = numVertices - 1; i >= 0; i--, vertex++)
        {
            const Q v = vertex->m_quad;
            const Q dist = hk4dGsk_planeDistance(v, matrices.m_plane.m_quad, matrices.m_pointOnPlane.m_quad);
            if (greater(dist, worstCaseDistance))
            {
                continue;
            }
            const Q velA = cross(matrices.m_angularVel[0].m_quad, sub(v, matrices.m_massCenterAinA.m_quad));
            const Q velB = cross(matrices.m_angularVel[1].m_quad, sub(v, matrices.m_massCenterBinA.m_quad));
            const Q projectedVel = dot3(sub(sub(velA, matrices.m_linearVel.m_quad), velB), matrices.m_plane.m_quad);
            const Q distAtEnd = add(mul(projectedVel, deltaRt), sub(dist, toiSeparation));
            const Q maxAccelDistance = mul(mul(mul(deltaRt, g_vectorfConstants[HK_QUADREAL_INV_2]), deltaRt), splat(input.m_maxAccel));
            if (greater(distAtEnd, maxAccelDistance))
            {
                continue;
            }
            if (!greaterEqual(dist, toiSeparation))
            {
                // closer than the TOI separation
                if (hk4dGsk_equalOrNaN(Rt, startRt))
                {
                    out.m_Rtoi = lane0(Rt);
                    out.m_collidingPoint = *vertex;
                    Rt1 = Rt;
                }
                else
                {
                    hkSimdFloat32 toi;
                    toi.m_real = Rt1;
                    hk4dGskCollidePointWithPlaneFinal(input, tol, *vertex, hk4dGsk_simd(Rt0), hk4dGsk_simd(Rt), hk4dGsk_simd(bcast<3>(v)), hk4dGsk_simd(dist), out, toi);
                    Rt1 = toi.m_real;
                }
                continue;
            }

            // kept for the next pass, with its distance in w
            *vertex = *kept;
            kept->m_quad = setXYZ_W(v, dist);
            kept++;
            numKept++;

            // the earliest Rt the vertex can reach the TOI separation at with the maximum acceleration
            const Q accelDelta2 = mul(sub(dist, minSeparation), invMaxAccel);
            const Q accelDelta = add(accelDelta2, accelDelta2);
            if (greater(accelDelta, mul(deltaRt, deltaRt)))
            {
                continue;
            }
            const Q sqrtDelta = _mm_sqrt_ps(accelDelta);
            const Q velDelta = mul(projectedVel, invMaxAccel);
            const Q toiDelta = mul(sub(toiSeparation, minSeparation), invMaxAccel);
            const Q receding = _mm_cmple_ps(hk4dGsk_zero(), projectedVel);
            const Q safeDelta = selectQ(receding, add(add(velDelta, velDelta), sqrtDelta), _mm_max_ps(sub(hk4dGsk_zero(), velDelta), sqrtDelta));
            if (greater(safeDelta, deltaRt))
            {
                continue;
            }
            const Q candidate = add(_mm_max_ps(add(_mm_sqrt_ps(add(toiDelta, toiDelta)), safeDelta), splat(tol.m_minSafeDeltaRt)), Rt);
            nextRt = _mm_min_ps(nextRt, candidate);
        }

        if (greater(Rt, Rt1))
        {
            return;
        }
        if (hk4dGsk_equalOrNaN(Rt, nextRt) && greaterEqual(Rt, Rt1))
        {
            return;
        }
        Rt0 = Rt;
        Rt = nextRt;
        numVertices = numKept;
    } while (numKept != 0);
}

// 0x00E85100 (gsk in EDI, tA in ESI, tB in EAX)
// The GSK at the transforms: the world separating normal (w: the distance minus the radii) and the closest point on
// B in B's space.
void hk4dGskCalcSeparatingNormal(const hkTransformf& tA, const hkTransformf& tB, const hkpConvexShape* shapeA, const hkpConvexShape* shapeB, hkpGsk& gsk, hkVector4f& separatingNormalOut, hkVector4f& pointOnBInB)
{
    hkTransformf aTb;
    aTb.setMulInverseMul(tA, tB);
    hkVector4f normalA;
    gsk.getClosestFeature(shapeA, shapeB, aTb, normalA);
    const Q n = normalA.m_quad;
    separatingNormalOut.m_quad = rotate(tA.getRotation(), n);

    Q pointAinA;
    if (gsk.m_dimA == 1)
    {
        pointAinA = gsk.m_verticesA[0].m_quad;
    }
    else if (gsk.m_dimB == 1)
    {
        pointAinA = add(mul(bcast<3>(n), n), gsk.m_verticesBinA[0].m_quad);
    }
    else
    {
        pointAinA = gsk.m_closestLineLineResult.m_closestPointA.m_quad;
    }
    const Q dist = bcast<3>(n);
    pointOnBInB.m_quad = rotateInverse(aTb.getRotation(), sub(sub(pointAinA, mul(dist, n)), aTb.getTranslation().m_quad));

    const float radiusSum = shapeA->m_radius + shapeB->m_radius;
    separatingNormalOut.m_quad = setXYZ_W(separatingNormalOut.m_quad, sub(dist, splat(radiusSum)));
}

// 0x00E85230 (msB in EAX, deltaAngles in EDX)
// The velocity (in Rt) the point closes the gap along the normal with: the point on A's rotation minus B's, minus the
// linear TIM info.
void hk4dGskCalcPointVelocity(const hkMotionState& msA, const hkMotionState& msB, const hkVector4f& separatingPoint, const hkVector4f& separatingNormal, const hkVector4f& linearTimInfo, const hkVector4f* deltaAngles, const hkSimdFloat32& t, hkSimdFloat32& separatingVelocity)
{
    const Q p = separatingPoint.m_quad;
    const Q velA = cross(deltaAngles[0].m_quad, sub(p, hk4dGsk_centerOfMassAt(msA, t.m_real)));
    const Q velB = cross(deltaAngles[1].m_quad, sub(p, hk4dGsk_centerOfMassAt(msB, t.m_real)));
    separatingVelocity.m_real = dot3(sub(add(neg(linearTimInfo.m_quad), velA), velB), separatingNormal.m_quad);
}

// 0x00E85320 (separatingNormal in ECX, t in EDI, separatingVelocity in EAX)
// hk4dGskCalcPointVelocity with the angular terms scaled by the assumed error toward the larger approach (an
// approaching term times 1.2, a receding one divided by it).
void hk4dGskCalcPointVelocityAssumingAngularVelError(const hkMotionState& msA, const hkMotionState& msB, const hkVector4f& separatingPoint, const hkVector4f& separatingNormal, const hkVector4f& linearTimInfo, const hkVector4f* deltaAngles, const hkSimdFloat32& t, hkSimdFloat32& separatingVelocity)
{
    const Q error = splat(HK_4DGSK_ANGULAR_VEL_ERROR);
    const Q n = separatingNormal.m_quad;
    const Q p = separatingPoint.m_quad;
    const Q linear = dot3(neg(linearTimInfo.m_quad), n);
    separatingVelocity.m_real = linear;

    const Q a = dot3(cross(deltaAngles[0].m_quad, sub(p, hk4dGsk_centerOfMassAt(msA, t.m_real))), n);
    const Q approachingA = _mm_cmple_ps(a, hk4dGsk_zero());
    const Q velA = add(selectQ(approachingA, mul(a, error), _mm_div_ps(a, error)), linear);
    separatingVelocity.m_real = velA;

    const Q b = dot3(cross(deltaAngles[1].m_quad, sub(p, hk4dGsk_centerOfMassAt(msB, t.m_real))), n);
    const Q approachingB = _mm_cmple_ps(hk4dGsk_zero(), b);
    separatingVelocity.m_real = sub(velA, selectQ(approachingB, mul(b, error), _mm_div_ps(b, error)));
}

// 0x00E85490 (in3 in EAX, collPInput in ESI)
// The bodies' part of the vertex input: motions, radii, the collision spheres of A (on the LIFO stack, freed by the
// caller) and the maximum acceleration of the rotations (m_deltaAngles must be set).
void hk4dGskCollPInputInitialize(const hkpAgent3ProcessInput& in3, hkp4dGskVertexCollidePointsInput& collPInput)
{
    const hkpCdBody& bodyA = *in3.m_bodyA;
    const hkpCdBody& bodyB = *in3.m_bodyB;
    const hkpConvexShape* shapeA = static_cast<const hkpConvexShape*>(bodyA.getShape());
    const hkpConvexShape* shapeB = static_cast<const hkpConvexShape*>(bodyB.getShape());
    const hkMotionState* msA = bodyA.getMotionState();
    const hkMotionState* msB = bodyB.getMotionState();
    collPInput.m_motionA = msA;
    collPInput.m_motionB = msB;
    collPInput.m_radiusSum = shapeB->m_radius + shapeA->m_radius;
    collPInput.m_linearTimInfo = in3.m_linearTimInfo;
    collPInput.m_stepInfo = &in3.m_input->m_stepInfo;

    const int numSpheres = shapeA->getNumCollisionSpheres();
    collPInput.m_allocatedNumVertices = numSpheres;
    collPInput.m_numVertices = numSpheres;
    hkVector4f* vertices = static_cast<hkVector4f*>(hkMemoryRouter::getInstance().stack().fastBlockAlloc(hk4dGsk_stackBytes(numSpheres)));
    collPInput.m_verticesA = vertices;
    shapeA->getCollisionSpheres(reinterpret_cast<hkSphere*>(vertices));

    const Q wA = bcast<3>(collPInput.m_deltaAngles[0].m_quad);
    const Q wB = bcast<3>(collPInput.m_deltaAngles[1].m_quad);
    const Q maxAccel = add(mul(mul(wB, wB), splat(msB->m_objectRadius)), mul(mul(wA, wA), splat(msA->m_objectRadius)));
    collPInput.m_maxAccel = lane0(maxAccel);
    collPInput.m_invMaxAccel = lane0(_mm_div_ps(g_vectorfConstants[HK_QUADREAL_1], add(maxAccel, g_vectorfConstants[HK_QUADREAL_EPS_SQRD])));
}

// 0x00E85590 (separatingNormal in ECX, deltaAngles in EAX)
// The approach along the normal over the step: the linear TIM part and the worst case of the rotations (the points
// at the bodies' object radii).
void hk4dGskCalcWorstCaseProjectedVelocity(const hkMotionState& msA, const hkMotionState& msB, const hkVector4f& separatingNormal, const hkVector4f& linearTimInfo, const hkVector4f* deltaAngles, hkSimdFloat32& realProjectedLinearDelta, hkSimdFloat32& projectedAngularDelta)
{
    const Q n = separatingNormal.m_quad;
    const Q crossA = cross(deltaAngles[0].m_quad, n);
    const Q crossB = cross(deltaAngles[1].m_quad, n);
    const Q angularA = mul(splat(msA.m_objectRadius), hk4dGsk_sqrt23SetZero(hadd3(mul(crossA, crossA))));
    const Q angularB = mul(hk4dGsk_sqrt23SetZero(hadd3(mul(crossB, crossB))), splat(msB.m_objectRadius));
    projectedAngularDelta.m_real = add(angularB, angularA);
    realProjectedLinearDelta.m_real = dot3(n, linearTimInfo.m_quad);
}

// 0x00E856D0 (endRtIn in EDX, distance0In in ECX)
// Bisects the time of impact between startRtIn (distance0In) and endRtIn (distance1In) at the TOI separation, then
// builds the contact point (welded against triangles), estimates the separating velocity and adds the TOI to the
// contact manager. featureChange 0 keeps the GSK's features (only the contact is recalculated along
// oldMasterNormal, the world normal of the last safe step).
void hk4dGskCalcToiFinal(const hkpAgent3ProcessInput& in3, const hkp4dGskTolerances& tol, hkpGsk& gsk, int featureChange, const hkVector4f& oldMasterNormal, const hkVector4f* deltaAngles, const hkSimdFloat32& startRtIn, const hkSimdFloat32& endRtIn, const hkSimdFloat32& distance0In, const hkSimdFloat32& distance1In, hkpProcessCollisionOutput& out)
{
    const hkpCdBody& bodyA = *in3.m_bodyA;
    const hkpCdBody& bodyB = *in3.m_bodyB;
    const hkpConvexShape* shapeA = static_cast<const hkpConvexShape*>(bodyA.getShape());
    const hkpConvexShape* shapeB = static_cast<const hkpConvexShape*>(bodyB.getShape());
    const hkMotionState& msA = *bodyA.getMotionState();
    const hkMotionState& msB = *bodyB.getMotionState();
    const hkpProcessCollisionInput& input = *in3.m_input;
    const hkStepInfo& stepInfo = input.m_stepInfo;

    Q Rt0 = startRtIn.m_real;
    Q Rt1 = endRtIn.m_real;
    Q d0 = distance0In.m_real;
    Q d1 = distance1In.m_real;
    const Q radiusSum = splat(shapeA->m_radius + shapeB->m_radius);
    // the separating velocity measured over the last step of the advancement (NaN without one: 0 / 0)
    const Q measuredVelocity = _mm_div_ps(sub(d1, d0), sub(Rt1, Rt0));
    const Q accuracy = splat(tol.m_toiAccuracy);
    const Q toiSeparation = splat(tol.m_toiSeparation);
    const Q minusAccuracy = sub(hk4dGsk_zero(), accuracy);

    bool converged;
    Q Rt;
    Q dist;
    hkTransformf wTa, wTb, aTb;
    hkVector4f normalA; // the GSK's normal in A's space, the distance in w
    hkVector4f pointAinA;
    for (int i = 0;;)
    {
        const Q delta = sub(d1, d0);
        const Q receding = _mm_cmplt_ps(minusAccuracy, delta);
        Rt1 = selectQ(receding, Rt0, Rt1);
        const Q divisor = selectQ(receding, g_vectorfConstants[HK_QUADREAL_INV_2], delta);
        if (lessOrNaN(abs(sub(d1, toiSeparation)), accuracy))
        {
            converged = true;
            featureChange = 0;
            Rt = Rt1;
        }
        else
        {
            const Q fraction = _mm_div_ps(sub(toiSeparation, d0), divisor);
            const Q maxFraction = splat(HK_4DGSK_MAX_FRACTION);
            const Q clamped = _mm_max_ps(splat(HK_4DGSK_MIN_FRACTION), selectQ(_mm_cmplt_ps(fraction, maxFraction), fraction, maxFraction));
            Rt = add(mul(clamped, sub(Rt1, Rt0)), Rt0);
            converged = false;
        }

        const hkSimdFloat32 tAddOn = hk4dGsk_simd(mul(splat(stepInfo.m_deltaTime), Rt));
        const hkSimdFloat32 t = hk4dGsk_simd(splat(stepInfo.m_startTime));
        hkSweptTransformUtil::lerp2Ha(msA.m_sweptTransform, t, tAddOn, wTa);
        hkSweptTransformUtil::lerp2Ha(msB.m_sweptTransform, t, tAddOn, wTb);
        aTb.setMulInverseMul(wTa, wTb);
        if (featureChange)
        {
            gsk.getClosestFeature(shapeA, shapeB, aTb, normalA);
            if (gsk.m_dimA == 1)
            {
                pointAinA = gsk.m_verticesA[0];
            }
            else if (gsk.m_dimB == 1)
            {
                pointAinA.m_quad = add(mul(bcast<3>(normalA.m_quad), normalA.m_quad), gsk.m_verticesBinA[0].m_quad);
            }
            else
            {
                pointAinA = gsk.m_closestLineLineResult.m_closestPointA;
            }
        }
        else
        {
            // B's vertices moved into A's space at Rt (a do-while: with m_dimB 0 the original transforms the vector
            // before m_verticesBinB, m_verticesBinA[3], into the one before m_verticesBinA, m_dummy[3])
            hkVector4f* const gskVectors = reinterpret_cast<hkVector4f*>(&gsk);
            const int binA = (int)(offsetof(hkpGsk, m_verticesBinA) / sizeof(hkVector4f));
            const int binB = (int)(offsetof(hkpGsk, m_verticesBinB) / sizeof(hkVector4f));
            int v = gsk.m_dimB - 1;
            do
            {
                gskVectors[binA + v].m_quad = transformPos(aTb, gskVectors[binB + v].m_quad);
            } while (--v >= 0);
            hkVector4f masterNormal;
            masterNormal.m_quad = rotateInverse(wTa.getRotation(), oldMasterNormal.m_quad);
            hkGskRecalcContactInternal(gsk.m_verticesA, gsk.m_verticesBinA, gsk.m_dimA, gsk.m_dimB, masterNormal, pointAinA, normalA);
        }

        dist = sub(bcast<3>(normalA.m_quad), radiusSum);
        if (lessOrNaN(abs(sub(dist, toiSeparation)), accuracy) || converged || hk4dGsk_equalOrNaN(Rt0, Rt1))
        {
            break;
        }
        const Q below = _mm_cmplt_ps(dist, toiSeparation);
        Rt1 = selectQ(below, Rt, Rt1);
        Rt0 = selectQ(below, Rt0, Rt);
        d0 = selectQ(below, d0, dist);
        d1 = selectQ(below, dist, d1);
        if (++i >= HK_4DGSK_MAX_BISECTION_STEPS)
        {
            break;
        }
    }

    const Q minToiDeltaTime = splat(tol.m_minToiDeltaTime);
    const Q toiTime = add(_mm_max_ps(mul(splat(stepInfo.m_deltaTime), Rt), minToiDeltaTime), splat(stepInfo.m_startTime));
    if (greaterEqual(toiTime, splat(out.m_toi.m_time)))
    {
        return;
    }
    if (greaterEqual(toiTime, sub(splat(stepInfo.m_endTime), minToiDeltaTime)))
    {
        return;
    }

    // the contact point on B's surface, the normal with the distance in w
    const Q normalW = rotate(wTa.getRotation(), normalA.m_quad);
    hkContactPoint cp;
    cp.m_position.m_quad = add(transformPos(wTa, pointAinA.m_quad), mul(sub(sub(hk4dGsk_zero(), splat(shapeA->m_radius)), dist), normalW));
    cp.m_separatingNormal.m_quad = setXYZ_W(normalW, dist);

    // welding against a triangle on either side (A's normal flipped for the call; A's welded point is not used)
    int weldResult = hkpConvexShape::WELD_RESULT_ACCEPT_CONTACT_POINT_UNMODIFIED;
    hkVector4f weldedNormal = cp.m_separatingNormal;
    if (shapeB->m_type.m_storage == hkcdShapeType::TRIANGLE)
    {
        const int dimB = gsk.m_dimB;
        unsigned char numFeaturePoints = (unsigned char)dimB;
        unsigned short featurePoints[3];
        featurePoints[0] = (unsigned short)gsk.m_verticesBinB[0].getId();
        featurePoints[1] = numFeaturePoints < 2 ? 0xFFFF : (unsigned short)gsk.m_verticesBinB[1].getId();
        featurePoints[2] = numFeaturePoints < 3 ? 0xFFFF : (unsigned short)gsk.m_verticesBinB[2].getId();
        hkVector4f weldedPoint = cp.m_position;
        weldResult = shapeB->weldContactPoint(featurePoints, numFeaturePoints, weldedPoint, &wTb, shapeA, &wTa, weldedNormal);
        cp.m_position = weldedPoint;
    }
    else if (shapeA->m_type.m_storage == hkcdShapeType::TRIANGLE)
    {
        unsigned char numFeaturePoints = (unsigned char)gsk.m_dimA.val();
        unsigned short featurePoints[3];
        featurePoints[0] = (unsigned short)gsk.m_verticesA[0].getId();
        featurePoints[1] = numFeaturePoints < 2 ? 0xFFFF : (unsigned short)gsk.m_verticesA[1].getId();
        featurePoints[2] = numFeaturePoints < 3 ? 0xFFFF : (unsigned short)gsk.m_verticesA[2].getId();
        hkVector4f pointOnA;
        pointOnA.m_quad = add(mul(bcast<3>(weldedNormal.m_quad), weldedNormal.m_quad), cp.m_position.m_quad);
        const Q flip3 = _mm_castsi128_ps(_mm_set_epi32(0, (int)0x80000000, (int)0x80000000, (int)0x80000000));
        weldedNormal.m_quad = _mm_xor_ps(weldedNormal.m_quad, flip3);
        weldResult = shapeA->weldContactPoint(featurePoints, numFeaturePoints, pointOnA, &wTa, shapeB, &wTb, weldedNormal);
        weldedNormal.m_quad = _mm_xor_ps(flip3, weldedNormal.m_quad);
    }
    cp.m_separatingNormal = weldedNormal;
    if (weldResult == hkpConvexShape::WELD_RESULT_REJECT_CONTACT_POINT && input.m_collisionQualityInfo->m_enableToiWeldRejection)
    {
        return;
    }

    hkSimdFloat32 projectedVelocity;
    hk4dGskCalcPointVelocity(msA, msB, cp.m_position, cp.m_separatingNormal, in3.m_linearTimInfo, deltaAngles, hk4dGsk_simd(toiTime), projectedVelocity);

    if (weldResult == hkpConvexShape::WELD_RESULT_ACCEPT_CONTACT_POINT_MODIFIED && greater(projectedVelocity.m_real, sub(hk4dGsk_zero(), g_vectorfConstants[HK_QUADREAL_EPS])))
    {
        // welding turned the normal: the collision spheres of A find the point against the welded plane
        hkp4dGskVertexCollidePointsInput collPInput;
        collPInput.m_startRt = lane0(Rt);
        const Q deltaTime = splat(stepInfo.m_deltaTime);
        collPInput.m_deltaAngles[0].m_quad = hk4dGsk_deltaAngle(msA, deltaTime);
        collPInput.m_deltaAngles[1].m_quad = hk4dGsk_deltaAngle(msB, deltaTime);
        hkSimdFloat32 linearDelta, angularDelta;
        hk4dGskCalcWorstCaseProjectedVelocity(msA, msB, cp.m_separatingNormal, in3.m_linearTimInfo, collPInput.m_deltaAngles, linearDelta, angularDelta);
        collPInput.m_worstCaseApproachingDelta = lane0(add(linearDelta.m_real, angularDelta.m_real));
        hk4dGskCollPInputInitialize(in3, collPInput);
        collPInput.m_planeB.m_quad = rotateInverse(wTb.getRotation(), cp.m_separatingNormal.m_quad);
        collPInput.m_pointOnPlaneB = gsk.m_verticesBinB[0];
        if (collPInput.m_verticesA.val())
        {
            hkp4dGskVertexCollidePointsOutput pointsOut;
            pointsOut.m_Rtoi = 1.0f;
            hk4dGskCollidePointsWithPlane(collPInput, tol, pointsOut);
            hkMemoryRouter::getInstance().stack().fastBlockFree(collPInput.m_verticesA, hk4dGsk_stackBytes(collPInput.m_allocatedNumVertices));
            if (1.0f > pointsOut.m_Rtoi)
            {
                cp.m_position.m_quad = transformPos(wTa, pointsOut.m_collidingPoint.m_quad);
            }
        }
    }

    // the separating velocity: the projected one unless the measured one is larger, unless it is not much larger
    // (then the angular terms with their assumed error, at most 0)
    Q velocity = projectedVelocity.m_real;
    if (!greaterEqual(measuredVelocity, velocity))
    {
        if (lessEqualOrNaN(measuredVelocity, mul(splat(HK_4DGSK_ANGULAR_VEL_ERROR), velocity)))
        {
            hkSimdFloat32 errorVelocity;
            hk4dGskCalcPointVelocityAssumingAngularVelError(msA, msB, cp.m_position, cp.m_separatingNormal, in3.m_linearTimInfo, deltaAngles, hk4dGsk_simd(toiTime), errorVelocity);
            velocity = _mm_min_ps(g_vectorfConstants[HK_QUADREAL_0], errorVelocity.m_real);
        }
        else
        {
            velocity = measuredVelocity;
        }
    }
    float separatingVelocity = lane0(mul(splat(stepInfo.m_invDeltaTime), velocity));

    hkContactPointPropertiesWithExtendedUserData16 properties;
    memset(&properties, 0, sizeof(properties));
    // Original quirk kept: the TOI's contact properties get a stray 321.0 in m_internalDataA.
    properties.m_internalDataA = HK_4DGSK_TOI_PROPERTIES_MARKER;
    // Original bug kept: exitAndExportCacheImpl writes only the used vertex ids and the dimensions, and the original's
    // cache shares its stack slot with distance0 of the last bisection step, so the unused ids, the max dimensions
    // byte and m_gskFlags of the TOI's cache are bytes of that (splat) float.
    hkGskCache16 cache;
    memcpy(&cache, &d0, sizeof(cache));
    gsk.exitAndExportCacheImpl(cache);
    const float toi = lane0(toiTime);
    if (in3.m_contactMgr->addToi(bodyA, bodyB, input, out, toi, cp, &cache, separatingVelocity, properties) != hkpContactMgr::TOI_ACCEPT)
    {
        return;
    }
    if (out.m_toi.m_time.val() != HK_REAL_MAX)
    {
        in3.m_contactMgr->removeToi(*out.m_constraintOwner, out.m_toi.m_properties);
    }
    out.m_toi.m_seperatingVelocity = separatingVelocity;
    out.m_toi.m_contactPoint = cp;
    out.m_toi.m_time = toi;
    memcpy(&out.m_toi.m_properties, &properties, sizeof(properties));
    memcpy(&out.m_toi.m_gskCache, &cache, sizeof(cache));
}

// 0x00E86570
// Advances the pair from the step start while the worst case approach cannot close the gap to the minimum
// separation, then bisects the time of impact (hk4dGskCalcToiFinal). Called with the separating normal of the
// pair's last GSK (w: the distance) when the pair may get closer than the minimum separation in this step.
void hk4dGskCollideCalcToi(const hkpAgent3ProcessInput& in3, const hkSimdFloat32& allowedPenetrationDepth, const hkSimdFloat32& minSeparation, const hkSimdFloat32& toiSeparation, hkpGskCache& gskCache, hkVector4f& separatingNormal, hkpProcessCollisionOutput& out)
{
    const hkpProcessCollisionInput& input = *in3.m_input;
    const hkpCollisionQualityInfo& quality = *input.m_collisionQualityInfo;
    const hkStepInfo& stepInfo = input.m_stepInfo;
    const Q allowed = allowedPenetrationDepth.m_real;

    hkp4dGskTolerances tol;
    tol.m_minSeparation = lane0(minSeparation.m_real);
    tol.m_toiSeparation = lane0(toiSeparation.m_real);
    const Q toiAccuracy = mul(splat(quality.m_toiAccuracy), allowed);
    tol.m_toiAccuracy = lane0(toiAccuracy);
    const Q minSafeDeltaRt = mul(_mm_max_ps(mul(splat(quality.m_minSafeDeltaTime), allowed), splat(quality.m_minAbsoluteSafeDeltaTime)), splat(stepInfo.m_invDeltaTime));
    tol.m_minSafeDeltaRt = lane0(minSafeDeltaRt);
    tol.m_minToiDeltaTime = quality.m_minToiDeltaTime;

    const hkpConvexShape* shapeA = static_cast<const hkpConvexShape*>(in3.m_bodyA->getShape());
    const hkpConvexShape* shapeB = static_cast<const hkpConvexShape*>(in3.m_bodyB->getShape());
    const hkMotionState& msA = *in3.m_bodyA->getMotionState();
    const hkMotionState& msB = *in3.m_bodyB->getMotionState();

    hkp4dGskVertexCollidePointsInput collPInput;
    collPInput.m_verticesA = nullptr;
    const Q deltaTime = splat(stepInfo.m_deltaTime);
    collPInput.m_deltaAngles[0].m_quad = hk4dGsk_deltaAngle(msA, deltaTime);
    collPInput.m_deltaAngles[1].m_quad = hk4dGsk_deltaAngle(msB, deltaTime);

    hkpGsk gsk;
    gsk.init(shapeA, shapeB, gskCache);
    bool pointsDone = false; // the vertex collision gave no better advance (or could not allocate)
    bool advanced = false;

    const Q Rt1 = g_vectorfConstants[HK_QUADREAL_1];
    hkVector4f oldNormal;
    oldNormal.m_quad = hk4dGsk_zero();
    Q d0 = bcast<3>(separatingNormal.m_quad);
    Q Rt = hk4dGsk_zero();
    Q Rt0 = hk4dGsk_zero();
    int featureChange = 1;
    const Q toiDistance = add(toiSeparation.m_real, toiAccuracy);
    bool toi = true;
    if (greater(bcast<3>(separatingNormal.m_quad), toiDistance))
    {
        do
        {
            const Q deltaRt = sub(Rt1, Rt);
            if (lessEqualOrNaN(deltaRt, hk4dGsk_zero()))
            {
                toi = false;
                break;
            }
            hkSimdFloat32 linearDelta, angularDelta;
            hk4dGskCalcWorstCaseProjectedVelocity(msA, msB, separatingNormal, in3.m_linearTimInfo, collPInput.m_deltaAngles, linearDelta, angularDelta);
            const Q approach = add(linearDelta.m_real, angularDelta.m_real);
            const Q dist = bcast<3>(separatingNormal.m_quad);
            const Q separatedAtEnd = _mm_cmplt_ps(minSeparation.m_real, sub(dist, mul(approach, deltaRt)));
            if (_mm_movemask_ps(_mm_or_ps(separatedAtEnd, _mm_cmple_ps(approach, hk4dGsk_zero()))))
            {
                toi = false;
                break;
            }
            const Q safeDeltaRt = _mm_div_ps(sub(dist, minSeparation.m_real), approach);
            Q advance = safeDeltaRt;
            // small steps dominated by rotation: try the collision spheres of A against the separating plane
            if (lessOrNaN(safeDeltaRt, g_vectorfConstants[HK_QUADREAL_INV_5]) && !pointsDone && advanced && lessOrNaN(linearDelta.m_real, angularDelta.m_real))
            {
                bool havePoints = true;
                if (!collPInput.m_verticesA.val())
                {
                    hk4dGskCollPInputInitialize(in3, collPInput);
                    if (!collPInput.m_verticesA.val())
                    {
                        pointsDone = true;
                        havePoints = false;
                    }
                }
                if (havePoints)
                {
                    collPInput.m_worstCaseApproachingDelta = lane0(approach);
                    collPInput.m_startRt = lane0(Rt);
                    hkp4dGskVertexCollidePointsOutput pointsOut;
                    pointsOut.m_Rtoi = 1.0f;
                    hk4dGskCollidePointsWithPlane(collPInput, tol, pointsOut);
                    const Q pointsDeltaRt = sub(splat(pointsOut.m_Rtoi), Rt);
                    if (lessOrNaN(pointsDeltaRt, add(safeDeltaRt, safeDeltaRt)))
                    {
                        pointsDone = true;
                    }
                    advance = _mm_max_ps(safeDeltaRt, pointsDeltaRt);
                }
            }
            if (greaterEqual(add(Rt, advance), Rt1))
            {
                toi = false;
                break;
            }
            Rt0 = Rt;
            Rt = _mm_min_ps(add(Rt, _mm_max_ps(minSafeDeltaRt, advance)), Rt1);
            oldNormal = separatingNormal;
            d0 = bcast<3>(oldNormal.m_quad);

            const hkSimdFloat32 tAddOn = hk4dGsk_simd(mul(splat(stepInfo.m_deltaTime), Rt));
            const hkSimdFloat32 t = hk4dGsk_simd(splat(stepInfo.m_startTime));
            hkTransformf wTa, wTb;
            hkSweptTransformUtil::lerp2Ha(msA.m_sweptTransform, t, tAddOn, wTa);
            hkSweptTransformUtil::lerp2Ha(msB.m_sweptTransform, t, tAddOn, wTb);
            hk4dGskCalcSeparatingNormal(wTa, wTb, shapeA, shapeB, gsk, separatingNormal, collPInput.m_pointOnPlaneB);
            collPInput.m_planeB.m_quad = rotateInverse(wTb.getRotation(), separatingNormal.m_quad);
            featureChange = gsk.m_featureChange;
            advanced = true;
        } while (greater(bcast<3>(separatingNormal.m_quad), add(toiSeparation.m_real, toiAccuracy)));
    }

    if (toi)
    {
        hk4dGskCalcToiFinal(in3, tol, gsk, featureChange, oldNormal, collPInput.m_deltaAngles, hk4dGsk_simd(Rt0), hk4dGsk_simd(Rt), hk4dGsk_simd(d0), hk4dGsk_simd(bcast<3>(separatingNormal.m_quad)), out);
    }

    if (collPInput.m_verticesA.val())
    {
        hkMemoryRouter::getInstance().stack().fastBlockFree(collPInput.m_verticesA, hk4dGsk_stackBytes(collPInput.m_allocatedNumVertices));
    }
    gsk.checkForChangesAndUpdateCache(gskCache);
}
