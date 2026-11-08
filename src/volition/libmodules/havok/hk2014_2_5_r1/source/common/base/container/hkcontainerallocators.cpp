// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/hkcontainerallocators.cpp
//
// Havok 2014.2.5 (havok_impl.md HK1): the container allocators. Each forwards to the calling thread's router (temp,
// debug or heap allocator); hkContainerDefaultMallocAllocator (the CRT heap) is not part of the subset.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/hkcontainerallocators.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/router/hkmemoryrouter.h"

// The library's statics are constructed before and destroyed after the game's (the CRT runs the lib segment first and
// tears it down last): game statics such as havok_bp's New_pairs free their arrays through these at exit.
#pragma warning(disable : 4073) // the library segment is intended, see above
#pragma init_seg(lib)

// ---- globals
hkContainerTempAllocator::Allocator hkContainerTempAllocator::s_alloc; // 0x016F2998
hkContainerHeapAllocator::Allocator hkContainerHeapAllocator::s_alloc; // 0x016F299C
hkContainerDebugAllocator::Allocator hkContainerDebugAllocator::s_alloc;

// ---- hkContainerTempAllocator (:12-18)
// 0x00CE6D90
void* hkContainerTempAllocator::Allocator::blockAlloc(int numBytes)
{
    return hkMemoryRouter::getInstance().temp().blockAlloc(numBytes);
}

// 0x00CE6DB0
void hkContainerTempAllocator::Allocator::blockFree(void* p, int numBytes)
{
    hkMemoryRouter::getInstance().temp().blockFree(p, numBytes);
}

// 0x00CE6DD0
void* hkContainerTempAllocator::Allocator::bufAlloc(int& reqNumBytesInOut)
{
    return hkMemoryRouter::getInstance().temp().bufAlloc(reqNumBytesInOut);
}

// 0x00CE6DF0
void hkContainerTempAllocator::Allocator::bufFree(void* p, int numBytes)
{
    hkMemoryRouter::getInstance().temp().bufFree(p, numBytes);
}

// 0x00CE6E10
void* hkContainerTempAllocator::Allocator::bufRealloc(void* pold, int oldNumBytes, int& reqNumBytesInOut)
{
    return hkMemoryRouter::getInstance().temp().bufRealloc(pold, oldNumBytes, reqNumBytesInOut);
}

// 0x00CE6E30
void hkContainerTempAllocator::Allocator::getMemoryStatistics(hkMemoryAllocator::MemoryStatistics& u) const
{
    hkMemoryRouter::getInstance().temp().getMemoryStatistics(u);
}

// 0x00CE6E50
int hkContainerTempAllocator::Allocator::getAllocatedSize(const void* obj, int nbytes) const
{
    return hkMemoryRouter::getInstance().temp().getAllocatedSize(obj, nbytes);
}

// ---- hkContainerDebugAllocator (:21-27)
// 0x00CE6E70
void* hkContainerDebugAllocator::Allocator::blockAlloc(int numBytes)
{
    return hkMemoryRouter::getInstance().debug().blockAlloc(numBytes);
}

// 0x00CE6E90
void hkContainerDebugAllocator::Allocator::blockFree(void* p, int numBytes)
{
    hkMemoryRouter::getInstance().debug().blockFree(p, numBytes);
}

// 0x00CE6EB0
void* hkContainerDebugAllocator::Allocator::bufAlloc(int& reqNumBytesInOut)
{
    return hkMemoryRouter::getInstance().debug().bufAlloc(reqNumBytesInOut);
}

// 0x00CE6ED0
void hkContainerDebugAllocator::Allocator::bufFree(void* p, int numBytes)
{
    hkMemoryRouter::getInstance().debug().bufFree(p, numBytes);
}

// 0x00CE6EF0
void* hkContainerDebugAllocator::Allocator::bufRealloc(void* pold, int oldNumBytes, int& reqNumBytesInOut)
{
    return hkMemoryRouter::getInstance().debug().bufRealloc(pold, oldNumBytes, reqNumBytesInOut);
}

// 0x00CE6F10
void hkContainerDebugAllocator::Allocator::getMemoryStatistics(hkMemoryAllocator::MemoryStatistics& u) const
{
    hkMemoryRouter::getInstance().debug().getMemoryStatistics(u);
}

// 0x00CE6F30
int hkContainerDebugAllocator::Allocator::getAllocatedSize(const void* obj, int nbytes) const
{
    return hkMemoryRouter::getInstance().debug().getAllocatedSize(obj, nbytes);
}

// ---- hkContainerHeapAllocator (:30-36)
// 0x00CE6D60
hkContainerHeapAllocator::Allocator::Allocator()
{
}

// 0x00CE6D70
hkContainerHeapAllocator::Allocator::~Allocator()
{
}

// 0x00CE6F50
void* hkContainerHeapAllocator::Allocator::blockAlloc(int numBytes)
{
    return hkMemoryRouter::getInstance().heap().blockAlloc(numBytes);
}

// 0x00CE6F70
void hkContainerHeapAllocator::Allocator::blockFree(void* p, int numBytes)
{
    hkMemoryRouter::getInstance().heap().blockFree(p, numBytes);
}

// 0x00CE6F90
void* hkContainerHeapAllocator::Allocator::bufAlloc(int& reqNumBytesInOut)
{
    return hkMemoryRouter::getInstance().heap().bufAlloc(reqNumBytesInOut);
}

// 0x00CE6FB0
void hkContainerHeapAllocator::Allocator::bufFree(void* p, int numBytes)
{
    hkMemoryRouter::getInstance().heap().bufFree(p, numBytes);
}

// 0x00CE6FD0
void* hkContainerHeapAllocator::Allocator::bufRealloc(void* pold, int oldNumBytes, int& reqNumBytesInOut)
{
    return hkMemoryRouter::getInstance().heap().bufRealloc(pold, oldNumBytes, reqNumBytesInOut);
}

// 0x00CE6FF0
void hkContainerHeapAllocator::Allocator::getMemoryStatistics(hkMemoryAllocator::MemoryStatistics& u) const
{
    hkMemoryRouter::getInstance().heap().getMemoryStatistics(u);
}

// 0x00CE7010
int hkContainerHeapAllocator::Allocator::getAllocatedSize(const void* obj, int nbytes) const
{
    return hkMemoryRouter::getInstance().heap().getAllocatedSize(obj, nbytes);
}
