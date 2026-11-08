// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/convex/triangle/hkptriangleshape.cpp
//
// Havok 2014.2.5 (havok_impl.md HK3): a triangle, optionally extruded (a prism along m_extrusion), with welding
// information per edge (5 bits each, an index into hkpWeldingUtility::m_sinCosTable). Vertex ids 0-2 are the
// vertices, 3-5 the extruded ones. With the out-of-line copies of its inline members (hkpTriangleShape.inl in Havok
// 2014.1).

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/convex/triangle/hkptriangleshape.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/matrix/hktransformf.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkvector4util.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/geometry/aabb/hkaabb.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/geometry/sphere/hksphere.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/geometry/internal/algorithms/raycast/hkcdraycasttriangle.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/geometry/internal/types/hkcdray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/geometry/internal/types/hkcdraybundle.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/geometry/internal/types/hkcdvertex.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/query/hkpshaperaybundlecastinput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/query/hkpshaperaycastinput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/query/hkpshaperaycastoutput.h"

namespace
{
// 0x016F60D4: 1 for the extruded vertex ids (3-5)
const float addExtrusionTable[6] = { 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f };

// 0x016F60EC: the triangle normal's sign by welding type (anticlockwise 1, clockwise -1)
const float flipNormalBasedOnWeldingTypeTable[5] = { 1.0f, 0.0f, 0.0f, 0.0f, -1.0f };

// 0x016F6100: the vertex of a vertex id (id mod 3)
const int mod3Table[6] = { 0, 1, 2, 0, 1, 2 };

// The ray cast tolerance (0x38D1B717): crossings inside each edge by at least -1e-4 * |n|^2 count.
const float s_rayTolerance = 1e-4f;

// 0x00D83300 (LTCG register arguments: vertex1 in ECX, normal in EAX)
void calcAntiClockwiseTriangleNormal(const hkVector4f& vertex0, const hkVector4f& vertex1, const hkVector4f& vertex2, hkVector4f& normal)
{
    hkVector4 e0; e0.setSub(vertex1, vertex0);
    hkVector4 e1; e1.setSub(vertex2, vertex1);
    normal.setCross(e0, e1);
    normal.normalize<3>();
}

// 0x00D82F60 (LTCG register arguments)
// The segment against the triangle. On a crossing closer than the fraction, the fraction and the normal (towards the
// segment's start, unnormalized until the crossing is found inside the edges) are written; returns whether the
// crossing is inside all three edges (by -tolerance * |n|^2).
hkBool32 hkcdSegmentTriangleIntersect(const hkcdRay& ray, const hkVector4f& vA, const hkVector4f& vB, const hkVector4f& vC, const hkSimdFloat32& tolerance, hkVector4f& normalOut, hkSimdFloat32& fractionInOut)
{
    hkVector4 ab; ab.setSub(vB, vA);
    hkVector4 ac; ac.setSub(vC, vA);
    hkVector4 n; n.setCross(ab, ac);

    hkVector4 rel; rel.setSub(ray.m_origin, vA);
    const hkSimdReal startDist = rel.dot<3>(n);
    const hkSimdReal dirDist = ray.m_direction.dot<3>(n);
    hkSimdReal invDirDist; invDirDist.setReciprocal<HK_ACC_23_BIT, HK_DIV_IGNORE>(dirDist);
    const hkSimdReal hitFraction = hkSimdReal_0 - invDirDist * startDist;
    const hkSimdReal endDist = dirDist + startDist;

    const hkSimdReal zero = hkSimdReal_0;
    hkVector4Comparison sameSide; sameSide.setOr(zero.equal(endDist), zero.notEqual(startDist));
    sameSide.setAnd(sameSide, zero.lessEqual(startDist * endDist));
    hkVector4Comparison mask; mask.setAndNot(hitFraction.less(fractionInOut), sameSide);
    if (!mask.anyIsSet())
    {
        return 0;
    }

    fractionInOut = hitFraction;
    hkVector4 normal; normal.setFlipSign(n, startDist);
    normalOut = normal;

    hkVector4 hit; hit.setMul(ray.m_direction, fractionInOut);
    hit.add(ray.m_origin);
    hkVector4 b; b.setSub(vB, hit);
    hkVector4 a; a.setSub(vA, hit);
    hkVector4 c; c.setSub(vC, hit);
    hkVector4 crossAB; crossAB.setCross(a, b);
    hkVector4 crossBC; crossBC.setCross(b, c);
    hkVector4 crossCA; crossCA.setCross(c, a);
    hkVector4 dots;
    hkVector4Util::dot3_4vs4(crossAB, n, crossBC, n, crossCA, n, n, n, dots);

    const hkSimdReal lenSq = dots.getComponent<3>();
    const hkSimdReal invLen = lenSq.sqrtInverse<HK_ACC_23_BIT, HK_SQRT_SET_ZERO>();
    normalOut.setMul(invLen, normal);

    hkVector4 tol; tol.setAll((hkSimdReal_0 - lenSq) * tolerance);
    hkVector4Comparison inside = tol.lessEqual(dots);
    inside.setAnd(inside, mask);
    return inside.allAreSet<hkVector4ComparisonMask::MASK_XYZ>();
}
}

