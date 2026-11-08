// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/router/hkmemoryrouter.cpp
//
// Havok 2014.2.5 (havok_impl.md HK1): the per-thread memory router. Its instance pointer is a Win32 TLS slot
// (hkThreadLocalData, TlsAlloc at static initialization). The router holds the thread's LIFO stack allocator and the
// temp, heap, debug and solver allocators the memory system wires in (hkFreeListMemorySystem::threadInit).
//
// easyAlloc and alignedAlloc put a 16-byte header in front of the block: the allocator at -16, the requested size at
// -12 and, for aligned blocks, the offset back to the real block at -8. The free functions zero the allocator slot.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/router/hkmemoryrouter.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/allocator/hkmemoryallocator.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>

// The library's statics are constructed before and destroyed after the game's (the CRT runs the lib segment first and
// tears it down last): game statics such as havok_bp's New_pairs free their arrays through these at exit.
#pragma warning(disable : 4073) // the library segment is intended, see above
#pragma init_seg(lib)

// ---- hkThreadLocalData (hkthreadlocaldata.h; inline in the SDK, the <hkMemoryRouter*> copies are out of line)
// 0x00CE7840 <hkMemoryRouter *>
template<typename T>
hkThreadLocalData<T>::hkThreadLocalData()
{
    m_slotID = TlsAlloc();
}

// 0x00CE7850 <hkMemoryRouter *>
template<typename T>
hkThreadLocalData<T>::~hkThreadLocalData()
{
    TlsFree(m_slotID);
}

// 0x00718740 <hkMemoryRouter *> (.h:69)
template<typename T>
T hkThreadLocalData<T>::getData() const
{
    return (T)TlsGetValue(m_slotID);
}

// 0x00CE7860 <hkMemoryRouter *>
template<typename T>
void hkThreadLocalData<T>::setData(T p)
{
    TlsSetValue(m_slotID, (void*)p);
}

template class hkThreadLocalData<hkMemoryRouter*>;

// ---- globals
hkThreadLocalData<hkMemoryRouter*> hkMemoryRouter::s_memoryRouter; // 0x03A2615C

// ---- hkMemoryRouter
// 0x00CE77B0 (:28)
void hkMemoryRouter::replaceInstance(hkMemoryRouter* a)
{
    s_memoryRouter.setData(a);
}

// 0x0071A690 (.h:147)
hkMemoryRouter& hkMemoryRouter::getInstance()
{
    return *s_memoryRouter.getData();
}

// 0x00813F10 (.h:151)
hkMemoryRouter* hkMemoryRouter::getInstancePtr()
{
    return s_memoryRouter.getData();
}

// 0x00CE77D0 (:38)
// m_userData is left alone (threadInit sets it to the thread data index).
hkMemoryRouter::hkMemoryRouter()
{
    m_temp = nullptr;
    m_heap = nullptr;
    m_debug = nullptr;
    m_solver = nullptr;
    m_refObjLocalStore = 0;
}

// 0x0081C6A0
hkMemoryRouter::~hkMemoryRouter()
{
}

// 0x00CE7780 (:104)
void hkMemoryRouter::resetPeakMemoryStatistics()
{
    m_debug->resetPeakMemoryStatistics();
    m_heap->resetPeakMemoryStatistics();
    m_solver->resetPeakMemoryStatistics();
    m_temp->resetPeakMemoryStatistics();
}

// 0x0080C2B0 (.h:85)
hkLifoAllocator& hkMemoryRouter::stack()
{
    return m_stack;
}

// 0x00CE6CB0
hkMemoryAllocator& hkMemoryRouter::temp()
{
    return *m_temp;
}

// 0x00CF3930
void hkMemoryRouter::setTemp(hkMemoryAllocator* a)
{
    m_temp = a;
}

// 0x00710530 (.h:93)
hkMemoryAllocator& hkMemoryRouter::heap()
{
    return *m_heap;
}

// 0x00CF3940
void hkMemoryRouter::setHeap(hkMemoryAllocator* a)
{
    m_heap = a;
}

// 0x00CE6CC0
hkMemoryAllocator& hkMemoryRouter::debug()
{
    return *m_debug;
}

// 0x00CF3950
void hkMemoryRouter::setDebug(hkMemoryAllocator* a)
{
    m_debug = a;
}

// 0x00E0B700
hkMemoryAllocator& hkMemoryRouter::solver()
{
    return *m_solver;
}

// 0x00CF3960
void hkMemoryRouter::setSolver(hkMemoryAllocator* a)
{
    m_solver = a;
}

// 0x00CF3970
void* hkMemoryRouter::getUserData() const
{
    return m_userData;
}

// 0x00CE7450
unsigned int* hkMemoryRouter::getRefObjectLocalStore()
{
    return &m_refObjLocalStore;
}

const unsigned int* hkMemoryRouter::getRefObjectLocalStore() const
{
    return &m_refObjLocalStore;
}

// 0x00CF3980
void hkMemoryRouter::setUserData(void* p)
{
    m_userData = p;
}

// 0x00CE7690 (:42)
void* hkMemoryRouter::alignedAlloc(hkMemoryAllocator& b, int nbytes, int alignment)
{
    char* unaligned = (char*)b.blockAlloc(nbytes + alignment + 16); // 16: the header
    char* aligned = (char*)((uintptr_t)(unaligned + alignment + 15) & ~(uintptr_t)(alignment - 1));
    *(int*)(aligned - 12) = nbytes + alignment;
    *(hkMemoryAllocator**)(aligned - 16) = &b;
    *(int*)(aligned - 8) = (int)(aligned - unaligned);
    return aligned;
}

// 0x00CE76D0 (:63)
void hkMemoryRouter::alignedFree(hkMemoryAllocator& b, void* p)
{
    if (p != nullptr)
    {
        char* aligned = (char*)p;
        *(hkMemoryAllocator**)(aligned - 16) = nullptr;
        b.blockFree(aligned - *(int*)(aligned - 8), *(int*)(aligned - 12) + 16);
    }
}

// 0x00CE7700 (:74)
void* hkMemoryRouter::easyAlloc(hkMemoryAllocator& b, int nbytes)
{
    int* header = (int*)b.blockAlloc(nbytes + 16);
    header[1] = nbytes;
    *(hkMemoryAllocator**)header = &b;
    return header + 4;
}

// 0x00CE7730 (:82)
unsigned int hkMemoryRouter::getEasyAllocSize(hkMemoryAllocator& b, const void* ptr)
{
    return *(const unsigned int*)((const char*)ptr - 12);
}

// 0x00CE7740 (:88)
const void* hkMemoryRouter::getEasyAllocStartAddress(hkMemoryAllocator& b, const void* ptr)
{
    return (const char*)ptr - 16;
}

// 0x00CE7750 (:94)
void hkMemoryRouter::easyFree(hkMemoryAllocator& b, void* p)
{
    if (p != nullptr)
    {
        char* block = (char*)p - 16;
        *(hkMemoryAllocator**)block = nullptr;
        b.blockFree(block, *(int*)(block + 4) + 16);
    }
}

// ---- hkMemory (the class allocator operators, hkmemoryclasses.h)
// 0x00CE77F0 (:112)
void* hkMemory::heapBlockAlloc(unsigned int nbytes)
{
    return hkMemoryRouter::getInstance().heap().blockAlloc((int)nbytes);
}

// 0x00CE7810 (:117)
void hkMemory::heapBlockFree(void* p, unsigned int nbytes)
{
    hkMemoryRouter::getInstance().heap().blockFree(p, (int)nbytes);
}
