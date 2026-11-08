// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/convex/cylinder/hkpcylindershape.cpp
//
// Havok 2014.2.5 (havok_impl.md HK3): a cylinder from m_vertexA to m_vertexB, with the convex radius around it.
//
// The round side is tessellated virtually: a supporting vertex is the rim point at one of a few fixed angles per
// octant (s_virtualTesselationParameter, set by setNumberOfVirtualSideSegments; Volition uses 64 segments). Its id
// encodes the base (bit 7), the signs of sin and cos (bits 6, 5), which of them is the smaller (bit 4) and the angle
// step of the smaller one (bits 0-3). The vertices keep cylinder radius + convex radius in w.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/convex/cylinder/hkpcylindershape.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/matrix/hktransformf.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkvector4util.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/geometry/aabb/hkaabb.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/geometry/sphere/hksphere.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/geometry/internal/algorithms/raycast/hkcdraycastcylinder.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/geometry/internal/types/hkcdvertex.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/query/hkpshaperaycastinput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/query/hkpshaperaycastoutput.h"

// 0x016F6258: ((16 / 8) - 0.1) * 1.4144272, the value for 16 side segments
float hkpCylinderShape::s_virtualTesselationParameter = 2.68741155f; // 0x402BFE8D
// 0x016F625C: 1 / s_virtualTesselationParameter
float hkpCylinderShape::s_virtualTesselationParameterInv = 0.372105271f; // 0x3EBE8495
// 0x016F6260: set on first use by initRoundUpThreshold; -1 until then
float hkpCylinderShape::s_intRoundUpThreshold = -1.0f;

// 0x00D915D0
void hkpCylinderShape::setNumberOfVirtualSideSegments(int numSegments)
{
    // 1.4144272: the tessellation factor per octant step
    s_virtualTesselationParameter = ((float)(numSegments / 8) - 0.1f) * 1.4144272f;
    s_virtualTesselationParameterInv = 1.0f / s_virtualTesselationParameter;
}

// 0x00D927D0
// The amount a float must be added to reach the next integer when truncated: 1 - the first multiple of 0.01 that
// truncates to a non-zero int (the accumulated 0.01 steps put it just above 1).
void hkpCylinderShape::initRoundUpThreshold()
{
    if (s_intRoundUpThreshold < 0.0f)
    {
        for (float f = 0.0f; f < 1.1f; f += 0.01f)
        {
            if ((int)f != 0)
            {
                s_intRoundUpThreshold = 1.0f - f;
                return;
            }
        }
        s_intRoundUpThreshold = 0.0f;
    }
}

// 0x00D92640
hkpCylinderShape::hkpCylinderShape(const hkVector4f& vertexA, const hkVector4f& vertexB, float cylinderRadius, float paddingRadius)
    : hkpConvexShape(hkcdShapeType::CYLINDER, paddingRadius)
{
    initRoundUpThreshold();
    m_vertexA = vertexA;
    m_vertexB = vertexB;
    setCylinderRadius(cylinderRadius);
    presetPerpendicularVector();
    m_cylBaseRadiusFactorForHeightFieldCollisions = 0.8f;
}

// 0x00D91960
hkpCylinderShape::hkpCylinderShape(hkFinishLoadedObjectFlag flag)
    : hkpConvexShape(flag)
{
    if (flag.m_finishing)
    {
        afterReflectNew();
    }
}

hkpCylinderShape::hkpCylinderShape()
{
}

// 0x00D92950
hkpCylinderShape::~hkpCylinderShape()
{
}

// 0x00D91700
void hkpCylinderShape::afterReflectNew()
{
    initRoundUpThreshold();
    m_type.m_storage = hkcdShapeType::CYLINDER;
}

// 0x00D915A0
const hkVector4f* hkpCylinderShape::getVertices() const
{
    return &m_vertexA;
}

// 0x00D915B0
const hkVector4f& hkpCylinderShape::getVertex(int i) const
{
    return (&m_vertexA)[i];
}

// 0x00D91620
float hkpCylinderShape::getCylinderRadius() const
{
    return m_cylRadius;
}

