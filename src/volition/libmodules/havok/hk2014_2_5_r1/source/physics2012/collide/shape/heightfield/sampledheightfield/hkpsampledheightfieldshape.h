// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/heightfield/sampledheightfield/hkpsampledheightfieldshape.h
//
// Havok 2014.2.5 (havok_impl.md HK3): the PDB declaration with the inline code of Havok 2014.1
// (hkpSampledHeightFieldShape.h/.inl). The cast functions are HK_CALL (__cdecl) members, called through the static
// member pointers. Hand-maintained (nobuild in the hdrgen specs).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/hkcontainerallocators.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkvector4f.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/geometry/aabb/hkaabb.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/heightfield/hkpheightfieldshape.h"

class hkClass;
struct hkInternalClassMember;
class hkTransformf;
class hkVector4fComparison;
struct hkcdRay;
class hkpCdBody;
class hkpRayHitCollector;
class hkpSampledHeightFieldBaseCinfo;
struct hkpShapeRayCastInput;
struct hkpShapeRayCastOutput;

// size 0x80
class hkpSampledHeightFieldShape : public hkpHeightFieldShape
{
public:
    enum
    {
        THIS_SHAPE_TYPE = 12,
    };

    enum HeightFieldType
    {
        HEIGHTFIELD_STORAGE = 0,
        HEIGHTFIELD_COMPRESSED = 1,
        HEIGHTFIELD_USER = 2,
        HEIGHTFIELD_MAX_ID = 3,
    };

    typedef void (HK_CALL hkpSampledHeightFieldShape::*RayCastInternalFunc)(const hkpShapeRayCastInput&, const hkpCdBody&, hkpRayHitCollector&) const;
    typedef void (HK_CALL hkpSampledHeightFieldShape::*SphereCastInternalFunc)(const hkpHeightFieldShape::hkpSphereCastInput&, const hkpCdBody&, hkpRayHitCollector&) const;

    // size 0x30
    struct AABBStackElement
    {
        hkAabb aabb; // 0x00
        int level; // 0x20
        int x; // 0x24
        int z; // 0x28
    };

    // size 0x14
    struct CoarseMinMaxLevel
    {
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(CoarseMinMaxLevel); // new(unsigned int n, void* p) 0x00D7CBC0, delete(void* p, unsigned int nbytes) 0x00D7CC90
        static const hkClass& staticClass(); // 0x00D7CBD0
        CoarseMinMaxLevel(const CoarseMinMaxLevel& l); // 0x00DA7EB0
        CoarseMinMaxLevel(hkFinishLoadedObjectFlag f); // 0x00D7CCC0
        CoarseMinMaxLevel(); // 0x00DB0E50
        ~CoarseMinMaxLevel(); // 0x00D7CCD0

