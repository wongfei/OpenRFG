// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/convex/box/hkpboxshape.cpp
//
// Havok 2014.2.5 (havok_impl.md HK0, HK3): a box centred at the origin, the convex radius around it, with the
// out-of-line copies of its inline members (hkpBoxShape.h/.inl in Havok 2014.1). The w of m_halfExtents holds the
// smallest half extent. Vertex ids are 3-bit masks of the negative axes.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/convex/box/hkpboxshape.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/matrix/hktransformf.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/geometry/aabb/hkaabb.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/geometry/aabb/hkaabbutil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/geometry/sphere/hksphere.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/geometry/internal/algorithms/raycast/hkcdraycastbox.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/geometry/internal/types/hkcdvertex.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/query/hkpshaperaycastinput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/query/hkpshaperaycastoutput.h"

// 0x00D821A0
hkpBoxShape::hkpBoxShape(const hkVector4f& halfExtents, float radius)
    : hkpConvexShape(hkcdShapeType::BOX, radius)
{
    m_halfExtents = halfExtents;
    setHalfExtents(m_halfExtents);
}

// 0x00D82220
hkpBoxShape::hkpBoxShape(hkFinishLoadedObjectFlag flag)
    : hkpConvexShape(flag)
{
    m_type.m_storage = hkcdShapeType::BOX;
}

hkpBoxShape::hkpBoxShape()
{
}

// 0x00D82040
hkpBoxShape::~hkpBoxShape()
{
}

// 0x00D82240: the smallest half extent goes to w
void hkpBoxShape::setHalfExtents(const hkVector4f& halfExtents)
{
    m_halfExtents.setXYZ_W(halfExtents, halfExtents.horizontalMin<3>());
}

// 0x00DBED90 (hkpBoxShape.inl)
const hkVector4f& hkpBoxShape::getHalfExtents() const
{
    return m_halfExtents;
}

// 0x00D828B0 (.h)
int hkpBoxShape::getSize() const
{
    return sizeof(*this);
}

// 0x00D828C0 (.h)
int hkpBoxShape::calcSizeForSpu(const hkpShape::CalcSizeForSpuInput& input, int spuBufferSizeLeft) const
{
    return sizeof(*this);
}

// 0x00D82060
void hkpBoxShape::getAabb(const hkTransformf& localToWorld, float tolerance, hkAabb& out) const
{
    hkAabbUtil::calcAabb(localToWorld, m_halfExtents, hkSimdReal::fromFloat(m_radius + tolerance), out);
}

// 0x00D82280: the corner in the direction; its id is the mask of its negative axes
void hkpBoxShape::getSupportingVertex(const hkVector4f& direction, hkcdVertex& supportingVertexOut) const
{
    hkVector4 support; support.setFlipSign(m_halfExtents, direction);
    support.setInt24W(support.lessZero().getMask<hkVector4ComparisonMask::MASK_XYZ>());
    supportingVertexOut.assign(support);
}

// 0x00D82100
void hkpBoxShape::convertVertexIdsToVertices(const unsigned short* ids, int numIds, hkcdVertex* verticesOut) const
{
    for (int i = numIds - 1; i >= 0; i--)
    {
        const int id = *ids++;
        hkVector4Comparison mask; mask.set((hkVector4ComparisonMask::Mask)id);
        hkVector4 v; v.setFlipSign(m_halfExtents, mask);
        v.setInt24W(id);
        (verticesOut++)->assign(v);
    }
}

// 0x00D820F0
void hkpBoxShape::getCentre(hkVector4f& centreOut) const
{
    centreOut.setZero();
}

// 0x00D82050
void hkpBoxShape::getFirstVertex(hkVector4f& v) const
{
    v = m_halfExtents;
}

// 0x00D828D0 (hkpBoxShape.inl)
int hkpBoxShape::getNumCollisionSpheres() const
{
    return 8;
}

// 0x00D82150: the 8 corners with the convex radius
const hkSphere* hkpBoxShape::getCollisionSpheres(hkSphere* sphereBuffer) const
{
    hkVector4 corner; corner.setXYZ_W(m_halfExtents, hkSimdReal::fromFloat(m_radius));
    for (int i = 0; i < 8; i++)
    {
        hkVector4Comparison mask; mask.set((hkVector4ComparisonMask::Mask)i);
        sphereBuffer[i].getPositionAndRadius().setFlipSign(corner, mask);
    }
    return sphereBuffer;
}

// 0x00D822D0 (:144)
// The slab test against the box grown by the convex radius. The normal is only written on a hit. The shipped build
// records a "rcBox" monitor timer around the query (TLS hkMonitorStream); it is not reproduced.
hkBool hkpBoxShape::castRay(const hkpShapeRayCastInput& input, hkpShapeRayCastOutput& results) const
{
    hkcdRay ray;
    ray.setEndPoints(input.m_from, input.m_to);
    hkVector4 halfExtents; halfExtents.setAll(m_radius);
    halfExtents.add(m_halfExtents);
    hkSimdReal fraction = hkSimdReal::fromFloat(results.m_hitFraction);
    hkVector4 normal;
    if (hkcdRayCastBox(ray, halfExtents, &fraction, &normal))
    {
        results.m_normal = normal;
        fraction.store<1>(&results.m_hitFraction);
        results.setKey(0xFFFFFFFF); // HK_INVALID_SHAPE_KEY: a box has no children
        return true;
    }
    return false;
}