// 0x00D916A0: the vertices' w is the cylinder radius plus the convex radius
void hkpCylinderShape::setCylinderRadius(const float radius)
{
    m_cylRadius = radius;
    m_vertexA(3) = m_radius + radius;
    m_vertexB(3) = m_radius + radius;
}

// 0x00D92920
int hkpCylinderShape::getSize() const
{
    return sizeof(*this);
}

// 0x00D92930
int hkpCylinderShape::calcSizeForSpu(const hkpShape::CalcSizeForSpuInput& input, int spuBufferSizeLeft) const
{
    return sizeof(*this);
}

// 0x00D91EB0: two unit vectors perpendicular to the axis and to each other
void hkpCylinderShape::presetPerpendicularVector()
{
    hkVector4 axis; axis.setSub(m_vertexB, m_vertexA);
    axis.normalize<3>();
    hkVector4Util::calculatePerpendicularVector(axis, m_perpendicular1);
    m_perpendicular1.normalize<3>();
    m_perpendicular2.setCross(axis, m_perpendicular1);
}

// 0x00D929F0: the rim point of a vertex id (w is the computed value; callers store the id there)
void hkpCylinderShape::decodeVertexId(unsigned short code, hkVector4f& result) const
{
    const int isBaseA = (code >> VERTEX_ID_ENCODING_IS_BASE_A_SHIFT) & 1;
    const int sinSign = (code >> VERTEX_ID_ENCODING_SIN_SIGN_SHIFT) & 1;
    const int cosSign = (code >> VERTEX_ID_ENCODING_COS_SIGN_SHIFT) & 1;
    const int isSinLesser = (code >> VERTEX_ID_ENCODING_IS_SIN_LESSER_SHIFT) & 1;
    const int value = code & VERTEX_ID_ENCODING_VALUE_MASK;

    // the smaller of sin and cos from the step (its centre), the larger from 1 - x^2
    hkVector4 lesser; lesser.setAll(hkSimdReal::fromFloat((float)value));
    lesser.add(hkVector4::getConstant<HK_QUADREAL_INV_2>());
    hkVector4 paramInv; paramInv.setAll(s_virtualTesselationParameterInv);
    lesser.mul(paramInv);
    hkVector4 greater; greater.setMul(lesser, lesser);
    greater.setSub(hkVector4::getConstant<HK_QUADREAL_1>(), greater);
    greater.setSqrt<HK_ACC_23_BIT, HK_SQRT_SET_ZERO>(greater);

    hkVector4 sinV, cosV;
    if (isSinLesser)
    {
        sinV = lesser;
        cosV = greater;
    }
    else
    {
        sinV = greater;
        cosV = lesser;
    }
    if (!sinSign)
    {
        sinV.setSub(hkVector4::getConstant<HK_QUADREAL_0>(), sinV);
    }
    if (!cosSign)
    {
        cosV.setSub(hkVector4::getConstant<HK_QUADREAL_0>(), cosV);
    }

    hkVector4 radiusV; radiusV.setAll(m_cylRadius);
    hkVector4 p; p.setMul(m_perpendicular1, cosV);
    hkVector4 q; q.setMul(m_perpendicular2, sinV);
    result.setAdd(p, q);
    result.mul(radiusV);
    result.add(isBaseA ? m_vertexA : m_vertexB);
}

