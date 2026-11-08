// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/broadphase/hkpbroadphase.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/debugutil/multithreadcheck/hkmultithreadcheck.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkvector4f.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/object/hkreferencedobject.h"

class hkAabb;
struct hkAabbUint32;
struct hkContainerHeapAllocator;
class hkCriticalSection;
class hkpBroadPhaseCastCollector;
class hkpBroadPhaseHandle;
class hkpBroadPhaseHandlePair;
class hkpCollidable;

// size 0x20
class hkpBroadPhase : public hkReferencedObject
{
public:
    enum BroadPhaseType
    {
        BROADPHASE_16BIT = 0,
        BROADPHASE_32BIT = 1,
        BROADPHASE_TREE_16BIT = 2,
        BROADPHASE_TREE_32BIT = 3,
    };

    enum Capabilities
    {
        SUPPORT_SPU_RAY_CAST = 1,
        SUPPORT_SPU_LINEAR_CAST = 2,
        SUPPORT_SPU_AABB_QUERY = 4,
        SUPPORT_SPU_CLOSEST_POINTS = 8,
        SUPPORT_SPU_CHAR_PROXY_INT = 16,
        SUPPORT_FLOAT_AABB = 32,
        ISA_SWEEP_AND_PRUNE = 4096,
    };

    typedef hkpBroadPhase* (*createSweepAndPruneBroadPhaseFunc)(const hkVector4f&, const hkVector4f&, int);
    typedef hkpBroadPhase* (*createTreeBroadPhaseFunc)(hkpBroadPhase*);
    typedef void (*updateTreeBroadPhaseFunc)(hkpBroadPhase*, float);

    // size 0x20
    struct hkpCastRayInput
    {
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpCastRayInput);
        hkpCastRayInput(); // 0x00D93040

