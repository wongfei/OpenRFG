// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/system/freelist/hkfreelistmemorysystem.cpp
//
// Havok 2014.2.5 (havok_impl.md HK1): the memory system the game uses (havok_memory_init: a large block allocator
// over Havok_mempool as the system allocator, a free list allocator on it as the heap). Each thread's router gets:
// - heap: its own hkThreadMemory cache over the heap allocator (USE_THREAD_MEMORY), or the heap allocator itself;
// - debug: the recall allocator over the system allocator;
// - stack: a LIFO allocator whose slabs come from the solver allocator (USE_SOLVER_ALLOCATOR_FOR_LIFO_SLABS) or the
//   heap, its large blocks and bookkeeping from the heap;
// - temp: the stack (USE_LIFO_ALLOCATOR_FOR_TEMP) or the heap; solver: the shared solver allocator, whose buffer
//   (FrameInfo::m_solverBufferSize) mainInit takes from the system allocator.
// Up to 64 threads (ThreadData slots, the slot index is the router's user data).
//
// Not reimplemented (S phase): printStatistics prints nothing (hkOstream), getMemorySnapshot fails (hkMemorySnapshot).

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/system/freelist/hkfreelistmemorysystem.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/util/hkmemutil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/system/error/hkerror.h"

#include <intrin.h>
#include <new>

// ---- ThreadData
// 0x00CF39E0
// m_memoryRouter is left alone.
hkFreeListMemorySystem::ThreadData::ThreadData()
{
    m_name = nullptr;
    m_inUse.m_bool = 0;
}

// 0x00CF3A30
hkFreeListMemorySystem::ThreadData::~ThreadData()
{
}

// ---- construction
// 0x00CF4360 (:21)
hkFreeListMemorySystem::hkFreeListMemorySystem(hkMemoryAllocator* systemAllocator, hkMemoryAllocator* heapAllocator, hkMemoryAllocator::ExtendedInterface* heapExtended, SetupFlags flags)
    : m_systemAllocator(systemAllocator)
    , m_frameInfo(0, 0x8000) // 32 KB: default stack slab size
    , m_heapAllocator(heapAllocator)
    , m_heapExtended(heapExtended)
    , m_debugAllocator(systemAllocator)
    , m_threadDataLock(0) // 0: the spin count from the number of hardware threads
    , m_flags(flags)
{
}

// 0x00CF3ED0 (:25)
hkFreeListMemorySystem::~hkFreeListMemorySystem()
{
}

// 0x00CF3F50 (:31)
hkMemoryRouter* hkFreeListMemorySystem::mainInit(const hkMemorySystem::FrameInfo& info, hkFlags<enum hkMemorySystem::FlagBits,int> flags)
{
    m_frameInfo = info;
    if ((flags.m_storage & FLAG_PERSISTENT) != 0)
    {
        threadInit(m_mainRouter, "main", flags);
    }
    if ((flags.m_storage & FLAG_TEMPORARY) != 0 && info.m_solverBufferSize != 0)
    {
        void* buffer = m_systemAllocator->blockAlloc(info.m_solverBufferSize);
        m_solverAllocator.setBuffer(buffer, info.m_solverBufferSize);
    }
    return &m_mainRouter;
}

// 0x00CF3FB0 (:53)
hkResult hkFreeListMemorySystem::mainQuit(hkFlags<enum hkMemorySystem::FlagBits,int> flags)
{
    if ((flags.m_storage & FLAG_TEMPORARY) != 0)
    {
        const int size = (int)(m_solverAllocator.m_bufferEnd - m_solverAllocator.m_bufferStart);
        if (size != 0)
        {
            m_systemAllocator->blockFree(m_solverAllocator.m_bufferStart, size);
        }
    }
    if ((flags.m_storage & FLAG_PERSISTENT) != 0)
    {
        threadQuit(m_mainRouter, flags);
        m_solverAllocator.setBuffer(nullptr, 0);
        if (m_heapExtended != nullptr)
        {
            m_heapExtended->garbageCollect();
        }
    }
    m_systemAllocator = nullptr;
    return hkResult(HK_SUCCESS);
}

// 0x00CF4030 (:80)
void hkFreeListMemorySystem::threadInit(hkMemoryRouter& router, const char* name, hkFlags<enum hkMemorySystem::FlagBits,int> flags)
{
    if ((flags.m_storage & FLAG_PERSISTENT) != 0)
    {
        m_threadDataLock.enter();
        int index = 0;
        for (; index < THREAD_MAX; index++)
        {
            if (!m_threadData[index].m_inUse)
            {
                m_threadData[index].m_inUse.m_bool = 1;
                break;
            }
        }
        m_threadDataLock.leave();
        if (index >= THREAD_MAX)
        {
            // HK_ERROR(0xf03454fe, "Too many threads")
            if (hkErrorFwd::messageError(0xF03454FE, "Too many threads", "Memory\\System\\FreeList\\hkFreeListMemorySystem.cpp", 100) != 0)
            {
                __debugbreak();
            }
            return;
        }
        ThreadData& td = m_threadData[index];
        td.m_name = name;
        td.m_heapThreadMemory.setMemory(m_heapAllocator, 8); // 8: blocks kept per size row
        router.m_temp = nullptr;
        router.m_heap = (m_flags & USE_THREAD_MEMORY) != 0 ? &td.m_heapThreadMemory : m_heapAllocator;
        router.m_debug = &m_debugAllocator;
        router.m_solver = nullptr;
        router.m_userData = (void*)(intptr_t)index;
    }
    if ((flags.m_storage & FLAG_TEMPORARY) != 0)
    {
        hkMemoryAllocator* large = (m_flags & USE_THREAD_MEMORY) != 0 ? &m_threadData[(intptr_t)router.m_userData].m_heapThreadMemory : m_heapAllocator;
        hkMemoryAllocator* slab = (m_flags & USE_SOLVER_ALLOCATOR_FOR_LIFO_SLABS) != 0 ? &m_solverAllocator : large;
        router.m_stack.init(slab, large, large, m_frameInfo.m_stackAllocatorSizeHint);
        router.m_temp = (m_flags & USE_LIFO_ALLOCATOR_FOR_TEMP) != 0 ? &router.m_stack : large;
        router.m_solver = &m_solverAllocator;
    }
}

