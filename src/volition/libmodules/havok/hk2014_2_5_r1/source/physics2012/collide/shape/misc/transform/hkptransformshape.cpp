// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/misc/transform/hkptransformshape.cpp
//
// Havok 2014.2.5 (havok_impl.md HK3): a child shape under a transform. Only the accessor of the implementation
// closure is here (rfg's havok wrapper reads the child of loaded transform shapes); the queries and constructors stay
// declared until a phase needs them.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/misc/transform/hkptransformshape.h"

// 0x00A12FE0 (hkptransformshape.inl:10)
const hkpShape* hkpTransformShape::getChildShape() const
{
    return m_childShape.getChild();
}
