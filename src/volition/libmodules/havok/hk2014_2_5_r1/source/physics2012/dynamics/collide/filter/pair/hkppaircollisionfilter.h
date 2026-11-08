// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/collide/filter/pair/hkppaircollisionfilter.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include <stdint.h>
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/hkcontainerallocators.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/pointermap/hkmap.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/filter/hkpcollisionfilter.h"

class hkBool;
class hkClass;
class hkFinishLoadedObjectFlag;
struct hkInternalClassMember;
class hkpCdBody;
class hkpCollidable;
struct hkpCollisionInput;
class hkpEntity;
class hkpShapeContainer;
struct hkpShapeRayCastInput;
struct hkpWorldRayCastInput;

// size 0x40
class hkpPairCollisionFilter : public hkpCollisionFilter
{
public:
    // size 0x8
    struct PairFilterKey
    {
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(PairFilterKey);

        hkpEntity* m_a; // 0x00
        hkpEntity* m_b; // 0x04
    };

    // size 0x1
    struct PairFilterPointerMapOperations
    {
        static unsigned int hash(PairFilterKey key, unsigned int modulus); // 0x00DEE340
        static void invalidate(PairFilterKey& key); // 0x00DEE370
        static unsigned int isValid(PairFilterKey key); // 0x00DEE390
        static unsigned int equal(PairFilterKey key0, PairFilterKey key1); // 0x00DEE3B0

    };

    // size 0xC
    class MapPairFilterKeyOverrideType
    {
    public:
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(MapPairFilterKeyOverrideType);
        static const hkClass& staticClass(); // 0x00DEE3D0

        static const hkInternalClassMember Members[];
        void* m_elem; // 0x00
        int m_numElems; // 0x04
        int m_hashMod; // 0x08
    };

    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int, void* p) 0x00DEE330, delete(void* p, unsigned int nbytes) 0x00DEE810
    static const hkClass& staticClass(); // 0x00DEE3F0
    hkpPairCollisionFilter(hkFinishLoadedObjectFlag flag); // 0x00DEE770
    hkpPairCollisionFilter(const hkpCollisionFilter* childFilter); // 0x00E0ED90
    virtual ~hkpPairCollisionFilter(); // vtbl+0x00 0x00E23A10
    int enableCollisionsBetween(const hkpEntity* entityA, const hkpEntity* entityB); // 0x00E23B50
    int disableCollisionsBetween(const hkpEntity* entityA, const hkpEntity* entityB); // 0x00E23C70
    virtual hkBool isCollisionEnabled(const hkpShapeRayCastInput& aInput, const hkpShapeContainer& bContainer, unsigned int bKey) const; // vtbl+0x00 0x00E23960
    virtual hkBool isCollisionEnabled(const hkpCollisionInput& input, const hkpCdBody& a, const hkpCdBody& b, const hkpShapeContainer& containerShapeA, const hkpShapeContainer& containerShapeB, unsigned int keyA, unsigned int keyB) const; // vtbl+0x00 0x00E23900
    virtual hkBool isCollisionEnabled(const hkpWorldRayCastInput& a, const hkpCollidable& collidableB) const; // vtbl+0x04 0x00E239B0
    virtual hkBool isCollisionEnabled(const hkpCollisionInput& input, const hkpCdBody& a, const hkpCdBody& b, const hkpShapeContainer& bContainer, unsigned int bKey) const; // vtbl+0x04 0x00E238B0
    virtual hkBool isCollisionEnabled(const hkpCollidable& collidableA, const hkpCollidable& collidableB) const; // vtbl+0x04 0x00E23AD0
    hkBool isCollisionEnabled(const hkpEntity* entityA, const hkpEntity* entityB) const; // 0x00E23A80
public:
    void calcKey(const hkpEntity* entityA, const hkpEntity* entityB, PairFilterKey& keyOut) const; // 0x00E23890
public:
    void clearAll(); // 0x00E23A00

public:
    static const hkInternalClassMember Members[];
    hkMap<PairFilterKey,uint64_t,PairFilterPointerMapOperations,hkContainerHeapAllocator> m_disabledPairs; // 0x30
    const hkpCollisionFilter* m_childFilter; // 0x3C
};