// 0x00815A80 (hkptriangleshape.inl:43)
hkpTriangleShape::hkpTriangleShape(const hkVector4f& v0, const hkVector4f& v1, const hkVector4f& v2, float radius)
    : hkpConvexShape(hkcdShapeType::TRIANGLE, radius)
{
    m_vertexA = v0;
    m_vertexB = v1;
    m_vertexC = v2;
    m_extrusion.setZero();
    m_weldingInfo = 0;
    m_weldingType.m_storage = hkpWeldingUtility::WELDING_TYPE_NONE;
    m_isExtruded = 0;
}

// 0x00D95E60: the vertices are set by the caller
hkpTriangleShape::hkpTriangleShape(float radius, unsigned short weldingInfo, hkpWeldingUtility::WeldingType type)
    : hkpConvexShape(hkcdShapeType::TRIANGLE, radius)
{
    m_weldingInfo = weldingInfo;
    m_weldingType.m_storage = (unsigned char)type;
    m_extrusion.setZero();
    m_isExtruded = 0;
}

// 0x00D82BB0
hkpTriangleShape::hkpTriangleShape(hkFinishLoadedObjectFlag flag)
    : hkpConvexShape(flag)
{
    m_type.m_storage = hkcdShapeType::TRIANGLE;
}

// 0x00815B00
hkpTriangleShape::~hkpTriangleShape()
{
}

// 0x00B63D90 (hkptriangleshape.inl:57)
const hkVector4f* hkpTriangleShape::getVertices() const
{
    return &m_vertexA;
}

// 0x00D83980
const hkVector4f& hkpTriangleShape::getVertex(int i) const
{
    return (&m_vertexA)[i];
}

// 0x00DA6090
hkVector4f& hkpTriangleShape::getVertex(int i)
{
    return (&m_vertexA)[i];
}

// 0x00DA3FF0
void hkpTriangleShape::setVertex(int i, const hkVector4f& vertex)
{
    (&m_vertexA)[i] = vertex;
}

// 0x0104EAB0
unsigned short hkpTriangleShape::getWeldingInfo() const
{
    return m_weldingInfo;
}

// 0x0080D780 (hkptriangleshape.inl:16)
void hkpTriangleShape::setWeldingInfo(unsigned short info)
{
    m_weldingInfo = info;
}

// 0x00E84320
hkpWeldingUtility::WeldingType hkpTriangleShape::getWeldingType() const
{
    return (hkpWeldingUtility::WeldingType)m_weldingType.m_storage;
}

// 0x00E83FC0
bool hkpTriangleShape::isExtruded() const
{
    return m_isExtruded != 0;
}

// 0x00D95EB0
void hkpTriangleShape::setExtrusion(const hkVector4f& extrusion)
{
    m_isExtruded = extrusion.lengthSquared<3>().getReal() > 0.0f;
    m_extrusion = extrusion;
}

// 0x00815AE0 (.h:27)
int hkpTriangleShape::getSize() const
{
    return sizeof(*this);
}

// 0x00815AF0 (.h:27)
int hkpTriangleShape::calcSizeForSpu(const hkpShape::CalcSizeForSpuInput& input, int spuBufferSizeLeft) const
{
    return sizeof(*this);
}

