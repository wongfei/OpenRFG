// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/query/castutil/hkpworldraycaster.cpp
//
// Havok 2014.2.5 (havok_impl.md HK5): world ray casts into ray hit collectors, from rfg.exe. The caster is the
// broadphase cast collector: the broadphase walks its boxes along the ray(s) and calls addBroadPhaseHandle for each
// object, which filters the collidable, moves the ray into its space and ray casts its shape into the collector; the
// collector's early-out fraction shortens the broadphase walk.
//
// The shipped build records the "RayCstCached"/"RayCastGroup"/"RayCastFSP" monitor timers (TLS hkMonitorStream); they
// are not reproduced. The out-of-line copies of the inline constructors (hkpworldraycaster.h,
// hkpworldraycastinput.h) are here too.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/query/castutil/hkpworldraycaster.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/hkmath.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/router/hkmemoryrouter.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/geometry/aabb/hkaabb.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcollidable.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/broadphase/hkpbroadphase.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/dispatch/broadphase/hkptypedbroadphasehandle.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/filter/hkpcollisionfilter.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/query/castutil/hkpworldraycastinput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/hkpshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/query/hkprayhitcollector.h"

// 0x00811430 (hkpworldraycastinput.h:39)
hkpWorldRayCastInput::hkpWorldRayCastInput()
{
    m_enableShapeCollectionFilter.m_bool = 0;
    m_filterInfo = 0;
    m_userData = 0;
}

// 0x008207B0 (.h:32): only the shape input is initialized
hkpWorldRayCaster::hkpWorldRayCaster()
{
}

// 0x00812620 (.h:34)
hkpWorldRayCaster::~hkpWorldRayCaster()
{
}

// 0x00D93250
// One ray (m_numCasts 1, its end at input.m_to), with the broadphase's AABB cache when one is given.
void hkpWorldRayCaster::castRay(const hkpBroadPhase& broadphase, const hkpWorldRayCastInput& input, const hkpCollisionFilter* filter, const char* cache, hkpRayHitCollector& collector)
{
    m_input = &input;
    m_collectorBase = &collector;
    m_collectorStriding = 0;
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

// 0x00D93340
// Rays with their own start points: one AABB cache over every ray (the box of all starts and ends), then one cached
// castRay per ray into its collector (collectorBase + i * collectorStriding).
void hkpWorldRayCaster::castRayGroup(const hkpBroadPhase& broadphase, const hkpWorldRayCastInput* inputArray, int numRays, const hkpCollisionFilter* filter, hkpRayHitCollector* collectorBase, int collectorStriding)
{
    hkAabb aabb;
    aabb.m_min.setMin(inputArray[0].m_from, inputArray[0].m_to);
    aabb.m_max.setMax(inputArray[0].m_from, inputArray[0].m_to);
    for (int i = 1; i < numRays; i++)
    {
        aabb.m_min.setMin(aabb.m_min, inputArray[i].m_to);
        aabb.m_max.setMax(aabb.m_max, inputArray[i].m_to);
        aabb.m_min.setMin(aabb.m_min, inputArray[i].m_from);
        aabb.m_max.setMax(aabb.m_max, inputArray[i].m_from);
    }

    // hkLocalBuffer<char> cache(size) on the LIFO allocator (rounded up to 128 bytes)
    const int cacheBytes = (broadphase.getAabbCacheSize() + 127) & ~127;
    hkLifoAllocator& stack = hkMemoryRouter::getInstance().stack();
    char* cache = (char*)stack.fastBlockAlloc(cacheBytes);
    broadphase.calcAabbCache(aabb, cache);

    const hkpWorldRayCastInput* input = inputArray;
    hkpRayHitCollector* collector = collectorBase;
    for (int i = numRays - 1; i >= 0; i--)
    {
        castRay(broadphase, *input, filter, cache, *collector);
        collector = (hkpRayHitCollector*)((char*)collector + collectorStriding);
        input++;
    }

    hkMemoryRouter::getInstance().stack().fastBlockFree(cache, cacheBytes);
}

// 0x00D934D0
// Rays that share the first ray's start point, cast together (m_numCasts numRays, the ends strided over the inputs):
// cast i goes into collectorBase + i * collectorStriding.
void hkpWorldRayCaster::castRaysFromSinglePoint(const hkpBroadPhase& broadphase, const hkpWorldRayCastInput* inputArray, int numRays, const hkpCollisionFilter* filter, const char* cache, hkpRayHitCollector* collectorBase, int collectorStriding)
{
    m_input = inputArray;
    m_filter = filter;
    m_collectorBase = collectorBase;
    m_collectorStriding = collectorStriding;
    m_shapeInput.m_rayShapeCollectionFilter = inputArray->m_enableShapeCollectionFilter.m_bool ? filter : nullptr;

    hkpBroadPhase::hkpCastRayInput rayInput;
    rayInput.m_from = inputArray->m_from;
    rayInput.m_numCasts = numRays;
    rayInput.m_toBase = &inputArray->m_to;
    rayInput.m_toStriding = sizeof(hkpWorldRayCastInput);
    rayInput.m_aabbCacheInfo = cache;
    broadphase.castRay(rayInput, this, 0);
}

// 0x00D93070
// The ray of cast castIndex against one broadphase object: a collidable with a shape that the ray filter accepts is
// ray cast in its own space into the cast's collector. The ray starts at the first input's m_from (castRay and
// castRaysFromSinglePoint share it; castRayGroup casts one input at a time), its end, filter info and user data are
// the cast's. Returns the collector's early-out fraction. The filter is called without a null check (a null filter
// crashes in the original too).
float hkpWorldRayCaster::addBroadPhaseHandle(const hkpBroadPhaseHandle* broadPhaseHandle, int castIndex)
{
    const hkpCollidable* collidable = (const hkpCollidable*)static_cast<const hkpTypedBroadPhaseHandle*>(broadPhaseHandle)->getOwner();
    const hkpShape* shape = collidable->m_shape;
    hkpRayHitCollector* collector = (hkpRayHitCollector*)((char*)m_collectorBase + m_collectorStriding * castIndex);
    if (shape != nullptr)
    {
        const hkpWorldRayCastInput& input = m_input[castIndex];
        if (m_filter->isCollisionEnabled(input, *collidable).m_bool)
        {
            const hkTransformf& transform = collidable->getTransform();
            m_shapeInput.m_from._setTransformedInversePos(transform, m_input->m_from);
            m_shapeInput.m_to._setTransformedInversePos(transform, input.m_to);
            m_shapeInput.m_filterInfo = input.m_filterInfo;
            m_shapeInput.m_collidable = collidable;
            m_shapeInput.m_userData = input.m_userData;
            shape->castRayWithCollector(m_shapeInput, *collidable, *collector);
        }
    }
    return collector->m_earlyOutHitFraction;
}
