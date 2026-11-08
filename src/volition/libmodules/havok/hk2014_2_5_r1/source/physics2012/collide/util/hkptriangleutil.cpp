// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/util/hkptriangleutil.cpp
//
// Havok 2014.2.5 (havok_impl.md HK3): the default triangle degeneracy tolerance.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/util/hkptriangleutil.h"

// 0x016F66AC: 1e-7 (0x33D6BF95), on the squared length of the triangle normals
float hkDefaultTriangleDegeneracyTolerance = 1e-7f;
