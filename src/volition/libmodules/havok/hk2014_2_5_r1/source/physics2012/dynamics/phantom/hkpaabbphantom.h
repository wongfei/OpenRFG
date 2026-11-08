// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/phantom/hkpaabbphantom.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/hkcontainerallocators.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/geometry/aabb/hkaabb.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/phantom/hkpphantom.h"

class hkClass;
struct hkInternalClassMember;
class hkMotionState;
class hkpCdPointCollector;
class hkpCollidable;
struct hkpLinearCastInput;
enum hkpPhantomType;
class hkpRayHitCollector;
struct hkpWorldRayCastInput;
struct hkpWorldRayCastOutput;

// size 0xD0
class hkpAabbPhantom : public hkpPhantom
{
public:
    // size 0x1
    class OrderByUid
    {
    public:
        hkBool operator()(const hkpCollidable* a, const hkpCollidable* b); // 0x00DF4410

    };

    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int, void* p) 0x00DF08C0, new(unsigned int nbytes) 0x00814880, delete(void* p, unsigned int nbytes) 0x008148A0
    static const hkClass& staticClass(); // 0x00DF08D0
    hkpAabbPhantom(hkFinishLoadedObjectFlag flag); // 0x00DF0A70
    hkpAabbPhantom(const hkAabb& aabb, unsigned int collisionFilterInfo); // 0x00DF4140
    virtual ~hkpAabbPhantom(); // vtbl+0x00 0x00DF3E60
    virtual hkpPhantomType getType() const; // vtbl+0x1C 0x00DF37B0
    hkArray<hkpCollidable*,hkContainerHeapAllocator>& getOverlappingCollidables(); // 0x00F7E9D0
    void castRay(const hkpWorldRayCastInput& input, hkpWorldRayCastOutput& output) const; // 0x00DF3AB0
    void castRay(const hkpWorldRayCastInput& input, hkpRayHitCollector& collector) const; // 0x00DF3850
    void linearCast(const hkpCollidable* const toBeCast, const hkpLinearCastInput& input, hkpCdPointCollector& castCollector, hkpCdPointCollector* startCollector) const; // 0x00DF3EC0
    virtual void calcAabb(hkAabb& aabb); // vtbl+0x20 0x00DF37C0
    virtual void addOverlappingCollidable(hkpCollidable* collidable); // vtbl+0x24 0x00DF3DC0
    virtual hkBool isOverlappingCollidableAdded(const hkpCollidable* collidable); // vtbl+0x28 0x00DF3810
    virtual void removeOverlappingCollidable(hkpCollidable* collidable); // vtbl+0x2C 0x00DF3D40
    virtual hkpPhantom* clone() const; // vtbl+0x34 0x00DF41D0
    void setAabb(const hkAabb& newAabb); // 0x00DF37E0
    const hkAabb& getAabb() const; // 0x00DE86E0
public:
    virtual hkMotionState* getMotionState(); // vtbl+0x18 0x00811830 (.h:120)
public:
    virtual void ensureDeterministicOrder(); // vtbl+0x30 0x00DF3E20
    virtual void deallocateInternalArrays(); // vtbl+0x3C 0x00DF40E0

    static const hkInternalClassMember Members[]; // 0x01361750
public:
    hkAabb m_aabb; // 0xA0
    hkArray<hkpCollidable*,hkContainerHeapAllocator> m_overlappingCollidables; // 0xC0
public:
    hkBool m_orderDirty; // 0xCC
};
