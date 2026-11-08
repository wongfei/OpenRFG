// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/staticcompound/hkpstaticcompoundshape.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/hkcontainerallocators.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/qstransform/hkqstransformf.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/compound/tree/hkpbvtreeshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/hkpshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/hkpshapecontainer.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/staticcompound/hkpshapekeytable.h"

class hkAabb;
class hkBool;
class hkClass;
class hkFinishLoadedObjectFlag;
struct hkInternalClassMember;
class hkTransformf;
class hkVector4f;
struct hkpAabbCastCollector;
class hkpCdBody;
class hkpRayHitCollector;
struct hkpShapeBufferStorage;
struct hkpShapeRayCastInput;
struct hkpShapeRayCastOutput;

// size 0x70
class hkpStaticCompoundShape : public hkpBvTreeShape, public hkpShapeContainer
{
public:
    enum Config
    {
        NUM_BYTES_FOR_TREE = 48,
    };

    enum
    {
        THIS_SHAPE_TYPE = 16,
    };

    // size 0x40
    struct Instance
    {
        enum
        {
            FLAG_IS_LEAF = 1,
            FLAG_HAS_TRANSFORM = 2,
            FLAG_HAS_SCALE = 4,
            FLAG_HAS_FLIP = 8,
            FLAG_IS_DISABLED = 16,
            FLAG_HAS_DISABLED_SMALL_KEYS = 32,
            FLAG_HAS_DISABLED_LARGE_KEYS = 64,
            FLAGS_HAS_DISABLED_KEYS = 96,
            FLAGS_ALL = 127,
        };

        enum
        {
            NUM_BITS_FOR_FLAGS = 7,
            NUM_BITS_FOR_SHAPE_SIZE = 4,
            NUM_BITS_FOR_DISABLED_SMALL_KEYS = 37,
        };

    public:
        void setShapeSizeForSpu(unsigned int size); // 0x00E72AD0
        void setFlags(unsigned int flags); // 0x00E25240
    public:
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(Instance);
        static const hkClass& staticClass(); // 0x00E33C90
        const hkQsTransformf& getTransform() const; // 0x00E6FEB0
        const hkpShape* getShape() const; // 0x00E24F00
        unsigned int getShapeSizeForSpu() const;
        unsigned int getFlags() const; // 0x00E25260

    public:
        hkQsTransformf m_transform; // 0x00
        const hkpShape* m_shape; // 0x30
    public:
        static const hkInternalClassMember Members[]; // 0x01363F80
        unsigned int m_filterInfo; // 0x34
        unsigned int m_childFilterInfoMask; // 0x38
        unsigned long m_userData; // 0x3C
    };

    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int, void* p) 0x00E33C80, delete(void* p, unsigned int nbytes) 0x00E78090
    static const hkClass& staticClass(); // 0x00E33CB0
    hkpStaticCompoundShape(hkFinishLoadedObjectFlag flag); // 0x00E70E50
    hkpStaticCompoundShape(hkpShapeContainer::ReferencePolicy ref); // 0x00E70F10
    void afterReflectNew(); // 0x00E70400
    virtual ~hkpStaticCompoundShape(); // vtbl+0x00 0x00E70FC0
    int addInstance(const hkpShape* shape, const hkQsTransformf& transform); // 0x00E70B50
    void bake(); // 0x00E71950
    void decomposeShapeKey(unsigned int keyIn, int& instanceIdOut, unsigned int& childKeyOut) const; // 0x00E6FE80
    unsigned int composeShapeKey(int instanceId, unsigned int childKey) const; // 0x00E6FE70
    signed char getNumBitsForChildShapeKey() const;
    const hkArray<Instance,hkContainerHeapAllocator>& getInstances() const; // 0x00E24EF0
    void setInstanceFilterInfo(int, unsigned int);
    unsigned int getInstanceFilterInfo(int) const;
    void setInstanceFilterInfoMask(int, unsigned int);
    unsigned int getInstanceFilterInfoMask(int) const;
    void setInstanceUserData(int, unsigned long);
    unsigned long getInstanceUserData(int) const;
    void setInstanceExtraInfo(int, unsigned short);
    unsigned short getInstanceExtraInfo(int instanceId) const; // 0x00E25270
    const hkArray<unsigned short,hkContainerHeapAllocator>& getInstanceExtraInfos() const;
    void setInstanceEnabled(int instanceId, unsigned int isEnabled); // 0x00E25280
    unsigned int isInstanceEnabled(int) const;
    void setShapeKeyEnabled(unsigned int key, unsigned int isEnabled); // 0x00E700F0
    unsigned int isShapeKeyEnabled(unsigned int key) const; // 0x00E70430
    void enableAllInstancesAndShapeKeys(); // 0x00E70210
    void castAabb(const hkpShape* shapeA, const hkTransformf& AtoB, const hkVector4f& to, hkpAabbCastCollector& collector, float tolerance) const; // 0x00E711A0
    virtual int getNumChildShapes() const; // vtbl+0x04 0x00E70DA0
    virtual unsigned int getFirstKey() const; // vtbl+0x08 0x00E6FFE0
    virtual unsigned int getNextKey(unsigned int oldKey) const; // vtbl+0x0C 0x00E70040
    virtual unsigned int getCollisionFilterInfo(unsigned int key) const; // vtbl+0x10 0x00E70260
    virtual void getAabb(const hkTransformf& localToWorld, float tolerance, hkAabb& out) const; // vtbl+0x14 0x00E704F0
    virtual const hkpShape* getChildShape(unsigned int key, hkpShapeBufferStorage& buffer) const; // vtbl+0x14 0x00E705B0
    virtual hkBool castRay(const hkpShapeRayCastInput& input, hkpShapeRayCastOutput& results) const; // vtbl+0x18 0x00E71660
    virtual void castRayWithCollector(const hkpShapeRayCastInput& input, const hkpCdBody& body, hkpRayHitCollector& collector) const; // vtbl+0x1C 0x00E71850
    virtual const hkpShapeContainer* getContainer() const; // vtbl+0x3C 0x00E78070
    virtual float getMaximumProjection(const hkVector4f& direction) const; // vtbl+0x40 0x00E70300
    virtual int calcSizeForSpu(const hkpShape::CalcSizeForSpuInput& input, int spuBufferSizeLeft) const; // vtbl+0x44 0x00E70EA0
    virtual void queryAabb(const hkAabb& aabb, hkArray<unsigned int,hkContainerHeapAllocator>& hits) const; // vtbl+0x48 0x00E712C0
    virtual void castAabbImpl(const hkAabb& from, const hkVector4f& to, hkpAabbCastCollector& collector) const; // vtbl+0x4C 0x00E710B0
    virtual unsigned int queryAabbImpl(const hkAabb& aabb, unsigned int* hits, int maxNumKeys) const; // vtbl+0x50 0x00E71480

    static const hkInternalClassMember Members[]; // 0x01363FF8
public:
    signed char m_numBitsForChildShapeKey; // 0x18
    signed char m_referencePolicy; // 0x19
    unsigned int m_childShapeKeyMask; // 0x1C
    hkArray<Instance,hkContainerHeapAllocator> m_instances; // 0x20
    hkArray<unsigned short,hkContainerHeapAllocator> m_instanceExtraInfos; // 0x2C
    hkpShapeKeyTable m_disabledLargeShapeKeyTable; // 0x38
    unsigned char m_tree[48]; // 0x40
};
