// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/convex/hkpconvexshape.cpp
//
// Havok 2014.2.5 (havok_impl.md HK3): hkpConvexShape (a shape with a convex radius around its core) and the
// out-of-line copies of the inline members of hkpConvexShape and hkpSphereRepShape (hkpConvexShape.inl,
// hkpSphereRepShape.h in Havok 2014.1). hkpConvexTransformShapeBase's constructor is compiled here too.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/convex/hkpconvexshape.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/matrix/hktransformf.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/system/error/hkerror.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/geometry/aabb/hkaabb.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/geometry/internal/types/hkcdvertex.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcdbody.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/convex/convextransform/hkpconvextransformshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/query/hkprayhitcollector.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/query/hkpshaperaycastinput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/query/hkpshaperaycastoutput.h"

// 0x016F50EC: the convex radius Havok's shape builders use by default (rfg sets it to 0.01 at startup)
float hkConvexShapeDefaultRadius = 0.05f;

// ---- hkpSphereRepShape (hkpSphereRepShape.h)

// 0x00D74BF0
hkpSphereRepShape::hkpSphereRepShape(hkFinishLoadedObjectFlag flag)
    : hkpShape(flag)
{
    m_type.m_storage = hkcdShapeType::SPHERE_REP;
}

hkpSphereRepShape::hkpSphereRepShape(hkcdShapeType::ShapeTypeEnum type)
    : hkpShape(type)
{
}

hkpSphereRepShape::hkpSphereRepShape()
{
}

// 0x007114A0
hkpSphereRepShape::~hkpSphereRepShape()
{
}

// ---- hkpConvexShape

// 0x00D74A40
hkpConvexShape::hkpConvexShape(hkFinishLoadedObjectFlag flag)
    : hkpSphereRepShape(flag)
{
    m_type.m_storage = hkcdShapeType::CONVEX;
}

// inline in Havok (hkpConvexShape.inl)
hkpConvexShape::hkpConvexShape(hkcdShapeType::ShapeTypeEnum type, float radius)
    : hkpSphereRepShape(type)
{
    m_radius = radius;
}

hkpConvexShape::hkpConvexShape()
{
}

// 0x007114B0
hkpConvexShape::~hkpConvexShape()
{
}

// 0x0071B6C0 (.h:77)
bool hkpConvexShape::isConvex() const
{
    return true;
}

// 0x007114C0 (hkpconvexshape.inl:19)
const float& hkpConvexShape::getRadius() const
{
    return m_radius;
}

// 0x0080D570 (hkpconvexshape.inl:30); the debug warning for convex vertices shapes is compiled out
void hkpConvexShape::setRadius(float radius)
{
    setRadiusUnchecked(radius);
}

// 0x0080D560 (hkpconvexshape.inl:24)
void hkpConvexShape::setRadiusUnchecked(float radius)
{
    m_radius = radius;
}

// 0x0071B6D0 (.h:83): not implemented for this class; the error returns whatever EAX held (the subclasses that are
// used override it)
int hkpConvexShape::getSize() const
{
    // HK_ERROR(0x97ef335c, "Not implemented")
    if (hkErrorFwd::messageError(0x97EF335C, "Not implemented", "C:\\unit4projects\\rfg\\root\\code\\volition\\libmodules\\havok\\hk2014_2_5_r1\\Source\\Physics2012/Collide/Shape/Convex/hkpConvexShape.h", 0x53) != 0)
    {
        __debugbreak();
    }
    return 0;
}

// 0x00D749B0
int hkpConvexShape::weldContactPoint(unsigned short* featurePoints, unsigned char& numFeaturePoints, hkVector4f& contactPointWs, const hkTransformf* thisObjTransform, const hkpConvexShape* collidingShape, const hkTransformf* collidingTransform, hkVector4f& separatingNormalInOut) const
{
    return WELD_RESULT_ACCEPT_CONTACT_POINT_UNMODIFIED;
}

// 0x00D749F0: the centre of the local AABB
void hkpConvexShape::getCentre(hkVector4f& centreOut) const
{
    hkAabb aabb;
    getAabb(hkTransform::getIdentity(), 0.0f, aabb);
    aabb.getCenter(centreOut);
}

// 0x00D74A60
// The shape's own castRay; a hit is reported with its normal rotated to world space.
void hkpConvexShape::castRayWithCollector(const hkpShapeRayCastInput& inputLocal, const hkpCdBody& cdBody, hkpRayHitCollector& collector) const
{
    hkpShapeRayCastOutput results;
    results.m_hitFraction = collector.m_earlyOutHitFraction;
    if (castRay(inputLocal, results))
    {
        results.m_normal._setRotatedDir(cdBody.getTransform().getRotation(), results.m_normal);
        collector.addRayHit(cdBody, results);
    }
}

// 0x00D74B40: the supporting vertex's projection plus the convex radius
float hkpConvexShape::getMaximumProjection(const hkVector4f& direction) const
{
    hkcdVertex supportingVertex;
    getSupportingVertex(direction, supportingVertex);
    const hkSimdReal dist = supportingVertex.dot<3>(direction);
    const hkSimdReal radius = hkSimdReal::fromFloat(m_radius);
    return (radius * direction.length<3>() + dist).getReal();
}

// ---- hkpConvexTransformShapeBase (hkpConvexTransformShapeBase in hkpConvexTransformShape.h)

// 0x00D74AF0
hkpConvexTransformShapeBase::hkpConvexTransformShapeBase(hkcdShapeType::ShapeTypeEnum type, float radius, const hkpConvexShape* childShape, hkpShapeContainer::ReferencePolicy ref)
    : hkpConvexShape(type, radius)
    , m_childShape(childShape, ref)
{
}