// 0x00D829F0
void hkpTriangleShape::getFirstVertex(hkVector4f& v) const
{
    v = m_vertexA;
}

// 0x00D82BD0
void hkpTriangleShape::getCentre(hkVector4f& centreOut) const
{
    centreOut.setAdd(m_vertexB, m_vertexA);
    centreOut.add(m_vertexC);
    centreOut.mul(hkSimdReal_Inv3);
}

// 0x00815B10 (hkptriangleshape.inl:139)
int hkpTriangleShape::getNumCollisionSpheres() const
{
    return 3 + m_isExtruded * 3;
}

// 0x00D82B20: the vertices (and the extruded ones) with the convex radius
const hkSphere* hkpTriangleShape::getCollisionSpheres(hkSphere* sphereBuffer) const
{
    const hkSimdReal radius = hkSimdReal::fromFloat(m_radius);
    hkVector4* spheres = reinterpret_cast<hkVector4*>(sphereBuffer);
    spheres[0].setXYZ_W(m_vertexA, radius);
    spheres[1].setXYZ_W(m_vertexB, radius);
    spheres[2].setXYZ_W(m_vertexC, radius);
    if (m_isExtruded)
    {
        hkVector4 v;
        v.setAdd(m_vertexA, m_extrusion);
        spheres[3].setXYZ_W(v, radius);
        v.setAdd(m_extrusion, m_vertexB);
        spheres[4].setXYZ_W(v, radius);
        v.setAdd(m_vertexC, m_extrusion);
        spheres[5].setXYZ_W(v, radius);
    }
    return sphereBuffer;
}

// 0x00D82A60
// Original quirk kept: the (local) extrusion is added to the world-space bounds untransformed.
void hkpTriangleShape::getAabb(const hkTransformf& localToWorld, float tolerance, hkAabb& out) const
{
    hkVector4 tol4; tol4.setAll(m_radius + tolerance);
    hkVector4 v0; v0.setTransformedPos(localToWorld, m_vertexA);
    hkVector4 v1; v1.setTransformedPos(localToWorld, m_vertexB);
    hkVector4 v2; v2.setTransformedPos(localToWorld, m_vertexC);
    out.m_min.setMin(v0, v1);
    out.m_min.setMin(out.m_min, v2);
    out.m_max.setMax(v0, v1);
    out.m_max.setMax(out.m_max, v2);

    hkVector4 extruded;
    extruded.setAdd(m_extrusion, out.m_min);
    out.m_min.setMin(out.m_min, extruded);
    extruded.setAdd(m_extrusion, out.m_max);
    out.m_max.setMax(out.m_max, extruded);

    out.m_min.sub(tol4);
    out.m_max.setAdd(tol4, out.m_max);
}

// 0x00D82E30: the vertex furthest along the direction (the last on ties), extruded if the extrusion points along it
void hkpTriangleShape::getSupportingVertex(const hkVector4f& direction, hkcdVertex& supportingVertexOut) const
{
    hkFourTransposedPointsf points;
    points.set(m_vertexA, m_vertexB, m_vertexC, m_vertexC);
    hkVector4 dots;
    points.dot3(direction, dots);
    hkVector4 maxDot; maxDot.setAll(dots.horizontalMax<3>());
    hkVector4Comparison isMax = maxDot.lessEqual(dots);
    hkVector4Comparison xyz; xyz.set<hkVector4ComparisonMask::MASK_XYZ>();
    isMax.setAnd(isMax, xyz);
    int id = isMax.getIndexOfLastComponentSet();

    hkVector4 v = getVertex(id);
    if (m_extrusion.dot<3>(direction).getReal() > 0.0f)
    {
        v.add(m_extrusion);
        id += 3;
    }
    v.setInt24W(id);
    supportingVertexOut.assign(v);
}

// 0x00D82A00
void hkpTriangleShape::convertVertexIdsToVertices(const unsigned short* ids, int numIds, hkcdVertex* verticesOut) const
{
    for (int i = numIds - 1; i >= 0; i--)
    {
        const int id = *ids++;
        hkVector4 extrusion; extrusion.setAll(addExtrusionTable[id]);
        extrusion.mul(m_extrusion);
        hkVector4 v; v.setAdd(extrusion, getVertex(mod3Table[id]));
        v.setInt24W(id);
        (verticesOut++)->assign(v);
    }
}

