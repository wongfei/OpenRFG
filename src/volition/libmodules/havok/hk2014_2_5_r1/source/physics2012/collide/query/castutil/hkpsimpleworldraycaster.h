// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/query/castutil/hkpsimpleworldraycaster.h
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
struct hkpWorldRayCastInput;
struct hkpWorldRayCastOutput;

// size 0x40
class hkpSimpleWorldRayCaster : public hkpBroadPhaseCastCollector
{
public:
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpSimpleWorldRayCaster); // delete(void* p, unsigned int nbytes) 0x00DEA260
    hkpSimpleWorldRayCaster(); // 0x00DEA240
    virtual ~hkpSimpleWorldRayCaster(); // vtbl+0x00 0x00DE86C0
    void castRay(const hkpBroadPhase& broadphase, const hkpWorldRayCastInput& input, const hkpCollisionFilter* filter, const char* cache, hkpWorldRayCastOutput& output); // 0x0104B940
    void castRay(const hkpBroadPhase& broadphase, const hkpWorldRayCastInput& input, const hkpCollisionFilter* filter, hkpWorldRayCastOutput& output); // 0x0104B850
    void castRayGroup(const hkpBroadPhase& broadphase, const hkpWorldRayCastInput* inputArray, int numRays, const hkpCollisionFilter* filter, hkpWorldRayCastOutput* outputs); // 0x0104BA30
    void castRaysFromSinglePoint(const hkpBroadPhase& broadphase, const hkpWorldRayCastInput* inputArray, int numRays, const hkpCollisionFilter* filter, const char* cache, hkpWorldRayCastOutput* outputs); // 0x0104BBC0
public:
    virtual float addBroadPhaseHandle(const hkpBroadPhaseHandle* broadPhaseHandle, int castIndex); // vtbl+0x04 0x0104B640

    const hkpWorldRayCastInput* m_input; // 0x04
    const hkpRayCollidableFilter* m_filter; // 0x08
    hkpWorldRayCastOutput* m_result; // 0x0C
    hkpShapeRayCastInput m_shapeInput; // 0x10
};
