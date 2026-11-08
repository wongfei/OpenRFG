// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/heightfield/hkpsphererepshape.h
//
// Seam only (Havok 2014.2.5).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/geometry/collide/shapes/hkcdshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/hkpshape.h"

class hkClass;
class hkFinishLoadedObjectFlag;

// size 0x10
class hkpSphereRepShape : public hkpShape
{
public:
    enum
    {
        THIS_SHAPE_TYPE = 29,
    };

    static const hkClass& staticClass(); // 0x00D7E710
    hkpSphereRepShape(hkFinishLoadedObjectFlag flag); // 0x00D74BF0
    hkpSphereRepShape(hkcdShapeType::ShapeTypeEnum);
    hkpSphereRepShape();
    virtual ~hkpSphereRepShape(); // vtbl+0x00 0x007114A0

};
