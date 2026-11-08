// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/gjk/penetration/hkpcvxcvxdistbyheuristicsampling.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/gjk/hkpgsk.h"


class hkTransformf;
class hkcdVertex;
class hkpConvexShape;

void hkpCvxCvxDistByHeuristicSampling(const hkpConvexShape* shapeA, const hkpConvexShape* shapeB, const hkTransformf& btoa, hkcdVertex* pointAinAOut, hkcdVertex* pointBinBOut, hkpGskOut& output); // 0x00ED6830
