// Original: volition/vlib/code/memory/rfg_split_mempool.cpp
//
// split_mempool owning its GPU half (gpu_mempool, named "<name> gpu"). On PC there is no separate GPU memory:
// init_from_mempools fails when a GPU source pool is given (it still allocates from it).

#include "volition/vlib/code/memory/rfg_split_mempool.h"
#include "volition/vlib/code/vlib_core/vlib_assert.h"

#include <string.h>

// 0x005ABDB0 (:24)
void append_gpu_name(char* name, unsigned int name_size)
{
    size_t length = strlen(name);
    if (length >= name_size - 6)
    {
        length = name_size - 6;
    }
    strncpy(name + length, " gpu", 5);
}

// Pool name with " gpu" appended, cut to 31 characters (inlined in the constructor and init_rfg_split_mempool).
static void make_gpu_pool_name(char (&gpu_name)[32], const char* pool_name)
{
    strncpy(gpu_name, pool_name, sizeof(gpu_name));
    gpu_name[sizeof(gpu_name) - 1] = '\0';
    size_t length = strlen(gpu_name);
    if (length > 0x19)
    {
        length = 0x1A;
    }
    strncpy(gpu_name + length, " gpu", 5);
}

// 0x005D0AF0 (.h:36)
rfg_split_mempool::rfg_split_mempool()
{
}

// 0x005BC720 (:36)
rfg_split_mempool::rfg_split_mempool(const char* pool_name, const void* cpu_mem, unsigned int cpu_mem_size, unsigned int cpu_align, const void* gpu_mem, unsigned int gpu_mem_size, unsigned int gpu_align, unsigned char mp_flags)
    : split_mempool(cpu_mem, cpu_mem_size, pool_name, cpu_align, mp_flags)
{
    char gpu_name[32];
    make_gpu_pool_name(gpu_name, pool_name);
    if (gpu_mem != nullptr)
    {
        gpu_mempool.init(gpu_mem, gpu_mem_size, gpu_name, gpu_align, mp_flags);
        flags |= 0x40;
        m_gpu_pool = &gpu_mempool;
    }
}

// 0x005BC850
rfg_split_mempool::~rfg_split_mempool()
{
}

// 0x005ABDF0 (:54)
void rfg_split_mempool::init_rfg_split_mempool(const char* pool_name, const void* cpu_mem, unsigned int cpu_mem_size, unsigned int cpu_align, const void* gpu_mem, unsigned int gpu_mem_size, unsigned int gpu_align, unsigned char mp_flags)
{
    char gpu_name[32];
    make_gpu_pool_name(gpu_name, pool_name);
    static_mempool_base::init_static_mempool(cpu_mem, cpu_mem_size, pool_name, cpu_align, mp_flags);
    end_pos = cpu_mem_size;
    if (gpu_mem != nullptr)
    {
        gpu_mempool.init(gpu_mem, gpu_mem_size, gpu_name, gpu_align, mp_flags);
        flags |= 0x40;
        m_gpu_pool = &gpu_mempool;
    }
}

// 0x005899D0 (:74)
void rfg_split_mempool::init_from_mempools(const char* namev, mempool_base* cpu_src_pool, unsigned int cpu_mem_size, unsigned int cpu_align, mempool_base* gpu_src_pool, unsigned int gpu_mem_size, unsigned int gpu_align, unsigned char mp_flags)
{
    void* cpu_mem = cpu_src_pool->alloc(cpu_mem_size, cpu_align);
    if (cpu_mem == nullptr)
    {
        VLIB_CRITICAL_FAILURE();
        return;
    }
    if (gpu_src_pool != nullptr)
    {
        gpu_src_pool->alloc(gpu_mem_size, gpu_align);
        VLIB_CRITICAL_FAILURE();
        return;
    }
    init_rfg_split_mempool(namev, cpu_mem, cpu_mem_size, cpu_align, nullptr, gpu_mem_size, gpu_align, mp_flags);
}
