// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/system/hkmemorysystem.cpp
//
// Havok 2014.2.5 (havok_impl.md HK1): the memory system singleton and the defaults of its optional virtuals.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/system/hkmemorysystem.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/router/hkmemoryrouter.h"

// ---- globals
hkMemorySystem* hkMemorySystem::s_instance; // 0x03A26FEC

// 0x00CF1060 (:14)
hkMemorySystem::FrameInfo::FrameInfo(int solverBufferSize, int stackAllocatorSizeHint)
    : m_solverBufferSize(solverBufferSize)
    , m_stackAllocatorSizeHint(stackAllocatorSizeHint)
{
}

// 0x00CF1080 (:20)
void hkMemorySystem::replaceInstance(hkMemorySystem* m)
{
    s_instance = m;
}

// 0x00CF1090 (:25)
hkMemorySystem& hkMemorySystem::getInstance()
{
    return *s_instance;
}

// 0x00CF10A0 (:30)
hkMemorySystem* hkMemorySystem::getInstancePtr()
{
    return s_instance;
}

// 0x00CF3A00
hkMemorySystem::hkMemorySystem()
{
}

// 0x00CF10B0 (:35)
hkMemorySystem::~hkMemorySystem()
{
}

// 0x00CF10C0 (:70)
void hkMemorySystem::garbageCollectThread(hkMemoryRouter&)
{
}

// 0x00CF10D0 (:74)
void hkMemorySystem::garbageCollectShared()
{
}

// 0x00CF1120 (:78)
void hkMemorySystem::garbageCollect()
{
    garbageCollectThread(hkMemoryRouter::getInstance());
    garbageCollectShared();
}

// 0x00CF10E0 (:84)
hkResult hkMemorySystem::setHeapSoftLimit(int nbytes)
{
    return hkResult(HK_FAILURE);
}

// 0x00CF10F0 (:89)
int hkMemorySystem::getHeapSoftLimit() const
{
    return -1;
}

// 0x00CF1100 (:94)
bool hkMemorySystem::solverCanAllocSingleBlock(int numBytes)
{
    return true;
}

// 0x00CF1110 (:99)
bool hkMemorySystem::heapCanAllocTotal(int numBytes)
{
    return true;
}

// 0x00CF1160 (hkmemorysystem.h:145)
void hkMemorySystem::advanceFrame()
{
}

// 0x00CF1170 (hkmemorysystem.h:150)
hkDebugMemorySystem* hkMemorySystem::getDebugInterface()
{
    return nullptr;
}

// 0x00CF11B0 (hkmemorysystem.h:153)
hkResult hkMemorySystem::getMemorySnapshot(hkMemorySnapshot& snapshot) const
{
    return hkResult(HK_FAILURE);
}

// 0x00CF11C0 (hkmemorysystem.h:159)
hkResult hkMemorySystem::getAllocationCallStack(const void* ptr, unsigned long* callStack, int& stackSize, unsigned int& allocSize)
{
    return hkResult(HK_FAILURE);
}

// 0x00CF1180 (hkmemorysystem.h:168)
void hkMemorySystem::setHeapScrubValues(unsigned int allocValue, unsigned int freeValue)
{
}

// 0x00CF1190 (hkmemorysystem.h:171)
unsigned int hkMemorySystem::isOk() const
{
    return 1;
}

// ---- MemoryStatistics
// 0x0083BF60
hkMemorySystem::MemoryStatistics::MemoryStatistics()
{
}

// 0x0083BF80
// The entries' names are destroyed last first, then the array frees its storage.
hkMemorySystem::MemoryStatistics::~MemoryStatistics()
{
    for (int i = m_entries.getSize() - 1; i >= 0; i--)
    {
        m_entries[i].m_allocatorName.~hkStringPtr();
    }
}

// 0x00CF4670
hkMemorySystem::MemoryStatistics::Entry::Entry()
{
}

// 0x00813EB0
hkMemorySystem::MemoryStatistics::Entry::~Entry()
{
}
