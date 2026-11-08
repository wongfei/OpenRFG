// Original: volition/vlib/code/memory/async_mempool.h
// Implementation: volition/vlib/code/memory/async_mempool.cpp
//
// A static_mempool_base whose alloc may be called from any thread (no ownership check); most other operations are
// disabled (decompiled stubs return false or 0).
#pragma once

#include "volition/vlib/code/memory/static_mempool.h"

// size 0x4C
class async_mempool : public static_mempool_base
{
public:
    async_mempool(void* buf, unsigned int size, const char* _name, unsigned int _alignment, unsigned char _flags); // 0x005BBED0 (:102)
    async_mempool(); // declared, never emitted
    virtual ~async_mempool(); // vtbl+0x40 0x005BC010 (.h:35)
    virtual void* alloc(unsigned int size, unsigned int align); // vtbl+0x1C 0x00589010 (:66)
    virtual void set_thread_ownership(); // vtbl+0x00 0x005BBF50 (.h:41)
    virtual unsigned int space_used() const; // vtbl+0x10 0x005BBF60 (.h:42)
    virtual unsigned int space_free() const; // vtbl+0x0C 0x005BBF80 (.h:43)
    virtual unsigned int space_max() const; // vtbl+0x14 0x005BBF90 (.h:44)
    virtual bool contains_address(const void* addr) const; // vtbl+0x24 0x00589050 (:130)
    virtual bool clear(); // vtbl+0x28 0x005BBFA0 (.h:47)
    virtual bool can_alloc(unsigned int size, unsigned int align) const; // vtbl+0x18 0x005BBFB0 (.h:50)
    virtual void* realloc(void* mem, unsigned int size); // vtbl+0x20 0x005BBFC0 (.h:51)
    virtual bool restore_to_mark(unsigned int mark); // vtbl+0x34 0x005BBFD0 (.h:52)
    virtual bool release_bytes(unsigned int num_bytes); // vtbl+0x38 0x005BBFE0 (.h:53)
    virtual bool pad_to_page(unsigned int page_size); // vtbl+0x3C 0x005BBFF0 (.h:54)
    void init(void* buf, unsigned int size, const char* _name, unsigned int _align, unsigned char _flags); // 0x005AAE40 (:116)
    virtual bool instantiable(); // vtbl+0x4C 0x005BC000 (.h:59)

};
