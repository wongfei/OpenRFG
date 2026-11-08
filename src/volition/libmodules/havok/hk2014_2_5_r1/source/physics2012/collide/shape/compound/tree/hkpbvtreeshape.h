// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/compound/tree/hkpbvtreeshape.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/geometry/collide/shapes/hkcdshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/hkpshape.h"

class hkAabb;
class hkClass;
struct hkContainerHeapAllocator;
struct hkInternalClassMember;
class hkVector4f;
struct hkpAabbCastCollector;
class hkpShapeContainer;

// size 0x14
class hkpBvTreeShape : public hkpShape
{
public:
    enum
    {
        THIS_SHAPE_TYPE = 22,
    };

    enum BvTreeType
    {
        BVTREE_MOPP = 0,
        BVTREE_TRISAMPLED_HEIGHTFIELD = 1,
        BVTREE_STATIC_COMPOUND = 2,
        BVTREE_COMPRESSED_MESH = 3,
        BVTREE_USER = 4,
        BVTREE_MAX = 5,
    };

    HK_DECLARE_CLASS_ALLOCATOR(); // delete(void* p, unsigned int nbytes) 0x0071AB90
    static const hkClass& staticClass(); // 0x00D7C780
    hkpBvTreeShape(hkFinishLoadedObjectFlag flag); // 0x00D74980
    hkpBvTreeShape(hkcdShapeType::ShapeTypeEnum, BvTreeType);
    hkpBvTreeShape() {} // no PDB record of its own, always inlined (HK_FORCE_INLINE in the 2014.1 SDK)
    unsigned int queryAabb(const hkAabb& aabb, unsigned int* hits, int maxNumKeys) const; // 0x00D747C0
    virtual ~hkpBvTreeShape(); // vtbl+0x00 0x00710920
    void castAabb(const hkAabb& from, const hkVector4f& to, hkpAabbCastCollector& collector) const; // 0x00DB1F20
    virtual const hkpShapeContainer* getContainer() const = 0; // vtbl+0x3C
    static void registerBvTreeCollideFunctions();
    static void registerBvTreeCollideFunctions_StaticCompound();
    static void registerBvTreeCollideQueryFunctions();
    void afterReflectNew(); // 0x00D74820
    virtual void queryAabb(const hkAabb&, hkArray<unsigned int,hkContainerHeapAllocator>&) const = 0; // vtbl+0x48
    virtual void castAabbImpl(const hkAabb& from, const hkVector4f& to, hkpAabbCastCollector& collector) const; // vtbl+0x4C 0x00D74830
    virtual unsigned int queryAabbImpl(const hkAabb&, unsigned int*, int) const = 0; // vtbl+0x50

    static const hkInternalClassMember Members[];
    hkEnum<enum BvTreeType,unsigned char> m_bvTreeType; // 0x10
};