        static const hkInternalClassMember Members[];
        hkArray<hkVector4f,hkContainerHeapAllocator> m_minMaxData; // 0x00
        int m_xRes; // 0x0C
        int m_zRes; // 0x10
    };

    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int, void* p) 0x00D7CBB0, delete(void* p, unsigned int nbytes) 0x00D9C380
    static const hkClass& staticClass(); // 0x00D7CBE0
    hkpSampledHeightFieldShape(hkFinishLoadedObjectFlag flag); // 0x00D9B830
    hkpSampledHeightFieldShape(); // 0x00DA8260
    hkpSampledHeightFieldShape(const hkpSampledHeightFieldBaseCinfo& ci, HeightFieldType hfType); // 0x00D9B6B0
    void afterReflectNew(); // 0x00D9B4D0
    virtual ~hkpSampledHeightFieldShape(); // vtbl+0x00 0x00D9B860
    void getCinfo(hkpSampledHeightFieldBaseCinfo& cinfo) const; // 0x00D9B100
    // 0x00D9AF80 (hkpsampledheightfieldshape.inl)
    float getHeightAt(int x, int z) const
    {
        return getHeightAtImpl(x, z);
    }
    // 0x00D9AF90 (hkpsampledheightfieldshape.inl)
    hkBool getTriangleFlip() const
    {
        return getTriangleFlipImpl();
    }
    void buildCoarseMinMaxTree(int coarseness); // 0x00DAF8C0
    virtual void getAabb(const hkTransformf& localToWorld, float tolerance, hkAabb& out) const; // vtbl+0x14 0x00D9B4E0
    void setMinMaxTreeCoarseness(int coarseness); // 0x00DAE3E0
    virtual hkBool castRay(const hkpShapeRayCastInput& input, hkpShapeRayCastOutput& results) const; // vtbl+0x18 0x00D9B110
    virtual void castRayWithCollector(const hkpShapeRayCastInput& input, const hkpCdBody& cdBody, hkpRayHitCollector& collector) const; // vtbl+0x1C 0x00D9B160
    virtual void collideSpheres(const hkpHeightFieldShape::CollideSpheresInput& input, hkVector4f* outputArray) const; // vtbl+0x48 0x00D9AFE0
    virtual void castSphere(const hkpHeightFieldShape::hkpSphereCastInput& input, const hkpCdBody& cdBody, hkpRayHitCollector& collector) const; // vtbl+0x4C 0x00D9B440
    virtual void getCoarseMinMax(int level, int x, int z, hkVector4f& minOut, hkVector4f& maxOut) const; // vtbl+0x50 0x00D9B3F0
    virtual float getHeightAtImpl(int x, int z) const; // vtbl+0x54 0x00D9B040
    virtual hkBool getTriangleFlipImpl() const; // vtbl+0x58 0x00D9B0A0
    void HK_CALL castRayDefault(const hkpShapeRayCastInput& input, const hkpCdBody& cdBody, hkpRayHitCollector& collector) const; // 0x00D83F40
    void HK_CALL castRayDda(const hkpShapeRayCastInput& input, const hkpCdBody& cdBody, hkpRayHitCollector& collector) const; // 0x00DAE2B0
    void HK_CALL castRayCoarseTree(const hkpShapeRayCastInput& input, const hkpCdBody& cdBody, hkpRayHitCollector& collector) const; // 0x00DAF5E0
    void HK_CALL castSphereDefault(const hkpHeightFieldShape::hkpSphereCastInput& input, const hkpCdBody& cdBody, hkpRayHitCollector& collector) const; // 0x00D83F60
    void HK_CALL castSphereDda(const hkpHeightFieldShape::hkpSphereCastInput& input, const hkpCdBody& cdBody, hkpRayHitCollector& collector) const; // 0x00DAE2E0
    void HK_CALL castSphereCoarseTree(const hkpHeightFieldShape::hkpSphereCastInput& input, const hkpCdBody& cdBody, hkpRayHitCollector& collector) const; // 0x00DAF630
public:
    void castRayDdaInternal(const hkpShapeRayCastInput& input, const hkpCdBody& cdBody, hkBool reportPenetratingStartPosition, float maxExtraPenetration, hkpRayHitCollector& collector) const; // 0x00DACD30
    void castRayCoarseTreeInternal(const hkVector4f& from, const hkVector4f& to, const hkpCdBody& cdBody, hkpRayHitCollector& collector) const; // 0x00DAE640
    void _getHeightAndNormalAt(int x, int z, float subX, float subZ, hkVector4f& normalOut, float& heightOut, int& triangleIndexOut) const; // 0x00D9BAA0
    void calcMinMax(int coarseness, int x, int z, float& minheightOut, float& maxheightOut) const; // 0x00DB0350
    void findStartCell(AABBStackElement& startOut, const hkVector4f& from, const hkVector4f& to) const; // 0x00DB0450
    void rayTriangleQuadCheck(const hkcdRay& ray, hkAabb& aabb, int x, int z, const hkVector4fComparison& flipSelect, const hkpCdBody& cdBody, hkpRayHitCollector& collector) const; // 0x00DB0A80
public:
    void getHeightAndNormalAt(int xPos, int zPos, float subX, float subZ, hkVector4f& normalOut, float& heightOut, int& triangleIndexOut) const; // 0x00D9B1A0

    static const hkInternalClassMember Members[];
    static RayCastInternalFunc s_rayCastFunc; // 0x03A2880C
    static SphereCastInternalFunc s_sphereCastFunc; // 0x03A28810
    hkArray<CoarseMinMaxLevel,hkContainerHeapAllocator> m_coarseTreeData; // 0x10
    int m_coarseness; // 0x1C
    float m_raycastMinY; // 0x20
    float m_raycastMaxY; // 0x24
    int m_xRes; // 0x28
    int m_zRes; // 0x2C
    float m_heightCenter; // 0x30
    hkBool m_useProjectionBasedHeight; // 0x34
    hkEnum<enum HeightFieldType,unsigned char> m_heightfieldType; // 0x35
    hkVector4f m_intToFloatScale; // 0x40
    hkVector4f m_floatToIntScale; // 0x50
    hkVector4f m_floatToIntOffsetFloorCorrected; // 0x60
    hkVector4f m_extents; // 0x70
};

void hkpSampledHeightField_registerAllRayCastFunctions(); // 0x00D83F80