// 0x00D91990
void hkpCylinderShape::getSupportingVertex(const hkVector4f& direction, hkcdVertex& supportingVertexOut) const
{
    // the direction's angle around the axis
    hkSimdReal cosAngle = direction.dot<3>(m_perpendicular1);
    hkSimdReal sinAngle = m_perpendicular2.dot<3>(direction);
    const hkSimdReal len2 = sinAngle * sinAngle + cosAngle * cosAngle;
    if (len2.getReal() >= hkSimdReal_EpsSqrd.getReal())
    {
        const hkSimdReal invLen = len2.sqrtInverse<HK_ACC_23_BIT, HK_SQRT_IGNORE>();
        cosAngle = invLen * cosAngle;
        sinAngle = invLen * sinAngle;
    }
    else
    {
        cosAngle = hkSimdReal_1;
        sinAngle = hkSimdReal_0;
    }

    hkVector4 axis; axis.setSub(m_vertexB, m_vertexA);
    const float axisDot = direction.dot<3>(axis).getReal();

    hkSimdReal absSin; absSin.setAbs(sinAngle);
    hkSimdReal absCos; absCos.setAbs(cosAngle);
    const hkVector4Comparison sinLesser = absSin.lessEqual(absCos);
    hkSimdReal lesser; lesser.setSelect(sinLesser, absSin, absCos);

    // the step of the smaller value; 0.05 rounds up
    const float value = (lesser.getReal() * s_virtualTesselationParameter - s_intRoundUpThreshold) - (-0.05f);

    int code = (axisDot > 0.0f) ? 0 : 1; // <= 0 or NaN: base A
    code = code * 2 + (sinAngle.getReal() >= 0.0f ? 1 : 0);
    code = code * 2 + (cosAngle.getReal() >= 0.0f ? 1 : 0);
    code = code * 2 + (sinLesser.anyIsSet() ? 1 : 0);
    code = (code << 4) + (int)value;
    const unsigned short vertexId = (unsigned short)code;

    hkVector4 v;
    decodeVertexId(vertexId, v);
    // Original bug kept: w gets the untruncated code. For a NaN direction the step converts to 0x80000000 (integer
    // indefinite), so w is 0xBF000000 | id (sign set) while the id (decoded above) is the low 16 bits.
    v.setInt24W(code);
    supportingVertexOut.assign(v);
}

// 0x00D91C00
void hkpCylinderShape::convertVertexIdsToVertices(const unsigned short* ids, int numIds, hkcdVertex* verticesOut) const
{
    for (int i = numIds - 1; i >= 0; i--)
    {
        const unsigned short id = *ids++;
        hkVector4 v;
        decodeVertexId(id, v);
        v.setInt24W(id);
        (verticesOut++)->assign(v);
    }
}

// 0x00D916D0
void hkpCylinderShape::getCentre(hkVector4f& centreOut) const
{
    centreOut.setAdd(m_vertexB, m_vertexA);
    centreOut.mul(hkSimdReal_Inv2);
}

// 0x00D916F0
void hkpCylinderShape::getFirstVertex(hkVector4f& v) const
{
    v = m_vertexB;
}

// 0x00D92940
int hkpCylinderShape::getNumCollisionSpheres() const
{
    return 18;
}

// 0x00D91760
// 8 spheres around each base (the convex radius each), and one at each end on the axis side for long cylinders
// (height field collisions use a reduced base radius there).
const hkSphere* hkpCylinderShape::getCollisionSpheres(hkSphere* sphereBuffer) const
{
    hkVector4 axis; axis.setSub(m_vertexB, m_vertexA);
    const float axisLen2 = axis.lengthSquared<3>().getReal();
    const float cylRadius = m_cylRadius;
    float radius = cylRadius;
    float extraEndRadius = 0.0f;
    if (axisLen2 >= cylRadius * 4.0f * cylRadius)
    {
        radius = m_cylBaseRadiusFactorForHeightFieldCollisions * cylRadius;
        extraEndRadius = cylRadius;
    }

    const float diagonal = radius * 0.70710677f; // radius * sqrt(1/2)
    hkVector4 radiusV; radiusV.setAll(radius);
    hkVector4 diagonalV; diagonalV.setAll(diagonal);
    hkVector4 v1; v1.setMul(radiusV, m_perpendicular1);
    hkVector4 v2; v2.setMul(radiusV, m_perpendicular2);
    hkVector4 v3; v3.setAdd(m_perpendicular2, m_perpendicular1);
    v3.mul(diagonalV);
    hkVector4 v4; v4.setSub(m_perpendicular1, m_perpendicular2);
    v4.mul(diagonalV);
    v1.zeroComponent<3>();
    v2.zeroComponent<3>();
    v3.zeroComponent<3>();
    v4.zeroComponent<3>();

    const hkSimdReal convexRadius = hkSimdReal::fromFloat(m_radius);
    hkVector4* spheres = reinterpret_cast<hkVector4*>(sphereBuffer);
    for (int i = 0; i < 2; i++)
    {
        hkVector4 base; base.setXYZ_W(getVertex(i), convexRadius);
        hkVector4* s = spheres + 8 * i;
        s[0].setAdd(base, v1);
        s[1].setAdd(base, v3);
        s[2].setAdd(base, v2);
        s[3].setSub(base, v4);
        s[4].setSub(base, v1);
        s[5].setSub(base, v3);
        s[6].setSub(base, v2);
        s[7].setAdd(base, v4);
    }

    hkVector4 cross; cross.setCross(m_perpendicular1, m_perpendicular2);
    hkVector4 extra; extra.setAll(extraEndRadius);
    spheres[16].setMul(extra, cross);
    spheres[16].add(m_vertexA);
    extra.setAll(-extraEndRadius);
    spheres[17].setMul(extra, cross);
    spheres[17].add(m_vertexB);
    const float endRadius = m_radius + extraEndRadius;
    spheres[16](3) = endRadius;
    spheres[17](3) = endRadius;
    return sphereBuffer;
}

