// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/hkpshape.cpp
//
// Havok 2014.2.5 (havok_impl.md HK3): hkpShape, with the out-of-line copies of its inline members (hkpShape.h/.inl in
// Havok 2014.1).

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/hkpshape.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/matrix/hktransformf.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/geometry/aabb/hkaabb.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/query/hkpshaperaybundlecastinput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/query/hkpshaperaycastinput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/query/hkpshaperaycastoutput.h"

// 0x00D746A0
hkpShape::hkpShape(hkFinishLoadedObjectFlag flag)
    : hkpShapeBase(flag)
{
    if (flag.m_finishing)
    {
        afterReflectNew();
    }
}

// inline in Havok (hkpShape.inl)
hkpShape::hkpShape(hkcdShapeType::ShapeTypeEnum type)
    : hkpShapeBase(type)
    , m_userData(0)
{
}

// inline in Havok (hkpShape.h)
hkpShape::hkpShape()
{
}

// 0x00D745D0: as hkpShapeBase::afterReflectNew
void hkpShape::afterReflectNew()
{
    m_type.m_storage = hkcdShapeType::INVALID;
}

// 0x007108D0
hkpShape::~hkpShape()
{
}

// 0x00D74470
// The ray bundle cast of a shape without its own: the four rays one by one, the ones the mask selects.
hkVector4fComparison hkpShape::castRayBundle(const hkpShapeRayBundleCastInput& input, hkpShapeRayBundleCastOutput& output, const hkVector4fComparison& mask) const
{
    hkVector4 from[4];
    hkVector4 to[4];
    input.m_from.extract(from[0], from[1], from[2], from[3]);
    input.m_to.extract(to[0], to[1], to[2], to[3]);

    // the user data of the bundle is not passed on
    hkpShapeRayCastInput rayInput;
    rayInput.m_filterInfo = input.m_filterInfo;
    rayInput.m_rayShapeCollectionFilter = input.m_rayShapeCollectionFilter;

    int hitMask = 0;
    for (int i = 0; i < 4; i++)
    {
        if (mask.getMask() & (1 << i))
        {
            rayInput.m_from = from[i];
            rayInput.m_to = to[i];
            if (castRay(rayInput, output.m_outputs[i]))
            {
                hitMask |= 1 << i;
            }
        }
    }
    hkVector4fComparison result;
    result.set((hkVector4ComparisonMask::Mask)hitMask);
    return result;
}

// 0x0071A990 (.h:92)
const hkpShapeContainer* hkpShape::getContainer() const
{
    return nullptr;
}

// 0x00D76030 (hkpShape.inl)
unsigned long hkpShape::getUserData() const
{
    return m_userData;
}

// 0x007108E0 (hkpshape.inl:20)
void hkpShape::setUserData(unsigned long data)
{
    m_userData = data;
}

// 0x00D745E0
// The extent of the shape's AABB along a direction (the AABB corner the direction points to).
float hkpShape::getMaximumProjection(const hkVector4f& direction) const
{
    hkAabb aabb;
    getAabb(hkTransform::getIdentity(), 0.0f, aabb);

    hkVector4 halfExtents; halfExtents.setSub(aabb.m_max, aabb.m_min);
    halfExtents.mul(hkSimdReal_Inv2);
    hkVector4 center; center.setAdd(aabb.m_max, aabb.m_min);
    center.mul(hkSimdReal_Inv2);

    hkVector4 support; support.setFlipSign(halfExtents, direction);
    support.add(center);
    return support.dot<3>(direction).getReal();
}

// 0x00D74460: not on SPU
int hkpShape::calcSizeForSpu(const CalcSizeForSpuInput& input, int spuBufferSizeLeft) const
{
    return -1;
}
