// Original: volition/libmodules/havok/hk2014_2_5_r1/source/geometry/collide/shapes/hkcdshape.cpp
//
// Havok 2014.2.5 (havok_impl.md HK3): hkcdShape, the base of every shape (type, dispatch type, key bits, codec), with
// the out-of-line copies of its inline members (hkcdShape.h/.inl in Havok 2014.1).

#include "volition/libmodules/havok/hk2014_2_5_r1/source/geometry/collide/shapes/hkcdshape.h"

// 0x00D1B400
hkcdShape::hkcdShape(hkFinishLoadedObjectFlag flag)
    : hkReferencedObject(flag)
{
}

// inline in Havok (hkcdShape.h)
hkcdShape::hkcdShape()
{
}

// inline in Havok (hkcdShape.h): new shapes dispatch as USER until a shape class says otherwise
hkcdShape::hkcdShape(hkcdShapeType::ShapeTypeEnum type)
{
    m_type.m_storage = (unsigned char)type;
    m_dispatchType.m_storage = hkcdShapeDispatchType::USER;
    m_bitsPerKey = 0;
    m_shapeInfoCodecType.m_storage = hkcdShapeInfoCodecType::NULL_CODEC;
}

// 0x007108B0
hkcdShape::~hkcdShape()
{
}

// inline in Havok (hkcdShape.inl)
hkcdShapeType::ShapeTypeEnum hkcdShape::getType() const
{
    return (hkcdShapeType::ShapeTypeEnum)m_type.m_storage;
}

// 0x00D74450 (hkcdShape.inl)
void hkcdShape::setType(hkcdShapeType::ShapeTypeEnum newType)
{
    m_type.m_storage = (unsigned char)newType;
}