        hkVector4f m_from; // 0x00
        int m_numCasts; // 0x10
        const hkVector4f* m_toBase; // 0x14
        int m_toStriding; // 0x18
        const char* m_aabbCacheInfo; // 0x1C
    };

    // size 0x40
    struct alignas(16) hkpCastAabbInput
    {
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpCastAabbInput);
        hkpCastAabbInput(); // 0x0104BCB0

        hkVector4f m_from; // 0x00
        hkVector4f m_to; // 0x10
        hkVector4f m_halfExtents; // 0x20
        const char* m_aabbCacheInfo; // 0x30
    };

    // size 0x4
    class Enable32BitBroadPhase
    {
    public:
        Enable32BitBroadPhase();
        ~Enable32BitBroadPhase();
    public:
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(Enable32BitBroadPhase);

    public:
        hkpBroadPhase* (*m_originalCreateBroadPhaseFunc)(const hkVector4f&, const hkVector4f&, int); // 0x00
    };

    // size 0x4
    class Enable32BitTreeBroadPhase
    {
    public:
        Enable32BitTreeBroadPhase();
        ~Enable32BitTreeBroadPhase();
    public:
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(Enable32BitTreeBroadPhase);

    public:
        hkpBroadPhase* (*m_originalCreateBroadPhaseFunc)(hkpBroadPhase*); // 0x00
    };

    // size 0x8
    class Enable32BitHybridBroadPhase
    {
    public:
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(Enable32BitHybridBroadPhase);

        Enable32BitBroadPhase m_enable32BitBroadPhase; // 0x00
        Enable32BitTreeBroadPhase m_enable32BitTreeBroadPhase; // 0x04
    };

    hkpBroadPhase(BroadPhaseType type, int size, int caps); // 0x00DB0FA0
    virtual ~hkpBroadPhase(); // vtbl+0x00 0x00DB1070
    HK_DECLARE_CLASS_ALLOCATOR(); // delete(void* p, unsigned int nbytes) 0x00DB10D0
    virtual BroadPhaseType getType() const = 0; // vtbl+0x10
    virtual const hkpBroadPhase* getCapabilityDelegate(Capabilities cap) const; // vtbl+0x14 0x00D8F8E0
    virtual void addObject(hkpBroadPhaseHandle*, const hkAabb&, hkArray<hkpBroadPhaseHandlePair,hkContainerHeapAllocator>&, bool) = 0; // vtbl+0x18
    virtual void addObject(hkpBroadPhaseHandle*, const hkAabbUint32&, hkArray<hkpBroadPhaseHandlePair,hkContainerHeapAllocator>&, bool) = 0; // vtbl+0x1C
    virtual void addObjectBatch(const hkArrayBase<hkpBroadPhaseHandle*>&, const hkArrayBase<hkAabb>&, hkArray<hkpBroadPhaseHandlePair,hkContainerHeapAllocator>&) = 0; // vtbl+0x20
    virtual void removeObject(hkpBroadPhaseHandle*, hkArray<hkpBroadPhaseHandlePair,hkContainerHeapAllocator>&) = 0; // vtbl+0x24
    virtual void removeObjectBatch(const hkArrayBase<hkpBroadPhaseHandle*>&, hkArray<hkpBroadPhaseHandlePair,hkContainerHeapAllocator>&) = 0; // vtbl+0x28
    virtual int getNumObjects() const = 0; // vtbl+0x2C
    virtual void updateAabbs(hkpBroadPhaseHandle**, const hkAabb*, int, hkArray<hkpBroadPhaseHandlePair,hkContainerHeapAllocator>&, hkArray<hkpBroadPhaseHandlePair,hkContainerHeapAllocator>&) = 0; // vtbl+0x30
    virtual void updateAabbsUint32(hkpBroadPhaseHandle**, const hkAabbUint32*, int, hkArray<hkpBroadPhaseHandlePair,hkContainerHeapAllocator>&, hkArray<hkpBroadPhaseHandlePair,hkContainerHeapAllocator>&) = 0; // vtbl+0x34
    virtual void defragment() = 0; // vtbl+0x38
    virtual void checkDeterminism(); // vtbl+0x3C 0x00DB10C0
    virtual void getAllAabbs(hkArray<hkAabb,hkContainerHeapAllocator>&) const = 0; // vtbl+0x40
    virtual void getAabb(const hkpBroadPhaseHandle*, hkAabb&) const = 0; // vtbl+0x44
    virtual void getExtents(hkVector4f&, hkVector4f&) const = 0; // vtbl+0x48
    virtual void querySingleAabb(const hkAabb&, hkArray<hkpBroadPhaseHandlePair,hkContainerHeapAllocator>&) const = 0; // vtbl+0x4C
    virtual void reQuerySingleObject(const hkpBroadPhaseHandle*, hkArray<hkpBroadPhaseHandlePair,hkContainerHeapAllocator>&) const = 0; // vtbl+0x50
    virtual void querySingleAabbWithCollector(const hkAabb&, hkpBroadPhaseCastCollector*) const = 0; // vtbl+0x54
    virtual bool areAabbsOverlapping(const hkpBroadPhaseHandle*, const hkpBroadPhaseHandle*) const = 0; // vtbl+0x58
    virtual void shiftAllObjects(const hkVector4f&, hkVector4f&, hkArray<hkpBroadPhaseHandlePair,hkContainerHeapAllocator>&) = 0; // vtbl+0x5C
    virtual void shiftBroadPhase(const hkVector4f&, hkVector4f&, hkArray<hkpBroadPhaseHandlePair,hkContainerHeapAllocator>&) = 0; // vtbl+0x60
    virtual void getOffsetLowHigh32bit(hkVector4f&, hkVector4f&) const = 0; // vtbl+0x64
    virtual void castRay(const hkpCastRayInput&, hkpBroadPhaseCastCollector*, int) const = 0; // vtbl+0x68
    virtual int getAabbCacheSize() const = 0; // vtbl+0x6C
    virtual void calcAabbCache(const hkArrayBase<hkpCollidable*>&, char*) const = 0; // vtbl+0x70
    virtual void calcAabbCache(const hkAabb&, char*) const = 0; // vtbl+0x74
    virtual void castAabb(const hkpCastAabbInput&, hkpBroadPhaseCastCollector&) const = 0; // vtbl+0x78
    virtual void set32BitOffsetAndScale(const hkVector4f&, const hkVector4f&, const hkVector4f&) = 0; // vtbl+0x7C
    void enableMultiThreading(int spinCountForCriticalSection); // 0x00DB1010
    const hkMultiThreadCheck& getMultiThreadCheck() const;
    hkMultiThreadCheck& getMultiThreadCheck(); // 0x00DE85A0
    void markForRead() const;
    void markForWrite(); // 0x00DB0F80
    void unmarkForRead() const;
    void unmarkForWrite(); // 0x00DB0F90
    void lock(); // 0x00DFFDA0
    void unlock(); // 0x00DFFDB0
public:
    void lockImplementation(); // 0x00DB0FF0
    void unlockImplementation(); // 0x00DB1000

public:
    const unsigned short m_type; // 0x08
    const unsigned short m_size; // 0x0A
    const unsigned int m_caps; // 0x0C
public:
    hkMultiThreadCheck m_multiThreadCheck; // 0x10
    hkCriticalSection* m_criticalSection; // 0x1C
public:
    static hkpBroadPhase* (*s_createSweepAndPruneBroadPhaseFunction)(const hkVector4f&, const hkVector4f&, int); // 0x03A29D60
    static hkpBroadPhase* (*s_createTreeBroadPhaseFunction)(hkpBroadPhase*); // 0x03A29D64
    static void (*s_updateTreeBroadPhaseFunction)(hkpBroadPhase*, float); // 0x03A29D68
    static void (*s_updateTreeBroadPhaseFunction32)(hkpBroadPhase*, float); // 0x03A29D6C
};
