// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/compound/tree/mopp/hkpmoppbvtreeshape.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkvector4f.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/geometry/collide/shapes/hkcdshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/compound/tree/hkpbvtreeshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/hkpshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/hkpshapecontainer.h"

class hkAabb;
class hkBool;
class hkClass;
struct hkContainerHeapAllocator;
class hkFinishLoadedObjectFlag;
struct hkInternalClassMember;
class hkTransformf;
class hkVector4fComparison;
class hkpCdBody;
class hkpMoppCode;
class hkpRayHitCollector;
class hkpShapeCollection;
struct hkpShapeRayBundleCastInput;
struct hkpShapeRayBundleCastOutput;
struct hkpShapeRayCastInput;
struct hkpShapeRayCastOutput;

// size 0x30
class hkMoppBvTreeShapeBase : public hkpBvTreeShape
{
public:
    HK_DECLARE_CLASS_ALLOCATOR(); // delete(void* p, unsigned int nbytes) 0x00D7C8A0
    static const hkClass& staticClass(); // 0x00D7C7A0
    hkMoppBvTreeShapeBase(hkFinishLoadedObjectFlag flag); // 0x00D7C940
    hkMoppBvTreeShapeBase(hkcdShapeType::ShapeTypeEnum type, const hkpMoppCode* code); // 0x00D9A100
    hkMoppBvTreeShapeBase();
    void afterReflectNew(); // 0x00D7C870
    virtual ~hkMoppBvTreeShapeBase(); // vtbl+0x00 0x00D7C880
    virtual void queryAabb(const hkAabb& aabb, hkArray<unsigned int,hkContainerHeapAllocator>& hits) const; // vtbl+0x48 0x00D9A050
    virtual unsigned int queryAabbImpl(const hkAabb& aabb, unsigned int* hits, int maxNumKeys) const; // vtbl+0x50 0x00D9A460
    virtual void queryObb(const hkTransformf& obbToMopp, const hkVector4f& extent, float tolerance, hkArray<unsigned int,hkContainerHeapAllocator>& hits) const; // vtbl+0x54 0x00D9A010

    static const hkInternalClassMember Members[];
    const hkpMoppCode* m_code; // 0x14
    const unsigned char* m_moppData; // 0x18
    unsigned int m_moppDataSize; // 0x1C
    hkVector4f m_codeInfoCopy; // 0x20
};

// size 0x40
class alignas(16) hkpMoppBvTreeShape : public hkMoppBvTreeShapeBase
{
public:
    enum
    {
        THIS_SHAPE_TYPE = 9,
    };

    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int, void* p) 0x00D7C790, new(unsigned int nbytes) 0x00815FE0, delete(void* p, unsigned int nbytes) 0x00D7CA00
    static const hkClass& staticClass(); // 0x00D7C7B0
    hkpMoppBvTreeShape(hkFinishLoadedObjectFlag flag); // 0x00D7C990
    hkpMoppBvTreeShape(const hkpShapeCollection* collection, const hkpMoppCode* code); // 0x00D9A170
    hkpMoppBvTreeShape();
    void afterReflectNew(); // 0x00D7C970
    virtual ~hkpMoppBvTreeShape(); // vtbl+0x00 0x00D9A1A0
    virtual void getAabb(const hkTransformf& localToWorld, float tolerance, hkAabb& out) const; // vtbl+0x14 0x00D99FE0
    virtual hkBool castRay(const hkpShapeRayCastInput& input, hkpShapeRayCastOutput& results) const; // vtbl+0x18 0x00D9A1E0
    virtual void castRayWithCollector(const hkpShapeRayCastInput& input, const hkpCdBody& cdBody, hkpRayHitCollector& collector) const; // vtbl+0x1C 0x00D9A390
    virtual hkVector4fComparison castRayBundle(const hkpShapeRayBundleCastInput& input, hkpShapeRayBundleCastOutput& results, const hkVector4fComparison& mask) const; // vtbl+0x20 0x00D9A2C0
    const hkpMoppCode* getMoppCode() const; // 0x00DB45E0
    void setMoppCode(const hkpMoppCode*);
    const hkpShapeCollection* getShapeCollection() const; // 0x0080DBE0 (hkpmoppbvtreeshape.inl:28)
    virtual const hkpShapeContainer* getContainer() const; // vtbl+0x3C 0x00D7C9F0
    virtual int calcSizeForSpu(const hkpShape::CalcSizeForSpuInput& input, int spuBufferSizeLeft) const; // vtbl+0x44 0x00D9A0B0
    const hkpShape* getChild() const; // 0x00E25910

    static const hkInternalClassMember Members[]; // 0x01353BF8
public:
    hkpSingleShapeContainer m_child; // 0x30
public:
    int m_childSize; // 0x38
};
