// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/system/hkmemorysystem.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/hkcontainerallocators.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/string/hkstringptr.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/allocator/hkmemoryallocator.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"

class hkDebugMemorySystem;
class hkMemoryRouter;
class hkMemorySnapshot;
class hkOstream;

// size 0x4
class hkMemorySystem
{
public:
    enum FlagBits
    {
        FLAG_PERSISTENT = 1,
        FLAG_TEMPORARY = 2,
        FLAG_ALL = 3,
        FLAG_ROUTER = 1,
        FLAG_FRAME = 2,
    };

    typedef hkFlags<enum FlagBits,int> Flags;

    // size 0x8
    struct FrameInfo
    {
        FrameInfo(int solverBufferSize, int stackAllocatorSizeHint); // 0x00CF1060

        int m_solverBufferSize; // 0x00
        int m_stackAllocatorSizeHint; // 0x04
    };

    // size 0xC
    struct MemoryStatistics
    {
        // size 0x1C
        struct Entry
        {
            HK_DECLARE_PLACEMENT_ALLOCATOR(); // delete(void*, unsigned int) 0x0080D960
            Entry(); // 0x00CF4670
            ~Entry(); // 0x00813EB0

            hkStringPtr m_allocatorName; // 0x00
            hkMemoryAllocator::MemoryStatistics m_allocatorStats; // 0x04
        };

        HK_DECLARE_PLACEMENT_ALLOCATOR();
        MemoryStatistics(); // 0x0083BF60
        ~MemoryStatistics(); // 0x0083BF80

        hkArray<Entry,hkContainerHeapAllocator> m_entries; // 0x00
    };

    HK_DECLARE_PLACEMENT_ALLOCATOR(); // new(unsigned int, void* p) 0x0080D950, delete(void*, unsigned int) 0x00CF1150
    virtual ~hkMemorySystem(); // vtbl+0x00 0x00CF10B0
    virtual hkMemoryRouter* mainInit(const FrameInfo&, hkFlags<enum FlagBits,int>) = 0; // vtbl+0x04
    virtual hkResult mainQuit(hkFlags<enum FlagBits,int>) = 0; // vtbl+0x08
    virtual void threadInit(hkMemoryRouter&, const char*, hkFlags<enum FlagBits,int>) = 0; // vtbl+0x0C
    virtual void threadQuit(hkMemoryRouter&, hkFlags<enum FlagBits,int>) = 0; // vtbl+0x10
    virtual hkResult setHeapSoftLimit(int nbytes); // vtbl+0x14 0x00CF10E0
    virtual int getHeapSoftLimit() const; // vtbl+0x18 0x00CF10F0
    virtual bool solverCanAllocSingleBlock(int numBytes); // vtbl+0x1C 0x00CF1100
    virtual bool heapCanAllocTotal(int numBytes); // vtbl+0x20 0x00CF1110
    virtual void printStatistics(hkOstream&) const = 0; // vtbl+0x24
    virtual void getMemoryStatistics(MemoryStatistics&) = 0; // vtbl+0x28
    virtual hkMemoryAllocator* getUncachedLockedHeapAllocator() = 0; // vtbl+0x2C
    virtual void garbageCollectThread(hkMemoryRouter&); // vtbl+0x30 0x00CF10C0
    virtual void garbageCollectShared(); // vtbl+0x34 0x00CF10D0
    virtual void garbageCollect(); // vtbl+0x38 0x00CF1120
    virtual void advanceFrame(); // vtbl+0x3C 0x00CF1160
    virtual hkDebugMemorySystem* getDebugInterface(); // vtbl+0x40 0x00CF1170
    virtual hkResult getMemorySnapshot(hkMemorySnapshot& snapshot) const; // vtbl+0x44 0x00CF11B0
    virtual hkResult getAllocationCallStack(const void* ptr, unsigned long* callStack, int& stackSize, unsigned int& allocSize); // vtbl+0x48 0x00CF11C0
    virtual void setHeapScrubValues(unsigned int allocValue, unsigned int freeValue); // vtbl+0x4C 0x00CF1180
    virtual unsigned int isOk() const; // vtbl+0x50 0x00CF1190
    static void replaceInstance(hkMemorySystem* m); // 0x00CF1080
    static hkMemorySystem& getInstance(); // 0x00CF1090
    static hkMemorySystem* getInstancePtr(); // 0x00CF10A0
    hkMemorySystem(); // 0x00CF3A00

    // 0x00 vfptr
public:
    static hkMemorySystem* s_instance; // 0x03A26FEC
};
