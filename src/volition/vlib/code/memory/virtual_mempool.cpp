// Original: volition/vlib/code/memory/virtual_mempool.cpp
//
// A bump allocator over reserved address space: init reserves upper_limit bytes (rounded up to page_size), alloc
// commits whole 4 KB pages as current_pos grows and charges them to the parent page_pool, restore_to_mark decommits
// the pages above the mark and returns them. With clear_mem, newly committed memory and memory released by
// restore_to_mark is zero-filled.

#include "volition/vlib/code/memory/virtual_mempool.h"
#include "volition/vlib/code/memory/page_pool.h"

#include "keen/base/trace.hpp"

#include <string.h>
#include <windows.h>

// 0x0058A5F0 (:54)
virtual_mempool::virtual_mempool()
{
    commit_size = 0;
    clearing_alloc = false;
    parent_pp = nullptr;
    vm_next = nullptr;
    vm_prev = nullptr;
}

// 0x005AC3C0 (:71)
virtual_mempool::virtual_mempool(page_pool* pp, const char* pool_name, unsigned int upper_limit, bool clear_mem, unsigned int page_size)
{
    parent_pp = nullptr;
    if (pool_name == nullptr)
    {
        pool_name = "Untitled";
    }
    if (page_size == 0)
    {
        page_size = 0x1000;
    }
    init(pp, pool_name, upper_limit, clear_mem, page_size);
}

// 0x005AC450 (:92)
virtual_mempool::~virtual_mempool()
{
    if (pool != nullptr)
    {
        static_mempool_base::clear();
        if (!VirtualFree(pool, 0, MEM_RELEASE))
        {
            keen::debug::nativePrintArgs("Unable to deallocate address space.");
        }
        pool = nullptr;
    }
    if (parent_pp != nullptr)
    {
        unlink_from_page_pool();
    }
}

// 0x0058A670 (:105)
void virtual_mempool::init(page_pool* pp, const char* pool_name, unsigned int upper_limit, bool clear_mem, unsigned int page_size)
{
    if (page_size == 0)
    {
        page_size = 0x1000;
    }
    destroy();
    if (pp == nullptr)
    {
        keen::debug::nativePrintArgs("virtual mempool has no page pool");
    }
    else
    {
        link_to_page_pool(pp);
    }
    if (upper_limit == 0)
    {
        upper_limit = parent_pp->size;
    }
    if (pool_name == nullptr)
    {
        pool_name = "anonymous init call";
    }
    const unsigned int padding = (upper_limit % page_size) == 0 ? 0 : page_size - upper_limit % page_size;
    if (parent_pp != nullptr && parent_pp->size < padding + upper_limit)
    {
        keen::debug::nativePrintArgs("oops, not enough VIRTUAL space for this virtual mempool, how the hell did you manage this?");
        unlink_from_page_pool();
        return;
    }
    void* mem = VirtualAlloc(nullptr, padding + upper_limit, MEM_RESERVE, PAGE_READWRITE);
    if (mem != nullptr)
    {
        clearing_alloc = clear_mem;
        commit_size = 0;
        static_mempool_base::init_static_mempool(mem, upper_limit, pool_name, 0x10, 0x10);
    }
}

// 0x0058A740 (:192)
void virtual_mempool::destroy()
{
    if (pool != nullptr)
    {
        clear();
        if (!VirtualFree(pool, 0, MEM_RELEASE))
        {
            keen::debug::nativePrintArgs("Unable to deallocate address space.");
        }
        pool = nullptr;
    }
    if (parent_pp != nullptr)
    {
        unlink_from_page_pool();
    }
}

// 0x0058A790 (:217)
void virtual_mempool::link_to_page_pool(page_pool* pp)
{
    InterlockedIncrement((volatile LONG*)&pp->virtual_mempool_count);
    parent_pp = pp;
}

// 0x0058A7B0 (:224)
void virtual_mempool::unlink_from_page_pool()
{
    if (parent_pp->virtual_mempool_count == 0)
    {
        keen::debug::nativePrintArgs("trying to unlink mempool when page pool doesn't think there are any linked mempools.");
        return;
    }
    InterlockedDecrement((volatile LONG*)&parent_pp->virtual_mempool_count);
    parent_pp = nullptr;
}

// 0x005AC4B0 (:238)
bool virtual_mempool::restore_to_mark(unsigned int pos)
{
    if (!assert_permission())
    {
        return false;
    }
    current_pos = pos;
    const unsigned int commit_end = (current_pos + 0xFFF) & ~0xFFFu;
    if (commit_end < commit_size)
    {
        VirtualFree(pool + commit_end, commit_size - commit_end, MEM_DECOMMIT);
        if (parent_pp != nullptr)
        {
            parent_pp->release_pages((commit_size - commit_end) >> 12);
        }
        commit_size = commit_end;
    }
    if (clearing_alloc)
    {
        memset(pool + current_pos, 0, commit_size - current_pos);
    }
    return true;
}

// 0x005AC540 (:302)
void* virtual_mempool::alloc(unsigned int size, unsigned int align)
{
    if (!assert_permission())
    {
        return nullptr;
    }
    if ((flags & 1) != 0 && current_pos != 0)
    {
        keen::debug::nativePrintArgs("Cannot alloc twice from SINGLE_ALLOC mempool");
        return nullptr;
    }
    if (align == 0)
    {
        align = def_align;
    }
    const unsigned int misalignment = (unsigned int)(pool + current_pos) % align;
    const unsigned int padding = misalignment == 0 ? 0 : align - misalignment;
    unsigned int end = current_pos + padding + size;
    if (end > max_size)
    {
        return nullptr;
    }
    if (end > commit_size)
    {
        // Commit whole pages up to the new end.
        end = (end + 0xFFF) & ~0xFFFu;
        if (parent_pp != nullptr && !parent_pp->alloc_pages((end - commit_size) >> 12))
        {
            keen::debug::nativePrintArgs("No page pool pages left for virtual_mempool allocation.");
            return nullptr;
        }
        if (VirtualAlloc(pool + commit_size, end - commit_size, MEM_COMMIT, PAGE_READWRITE) == nullptr)
        {
            keen::debug::nativePrintArgs("Mempool %s is out of memory allocating %d bytes, size so far is %d.\n", name, size, commit_size);
            if (parent_pp != nullptr)
            {
                parent_pp->release_pages((end - commit_size) >> 12);
            }
            return nullptr;
        }
        if (clearing_alloc)
        {
            memset(pool + commit_size, 0, end - commit_size);
        }
        commit_size = end;
    }
    if (peak_usage < space_used())
    {
        peak_usage = space_used();
    }
    return record_last_alloc(padding, size);
}

// 0x0058A650 (.h:44)
unsigned int virtual_mempool::space_free() const
{
    return max_size - current_pos;
}

// 0x0058A660 (.h:59)
bool virtual_mempool::instantiable()
{
    return true;
}
