// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/util/hkpcollidecapsuleutil.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/util/hkpcollidetriangleutil.h"

class hkContactPoint;
struct hkResult;
class hkVector4f;
struct hkpFeatureOutput;

hkResult hkCollideCapsuleUtilClostestPointCapsVsCaps(const hkVector4f* capsA, const float& capsARadius, const hkVector4f* capsB, const float& capsBRadius, float collisionTolerance, hkContactPoint& cpoint); // 0x00DD26F0
void hkCollideCapsuleUtilCapsVsTri(const hkVector4f* capsAPoints, float capsARadius, const hkVector4f* triVertices, float triBRadius, const hkpCollideTriangleUtil::PointTriangleDistanceCache& cache, float collisionTolerance, int searchManifold, hkContactPoint* points, hkpFeatureOutput* featuresOut); // 0x00DD2BC0
