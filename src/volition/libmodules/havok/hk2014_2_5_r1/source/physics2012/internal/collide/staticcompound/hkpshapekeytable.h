// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/staticcompound/hkpshapekeytable.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"

class hkClass;
struct hkContainerHeapAllocator;
class hkFinishLoadedObjectFlag;
struct hkInternalClassMember;

// size 0x8
class hkpShapeKeyTable
{
public:
    enum
    {
        NUM_LISTS = 32,
        NUM_SLOTS_PER_BLOCK = 63,
    };

    enum
    {
        NUM_BYTES_PER_BLOCK = 256,
    };

    // size 0x100
    struct Block
    {
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(Block); // new(unsigned int n, void* p) 0x00EB8160, new(unsigned int nbytes) 0x00EB8720, delete(void* p, unsigned int nbytes) 0x00E33DF0
        static const hkClass& staticClass(); // 0x00E33D60
        Block(); // 0x00EB8180
        ~Block(); // 0x00EB81B0

        static const hkInternalClassMember Members[];
        unsigned int m_slots[63]; // 0x00
        Block* m_next; // 0xFC
    };

    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpShapeKeyTable); // new(unsigned int n, void* p) 0x00E33D50, delete(void* p, unsigned int nbytes) 0x00E33DC0
    static const hkClass& staticClass(); // 0x00E33D70
    hkpShapeKeyTable(hkFinishLoadedObjectFlag flag); // 0x00EB81E0
    hkpShapeKeyTable(); // 0x00EB81D0
    ~hkpShapeKeyTable(); // 0x00EB8620
    void insert(unsigned int key); // 0x00EB82F0
    void remove(unsigned int key); // 0x00EB8480
    bool exists(unsigned int key) const; // 0x00EB81F0
    bool isEmpty() const; // 0x00E6FE60
    void clear(); // 0x00EB8270
    void getDistribution(hkArray<int,hkContainerHeapAllocator>& counts) const; // 0x00EB8630

    static const hkInternalClassMember Members[]; // 0x01364294
public:
    Block* m_lists; // 0x00
    unsigned int m_occupancyBitField; // 0x04
};
