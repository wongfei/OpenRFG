// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcollidable.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcdbody.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcollidablequalitytype.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/dispatch/broadphase/hkptypedbroadphasehandle.h"

struct hkAabbUint32;
class hkBool;
class hkClass;
class hkFinishLoadedObjectFlag;
struct hkInternalClassMember;
class hkMotionState;
class hkTransformf;
class hkpShape;

// size 0x50
class hkpCollidable : public hkpCdBody
{
public:
    enum ForceCollideOntoPpuReasons
    {
        FORCE_PPU_USER_REQUEST = 1,
        FORCE_PPU_SHAPE_REQUEST = 2,
        FORCE_PPU_MODIFIER_REQUEST = 4,
        FORCE_PPU_SHAPE_UNCHECKED = 8,
    };

    // size 0x2C
    struct BoundingVolumeData
    {
        static const hkClass& staticClass(); // 0x00D7DC00
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(BoundingVolumeData); // new(unsigned int n, void* p) 0x00D7DCC0
        BoundingVolumeData(hkFinishLoadedObjectFlag flag); // 0x00D7D970
        BoundingVolumeData(); // 0x00DCADD0
        void afterReflectNew(); // 0x00D7D8E0
        bool hasAllocations() const; // 0x00DB9340
        void deallocate(); // 0x00DCAE80
        void allocate(int numChildShapes); // 0x00DCAEF0
        void invalidate(); // 0x00D7D8C0
        bool isValid() const; // 0x00DB9330

        static const hkInternalClassMember Members[];
        unsigned int m_min[3]; // 0x00
        unsigned char m_expansionMin[3]; // 0x0C
        unsigned char m_expansionShift; // 0x0F
        unsigned int m_max[3]; // 0x10
        unsigned char m_expansionMax[3]; // 0x1C
        unsigned char m_padding; // 0x1F
        unsigned short m_numChildShapeAabbs; // 0x20
        unsigned short m_capacityChildShapeAabbs; // 0x22
        hkAabbUint32* m_childShapeAabbs; // 0x24
        unsigned int* m_childShapeKeys; // 0x28
    };

    static const hkClass& staticClass(); // 0x00D7DC20
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpCollidable); // new(unsigned int n, void* p) 0x00D7DCB0, new(unsigned int nbytes) 0x00F1CC10, delete(void* p, unsigned int nbytes) 0x00D7DCD0
    hkpCollidable(hkFinishLoadedObjectFlag flag); // 0x00D7DA00
    hkpCollidable(const hkpShape* shape, const hkMotionState* ms, int type); // 0x00DB9B20
    hkpCollidable(const hkpShape* shape, const hkTransformf* t, int type); // 0x00D9EC90
    ~hkpCollidable(); // 0x00D7D900
    // 0x00710F90 (hkpcollidable.inl:99)
    const hkpShape* getShape() const
    {
        return m_shape;
    }
    void setShape(const hkpShape* shape); // 0x00DF2870
    void setOwner(void* owner); // 0x00DF4400
    // 0x00710F80 (.h:71)
    void* getOwner() const
    {
        return const_cast<char*>(reinterpret_cast<const char*>(this)) + m_ownerOffset;
    }
    hkBool checkPerformance(); // 0x00DCAE00
    hkpTypedBroadPhaseHandle* getBroadPhaseHandle(); // 0x0080CCB0 (hkpcollidable.inl:121)
    const hkpTypedBroadPhaseHandle* getBroadPhaseHandle() const; // 0x0080CCA0 (hkpcollidable.inl:116)
    hkpCollidableQualityType getQualityType() const; // 0x00DDCC20
    void setQualityType(hkpCollidableQualityType);
    float getAllowedPenetrationDepth() const; // 0x00DB7070
    void setAllowedPenetrationDepth(float);
    unsigned int getCollisionFilterInfo() const; // 0x0080CCC0 (hkpcollidable.inl:141)
    void setCollisionFilterInfo(unsigned int info); // 0x0080CCD0 (hkpcollidable.inl:146)
    void afterReflectNew(); // 0x00D7D8D0
    int getType() const; // 0x00710FA0 (hkpcollidable.inl:126)

    static const hkInternalClassMember Members[]; // 0x01355B38
public:
    signed char m_ownerOffset; // 0x10
public:
    unsigned char m_forceCollideOntoPpu; // 0x11
    unsigned short m_shapeSizeOnSpu; // 0x12
    hkpTypedBroadPhaseHandle m_broadPhaseHandle; // 0x14
    BoundingVolumeData m_boundingVolumeData; // 0x20
    float m_allowedPenetrationDepth; // 0x4C
};