// 0x00CF41A0 (:164)
void hkFreeListMemorySystem::threadQuit(hkMemoryRouter& router, hkFlags<enum hkMemorySystem::FlagBits,int> flags)
{
    ThreadData& td = m_threadData[(intptr_t)router.m_userData];
    if ((flags.m_storage & FLAG_TEMPORARY) != 0)
    {
        router.m_stack.quit(nullptr);
        td.m_heapThreadMemory.releaseCachedMemory();
        router.m_temp = nullptr;
        router.m_solver = nullptr;
    }
    if ((flags.m_storage & FLAG_PERSISTENT) != 0)
    {
        td.m_heapThreadMemory.releaseCachedMemory();
        td.m_name = nullptr;
        td.m_inUse.m_bool = 0;
        hkMemUtil::memSet(&router, 0, sizeof(hkMemoryRouter));
    }
}

// 0x00CF3DE0 (:285)
void hkFreeListMemorySystem::garbageCollectThread(hkMemoryRouter& router)
{
    if ((m_flags & USE_THREAD_MEMORY) != 0)
    {
        static_cast<hkThreadMemory&>(router.heap()).releaseCachedMemory();
    }
}

// 0x00CF3E00 (:293)
void hkFreeListMemorySystem::garbageCollectShared()
{
    if (m_heapExtended != nullptr)
    {
        m_heapExtended->garbageCollect();
    }
}

// 0x00CF3E20 (:332)
hkResult hkFreeListMemorySystem::setHeapSoftLimit(int nbytes)
{
    if (m_heapExtended != nullptr)
    {
        return m_heapExtended->setMemorySoftLimit(nbytes);
    }
    return hkResult(HK_SUCCESS);
}

// 0x00CF3E50 (:341)
int hkFreeListMemorySystem::getHeapSoftLimit() const
{
    if (m_heapExtended != nullptr)
    {
        return (int)m_heapExtended->getMemorySoftLimit();
    }
    return 0;
}

// 0x00CF3E70 (:350)
bool hkFreeListMemorySystem::solverCanAllocSingleBlock(int numBytes)
{
    return m_solverAllocator.canAllocSingleBlock(numBytes);
}

// 0x00CF3E80 (:355)
bool hkFreeListMemorySystem::heapCanAllocTotal(int numBytes)
{
    if (m_heapExtended != nullptr)
    {
        return m_heapExtended->canAllocTotal(numBytes);
    }
    return true;
}

// 0x00CF3EA0 (:364)
hkMemoryAllocator* hkFreeListMemorySystem::getUncachedLockedHeapAllocator()
{
    return m_heapAllocator;
}

// 0x00CF3EB0 (:369)
void hkFreeListMemorySystem::setHeapScrubValues(unsigned int allocValue, unsigned int freeValue)
{
    if (m_heapExtended != nullptr)
    {
        m_heapExtended->setScrubValues(allocValue, freeValue);
    }
}

// 0x00CF4400 (:194)
// One entry per allocator: "System", "Heap", "Debug", "Solver" (the entries of an earlier call are dropped).
void hkFreeListMemorySystem::getMemoryStatistics(hkMemorySystem::MemoryStatistics& stats)
{
    for (int i = stats.m_entries.getSize() - 1; i >= 0; i--)
    {
        stats.m_entries[i].m_allocatorName.~hkStringPtr();
    }
    stats.m_entries.clear();
    struct Source
    {
        const char* name;
        const hkMemoryAllocator* allocator;
    };
    const Source sources[4] = {
        { "System", m_systemAllocator },
        { "Heap", m_heapAllocator },
        { "Debug", &m_debugAllocator },
        { "Solver", &m_solverAllocator },
    };
    for (int i = 0; i < 4; i++)
    {
        hkMemorySystem::MemoryStatistics::Entry* entry = new (&stats.m_entries.expandOne()) hkMemorySystem::MemoryStatistics::Entry();
        entry->m_allocatorName = sources[i].name;
        sources[i].allocator->getMemoryStatistics(entry->m_allocatorStats);
    }
}

// 0x00CF3A80 (:219)
// hkOstream is S phase (not reimplemented): nothing is printed.
void hkFreeListMemorySystem::printStatistics(hkOstream& ostr) const
{
}

// 0x00CF4210 (:301)
// hkMemorySnapshot is S phase (not reimplemented).
hkResult hkFreeListMemorySystem::getMemorySnapshot(hkMemorySnapshot& snapshot) const
{
    return hkResult(HK_FAILURE);
}
