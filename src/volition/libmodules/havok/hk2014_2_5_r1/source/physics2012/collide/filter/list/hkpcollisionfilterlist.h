// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/filter/list/hkpcollisionfilterlist.h
//
// Seam only (Havok 2014.2.5).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/hkcontainerallocators.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/filter/hkpcollisionfilter.h"

class hkBool;
class hkClass;
class hkFinishLoadedObjectFlag;
class hkpCdBody;
class hkpCollidable;
struct hkpCollisionInput;
class hkpShapeContainer;
struct hkpShapeRayCastInput;
class hkpWorld;
struct hkpWorldRayCastInput;

// size 0x3C
class hkpCollisionFilterList : public hkpCollisionFilter
{
public:
    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int, void* p) 0x00D7F810, delete(void* p, unsigned int nbytes) 0x00D7FA80
    static const hkClass& staticClass(); // 0x00D7F820
    hkpCollisionFilterList(hkFinishLoadedObjectFlag flag); // 0x00D7F9E0
    hkpCollisionFilterList(const hkArray<hkpCollisionFilter*,hkContainerHeapAllocator>& collisionFilters); // 0x00DAC6D0
    hkpCollisionFilterList(); // 0x00DAC8D0
    virtual ~hkpCollisionFilterList(); // vtbl+0x00 0x00DAC7E0
    const hkArray<hkpCollisionFilter*,hkContainerHeapAllocator>& getCollisionFilters() const; // 0x00DAC410
    void addCollisionFilter(hkpCollisionFilter* filter); // 0x00DAC660
    void removeCollisionFilter(hkpCollisionFilter* filter); // 0x00DAC620
    virtual hkBool isCollisionEnabled(const hkpShapeRayCastInput& aInput, const hkpShapeContainer& bContainer, unsigned int bKey) const; // vtbl+0x00 0x00DAC4D0
    virtual hkBool isCollisionEnabled(const hkpCollisionInput& input, const hkpCdBody& collectionBodyA, const hkpCdBody& collectionBodyB, const hkpShapeContainer& containerShapeA, const hkpShapeContainer& containerShapeB, unsigned int keyA, unsigned int keyB) const; // vtbl+0x00 0x00DAC530
    virtual hkBool isCollisionEnabled(const hkpWorldRayCastInput& a, const hkpCollidable& collidableB) const; // vtbl+0x04 0x00DAC5A0
    virtual hkBool isCollisionEnabled(const hkpCollisionInput& input, const hkpCdBody& a, const hkpCdBody& b, const hkpShapeContainer& bContainer, unsigned int bKey) const; // vtbl+0x04 0x00DAC470
    virtual hkBool isCollisionEnabled(const hkpCollidable& a, const hkpCollidable& b) const; // vtbl+0x04 0x00DAC420
    virtual void init(hkpWorld* world); // vtbl+0x10 0x00DAC5F0
    void afterReflectNew(); // 0x00D7F8F0

public:
    hkArray<hkpCollisionFilter*,hkContainerHeapAllocator> m_collisionFilters; // 0x30
};
