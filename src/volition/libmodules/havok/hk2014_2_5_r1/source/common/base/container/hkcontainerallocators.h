// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/hkcontainerallocators.h
//
// Hand-written (havok_impl.md HK1): the allocator tags of hkArray. Each tag's static allocator forwards every call to
// the matching allocator of the calling thread's memory router (heap, temp or debug), so the array storage follows
// the thread that grows the array (hkcontainerallocators.cpp).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/allocator/hkmemoryallocator.h"

// size 0x1
struct hkContainerTempAllocator
{
    // size 0x4
    struct Allocator : public hkMemoryAllocator
    {
        HK_DECLARE_PLACEMENT_ALLOCATOR(); // delete(void*, unsigned int) 0x00CE7030
        virtual void* blockAlloc(int numBytes); // vtbl+0x04 0x00CE6D90
        virtual void blockFree(void* p, int numBytes); // vtbl+0x08 0x00CE6DB0
        virtual void* bufAlloc(int& reqNumBytesInOut); // vtbl+0x0C 0x00CE6DD0
        virtual void bufFree(void* p, int numBytes); // vtbl+0x10 0x00CE6DF0
        virtual void* bufRealloc(void* pold, int oldNumBytes, int& reqNumBytesInOut); // vtbl+0x14 0x00CE6E10
        virtual void getMemoryStatistics(hkMemoryAllocator::MemoryStatistics& u) const; // vtbl+0x20 0x00CE6E30
        virtual int getAllocatedSize(const void* obj, int nbytes) const; // vtbl+0x24 0x00CE6E50
    };

    hkMemoryAllocator& get(const void*) { return s_alloc; } // 0x00CE6260 (.h:32)

    static Allocator s_alloc; // 0x016F2998
};

// size 0x1
struct hkContainerHeapAllocator
{
    // size 0x4
    struct Allocator : public hkMemoryAllocator
    {
        HK_DECLARE_PLACEMENT_ALLOCATOR(); // delete(void*, unsigned int) 0x00CE7060
        virtual ~Allocator(); // vtbl+0x00 0x00CE6D70
        virtual void* blockAlloc(int numBytes); // vtbl+0x04 0x00CE6F50
        virtual void blockFree(void* p, int numBytes); // vtbl+0x08 0x00CE6F70
        virtual void* bufAlloc(int& reqNumBytesInOut); // vtbl+0x0C 0x00CE6F90
        virtual void bufFree(void* p, int numBytes); // vtbl+0x10 0x00CE6FB0
        virtual void* bufRealloc(void* pold, int oldNumBytes, int& reqNumBytesInOut); // vtbl+0x14 0x00CE6FD0
        virtual void getMemoryStatistics(hkMemoryAllocator::MemoryStatistics& u) const; // vtbl+0x20 0x00CE6FF0
        Allocator(); // 0x00CE6D60
        virtual int getAllocatedSize(const void* obj, int nbytes) const; // vtbl+0x24 0x00CE7010
    };

    hkMemoryAllocator& get(const void*) { return s_alloc; } // 0x005DB490

    static Allocator s_alloc; // 0x016F299C
};

// size 0x1
struct hkContainerDebugAllocator
{
    // size 0x4
    struct Allocator : public hkMemoryAllocator
    {
        HK_DECLARE_PLACEMENT_ALLOCATOR(); // delete(void*, unsigned int) 0x00CE7090
        virtual void* blockAlloc(int numBytes); // vtbl+0x04 0x00CE6E70
        virtual void blockFree(void* p, int numBytes); // vtbl+0x08 0x00CE6E90
        virtual void* bufAlloc(int& reqNumBytesInOut); // vtbl+0x0C 0x00CE6EB0
        virtual void bufFree(void* p, int numBytes); // vtbl+0x10 0x00CE6ED0
        virtual void* bufRealloc(void* pold, int oldNumBytes, int& reqNumBytesInOut); // vtbl+0x14 0x00CE6EF0
        virtual void getMemoryStatistics(hkMemoryAllocator::MemoryStatistics& u) const; // vtbl+0x20 0x00CE6F10
        virtual int getAllocatedSize(const void* obj, int nbytes) const; // vtbl+0x24 0x00CE6F30
    };

    hkMemoryAllocator& get(const void*) { return s_alloc; } // 0x00D06EE0 (.h:70)

    static Allocator s_alloc;
};
