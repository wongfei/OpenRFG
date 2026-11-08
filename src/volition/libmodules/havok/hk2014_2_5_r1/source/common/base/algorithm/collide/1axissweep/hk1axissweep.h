// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/algorithm/collide/1axissweep/hk1axissweep.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/algorithm/sort/hkradixsort.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/geometry/aabb/hkaabb.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasedefs.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"

struct hkContainerHeapAllocator;
struct hkKeyPair;

// size 0x1
class hk1AxisSweep
{
public:
    // size 0x20
    struct AabbInt : public hkAabbUint32
    {
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(AabbInt);
        const unsigned int& getKey() const; // 0x00FB5090
        unsigned int& getKey(); // 0x00DB79C0
        const unsigned int& getUserData() const;
        unsigned int& getUserData();
        bool operator<(const AabbInt& aabb1) const; // 0x00DB79D0
        void operator=(const AabbInt& other); // 0x00DB95B0
        void setEndMarker(); // 0x00D51470
        void set(const hkAabb& aabbIn, int key); // 0x00FB50D0
        static unsigned int yzDisjoint(const AabbInt& aabb, const AabbInt& other); // 0x00FB50A0

    };

    // size 0x10
    struct IteratorAA
    {
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(IteratorAA);
        IteratorAA(const AabbInt*, int);
        unsigned int isValid() const;
        void next();
        void getKeyPair(hkKeyPair&);

    public:
        const AabbInt* m_aabbs; // 0x00
        int m_numAabbs; // 0x04
        int m_current; // 0x08
        int m_potential; // 0x0C
    };

    // size 0x2C
    struct IteratorAB
    {
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(IteratorAB);
        IteratorAB(AabbInt*, int, AabbInt*, int);
        unsigned int isValid() const;
        void next();
        void getKeyPair(hkKeyPair&);

    public:
        unsigned int m_aIsBigger; // 0x00
        const AabbInt* m_currentPtr; // 0x04
        const AabbInt* m_potentialPtr; // 0x08
        const AabbInt* m_pa; // 0x0C
        const AabbInt* m_pb; // 0x10
        int m_numA; // 0x14
        int m_numB; // 0x18
        hkDebugOnlyMember<AabbInt const* __restrict> m_originalA; // 0x1C
        hkDebugOnlyMember<AabbInt const* __restrict> m_originalB; // 0x20
        hkDebugOnlyMember<int> m_totalNumA; // 0x24
        hkDebugOnlyMember<int> m_totalNumB; // 0x28
    };

    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hk1AxisSweep);
    static void collide(const AabbInt* pa, int numA, hkArray<hkKeyPair,hkContainerHeapAllocator>& pairsOut); // 0x00FB5C60
    static int collide(const AabbInt* pa, int numA, hkKeyPair* pairsOut, int maxNumPairs, hkPadSpu<int>& numPairsSkipped); // 0x00FB55C0
    static void collide(const AabbInt* pa, int numA, const AabbInt* pb, int numB, hkArray<hkKeyPair,hkContainerHeapAllocator>& pairsOut); // 0x00FB57E0
    static int collide(const AabbInt* pa, int numA, const AabbInt* pb, int numB, hkKeyPair* pairsOut, int maxNumPairs, hkPadSpu<int>& numPairsSkipped); // 0x00FB51F0
    static void sortAabbs(AabbInt* aabbs, int size, hkArrayBase<hkRadixSort::SortData32>& sortArray, hkArrayBase<AabbInt>& sortedAabbs); // 0x00FB5140
    static void sortAabbs(AabbInt* aabbs, int size); // 0x00FB5EB0

};
