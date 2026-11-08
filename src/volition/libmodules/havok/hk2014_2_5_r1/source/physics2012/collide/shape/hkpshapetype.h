// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/hkpshapetype.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/geometry/collide/shapes/hkcdshape.h"

class hkpCollisionDispatcher;

void hkpRegisterAlternateShapeTypes(hkpCollisionDispatcher* dis); // 0x00DCA900
const char* hkGetShapeTypeName(hkcdShapeType::ShapeTypeEnum type); // 0x00DCA9F0
