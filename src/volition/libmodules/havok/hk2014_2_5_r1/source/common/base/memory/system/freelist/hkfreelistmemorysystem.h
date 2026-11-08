// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/system/freelist/hkfreelistmemorysystem.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/allocator/hkmemoryallocator.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/allocator/recall/hkrecallallocator.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/allocator/solver/hksolverallocator.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/allocator/thread/hkthreadmemory.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/router/hkmemoryrouter.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/system/hkmemorysystem.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/thread/criticalsection/hkcriticalsection.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"

class hkMemorySnapshot;
class hkOstream;

// size 0x4EEC
class hkFreeListMemorySystem : public hkMemorySystem
{
public:
    enum SetupFlags
    {
        USE_LIFO_ALLOCATOR_FOR_TEMP = 1,
        USE_SOLVER_ALLOCATOR_FOR_LIFO_SLABS = 2,
        USE_THREAD_MEMORY = 4,
        DEFAULT_SETUP_FLAGS = 5,
    };

    enum
    {
        THREAD_MAX = 64,
    };

    // size 0x130
    struct ThreadData
    {
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(ThreadData);
        ThreadData(); // 0x00CF39E0
        ~ThreadData(); // 0x00CF3A30

        hkThreadMemory m_heapThreadMemory; // 0x00
        hkMemoryRouter* m_memoryRouter; // 0x124
        const char* m_name; // 0x128
        hkBool m_inUse; // 0x12C
    };

    hkFreeListMemorySystem(hkMemoryAllocator* systemAllocator, hkMemoryAllocator* heapAllocator, hkMemoryAllocator::ExtendedInterface* heapExtended, SetupFlags flags); // 0x00CF4360
    virtual ~hkFreeListMemorySystem(); // vtbl+0x00 0x00CF3ED0
    virtual hkMemoryRouter* mainInit(const hkMemorySystem::FrameInfo& info, hkFlags<enum hkMemorySystem::FlagBits,int> flags); // vtbl+0x04 0x00CF3F50
    virtual hkResult mainQuit(hkFlags<enum hkMemorySystem::FlagBits,int> flags); // vtbl+0x08 0x00CF3FB0
    virtual void threadInit(hkMemoryRouter& router, const char* name, hkFlags<enum hkMemorySystem::FlagBits,int> flags); // vtbl+0x0C 0x00CF4030
    virtual void threadQuit(hkMemoryRouter& router, hkFlags<enum hkMemorySystem::FlagBits,int> flags); // vtbl+0x10 0x00CF41A0
    virtual hkResult setHeapSoftLimit(int nbytes); // vtbl+0x14 0x00CF3E20
    virtual int getHeapSoftLimit() const; // vtbl+0x18 0x00CF3E50
    hkMemoryAllocator* getAllocator();
    virtual bool solverCanAllocSingleBlock(int numBytes); // vtbl+0x1C 0x00CF3E70
    virtual bool heapCanAllocTotal(int numBytes); // vtbl+0x20 0x00CF3E80
    virtual void printStatistics(hkOstream& ostr) const; // vtbl+0x24 0x00CF3A80
    virtual void getMemoryStatistics(hkMemorySystem::MemoryStatistics& stats); // vtbl+0x28 0x00CF4400
    virtual hkMemoryAllocator* getUncachedLockedHeapAllocator(); // vtbl+0x2C 0x00CF3EA0
    virtual void garbageCollectThread(hkMemoryRouter& router); // vtbl+0x30 0x00CF3DE0
    virtual void garbageCollectShared(); // vtbl+0x34 0x00CF3E00
    virtual hkResult getMemorySnapshot(hkMemorySnapshot& snapshot) const; // vtbl+0x44 0x00CF4210
    virtual void setHeapScrubValues(unsigned int allocValue, unsigned int freeValue); // vtbl+0x4C 0x00CF3EB0

public:
    hkMemoryAllocator* m_systemAllocator; // 0x04
    hkMemorySystem::FrameInfo m_frameInfo; // 0x08
    hkMemoryAllocator* m_heapAllocator; // 0x10
    hkMemoryAllocator::ExtendedInterface* m_heapExtended; // 0x14
    hkRecallAllocator m_debugAllocator; // 0x18
    hkMemoryRouter m_mainRouter; // 0x54
    hkSolverAllocator m_solverAllocator; // 0x94
    ThreadData m_threadData[64]; // 0x2D0
    hkCriticalSection m_threadDataLock; // 0x4ED0
    int m_flags; // 0x4EE8
};
