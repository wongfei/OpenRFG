// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/query/castutil/hkpworldlinearcaster.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/geometry/collide/shapes/hkcdshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/query/hkplinearcastcollisioninput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/broadphase/hkpbroadphasecastcollector.h"

class hkpBroadPhase;
class hkpBroadPhaseHandle;
class hkpCdPointCollector;
class hkpCollidable;
class hkpCollidableCollidableFilter;
struct hkpCollisionAgentConfig;
struct hkpCollisionInput;
struct hkpLinearCastInput;

// size 0x90
class hkpWorldLinearCaster : public hkpBroadPhaseCastCollector
{
public:
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpWorldLinearCaster); // delete(void* p, unsigned int nbytes) 0x00DEA2E0
    hkpWorldLinearCaster(); // 0x00DEA2C0
    virtual ~hkpWorldLinearCaster(); // vtbl+0x00 0x00DE86D0
    void linearCast(const hkpBroadPhase& broadphase, const hkpCollidable* collA, const hkpLinearCastInput& input, const hkpCollidableCollidableFilter* filter, const hkpCollisionInput& collInput, hkpCollisionAgentConfig* config, char* m_broadPhaseCache, hkpCdPointCollector& castCollector, hkpCdPointCollector* startPointCollector); // 0x0104BD80
public:
    virtual float addBroadPhaseHandle(const hkpBroadPhaseHandle* broadPhaseHandle, int castIndex); // vtbl+0x04 0x0104BCF0

    const hkpLinearCastInput* m_input; // 0x04
    const hkpCollidableCollidableFilter* m_filter; // 0x08
    hkpCdPointCollector* m_castCollector; // 0x0C
    hkpCdPointCollector* m_startPointCollector; // 0x10
    const hkpCollidable* m_collidableA; // 0x14
    hkcdShapeType::ShapeTypeEnum m_typeA; // 0x18
    hkpLinearCastCollisionInput m_shapeInput; // 0x20
};
