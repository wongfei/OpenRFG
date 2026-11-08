// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/bvcompressedmesh/hkpbvcompressedmeshshape.h
//
// Seam only (Havok 2014.2.5).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/hkcontainerallocators.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/string/hkstringptr.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/compound/tree/hkpbvtreeshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/hkpshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/hkpshapecontainer.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/util/welding/hkpweldingutility.h"

class hkAabb;
class hkClass;
struct hkGeometry;
class hkTransformf;
class hkVector4f;
struct hkpAabbCastCollector;
class hkpBvCompressedMeshShapeCinfo;
class hkpCdBody;
class hkpRayHitCollector;
struct hkpShapeBufferStorage;
struct hkpShapeRayCastInput;
struct hkpShapeRayCastOutput;

// size 0xE0
class alignas(16) hkpBvCompressedMeshShape : public hkpBvTreeShape, public hkpShapeContainer
{
public:
    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int, void* p) 0x00E340E0, delete(void* p, unsigned int nbytes) 0x00EC7DC0
    enum PerPrimitiveDataMode
    {
        PER_PRIMITIVE_DATA_NONE = 0,
        PER_PRIMITIVE_DATA_8_BIT = 1,
        PER_PRIMITIVE_DATA_PALETTE = 2,
        PER_PRIMITIVE_DATA_STRING_PALETTE = 3,
    };

    enum PrimitiveType
    {
        PRIMITIVE_TYPE_BOX = 0,
        PRIMITIVE_TYPE_HULL = 1,
        PRIMITIVE_TYPE_SPHERE = 2,
        PRIMITIVE_TYPE_CAPSULE = 3,
        PRIMITIVE_TYPE_CYLINDER = 4,
    };

    enum Config
    {
        NUM_BYTES_FOR_TREE = 144,
        MAX_NUM_VERTICES_PER_HULL = 255,
        MAX_NUM_PRIMITIVES = 0x800000,
    };

    enum
    {
        THIS_SHAPE_TYPE = 17,
    };

    static const hkClass& staticClass(); // 0x00E340F0
    hkpBvCompressedMeshShape(hkFinishLoadedObjectFlag flag); // 0x00EB8C00
    hkpBvCompressedMeshShape();
    hkpBvCompressedMeshShape(const hkpBvCompressedMeshShapeCinfo& cInfo); // 0x00EB93C0
    void afterReflectNew(); // 0x00EB88D0
    virtual ~hkpBvCompressedMeshShape(); // vtbl+0x00 0x00EB9D70
    float getConvexRadius() const; // 0x00ED6B80
    unsigned int getPrimitiveUserData(unsigned int key) const; // 0x00ED6FB0
    hkStringPtr getPrimitiveUserString(unsigned int key) const; // 0x00ED6FF0
    PerPrimitiveDataMode getCollisionFilterInfoMode() const;
    PerPrimitiveDataMode getUserDataMode() const;
    hkArray<unsigned int,hkContainerHeapAllocator>& accessCollisionFilterInfoPalette();
    hkArray<unsigned int,hkContainerHeapAllocator>& accessUserDataPalette();
    hkArray<hkStringPtr,hkContainerHeapAllocator>& accessUserStringPalette();
    void convertToGeometry(hkGeometry& geometryOut, const hkArray<unsigned int,hkContainerHeapAllocator>* inclKeys, const hkArray<unsigned int,hkContainerHeapAllocator>* exclKeys) const; // 0x00ED8040
    virtual int getNumChildShapes() const; // vtbl+0x04 0x00ED6C70
    virtual unsigned int getFirstKey() const; // vtbl+0x08 0x00ED7040
    virtual unsigned int getNextKey(unsigned int oldKey) const; // vtbl+0x0C 0x00ED70D0
    virtual unsigned int getCollisionFilterInfo(unsigned int key) const; // vtbl+0x10 0x00ED6EB0
    virtual void getAabb(const hkTransformf& localToWorld, float tolerance, hkAabb& out) const; // vtbl+0x14 0x00ED6C20
    virtual const hkpShape* getChildShape(unsigned int key, hkpShapeBufferStorage& buffer) const; // vtbl+0x14 0x00ED7150
    virtual hkBool castRay(const hkpShapeRayCastInput& input, hkpShapeRayCastOutput& results) const; // vtbl+0x18 0x00ED8610
    virtual void castRayWithCollector(const hkpShapeRayCastInput& input, const hkpCdBody& body, hkpRayHitCollector& collector) const; // vtbl+0x1C 0x00ED84B0
    virtual const hkpShapeContainer* getContainer() const; // vtbl+0x3C 0x00EC7DA0
    virtual int calcSizeForSpu(const hkpShape::CalcSizeForSpuInput& input, int spuBufferSizeLeft) const; // vtbl+0x44 0x00ED6C60
    virtual void queryAabb(const hkAabb& aabb, hkArray<unsigned int,hkContainerHeapAllocator>& hits) const; // vtbl+0x48 0x00ED8300
    virtual void castAabbImpl(const hkAabb& from, const hkVector4f& to, hkpAabbCastCollector& collector) const; // vtbl+0x4C 0x00ED8080
    virtual unsigned int queryAabbImpl(const hkAabb& aabb, unsigned int* hits, int maxNumKeys) const; // vtbl+0x50 0x00ED8140

public:
    float m_convexRadius; // 0x18
    hkEnum<enum hkpWeldingUtility::WeldingType,unsigned char> m_weldingType; // 0x1C
    hkBool m_hasPerPrimitiveCollisionFilterInfo; // 0x1D
    hkBool m_hasPerPrimitiveUserData; // 0x1E
    hkArray<unsigned int,hkContainerHeapAllocator> m_collisionFilterInfoPalette; // 0x20
    hkArray<unsigned int,hkContainerHeapAllocator> m_userDataPalette; // 0x2C
    hkArray<hkStringPtr,hkContainerHeapAllocator> m_userStringPalette; // 0x38
    unsigned char m_tree[144]; // 0x50
};
