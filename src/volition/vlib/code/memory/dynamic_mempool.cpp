// Original: volition/vlib/code/memory/dynamic_mempool.cpp
//
// A pool over its own Win32 heap (decompiled): alloc rounds the size up to page_size (8) and calls HeapAlloc, free and
// realloc go to the heap, clear destroys and recreates it. Mark/restore are not supported. The free and maximum space
// are the machine's physical plus virtual memory (low 32 bits of each).
//
// Notes (decompiled behaviour, kept as is):
// - space_max does not set the MEMORYSTATUSEX length before GlobalMemoryStatusEx (so the call fails and the result is
//   whatever the stack held). Here the length is set so the result is defined.
// - realloc and free do not update total_alloced.

#include "volition/vlib/code/memory/dynamic_mempool.h"

#include "keen/base/trace.hpp"

#include <windows.h>

static unsigned int page_counter; // counts HeapAlloc calls (name from the PDB)
static ULONG Heap_compatibility_lfh = 2; // ::x, HeapCompatibilityInformation 2 = the low fragmentation heap

// The heap and flags, shared by the constructors and init.
static void* create_heap()
{
    SYSTEM_INFO system_info;
    GetSystemInfo(&system_info);
    HANDLE heap = HeapCreate(0, 0, 0);
    HeapSetInformation(heap, HeapCompatibilityInformation, &Heap_compatibility_lfh, 4);
    return heap;
}

// 0x00589070 (:13)
void dynamic_mempool::init()
{
    heap = create_heap();
    flags |= 2; // dynamic
    page_size = 8;
    total_alloced = 0;
}

// 0x005AAEF0 (:42)
dynamic_mempool::dynamic_mempool()
    : mempool_base("dynamic", 0)
{
    heap = create_heap();
    flags |= 2;
    total_alloced = 0;
    page_size = 8;
}

// 0x005AB000 (:50)
dynamic_mempool::dynamic_mempool(const char* name)
    : mempool_base(name != nullptr ? name : "Untitled", 0)
{
    heap = create_heap();
    flags |= 2;
    total_alloced = 0;
    page_size = 8;
}

// 0x005AB090 (:57)
dynamic_mempool::~dynamic_mempool()
{
    if (!HeapDestroy(heap))
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
    }
}

// 0x005AAF70 (.h:31)
bool dynamic_mempool::is_dynamic() const
{
    return true;
}

// 0x005AAF80 (.h:34)
bool dynamic_mempool::is_owned() const
{
    return true;
}

// 0x00563B60 (.h:37)
void dynamic_mempool::set_thread_ownership()
{
}

// 0x005890C0 (:66)
unsigned int dynamic_mempool::space_free() const
{
    MEMORYSTATUSEX status;
    status.dwLength = sizeof(status);
    GlobalMemoryStatusEx(&status);
    return (unsigned int)status.ullAvailPhys + (unsigned int)status.ullAvailVirtual;
}

// 0x00589100 (:78)
unsigned int dynamic_mempool::space_used() const
{
    return total_alloced;
}

// 0x00589110 (:95)
unsigned int dynamic_mempool::space_max() const
{
    MEMORYSTATUSEX status;
    status.dwLength = sizeof(status); // not set in the original (see the file comment)
    GlobalMemoryStatusEx(&status);
    return (unsigned int)status.ullTotalPhys + (unsigned int)status.ullTotalVirtual;
}

// 0x005AB0D0 (:112)
bool dynamic_mempool::can_alloc(unsigned int size, unsigned int align) const
{
    return !is_locked() && is_owned();
}

// 0x005BC250 (:141)
void* dynamic_mempool::alloc(unsigned int size, unsigned int align)
{
    if (!assert_permission())
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        return nullptr;
    }
    const unsigned int rest = (page_size - 1) & size;
    if (rest != 0)
    {
        size += page_size - rest;
    }
    void* mem = HeapAlloc(heap, 0, size);
    page_counter++;
    total_alloced += size;
    if (mem == nullptr)
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
    }
    const unsigned int used = space_used();
    if (peak_usage < used)
    {
        peak_usage = used;
    }
    return mem;
}

// 0x005BC2E0 (:181)
void* dynamic_mempool::realloc(void* mem, unsigned int new_size)
{
    if (!assert_permission())
    {
        return nullptr;
    }
    void* new_mem = HeapReAlloc(heap, 0, mem, new_size);
    if (new_mem == nullptr)
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
    }
    return new_mem;
}

// 0x00589140 (:203)
bool dynamic_mempool::contains_address(const void* mem) const
{
    return true;
}

// 0x005BC330 (:214)
bool dynamic_mempool::free(void* mem)
{
    if (!assert_permission())
    {
        return false;
    }
    if (!HeapFree(heap, 0, mem))
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
    }
    return true;
}

// 0x005BC380 (:229)
bool dynamic_mempool::clear()
{
    if (!assert_permission())
    {
        return false;
    }
    if (!HeapDestroy(heap))
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
    }
    heap = HeapCreate(0, 0, 0);
    total_alloced = 0;
    return true;
}

// 0x005AAF90 (.h:55)
void* dynamic_mempool::get_base()
{
    return nullptr;
}

// 0x005AAFA0 (.h:56)
unsigned int dynamic_mempool::mark()
{
    return 0;
}

// 0x005AAFB0 (.h:57)
bool dynamic_mempool::restore_to_mark(unsigned int pos)
{
    return false;
}

// 0x005AAFC0 (.h:58)
bool dynamic_mempool::release_bytes(unsigned int num_bytes)
{
    return false;
}

// 0x005AAFD0 (.h:59)
#pragma warning(suppress : 4458) // the parameter's name is from rfg.pdb
bool dynamic_mempool::pad_to_page(unsigned int page_size)
{
    return false;
}

// 0x005AAFE0 (.h:62)
bool dynamic_mempool::instantiable()
{
    return true;
}
