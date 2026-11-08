// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/heightfield/trisampledheightfield/hkptrisampledheightfieldbvtreeshape.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/compound/tree/hkpbvtreeshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/hkpshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/hkpshapecontainer.h"

class hkAabb;
class hkClass;
struct hkContainerHeapAllocator;
struct hkInternalClassMember;
class hkTransformf;
class hkVector4f;
class hkpCdBody;
class hkpRayHitCollector;
struct hkpShapeRayCastInput;
struct hkpShapeRayCastOutput;
class hkpTriSampledHeightFieldCollection;

// size 0x30
class hkpTriSampledHeightFieldBvTreeShape : public hkpBvTreeShape
{
public:
    enum
    {
        THIS_SHAPE_TYPE = 7,
    };

    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int, void* p) 0x00D7DFA0, delete(void* p, unsigned int nbytes) 0x00DA6D00
    static const hkClass& staticClass(); // 0x00D7DFB0
    hkpTriSampledHeightFieldBvTreeShape(hkFinishLoadedObjectFlag flag); // 0x00DA64B0
    hkpTriSampledHeightFieldBvTreeShape(const hkpTriSampledHeightFieldCollection* c, hkBool doAabbRejection); // 0x00DA6450
    hkpTriSampledHeightFieldBvTreeShape();
    virtual void getAabb(const hkTransformf& localToWorld, float tolerance, hkAabb& out) const; // vtbl+0x14 0x00DA62D0
    virtual hkBool castRay(const hkpShapeRayCastInput& input, hkpShapeRayCastOutput& output) const; // vtbl+0x18 0x00DA6300
    virtual void castRayWithCollector(const hkpShapeRayCastInput& input, const hkpCdBody& cdBody, hkpRayHitCollector& collector) const; // vtbl+0x1C 0x00DA6330
    virtual const hkpShapeContainer* getContainer() const; // vtbl+0x3C 0x00DA6CF0
    virtual int calcSizeForSpu(const hkpShape::CalcSizeForSpuInput& input, int spuBufferSizeLeft) const; // vtbl+0x44 0x00DA6350
    virtual void queryAabb(const hkAabb& aabbIn, hkArray<unsigned int,hkContainerHeapAllocator>& hits) const; // vtbl+0x48 0x00DA6950
    void afterReflectNew(); // 0x00DA64A0
    const hkpTriSampledHeightFieldCollection* getShapeCollection() const; // 0x0080DBB0 (hkptrisampledheightfieldbvtreeshape.inl:14)
    virtual unsigned int queryAabbImpl(const hkAabb& aabbIn, unsigned int* hits, int maxNumKeys) const; // vtbl+0x50 0x00DA66C0
    virtual void queryObb(const hkTransformf& obbTransform, const hkVector4f& obbExtent, float tolerance, hkArray<unsigned int,hkContainerHeapAllocator>& hits) const; // vtbl+0x54 0x00DA63A0
    hkBool getWantAabbRejectionTest() const;
    void setWantAabbRejectionTest(hkBool);
public:
    static hkBool getExtentsForQueryAabb(hkAabb& aabb, const hkpTriSampledHeightFieldCollection* collection, unsigned int& minX, unsigned int& maxX, unsigned int& minZ, unsigned int& maxZ); // 0x00DA64E0
public:
    virtual ~hkpTriSampledHeightFieldBvTreeShape(); // vtbl+0x00 0x00DA6D40

    static const hkInternalClassMember Members[]; // 0x01356018
public:
    hkpSingleShapeContainer m_childContainer; // 0x14
    int m_childSize; // 0x1C
    hkBool m_wantAabbRejectionTest; // 0x20
    unsigned char m_padding[12]; // 0x21
};