// 0x00D83170
// The shipped build records a "rcTriangle" monitor timer around the query; it is not reproduced.
hkBool hkpTriangleShape::castRay(const hkpShapeRayCastInput& input, hkpShapeRayCastOutput& results) const
{
    hkcdRay ray;
    ray.setEndPoints(input.m_from, input.m_to);
    hkSimdReal fraction = hkSimdReal::fromFloat(results.m_hitFraction);
    const hkSimdReal tolerance = hkSimdReal::fromFloat(s_rayTolerance);
    hkVector4 normal;
    const hkBool32 hit = hkcdSegmentTriangleIntersect(ray, m_vertexA, m_vertexB, m_vertexC, tolerance, normal, fraction);
    if (hit)
    {
        results.m_normal = normal;
        fraction.store<1>(&results.m_hitFraction);
        results.setKey(0xFFFFFFFF); // HK_INVALID_SHAPE_KEY
    }
    return hit != 0;
}

// 0x00D82C00
// The shipped build records a "rayBundleTriangle" monitor timer around the query; it is not reproduced.
hkVector4fComparison hkpTriangleShape::castRayBundle(const hkpShapeRayBundleCastInput& input, hkpShapeRayBundleCastOutput& results, const hkVector4fComparison& mask) const
{
    hkcdRayBundle rayBundle;
    rayBundle.m_start = input.m_from;
    rayBundle.m_end = input.m_to;
    rayBundle.m_activeRays = mask;

    hkVector4 fractions;
    fractions.set(results.m_outputs[0].m_hitFraction, results.m_outputs[1].m_hitFraction, results.m_outputs[2].m_hitFraction, results.m_outputs[3].m_hitFraction);
    hkFourTransposedPointsf normals; // zeroed by the constructor
    const hkVector4fComparison hitMask = hkcdRayCastTriangle::hkcdSegmentBundleTriangleIntersect(rayBundle, m_vertexA, m_vertexB, m_vertexC, hkSimdReal::fromFloat(s_rayTolerance), normals, fractions);

    hkVector4 vNormals[4];
    normals.extract(vNormals[0], vNormals[1], vNormals[2], vNormals[3]);
    const int hits = hitMask.getMask();
    for (int i = 0; i < 4; i++)
    {
        if (hits & (1 << i))
        {
            results.m_outputs[i].m_hitFraction = fractions(i);
            results.m_outputs[i].m_normal = vNormals[i];
            results.m_outputs[i].setKey(0xFFFFFFFF); // HK_INVALID_SHAPE_KEY
        }
    }
    return hitMask;
}

