// Original: volition/vlib/code/memory/async_mempool.cpp
//
// async_mempool (header comment in async_mempool.h), including the inline members of async_mempool.h.
//
// Notes (decompiled behaviour, kept as is):
// - alloc ignores the alignment and advances the position even when the pool is full, so a failed allocation leaves
//   current_pos past max_size until clear.

#include "volition/vlib/code/memory/async_mempool.h"

#include <intrin.h>

// 0x005BBED0 (:102)
// Always 1 byte aligned.
async_mempool::async_mempool(void* buf, unsigned int size, const char* _name, unsigned int _alignment, unsigned char _flags)
{
    init_static_mempool(buf, size, _name, 1, _flags);
    max_size = size;
    current_pos = 0;
}

// 0x005BC010 (.h:35)
async_mempool::~async_mempool()
{
    max_size = 0;
}

// 0x00589010 (:66)
// Any thread: an atomic bump of the position; nullptr past the end.
void* async_mempool::alloc(unsigned int size, unsigned int align)
{
    const unsigned int pos = (unsigned int)_InterlockedExchangeAdd((volatile long*)&current_pos, (long)size);
    if (max_size < current_pos)
    {
        return nullptr;
    }
    if (peak_usage < current_pos)
    {
        peak_usage = current_pos;
    }
    return pool + pos;
}

// 0x005BBF50 (.h:41)
void async_mempool::set_thread_ownership()
{
}

// 0x005BBF60 (.h:42)
unsigned int async_mempool::space_used() const
{
    return max_size - space_free();
}

// 0x005BBF80 (.h:43)
unsigned int async_mempool::space_free() const
{
    return max_size - current_pos;
}

// 0x005BBF90 (.h:44)
unsigned int async_mempool::space_max() const
{
    return max_size;
}

// 0x00589050 (:130)
bool async_mempool::contains_address(const void* addr) const
{
    return pool <= addr && addr < pool + max_size;
}

// 0x005BBFA0 (.h:47)
bool async_mempool::clear()
{
    current_pos = 0;
    return true;
}

// 0x005BBFB0 (.h:50)
bool async_mempool::can_alloc(unsigned int size, unsigned int align) const
{
    return false;
}

// 0x005BBFC0 (.h:51)
void* async_mempool::realloc(void* mem, unsigned int size)
{
    return nullptr;
}

// 0x005BBFD0 (.h:52)
bool async_mempool::restore_to_mark(unsigned int mark)
{
    return false;
}

// 0x005BBFE0 (.h:53)
bool async_mempool::release_bytes(unsigned int num_bytes)
{
    return false;
}

// 0x005BBFF0 (.h:54)
bool async_mempool::pad_to_page(unsigned int page_size)
{
    return false;
}

// 0x005AAE40 (:116)
void async_mempool::init(void* buf, unsigned int size, const char* _name, unsigned int _align, unsigned char _flags)
{
    init_static_mempool(buf, size, _name, 1, _flags);
    max_size = size;
    current_pos = 0;
}

// 0x005BC000 (.h:59)
bool async_mempool::instantiable()
{
    return true;
}
