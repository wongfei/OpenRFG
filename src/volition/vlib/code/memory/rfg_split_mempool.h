// Original: volition/vlib/code/memory/rfg_split_mempool.h
// Implementation: volition/vlib/code/memory/rfg_split_mempool.cpp
//
// split_mempool that owns its GPU half (gpu_mempool, named "<name> gpu"). init_rfg_split_mempool sets flag 0x40 and
// m_gpu_pool when GPU memory is given (decompiled). Used for the level pools of rfg.cpp (Permanent_mempool, ...).
#pragma once

#include "volition/vlib/code/memory/split_mempool.h"

// size 0xA4
class rfg_split_mempool : public split_mempool
{
public:
    rfg_split_mempool(const char* pool_name, const void* cpu_mem, unsigned int cpu_mem_size, unsigned int cpu_align, const void* gpu_mem, unsigned int gpu_mem_size, unsigned int gpu_align, unsigned char mp_flags); // 0x005BC720 (:36)
    rfg_split_mempool(); // 0x005D0AF0 (.h:36)
    virtual void init_rfg_split_mempool(const char* pool_name, const void* cpu_mem, unsigned int cpu_mem_size, unsigned int cpu_align, const void* gpu_mem, unsigned int gpu_mem_size, unsigned int gpu_align, unsigned char mp_flags); // vtbl+0x70 0x005ABDF0 (:54)
    virtual void init_from_mempools(const char* namev, mempool_base* cpu_src_pool, unsigned int cpu_mem_size, unsigned int cpu_align, mempool_base* gpu_src_pool, unsigned int gpu_mem_size, unsigned int gpu_align, unsigned char mp_flags); // vtbl+0x74 0x005899D0 (:74)
    virtual ~rfg_split_mempool(); // vtbl+0x40 0x005BC850

protected:
    mempool gpu_mempool; // 0x54
};

void append_gpu_name(char* name, unsigned int name_size); // 0x005ABDB0 (:24)
