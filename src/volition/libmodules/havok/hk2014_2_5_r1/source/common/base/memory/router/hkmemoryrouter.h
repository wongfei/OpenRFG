// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/router/hkmemoryrouter.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/allocator/lifo/hklifoallocator.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/thread/thread/hkthreadlocaldata.h"

class hkMemoryAllocator;

// size 0x40
class hkMemoryRouter
{
public:
    typedef hkLifoAllocator Stack;
    typedef hkMemoryAllocator Allocator;

    HK_DECLARE_PLACEMENT_ALLOCATOR(); // new(unsigned int, void* p) 0x0080C290, delete(void*, unsigned int) 0x0080C2A0
    static void replaceInstance(hkMemoryRouter* a); // 0x00CE77B0
    static hkMemoryRouter& getInstance(); // 0x0071A690 (.h:147)
    static hkMemoryRouter* getInstancePtr(); // 0x00813F10 (.h:151)
    hkMemoryRouter(); // 0x00CE77D0
    void resetPeakMemoryStatistics(); // 0x00CE7780
    hkLifoAllocator& stack(); // 0x0080C2B0 (.h:85)
    hkMemoryAllocator& temp(); // 0x00CE6CB0
    void setTemp(hkMemoryAllocator* a); // 0x00CF3930
    hkMemoryAllocator& heap(); // 0x00710530 (.h:93)
    void setHeap(hkMemoryAllocator* a); // 0x00CF3940
    hkMemoryAllocator& debug(); // 0x00CE6CC0
    void setDebug(hkMemoryAllocator* a); // 0x00CF3950
    hkMemoryAllocator& solver(); // 0x00E0B700
    void setSolver(hkMemoryAllocator* a); // 0x00CF3960
    void* getUserData() const; // 0x00CF3970
    unsigned int* getRefObjectLocalStore(); // 0x00CE7450
    const unsigned int* getRefObjectLocalStore() const;
    void setUserData(void* p); // 0x00CF3980
    static void* alignedAlloc(hkMemoryAllocator& b, int nbytes, int alignment); // 0x00CE7690
    static void alignedFree(hkMemoryAllocator& b, void* p); // 0x00CE76D0
    static void* easyAlloc(hkMemoryAllocator& b, int nbytes); // 0x00CE7700
    static unsigned int getEasyAllocSize(hkMemoryAllocator& b, const void* ptr); // 0x00CE7730
    static const void* getEasyAllocStartAddress(hkMemoryAllocator& b, const void* ptr); // 0x00CE7740
    static void easyFree(hkMemoryAllocator& b, void* p); // 0x00CE7750
    ~hkMemoryRouter(); // 0x0081C6A0

public:
    hkLifoAllocator m_stack; // 0x00
    hkMemoryAllocator* m_temp; // 0x28
    hkMemoryAllocator* m_heap; // 0x2C
    hkMemoryAllocator* m_debug; // 0x30
    hkMemoryAllocator* m_solver; // 0x34
    void* m_userData; // 0x38
    unsigned int m_refObjLocalStore; // 0x3C
    static hkThreadLocalData<hkMemoryRouter*> s_memoryRouter; // 0x03A2615C
};
