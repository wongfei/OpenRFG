// Original: volition/vlib/code/memory/async_virtual_mempool.h
// Implementation: volition/vlib/code/memory/async_virtual_mempool.cpp
//
// A virtual_mempool whose alloc is serialised by a critical section, for allocation from several threads.
#pragma once

#include "volition/vlib/code/memory/virtual_mempool.h"
#include "volition/vlib/code/os/cs_wrapper.h"

// size 0x9C
class async_virtual_mempool : public virtual_mempool
{
public:
    async_virtual_mempool(page_pool* pp, char* name, unsigned int _size, bool clearing); // 0x005BC0A0 (:64)
    async_virtual_mempool(); // 0x005BC050 (:60)
    virtual ~async_virtual_mempool(); // vtbl+0x40 0x005BC100 (:69)
    virtual void destroy(); // vtbl+0x5C 0x005AAEA0 (:101)
    virtual void* alloc(unsigned int size, unsigned int align); // vtbl+0x1C 0x005BC1C0 (:74)
    virtual bool clear(); // vtbl+0x28 0x005AAE70 (:87)
    virtual bool instantiable(); // vtbl+0x4C 0x005BC090 (.h:40)

private:
    cs_wrapper alloc_cs; // 0x60
};
