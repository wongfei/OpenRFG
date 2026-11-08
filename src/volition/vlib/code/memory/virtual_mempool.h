// Original: volition/vlib/code/memory/virtual_mempool.h
// Implementation: volition/vlib/code/memory/virtual_mempool.cpp
//
// Bump allocator over reserved address space (decompiled): pool..pool+max_size is reserved up front; alloc commits
// whole 4 KB pages (VirtualAlloc MEM_COMMIT) as current_pos grows past commit_size, charging them to the parent
// page_pool, and zero-fills them when clearing_alloc is set. restore_to_mark decommits again. Instances come from
// page_pool::get_virtual_mempool or are globals such as Common_mempool (rfg.h).
#pragma once

#include "volition/vlib/code/memory/static_mempool.h"

class page_pool;

// size 0x60
class virtual_mempool : public static_mempool_base
{
public:
    virtual void init(page_pool* pp, const char* pool_name, unsigned int upper_limit, bool clear_mem, unsigned int page_size); // vtbl+0x50 0x0058A670 (:105)
    virtual unsigned int space_free() const; // vtbl+0x0C 0x0058A650 (.h:44)
    virtual bool restore_to_mark(unsigned int pos); // vtbl+0x34 0x005AC4B0 (:238)
    virtual void* alloc(unsigned int size, unsigned int align); // vtbl+0x1C 0x005AC540 (:302)
    virtual void link_to_page_pool(page_pool* pp); // vtbl+0x54 0x0058A790 (:217)
    virtual void unlink_from_page_pool(); // vtbl+0x58 0x0058A7B0 (:224)
    virtual void destroy(); // vtbl+0x5C 0x0058A740 (:192)
    virtual bool instantiable(); // vtbl+0x4C 0x0058A660 (.h:59)
    virtual_mempool(page_pool* pp, const char* pool_name, unsigned int upper_limit, bool clear_mem, unsigned int page_size); // 0x005AC3C0 (:71)
    virtual_mempool(); // 0x0058A5F0 (:54)
    virtual ~virtual_mempool(); // vtbl+0x40 0x005AC450 (:92)

private:
    friend class page_pool; // page_pool keeps its records in the vm_next/vm_prev lists (inferred)

    unsigned int commit_size; // 0x4C
    bool clearing_alloc; // 0x50
    page_pool* parent_pp; // 0x54
    virtual_mempool* vm_next; // 0x58
    virtual_mempool* vm_prev; // 0x5C
};
