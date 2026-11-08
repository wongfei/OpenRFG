// Original: volition/vlib/code/memory/page_pool.cpp
//
// A budget of 4 KB pages shared by virtual_mempools. alloc_pages/release_pages only count (atomically) unless the pool
// keeps its memory allocated (keep_allocated), in which case the pages of a reserved block are committed and
// decommitted to mirror the count. The pool also hands out up to 128 virtual_mempool records (get_virtual_mempool).

#include "volition/vlib/code/memory/page_pool.h"

#include "keen/base/trace.hpp"

#include <string.h>
#include <windows.h>

// 0x005BC4B0 (:48)
page_pool::page_pool()
    : pp_cs(0), keep_allocated_cs(0), virtual_mempool_record_cs(0)
{
    size = 0;
    pages = 0;
    pages_free = 0;
    virtual_mempool_count = 0;
}

// 0x005BC590 (:52)
page_pool::page_pool(const char* _name, unsigned int _size, bool _keep_allocated)
    : pp_cs(0), keep_allocated_cs(0), virtual_mempool_record_cs(0)
{
    init(_name, _size, _keep_allocated);
}

// 0x005AB930 (:61)
bool page_pool::init(const char* _name, unsigned int _size, bool _keep_allocated)
{
    strncpy(name, _name, sizeof(name));
    name[sizeof(name) - 1] = '\0';
    size = (_size + 0xFFF) & ~0xFFFu;
    pages = size >> 12;
    pages_free = pages;
    virtual_mempool_count = 0;
    peak_usage_size = 0;
    keep_allocated = _keep_allocated;
    keep_allocated_memory = _keep_allocated ? (unsigned char*)VirtualAlloc(nullptr, pages << 12, MEM_COMMIT, PAGE_NOACCESS) : nullptr;

    cs_scope lock(virtual_mempool_record_cs);
    virtual_mempool_free_list = nullptr;
    virtual_mempool_used_list = nullptr;
    for (int i = 0; i < 128; ++i)
    {
        virtual_mempool* vm = &virtual_mempool_pool[i];
        if (virtual_mempool_free_list == nullptr)
        {
            vm->vm_next = vm;
            vm->vm_prev = vm;
            virtual_mempool_free_list = vm;
        }
        else
        {
            vm->vm_prev = virtual_mempool_free_list->vm_prev;
            vm->vm_next = virtual_mempool_free_list;
            virtual_mempool_free_list->vm_prev->vm_next = vm;
            virtual_mempool_free_list->vm_prev = vm;
        }
    }
    return true;
}

// 0x005BC670 (:120)
page_pool::~page_pool()
{
    destroy();
}

// 0x005ABA20 (:125)
bool page_pool::destroy()
{
    if (keep_allocated)
    {
        keep_allocated_cs.enter();
    }
    {
        cs_scope lock(virtual_mempool_record_cs);
        virtual_mempool* vm = virtual_mempool_used_list;
        while (vm != nullptr)
        {
            virtual_mempool* next = vm->vm_next;
            vm->destroy();
            vm = next != virtual_mempool_used_list ? next : nullptr;
        }
    }
    if (keep_allocated)
    {
        VirtualFree(keep_allocated_memory, pages_free << 12, MEM_DECOMMIT);
        keep_allocated_cs.leave();
    }
    return true;
}

// 0x005ABAA0 (:167)
bool page_pool::alloc_pages(unsigned int num_pages)
{
    if (keep_allocated)
    {
        keep_allocated_cs.enter();
    }
    const unsigned int old_free = (unsigned int)InterlockedExchangeAdd((volatile LONG*)&pages_free, -(LONG)num_pages);
    if (old_free < num_pages)
    {
        keen::debug::nativePrintArgs("Cannot allocate pages from page pool - out of pages.");
        InterlockedExchangeAdd((volatile LONG*)&pages_free, (LONG)num_pages);
        if (keep_allocated)
        {
            keep_allocated_cs.leave();
        }
        return false;
    }
    const unsigned int used = size - pages_free * 0x1000;
    peak_usage_size = used < peak_usage_size ? peak_usage_size : used;
    if (keep_allocated)
    {
        VirtualFree(keep_allocated_memory + pages_free * 0x1000, num_pages << 12, MEM_DECOMMIT);
        keep_allocated_cs.leave();
    }
    return true;
}

