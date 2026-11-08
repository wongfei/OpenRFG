// Original: volition/vlib/code/memory/dynamic_mempool.h
// Implementation: volition/vlib/code/memory/dynamic_mempool.cpp
//
// General heap pool over a private Win32 heap: alloc rounds the size up to page_size and calls HeapAlloc; free and
// realloc are supported, mark/restore are not (decompiled). Vlib_mempool (vmemory.h) is one.
#pragma once

#include "volition/vlib/code/memory/mempool_base.h"

// size 0x44
class dynamic_mempool : public mempool_base
{
public:
    virtual bool is_dynamic() const; // vtbl+0x08 0x005AAF70 (.h:31)
    virtual bool is_owned() const; // vtbl+0x04 0x005AAF80 (.h:34)
    virtual void set_thread_ownership(); // vtbl+0x00 0x00563B60 (.h:37)
    virtual unsigned int space_free() const; // vtbl+0x0C 0x005890C0 (:66)
    virtual unsigned int space_used() const; // vtbl+0x10 0x00589100 (:78)
    virtual unsigned int space_max() const; // vtbl+0x14 0x00589110 (:95)
    virtual bool can_alloc(unsigned int size, unsigned int align) const; // vtbl+0x18 0x005AB0D0 (:112)
    virtual void* alloc(unsigned int size, unsigned int align); // vtbl+0x1C 0x005BC250 (:141)
    virtual void* realloc(void* mem, unsigned int new_size); // vtbl+0x20 0x005BC2E0 (:181)
    virtual bool contains_address(const void* mem) const; // vtbl+0x24 0x00589140 (:203)
    virtual bool free(void* mem); // vtbl+0x44 0x005BC330 (:214)
    virtual bool clear(); // vtbl+0x28 0x005BC380 (:229)
    virtual void* get_base(); // vtbl+0x2C 0x005AAF90 (.h:55)
    virtual unsigned int mark(); // vtbl+0x30 0x005AAFA0 (.h:56)
    virtual bool restore_to_mark(unsigned int pos); // vtbl+0x34 0x005AAFB0 (.h:57)
    virtual bool release_bytes(unsigned int num_bytes); // vtbl+0x38 0x005AAFC0 (.h:58)
    virtual bool pad_to_page(unsigned int page_size); // vtbl+0x3C 0x005AAFD0 (.h:59)
    virtual bool instantiable(); // vtbl+0x48 0x005AAFE0 (.h:62)
    void init(); // 0x00589070 (:13)
    dynamic_mempool(const char* name); // 0x005AB000 (:50)
    dynamic_mempool(); // 0x005AAEF0 (:42)
    virtual ~dynamic_mempool(); // vtbl+0x40 0x005AB090 (:57)

private:
    unsigned int page_size; // 0x38
    void* heap; // 0x3C
    unsigned int total_alloced; // 0x40
};
