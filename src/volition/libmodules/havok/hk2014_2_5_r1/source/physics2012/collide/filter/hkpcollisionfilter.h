// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/filter/hkpcollisionfilter.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/object/hkreferencedobject.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/filter/hkpcollidablecollidablefilter.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/filter/hkpraycollidablefilter.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/filter/hkpshapecollectionfilter.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/query/hkprayshapecollectionfilter.h"

class hkAabb;
class hkClass;
struct hkInternalClassMember;
class hkpBvTreeShape;
class hkpCdBody;
class hkpCollidable;
struct hkpCollisionInput;
class hkpShapeContainer;
struct hkpShapeRayCastInput;
class hkpWorld;
struct hkpWorldRayCastInput;

// size 0x30
class hkpCollisionFilter : public hkReferencedObject, public hkpCollidableCollidableFilter, public hkpShapeCollectionFilter, public hkpRayShapeCollectionFilter, public hkpRayCollidableFilter
{
public:
    enum hkpFilterType
    {
        HK_FILTER_UNKNOWN = 0,
        HK_FILTER_NULL = 1,
        HK_FILTER_GROUP = 2,
        HK_FILTER_LIST = 3,
        HK_FILTER_CUSTOM = 4,
        HK_FILTER_PAIR = 5,
        HK_FILTER_CONSTRAINT = 6,
    };

    static const hkClass& staticClass(); // 0x00D7C760
    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int nbytes) 0x008145F0, delete(void* p, unsigned int nbytes) 0x0081BE90
    hkpCollisionFilter(hkFinishLoadedObjectFlag flag); // 0x00D7EDC0
    hkpCollisionFilter(); // 0x00D92FE0
    virtual hkBool isCollisionEnabled(const hkpShapeRayCastInput&, const hkpShapeContainer&, unsigned int) const = 0; // vtbl+0x00
    virtual hkBool isCollisionEnabled(const hkpCollisionInput&, const hkpCdBody&, const hkpCdBody&, const hkpShapeContainer&, const hkpShapeContainer&, unsigned int, unsigned int) const = 0; // vtbl+0x00
    virtual ~hkpCollisionFilter(); // vtbl+0x00 0x0080CFF0
    virtual hkBool isCollisionEnabled(const hkpWorldRayCastInput&, const hkpCollidable&) const = 0; // vtbl+0x04
    virtual hkBool isCollisionEnabled(const hkpCollisionInput&, const hkpCdBody&, const hkpCdBody&, const hkpShapeContainer&, unsigned int) const = 0; // vtbl+0x04
    virtual hkBool isCollisionEnabled(const hkpCollidable&, const hkpCollidable&) const = 0; // vtbl+0x04
    virtual int numShapeKeyHitsLimitBreached(const hkpCollisionInput& input, const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpBvTreeShape* bvTreeShapeB, hkAabb& aabb, unsigned int* shapeKeysInOut, int shapeKeysCapacity) const; // vtbl+0x08 0x00D92FD0
    virtual void init(hkpWorld* world); // vtbl+0x10 0x0081BE10 (.h:57)

    static const hkInternalClassMember Members[];
    unsigned int m_prepad[2]; // 0x18
    hkEnum<enum hkpFilterType,unsigned int> m_type; // 0x20
    unsigned int m_postpad[3]; // 0x24
};