// 0x00D83390
// Welding: a contact on an edge (or a vertex) whose normal leaves the edge's allowed angle range is snapped to the
// range, or rejected. featurePoints are the vertex ids the contact lies on; a contact on the face is accepted
// unmodified. The separating normal's w is the distance (scaled by the cosine to the triangle normal).
int hkpTriangleShape::weldContactPoint(unsigned short* featurePoints, unsigned char& numFeaturePoints, hkVector4f& contactPointWs, const hkTransformf* thisObjTransform, const hkpConvexShape* collidingShape, const hkTransformf* collidingTransform, hkVector4f& separatingNormalInOut) const
{
    const int weldingType = m_weldingType.m_storage;
    if (weldingType == hkpWeldingUtility::WELDING_TYPE_NONE)
    {
        return WELD_RESULT_ACCEPT_CONTACT_POINT_UNMODIFIED;
    }

    // the edge: from vertex edgeId to the next one
    int edgeId = mod3Table[featurePoints[0]];
    if (numFeaturePoints > 1)
    {
        int edgeId1 = mod3Table[featurePoints[1]];
        if (numFeaturePoints == 3)
        {
            if (featurePoints[1] + featurePoints[2] + featurePoints[0] == 3) // the face (0 + 1 + 2)
            {
                return WELD_RESULT_ACCEPT_CONTACT_POINT_UNMODIFIED;
            }
            if (edgeId1 == edgeId)
            {
                edgeId1 = mod3Table[featurePoints[2]];
            }
            numFeaturePoints = 2;
            featurePoints[0] = (unsigned short)edgeId;
            featurePoints[1] = (unsigned short)edgeId1;
        }
        if (mod3Table[edgeId + 1] != edgeId1)
        {
            edgeId = edgeId1;
        }
    }

    hkVector4 collisionNormal; collisionNormal._setRotatedInverseDir(thisObjTransform->getRotation(), separatingNormalInOut);
    hkVector4 triangleNormal;
    calcAntiClockwiseTriangleNormal(m_vertexA, m_vertexB, m_vertexC, triangleNormal);
    const hkSimdReal cosAngle = triangleNormal.dot<3>(collisionNormal);
    hkSimdReal absCos; absCos.setAbs(cosAngle);
    hkVector4 distance; distance.setAll(absCos * separatingNormalInOut.getComponent<3>());

    hkVector4 edge; edge.setSub(getVertex(mod3Table[edgeId + 1]), getVertex(edgeId));
    edge.normalize<3>();
    const int edgeBitcode = (m_weldingInfo >> (edgeId * 5)) & 0x1F;

    hkVector4 snappedNormalWs;
    if (weldingType == hkpWeldingUtility::WELDING_TYPE_TWO_SIDED)
    {
        const hkpWeldingUtility::SectorType sector = hkpWeldingUtility::getSector(triangleNormal, collisionNormal, edgeBitcode);
        if (sector != hkpWeldingUtility::SNAP_0)
        {
            if (sector == hkpWeldingUtility::REJECT)
            {
                return WELD_RESULT_REJECT_CONTACT_POINT;
            }
            if (sector != hkpWeldingUtility::SNAP_1)
            {
                return WELD_RESULT_ACCEPT_CONTACT_POINT_UNMODIFIED;
            }
        }
        if (numFeaturePoints == 1)
        {
            return WELD_RESULT_REJECT_CONTACT_POINT; // a vertex in a snap sector
        }
        hkpWeldingUtility::snapCollisionNormal(triangleNormal, edge, edgeBitcode, sector, collisionNormal);
        snappedNormalWs._setRotatedDir(thisObjTransform->getRotation(), collisionNormal);
    }
    else
    {
        if (edgeBitcode == hkpWeldingUtility::NUM_ANGLES)
        {
            return WELD_RESULT_ACCEPT_CONTACT_POINT_UNMODIFIED;
        }
        if (!hkpWeldingUtility::shouldSnapOneSided((hkpWeldingUtility::WeldingType)weldingType, triangleNormal, collisionNormal, edgeBitcode))
        {
            return WELD_RESULT_ACCEPT_CONTACT_POINT_UNMODIFIED;
        }
        hkVector4 snapVector;
        hkpWeldingUtility::calcSnapVectorOneSided(triangleNormal, edge, edgeBitcode, (hkpWeldingUtility::WeldingType)weldingType, snapVector);
        snappedNormalWs._setRotatedDir(thisObjTransform->getRotation(), snapVector);

        if (!m_isExtruded)
        {
            // the colliding shape's centre in front of the snapped plane: reject
            hkVector4 centre;
            collidingShape->getCentre(centre);
            centre.setTransformedPos(*collidingTransform, centre);
            hkVector4 toContact; toContact.setSub(contactPointWs, centre);
            if (toContact.dot<3>(snappedNormalWs).getReal() >= 0.0f)
            {
                return WELD_RESULT_REJECT_CONTACT_POINT;
            }
        }
        else
        {
            // the distance to the (flipped) triangle plane limits the distance
            hkVector4 flip; flip.setAll(flipNormalBasedOnWeldingTypeTable[weldingType]);
            hkVector4 planeNormal; planeNormal.setMul(flip, triangleNormal);
            hkVector4 contactLocal; contactLocal.setTransformedInversePos(*thisObjTransform, contactPointWs);
            hkVector4 rel; rel.setSub(contactLocal, m_vertexA);
            hkVector4 planeDistance; planeDistance.setAll(rel.dot<3>(planeNormal));
            distance.setMin(planeDistance, distance);
        }
    }

    separatingNormalInOut.setXYZ_W(snappedNormalWs, distance);
    return WELD_RESULT_ACCEPT_CONTACT_POINT_MODIFIED;
}
