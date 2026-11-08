// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/query/castutil/hkpworldraycaster.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/broadphase/hkpbroadphasecastcollector.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/query/hkpshaperaycastinput.h"

class hkpBroadPhase;
class hkpBroadPhaseHandle;
class hkpCollisionFilter;
class hkpRayCollidableFilter;
class hkpRayHitCollector;
struct hkpWorldRayCastInput;

// size 0x50
class hkpWorldRayCaster : public hkpBroadPhaseCastCollector
{
public:
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpWorldRayCaster); // delete(void* p, unsigned int nbytes) 0x008207D0
    hkpWorldRayCaster(); // 0x008207B0 (.h:32)
    virtual ~hkpWorldRayCaster(); // vtbl+0x00 0x00812620 (.h:34)
    void castRay(const hkpBroadPhase& broadphase, const hkpWorldRayCastInput& input, const hkpCollisionFilter* filter, const char* cache, hkpRayHitCollector& collector); // 0x00D93250
    void castRayGroup(const hkpBroadPhase& broadphase, const hkpWorldRayCastInput* inputArray, int numRays, const hkpCollisionFilter* filter, hkpRayHitCollector* collectorBase, int collectorStriding); // 0x00D93340
    void castRaysFromSinglePoint(const hkpBroadPhase& broadphase, const hkpWorldRayCastInput* inputArray, int numRays, const hkpCollisionFilter* filter, const char* cache, hkpRayHitCollector* collectorBase, int collectorStriding); // 0x00D934D0
public:
    virtual float addBroadPhaseHandle(const hkpBroadPhaseHandle* broadPhaseHandle, int castIndex); // vtbl+0x04 0x00D93070

    const hkpWorldRayCastInput* m_input; // 0x04
    const hkpRayCollidableFilter* m_filter; // 0x08
    hkpRayHitCollector* m_collectorBase; // 0x0C
    int m_collectorStriding; // 0x10
    hkpShapeRayCastInput m_shapeInput; // 0x20
};
