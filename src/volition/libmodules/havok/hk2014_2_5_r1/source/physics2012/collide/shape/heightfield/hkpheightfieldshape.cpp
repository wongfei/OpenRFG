// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/heightfield/hkpheightfieldshape.cpp
//
// Havok 2014.2.5 (havok_impl.md HK3): the height field shape interface (sphere collision and sphere casts), from
// rfg.exe.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/heightfield/hkpheightfieldshape.h"

// 0x00D9BE30
hkpHeightFieldShape::hkpHeightFieldShape(hkcdShapeType::ShapeTypeEnum type)
    : hkpShape(type)
{
}

// 0x00D9BEA0
hkpHeightFieldShape::hkpHeightFieldShape(hkFinishLoadedObjectFlag flag)
    : hkpShape(flag)
{
    m_type.m_storage = hkcdShapeType::HEIGHT_FIELD;
}

// 0x00D9BA90
hkpHeightFieldShape::~hkpHeightFieldShape()
{
}

// 0x00D8FA60 (the ray part is set by its constructor; radius and penetration are left to the caller)
hkpHeightFieldShape::hkpSphereCastInput::hkpSphereCastInput()
{
}