// 0x00D91D40: the two bases transformed, each grown by the rim's extent along each world axis
void hkpCylinderShape::getAabb(const hkTransformf& localToWorld, float tolerance, hkAabb& out) const
{
    hkVector4 obj[2];
    for (int i = 1; i >= 0; i--)
    {
        obj[i]._setTransformedPos(localToWorld, getVertex(i));
    }
    hkVector4 axis; axis.setSub(obj[1], obj[0]);
    hkVector4 invAxisLen2; invAxisLen2.setAll(axis.lengthSquared<3>());
    invAxisLen2.setReciprocal<HK_ACC_23_BIT, HK_DIV_IGNORE>(invAxisLen2);

    // the rim extent per axis: sqrt(1 - axis_i^2 / |axis|^2) = sqrt((axis_j^2 + axis_k^2) / |axis|^2)
    hkVector4 yzx; yzx.setPermutation<hkVectorPermutation::YZXW>(axis);
    hkVector4 zxy; zxy.setPermutation<hkVectorPermutation::ZXYW>(axis);
    yzx.mul(yzx);
    zxy.mul(zxy);
    hkVector4 ratio; ratio.setAdd(zxy, yzx);
    ratio.mul(invAxisLen2);
    ratio.zeroIfTrue(ratio.less(hkVector4::getConstant<HK_QUADREAL_EPS>()));
    ratio.setSqrt<HK_ACC_23_BIT, HK_SQRT_SET_ZERO>(ratio);

    hkVector4 cylRadius; cylRadius.setAll(m_cylRadius);
    hkVector4 tol4; tol4.setAll(m_radius + tolerance);
    hkVector4 extent; extent.setMul(cylRadius, ratio);
    extent.add(tol4);

    out.m_min.setMin(obj[0], obj[1]);
    out.m_min.sub(extent);
    out.m_max.setMax(obj[0], obj[1]);
    out.m_max.add(extent);
}

// 0x00D91FF0
// The cylinder lengthened by the convex radius at each end and widened by it. The shipped build records a
// "rcCylinder" monitor timer around the query; it is not reproduced.
hkBool hkpCylinderShape::castRay(const hkpShapeRayCastInput& input, hkpShapeRayCastOutput& results) const
{
    hkcdRay ray;
    ray.setEndPoints(input.m_from, input.m_to);
    const hkSimdReal radius = hkSimdReal::fromFloat(m_cylRadius + m_radius);

    hkVector4 axis; axis.setSub(m_vertexB, m_vertexA);
    axis.normalize<3>();
    axis.mul(hkSimdReal::fromFloat(m_radius));
    hkVector4 p; p.setSub(m_vertexA, axis);
    hkVector4 q; q.setAdd(axis, m_vertexB);

    hkSimdReal fraction = hkSimdReal::fromFloat(results.m_hitFraction);
    hkVector4 normal;
    if (hkcdRayCastCylinder(ray, p, q, radius, &fraction, &normal))
    {
        fraction.store<1>(&results.m_hitFraction);
        results.m_normal = normal;
        results.setKey(0xFFFFFFFF); // HK_INVALID_SHAPE_KEY
        return true;
    }
    return false;
}
