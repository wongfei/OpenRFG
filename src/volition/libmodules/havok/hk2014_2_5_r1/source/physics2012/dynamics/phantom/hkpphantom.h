// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/phantom/hkpphantom.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/hkcontainerallocators.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/hkpworldobject.h"

class hkAabb;
class hkBool;
class hkClass;
class hkFinishLoadedObjectFlag;
struct hkInternalClassMember;
class hkpCollidable;
enum hkpCollidableAccept;
class hkpPhantomListener;
class hkpPhantomOverlapListener;
enum hkpPhantomType;
class hkpShape;

// size 0xA0
class hkpPhantom : public hkpWorldObject
{
public:
    static const hkClass& staticClass(); // 0x00DED960
    HK_DECLARE_CLASS_ALLOCATOR(); // delete(void* p, unsigned int nbytes) 0x00DEE110
    virtual ~hkpPhantom(); // vtbl+0x00 0x00E05610
    virtual hkpPhantomType getType() const = 0; // vtbl+0x1C
    virtual void calcAabb(hkAabb&) = 0; // vtbl+0x20
    virtual void addOverlappingCollidable(hkpCollidable*) = 0; // vtbl+0x24
    virtual hkBool isOverlappingCollidableAdded(const hkpCollidable*) = 0; // vtbl+0x28
    virtual void removeOverlappingCollidable(hkpCollidable*) = 0; // vtbl+0x2C
    virtual void ensureDeterministicOrder() = 0; // vtbl+0x30
    virtual hkpPhantom* clone() const = 0; // vtbl+0x34
    void addPhantomListener(hkpPhantomListener* el); // 0x00E04FC0
    void removePhantomListener(hkpPhantomListener* el); // 0x00E04EE0
    void addPhantomOverlapListener(hkpPhantomOverlapListener* el); // 0x00E05000
    void removePhantomOverlapListener(hkpPhantomOverlapListener* el); // 0x00E04F50
    const hkArray<hkpPhantomListener*,hkContainerHeapAllocator>& getPhantomListeners() const;
    const hkArray<hkpPhantomOverlapListener*,hkContainerHeapAllocator>& getPhantomOverlapListeners() const;
    virtual void updateShapeCollectionFilter(); // vtbl+0x38 0x00811820 (.h:123)
    void firePhantomAdded(); // 0x00E04E60
    void firePhantomRemoved(); // 0x00E04E20
    void firePhantomShapeSet(); // 0x00E04EA0
    hkpPhantom(hkFinishLoadedObjectFlag flag); // 0x00DEE0C0
public:
    hkpPhantom(const hkpShape* shape); // 0x00DF4580
    void firePhantomDeleted(); // 0x00E04D20
    hkpCollidableAccept fireCollidableAdded(const hkpCollidable* collidable); // 0x00DE05B0
    void fireCollidableRemoved(const hkpCollidable* collidable, hkBool collidableWasAdded); // 0x00DE0910
public:
    void updateBroadPhase(const hkAabb& aabb); // 0x00E052F0
    void setBoundingVolumeData(const hkAabb& aabb); // 0x00E05040
public:
    void removeNullOverlapListeners(); // 0x00E04D60
    void removeNullPhantomListeners(); // 0x00E04DC0
public:
    virtual void deallocateInternalArrays(); // vtbl+0x3C 0x00E05250

    static const hkInternalClassMember Members[]; // 0x0135C2C0
public:
    hkArray<hkpPhantomOverlapListener*,hkContainerHeapAllocator> m_overlapListeners; // 0x88
    hkArray<hkpPhantomListener*,hkContainerHeapAllocator> m_phantomListeners; // 0x94
};
