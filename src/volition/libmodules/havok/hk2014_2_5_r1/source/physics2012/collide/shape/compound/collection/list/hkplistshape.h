// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/compound/collection/list/hkplistshape.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/hkcontainerallocators.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkvector4f.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/compound/collection/hkpshapecollection.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/hkpshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/hkpshapecontainer.h"

class hkAabb;
class hkBitField;
class hkBool;
class hkClass;
class hkFinishLoadedObjectFlag;
struct hkInternalClassMember;
class hkTransformf;
class hkpCdBody;
class hkpRayHitCollector;
struct hkpShapeBufferStorage;
struct hkpShapeRayCastInput;
struct hkpShapeRayCastOutput;

// size 0x70
class hkpListShape : public hkpShapeCollection
{
public:
    enum
    {
        THIS_SHAPE_TYPE = 8,
    };

    enum
    {
        MAX_CHILDREN_FOR_SPU_MIDPHASE = 252,
    };

    enum ListShapeFlags
    {
        ALL_FLAGS_CLEAR = 0,
        DISABLE_SPU_CACHE_FOR_LIST_CHILD_INFO = 1,
    };

    enum
    {
        MAX_DISABLED_CHILDREN = 256,
    };

    // size 0x10
    struct ChildInfo
    {
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(ChildInfo);
        static const hkClass& staticClass(); // 0x00D7D780

        static const hkInternalClassMember Members[];
        const hkpShape* m_shape; // 0x00
        unsigned int m_collisionFilterInfo; // 0x04
        unsigned short m_shapeInfo; // 0x08
        short m_shapeSize; // 0x0A
        int m_numChildShapes; // 0x0C
    };

    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int, void* p) 0x00D7D770, new(unsigned int nbytes) 0x00815F50, delete(void* p, unsigned int nbytes) 0x0081BB00
    static const hkClass& staticClass(); // 0x00D7D7A0
    hkpListShape(hkFinishLoadedObjectFlag flag); // 0x00D803D0
    hkpListShape(const hkpShape* const * shapeArray, int numShapes, hkpShapeContainer::ReferencePolicy ref); // 0x00D80660
    hkpListShape();
    void afterReflectNew(); // 0x00D7FE80
    virtual ~hkpListShape(); // vtbl+0x00 0x00D80400
    virtual int getNumChildShapes() const; // vtbl+0x04 0x0081BB40 (.h:145)
    virtual unsigned int getFirstKey() const; // vtbl+0x08 0x00822490 (.h:148)
    virtual unsigned int getNextKey(unsigned int key) const; // vtbl+0x0C 0x0081BB50 (.h:152)
    virtual unsigned int getCollisionFilterInfo(unsigned int key) const; // vtbl+0x10 0x00D7FDC0
    virtual const hkpShape* getChildShape(unsigned int key, hkpShapeBufferStorage& buffer) const; // vtbl+0x14 0x00D7FDB0
    void setShapeInfo(unsigned int, unsigned short);
    unsigned short getShapeInfo(unsigned int shapeKey) const; // 0x00E25B50
    const hkpShape* getChildShapeInl(int i) const; // 0x00815F70 (.h:106)
    void recalcAabbExtents(hkAabb& aabb); // 0x00D7FFF0
    void recalcAabbExtents(); // 0x00D803B0
    void disableChild(unsigned int index); // 0x00D7FCD0
    void enableChild(unsigned int index); // 0x00D7FD00
    void setEnabledChildren(const hkBitField& enabledChildren); // 0x00D800B0
    unsigned int isChildEnabled(unsigned int index) const; // 0x0081BAA0 (.h:128)
    void setCollisionFilterInfo(unsigned int index, unsigned int filterInfo); // 0x00D7FDD0
    virtual void getAabb(const hkTransformf& localToWorld, float tolerance, hkAabb& out) const; // vtbl+0x14 0x00D805B0
    virtual hkBool castRay(const hkpShapeRayCastInput& input, hkpShapeRayCastOutput& results) const; // vtbl+0x18 0x00D80190
    virtual void castRayWithCollector(const hkpShapeRayCastInput& input, const hkpCdBody& cdBody, hkpRayHitCollector& collector) const; // vtbl+0x1C 0x00D7FE90
    virtual int calcSizeForSpu(const hkpShape::CalcSizeForSpuInput& input, int spuBufferSizeLeft) const; // vtbl+0x44 0x00D7FDF0
    int getNumAabbsForSharedBufferForAabbsAndChildInfos() const;
public:
    void setShapes(const hkpShape* const * shapeArray, int numShapes, const unsigned int* filterInfo, hkpShapeContainer::ReferencePolicy ref); // 0x00D804D0

public:
    static const hkInternalClassMember Members[];
    hkArray<ChildInfo,hkContainerHeapAllocator> m_childInfo; // 0x18
    unsigned short m_flags; // 0x24
    unsigned short m_numDisabledChildren; // 0x26
    hkVector4f m_aabbHalfExtents; // 0x30
    hkVector4f m_aabbCenter; // 0x40
    unsigned int m_enabledChildren[8]; // 0x50
};
