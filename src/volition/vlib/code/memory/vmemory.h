// Original: volition/vlib/code/memory/vmemory.h
// Implementation: volition/vlib/code/memory/vmemory.cpp
//
// mempool: the standard vlib pool, a static_mempool_base that can also allocate downwards from the end of its buffer
// (alloc_from_end, end_pos), so one buffer serves two lifetimes. mempool_mark_scope restores a pool (and the GPU half of
// a split pool, flag 0x40) to the mark taken at construction.
// Thread scratch: memory_scratch_thread_init(pool) registers a per-thread scratch pool in Vlib_scratch_pool_list and in
// the thread-local Vlib_scratch_pool (decompiled; TLS slot +0x30).
// memory_status/memory_global_status: physical memory totals. Placement of memory_status inferred.
#pragma once

#include "volition/vlib/code/memory/static_mempool.h"

class dynamic_mempool;
class page_pool;

// size 0x50
class mempool : public static_mempool_base
{
public:
    virtual void init(const void* mem, unsigned int mem_size, const char* pool_name, unsigned int align, unsigned char mp_flags); // vtbl+0x50 0x0058A8B0 (:53)
    virtual void init_from_mempool(mempool_base* src_pool, unsigned int mem_size, const char* pool_name, unsigned int align, unsigned char mp_flags); // vtbl+0x54 0x0058A8E0 (:70)
    virtual unsigned int space_free() const; // vtbl+0x0C 0x0045B070 (.h:84)
    virtual unsigned int mark_at_end(); // vtbl+0x58 0x0058A940 (:86)
    virtual bool restore_to_mark_at_end(unsigned int pos); // vtbl+0x5C 0x005AC710 (:95)
    virtual bool can_alloc_from_end(unsigned int size, unsigned int align); // vtbl+0x60 0x0058A950 (:121)
    virtual void* alloc_from_end(unsigned int size, unsigned int align); // vtbl+0x64 0x005AC730 (:158)
    virtual bool clear(); // vtbl+0x28 0x0058A9C0 (:208)
    virtual bool clear_front(); // vtbl+0x68 0x005AC7B0 (:215)
    virtual bool clear_end(); // vtbl+0x6C 0x005AC7D0 (:228)
    virtual bool instantiable(); // vtbl+0x4C 0x0045B080 (.h:102)
    mempool(const void* mem, unsigned int mem_size, const char* pool_name, unsigned int align, unsigned char mp_flags); // 0x005AC6D0 (:35)
    mempool(const char* name); // 0x0058A830 (:22)
    mempool(); // 0x0058A7E0 (:17)
    virtual ~mempool(); // vtbl+0x40 0x0058A8A0 (:41)

protected:
    volatile unsigned int end_pos; // 0x4C
};

// size 0xC
class mempool_mark_scope
{
public:
    mempool_mark_scope(mempool_base* _pool); // 0x0058A9F0 (:239)
    ~mempool_mark_scope(); // 0x005BCAD0 (:249)

private:
    mempool_base* pool; // 0x00
    int mark; // 0x04
    int gpu_mark; // 0x08
};

// size 0x8
struct memory_status
{
    unsigned int total_physical; // 0x00
    unsigned int avail_physical; // 0x04
};

// A mempool with an inline buffer.
// size 0x50 + SIZE
template<unsigned int SIZE>
class fixed_mempool : public mempool
{
public:
    fixed_mempool(const char* pool_name, unsigned int align, unsigned char mp_flags); // <16384> 0x00750410 (.h:124)

private:
    unsigned char data[SIZE]; // 0x50
};

// <16384> 0x00750410 (.h:124)
template<unsigned int SIZE>
inline fixed_mempool<SIZE>::fixed_mempool(const char* pool_name, unsigned int align, unsigned char mp_flags) : mempool(data, SIZE, pool_name, align, mp_flags)
{
}

// Placement of the Vlib_* pool globals inferred (they are set up by vlib initialisation).
extern mempool_base* Vlib_scratch_pool_list[16]; // 0x01CD2908
extern int Vlib_scratch_pool_list_size; // 0x01CE86F8
extern dynamic_mempool Vlib_mempool; // 0x0163EA50
extern mempool_base* Vlib_temp_mempool; // 0x01CE9904
extern page_pool* Vlib_temp_page_pool; // 0x01CE9908

void memory_global_status(memory_status* status); // 0x0058AA50 (:269)
void memory_scratch_thread_init(mempool_base* thread_scratch_pool); // 0x0058AA70 (:337)

// Per-thread scratch pool (__declspec(thread)), set by memory_scratch_thread_init.
extern __declspec(thread) mempool_base* Vlib_scratch_pool;
