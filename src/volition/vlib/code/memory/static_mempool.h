// Original: volition/vlib/code/memory/static_mempool.h
// Implementation: volition/vlib/code/memory/static_mempool.cpp
//
// Bump allocator over a fixed buffer (decompiled): alloc aligns current_pos (align 0 = def_align), fails with a log
// when the rest does not fit, records last_alloc (so realloc can grow the newest block in place) and the peak usage.
// Always owned by every thread (is_owned returns true). Concrete subclasses: mempool (vmemory.h), virtual_mempool,
// page_mempool, split_page_mempool, async_mempool.
#pragma once

#include "volition/vlib/code/memory/mempool_base.h"

// size 0x4C
class static_mempool_base : public mempool_base
{
protected:
    void* record_last_alloc(unsigned int offset, unsigned int size); // 0x0058A3E0 (:92)
public:
    virtual void init_static_mempool(const void* mem, unsigned int mem_size, const char* pool_name, unsigned int align, unsigned char mp_flags); // vtbl+0x44 0x0058A420 (:122)
    virtual void* get_base(); // vtbl+0x2C 0x0045B020 (.h:40)
    virtual bool is_dynamic() const; // vtbl+0x08 0x0045B030 (.h:43)
    virtual bool is_owned() const; // vtbl+0x04 0x0058A3C0 (:63)
    virtual void set_thread_ownership(); // vtbl+0x00 0x0058A3D0 (:80)
    virtual unsigned int space_free() const; // vtbl+0x0C 0x0058A340 (.h:52)
    virtual unsigned int space_used() const; // vtbl+0x10 0x0045B040 (.h:53)
    virtual unsigned int space_max() const; // vtbl+0x14 0x0045B060 (.h:54)
    virtual unsigned int mark(); // vtbl+0x30 0x0058A4A0 (:142)
    virtual bool restore_to_mark(unsigned int pos); // vtbl+0x34 0x005AC280 (:152)
    virtual bool can_alloc(unsigned int size, unsigned int align) const; // vtbl+0x18 0x0058A4B0 (:173)
    virtual void* alloc(unsigned int size, unsigned int align); // vtbl+0x1C 0x005AC2A0 (:206)
    virtual void* alloc_all(unsigned int align); // vtbl+0x48 0x0058A520 (:248)
    virtual void* realloc(void* mem, unsigned int new_size); // vtbl+0x20 0x005AC370 (:273)
    virtual bool contains_address(const void* mem) const; // vtbl+0x24 0x0058A560 (:316)
    virtual bool pad_to_page(unsigned int page_size); // vtbl+0x3C 0x0058A580 (:334)
    virtual bool release_bytes(unsigned int num_bytes); // vtbl+0x38 0x0058A5C0 (:356)
    virtual bool clear(); // vtbl+0x28 0x0058A5E0 (:377)
    virtual bool instantiable() = 0; // vtbl+0x4C
    unsigned int get_alignment() const;
    static_mempool_base(const void* mem, unsigned int mem_size, const char* pool_name, unsigned int align, unsigned char mp_flags); // 0x005AC200 (:42)
    static_mempool_base(const char* name); // 0x0058A350 (:26)
    static_mempool_base(); // 0x0058A2F0 (:18)
    virtual ~static_mempool_base(); // vtbl+0x40 0x0058A3B0 (:53)

private:
    unsigned char* last_alloc; // 0x38
protected:
    unsigned char* pool; // 0x3C
    unsigned int max_size; // 0x40
    unsigned int def_align; // 0x44
    volatile unsigned int current_pos; // 0x48
};
