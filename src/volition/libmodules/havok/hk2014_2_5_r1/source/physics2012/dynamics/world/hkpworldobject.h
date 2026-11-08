// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/hkpworldobject.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/util/hkpworldoperationqueue.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/hkcontainerallocators.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/string/hkstringptr.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/debugutil/multithreadcheck/hkmultithreadcheck.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/object/hkreferencedobject.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/properties/hksimpleproperty.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent3/machine/nn/hkplinkedcollidable.h"

class hkBool;
class hkClass;
class hkFinishLoadedObjectFlag;
struct hkInternalClassMember;
class hkMotionState;
class hkpCollidable;
class hkpShape;
class hkpShapeModifier;
class hkpWorld;

// size 0x88
class hkpWorldObject : public hkReferencedObject
{
public:
    enum MtChecks
    {
        MULTI_THREADING_CHECKS_ENABLE = 0,
        MULTI_THREADING_CHECKS_IGNORE = 1,
    };

    enum BroadPhaseType
    {
        BROAD_PHASE_INVALID = 0,
        BROAD_PHASE_ENTITY = 1,
        BROAD_PHASE_PHANTOM = 2,
        BROAD_PHASE_BORDER = 3,
        BROAD_PHASE_MAX_ID = 4,
    };

    HK_DECLARE_CLASS_ALLOCATOR(); // delete(void* p, unsigned int nbytes) 0x00DDFA30
    static const hkClass& staticClass(); // 0x00DED920
    void addProperty(unsigned int key, hkSimplePropertyValue value); // 0x00DF4770
    hkSimplePropertyValue removeProperty(unsigned int key); // 0x00DF46B0
    void setProperty(unsigned int key, hkSimplePropertyValue value); // 0x00DF47E0
    void editProperty(unsigned int key, hkSimplePropertyValue value, MtChecks mtCheck); // 0x00DF4670
    hkSimplePropertyValue getProperty(unsigned int, MtChecks) const;
    bool hasProperty(unsigned int, MtChecks) const;
    void lockProperty(unsigned int key); // 0x00DF45F0
    void unlockProperty(unsigned int key); // 0x00DF4610
    void clearAndDeallocateProperties();
    hkpCollidable* getCollidableRw(); // 0x0080CD50 (hkpworldobject.inl:36)
    const hkpCollidable* getCollidable() const; // 0x00742370 (hkpworldobject.inl:44)
    const hkpLinkedCollidable* getLinkedCollidable() const; // 0x00DDE660
    hkpLinkedCollidable* getLinkedCollidable(); // 0x00DDE650
    const hkpLinkedCollidable* getCollidableMtUnchecked() const; // 0x00E23F70
    hkBool isAddedToWorld() const; // 0x0080CD60 (hkpworldobject.inl:93)
    hkpWorld* getWorld() const; // 0x00710FC0 (hkpworldobject.inl:11)
    unsigned long getUserData() const; // 0x00DF2880
    void setUserData(unsigned long data); // 0x00DF2890
    const char* getName() const; // 0x00DF2D50
    void setName(const char* name); // 0x00DF28A0
    virtual hkWorldOperation::Result setShape(const hkpShape* shape); // vtbl+0x10 0x00DF45D0
    virtual hkWorldOperation::Result updateShape(hkpShapeModifier* shapeModifier); // vtbl+0x14 0x00DF45E0
    void markForRead();
    void markForWrite(); // 0x00DF8D40
    void markForWriteImpl(); // 0x00DF4630
    void unmarkForRead();
    void unmarkForWrite(); // 0x00DF8D50
    void checkReadWrite(); // 0x00DF4640
    void checkReadOnly() const; // 0x00DF4650
public:
    void setWorld(hkpWorld* world); // 0x00DE9200
public:
    hkpWorldObject(hkFinishLoadedObjectFlag flag); // 0x00DF4870
public:
    hkpWorldObject(const hkpShape* shape, BroadPhaseType type); // 0x00DF48F0
public:
    virtual ~hkpWorldObject(); // vtbl+0x00 0x00DDF990
    virtual hkMotionState* getMotionState() = 0; // vtbl+0x18
    const hkMultiThreadCheck& getMultiThreadCheck() const;
    hkMultiThreadCheck& getMultiThreadCheck(); // 0x00DF8D30
    void copyProperties(const hkpWorldObject* otherObj); // 0x00DE0C50
    void afterReflectNew(); // 0x00DF4660

    static const hkInternalClassMember Members[]; // 0x0135C160
public:
    hkpWorld* m_world; // 0x08
    unsigned long m_userData; // 0x0C
    hkpLinkedCollidable m_collidable; // 0x10
    hkMultiThreadCheck m_multiThreadCheck; // 0x6C
    hkStringPtr m_name; // 0x78
public:
    hkArray<hkSimpleProperty,hkContainerHeapAllocator> m_properties; // 0x7C
};
