// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/hkpshapebase.cpp
//
// Havok 2014.2.5 (havok_impl.md HK3): hkpShapeBase, the default (empty) implementations of the shape queries, with
// the out-of-line copies of its inline members (hkpShapeBase.h/.inl in Havok 2014.1).

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/hkpshapebase.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkvector4fcomparison.h"

// 0x00D74400
hkpShapeBase::hkpShapeBase(hkFinishLoadedObjectFlag flag)
    : hkcdShape(flag)
{
    if (flag.m_finishing)
    {
        afterReflectNew();
    }
}

// inline in Havok (hkpShapeBase.h)
hkpShapeBase::hkpShapeBase(hkcdShapeType::ShapeTypeEnum type)
    : hkcdShape(type)
{
}

// inline in Havok (hkpShapeBase.h)
hkpShapeBase::hkpShapeBase()
{
}

// 0x00D743F0: a loaded hkpShapeBase is typed INVALID; the finishing constructors of the shape classes set their own
// type after it.
void hkpShapeBase::afterReflectNew()
{
    m_type.m_storage = hkcdShapeType::INVALID;
}

// 0x007108C0
hkpShapeBase::~hkpShapeBase()
{
}

// 0x00DF2D10 (hkpShapeBase.inl)
bool hkpShapeBase::isBvTree() const
{
    switch (m_type.m_storage)
    {
    case hkcdShapeType::BV_TREE:
    case hkcdShapeType::MOPP:
    case hkcdShapeType::STATIC_COMPOUND:
    case hkcdShapeType::BV_COMPRESSED_MESH:
    case hkcdShapeType::TRI_SAMPLED_HEIGHT_FIELD_BV_TREE:
        return true;
    default:
        return false;
    }
}

// inline in Havok (hkpShapeBase.inl)
bool hkpShapeBase::isCollection() const
{
    switch (m_type.m_storage)
    {
    case hkcdShapeType::COLLECTION:
    case hkcdShapeType::LIST:
    case hkcdShapeType::EXTENDED_MESH:
    case hkcdShapeType::TRI_SAMPLED_HEIGHT_FIELD_COLLECTION:
        return true;
    default:
        return false;
    }
}

// 0x0071A8C0 (.h:57)
bool hkpShapeBase::isConvex() const
{
    return false;
}

// 0x00D74350
void hkpShapeBase::getAabb(const hkTransformf& localToWorld, float tolerance, hkAabb& aabbOut) const
{
}

// 0x00D743D0
hkBool hkpShapeBase::castRay(const hkpShapeRayCastInput& input, hkpShapeRayCastOutput& output) const
{
    return false;
}

// 0x00D74360
void hkpShapeBase::castRayWithCollector(const hkpShapeRayCastInput& input, const hkpCdBody& cdBody, hkpRayHitCollector& collector) const
{
}

// 0x00D743E0
hkVector4fComparison hkpShapeBase::castRayBundle(const hkpShapeRayBundleCastInput& input, hkpShapeRayBundleCastOutput& output, const hkVector4fComparison& mask) const
{
    hkVector4fComparison none;
    none.set<hkVector4ComparisonMask::MASK_NONE>();
    return none;
}

// 0x00D74370
void hkpShapeBase::getSupportingVertex(const hkVector4f& direction, hkcdVertex& supportingVertexOut) const
{
}

// 0x00D74380
void hkpShapeBase::convertVertexIdsToVertices(const unsigned short* ids, int numIds, hkcdVertex* vertexArrayOut) const
{
}

// 0x00D74390
void hkpShapeBase::getCentre(hkVector4f& centreOut) const
{
}

// 0x00D743A0
int hkpShapeBase::getNumCollisionSpheres() const
{
    return 0;
}

// 0x00D743B0
const hkSphere* hkpShapeBase::getCollisionSpheres(hkSphere* sphereBuffer) const
{
    return nullptr;
}

// 0x00D743C0
int hkpShapeBase::weldContactPoint(unsigned short* featurePoints, unsigned char& numFeaturePoints, hkVector4f& contactPointWs, const hkTransformf* thisObjTransform, const hkpConvexShape* collidingConvexShape, const hkTransformf* collidingTransform, hkVector4f& separatingNormalInOut) const
{
    return 0; // WELD_RESULT_ACCEPT_CONTACT_POINT_UNMODIFIED
}
