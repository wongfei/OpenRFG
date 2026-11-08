// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/query/castutil/hkpsimpleworldraycaster.cpp
//
// Havok 2014.2.5 (havok_impl.md HK5): world ray casts into a closest-hit output (no collector), from rfg.exe: the
// broadphase calls addBroadPhaseHandle for each object along the ray, which ray casts the shape straight into the
// output (hkpShape::castRay keeps only a closer hit) and turns a new hit's normal into world space.
//
// Only the cached castRay is reached (hkpWorld::castRay); the uncached castRay (0x0104B850), castRayGroup (0x0104BA30)
// and castRaysFromSinglePoint (0x0104BBC0) have no caller in scope. The shipped build records the "RayCstCchSim"
// monitor timer (TLS hkMonitorStream); it is not reproduced.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/query/castutil/hkpsimpleworldraycaster.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/hkmath.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcollidable.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/broadphase/hkpbroadphase.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/dispatch/broadphase/hkptypedbroadphasehandle.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/filter/hkpcollisionfilter.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/query/castutil/hkpworldraycastinput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/query/castutil/hkpworldraycastoutput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/hkpshape.h"

// 0x00DEA240 (.h:32): only the shape input is initialized
hkpSimpleWorldRayCaster::hkpSimpleWorldRayCaster()
{
}

// 0x00DE86C0 (.h:34)
hkpSimpleWorldRayCaster::~hkpSimpleWorldRayCaster()
{
}

// 0x0104B940
// One ray (m_numCasts 1, its end at input.m_to), with the broadphase's AABB cache when one is given.
void hkpSimpleWorldRayCaster::castRay(const hkpBroadPhase& broadphase, const hkpWorldRayCastInput& input, const hkpCollisionFilter* filter, const char* cache, hkpWorldRayCastOutput& output)
{
    m_input = &input;
    m_result = &output;
    m_filter = filter; // the hkpRayCollidableFilter base (+0x14), null for no filter
    m_shapeInput.m_rayShapeCollectionFilter = input.m_enableShapeCollectionFilter.m_bool ? filter : nullptr; // +0x10

    hkpBroadPhase::hkpCastRayInput rayInput;
    rayInput.m_from = input.m_from;
    rayInput.m_numCasts = 1;
    rayInput.m_toBase = &input.m_to;
    rayInput.m_toStriding = sizeof(hkVector4f);
    rayInput.m_aabbCacheInfo = cache;
    broadphase.castRay(rayInput, this, 0);
}

// 0x0104B640
// The ray of cast castIndex (its own input, start included) against one broadphase object: a collidable with a shape
// that the ray filter accepts is ray cast in its own space into the cast's output. On a (closer) hit the output gets
// the root collidable and its normal is rotated back into world space. Returns the output's hit fraction. The filter
// is called without a null check (a null filter crashes in the original too).
float hkpSimpleWorldRayCaster::addBroadPhaseHandle(const hkpBroadPhaseHandle* broadPhaseHandle, int castIndex)
{
    const hkpCollidable* collidable = (const hkpCollidable*)static_cast<const hkpTypedBroadPhaseHandle*>(broadPhaseHandle)->getOwner();
    const hkpShape* shape = collidable->m_shape;
    hkpWorldRayCastOutput& output = m_result[castIndex];
    if (shape != nullptr)
    {
        const hkpWorldRayCastInput& input = m_input[castIndex];
        if (m_filter->isCollisionEnabled(input, *collidable).m_bool)
        {
            const hkTransformf& transform = collidable->getTransform();
            m_shapeInput.m_from._setTransformedInversePos(transform, input.m_from);
            m_shapeInput.m_to._setTransformedInversePos(transform, input.m_to);
            m_shapeInput.m_filterInfo = input.m_filterInfo;
            m_shapeInput.m_collidable = collidable;
            m_shapeInput.m_userData = input.m_userData;
            if (shape->castRay(m_shapeInput, output).m_bool)
            {
                output.m_rootCollidable = collidable;
                output.m_normal._setRotatedDir(transform.getRotation(), output.m_normal);
            }
        }
    }
    return output.m_hitFraction;
}