// 0x005ABB40 (:215)
bool page_pool::release_pages(unsigned int num_pages)
{
    if (keep_allocated)
    {
        keep_allocated_cs.enter();
    }
    const unsigned int old_free = (unsigned int)InterlockedExchangeAdd((volatile LONG*)&pages_free, (LONG)num_pages);
    if (old_free + num_pages > pages)
    {
        InterlockedExchangeAdd((volatile LONG*)&pages_free, -(LONG)num_pages);
        if (keep_allocated)
        {
            keep_allocated_cs.leave();
        }
        return false;
    }
    if (keep_allocated)
    {
        VirtualAlloc(keep_allocated_memory + old_free * 0x1000, num_pages << 12, MEM_COMMIT, PAGE_NOACCESS);
        keep_allocated_cs.leave();
    }
    return true;
}

// 0x005ABBC0 (:263)
virtual_mempool* page_pool::get_virtual_mempool(const char* namev, unsigned int max_size)
{
    cs_scope lock(virtual_mempool_record_cs);
    virtual_mempool* vm = virtual_mempool_free_list;
    if (vm == nullptr)
    {
        keen::debug::nativePrintArgs("Out of virtual mempool records! (arbitrary number, bump up MAX_DYNAMIC_VIRTUAL_MEMPOOLS)");
        return nullptr;
    }
    if (vm->vm_next == vm)
    {
        virtual_mempool_free_list = nullptr;
    }
    else
    {
        virtual_mempool_free_list = vm->vm_next;
        vm->vm_prev->vm_next = vm->vm_next;
        vm->vm_next->vm_prev = vm->vm_prev;
    }
    vm->vm_next = nullptr;
    vm->vm_prev = nullptr;
    if (virtual_mempool_used_list == nullptr)
    {
        vm->vm_next = vm;
        vm->vm_prev = vm;
        virtual_mempool_used_list = vm;
    }
    else
    {
        vm->vm_prev = virtual_mempool_used_list->vm_prev;
        vm->vm_next = virtual_mempool_used_list;
        virtual_mempool_used_list->vm_prev->vm_next = vm;
        virtual_mempool_used_list->vm_prev = vm;
    }
    // Inferred arguments: the call passes this pool, the name and the size (LTCG hides the rest).
    vm->init(this, namev, max_size, false, 0);
    return vm;
}

// 0x005ABCB0 (:290)
bool page_pool::release_virtual_mempool(virtual_mempool* mempool)
{
    cs_scope lock(virtual_mempool_record_cs);
    virtual_mempool* vm = virtual_mempool_used_list;
    if (vm != nullptr)
    {
        while (vm != mempool)
        {
            vm = vm->vm_next;
            if (vm == virtual_mempool_used_list || vm == nullptr)
            {
                vm = nullptr;
                break;
            }
        }
    }
    if (vm == nullptr)
    {
        keen::debug::nativePrintArgs("Can't release virtual mempool, can't find it in the used list.");
        return false;
    }
    vm->destroy();
    if (virtual_mempool_used_list == vm && vm->vm_next == vm)
    {
        virtual_mempool_used_list = nullptr;
    }
    else
    {
        if (virtual_mempool_used_list == vm)
        {
            virtual_mempool_used_list = vm->vm_next;
        }
        vm->vm_prev->vm_next = vm->vm_next;
        vm->vm_next->vm_prev = vm->vm_prev;
    }
    vm->vm_next = nullptr;
    vm->vm_prev = nullptr;
    if (virtual_mempool_free_list == nullptr)
    {
        vm->vm_next = vm;
        vm->vm_prev = vm;
        virtual_mempool_free_list = vm;
    }
    else
    {
        vm->vm_prev = virtual_mempool_free_list->vm_prev;
        vm->vm_next = virtual_mempool_free_list;
        virtual_mempool_free_list->vm_prev->vm_next = vm;
        virtual_mempool_free_list->vm_prev = vm;
    }
    return true;
}

// 0x00589970 (:321)
unsigned int page_pool::space_free() const
{
    return pages_free << 12;
}

unsigned int page_pool::space_used() const
{
    return size - space_free();
}

// 0x00563B20 (.h:64)
unsigned int page_pool::space_max() const
{
    return size;
}

// 0x00589980 (:327)
bool page_pool::link_to_mempool(mempool_base* pool)
{
    InterlockedIncrement((volatile LONG*)&virtual_mempool_count);
    return true;
}

// 0x005899A0 (:350)
bool page_pool::unlink_from_mempool(mempool_base* pool)
{
    if (virtual_mempool_count == 0)
    {
        keen::debug::nativePrintArgs("trying to unlink mempool when page pool doesn't think there are any linked mempools.");
        return false;
    }
    InterlockedDecrement((volatile LONG*)&virtual_mempool_count);
    return true;
}

const char* page_pool::get_name() const
{
    return name;
}
