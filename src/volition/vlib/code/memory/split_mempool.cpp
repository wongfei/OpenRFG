// Original: volition/vlib/code/memory/split_mempool.cpp
//
// A mempool (the CPU half) with an optional GPU mempool. The GPU functions do nothing (or fail) without one.
// restore_to_mark warns for a pool with a GPU half (flag 0x40) but restores the CPU half only.

#include "volition/vlib/code/memory/split_mempool.h"
#include "volition/vlib/code/vlib_core/vlib_assert.h"

#include "keen/base/trace.hpp"

// 0x005D0AD0 (.h:33)
split_mempool::split_mempool()
    : m_gpu_pool(nullptr)
{
}

// 0x005B5000 (.h:35)
split_mempool::split_mempool(const void* mem, unsigned int mem_size, const char* pool_name, unsigned int align, unsigned char mp_flags)
    : mempool(mem, mem_size, pool_name, align, mp_flags)
{
    // m_gpu_pool is left uninitialised (as in the original).
}

// 0x00597520
split_mempool::~split_mempool()
{
}

// 0x004A9BA0 (.h:42)
mempool* split_mempool::get_gpu_pool()
{
    return m_gpu_pool;
}

// 0x00589F40 (:16)
void split_mempool::attach_gpu_pool(mempool* gpu)
{
    flags |= 0x40;
    m_gpu_pool = gpu;
}

// 0x00589F50 (:23)
bool split_mempool::can_alloc_split(unsigned int cpu_size, unsigned int gpu_size, unsigned int cpu_align, unsigned int gpu_align)
{
    if (!can_alloc(cpu_size, cpu_align))
    {
        return false;
    }
    if (gpu_size == 0)
    {
        return true;
    }
    return m_gpu_pool != nullptr && m_gpu_pool->can_alloc(gpu_size, gpu_align);
}

// 0x005BC9E0 (:48)
void* split_mempool::alloc_cpu(unsigned int size, unsigned int align)
{
    // static_mempool_base::alloc inlined.
    return static_mempool_base::alloc(size, align);
}

// 0x00589FA0 (:53)
void* split_mempool::alloc_gpu(unsigned int size, unsigned int align)
{
    return m_gpu_pool->alloc(size, align);
}

// 0x005C8590 (:62)
bool split_mempool::clear()
{
    if (!clear_cpu())
    {
        return false;
    }
    return m_gpu_pool == nullptr || m_gpu_pool->clear();
}

// 0x005C4500 (:70)
bool split_mempool::clear_cpu()
{
    // mempool::clear without the virtual restore_to_mark (which would warn).
    if (!assert_permission())
    {
        return false;
    }
    current_pos = 0;
    return restore_to_mark_at_end(max_size);
}

// 0x00589FB0 (:75)
bool split_mempool::clear_gpu()
{
    if (m_gpu_pool == nullptr)
    {
        return true;
    }
    return m_gpu_pool->clear();
}

// 0x00589FD0 (:85)
void split_mempool::lock_cpu()
{
    lock();
}

// 0x00589FE0 (:90)
void split_mempool::lock_gpu()
{
    if (m_gpu_pool != nullptr)
    {
        m_gpu_pool->lock();
    }
}

// 0x0058A000 (:100)
void split_mempool::unlock_cpu()
{
    unlock();
}

// 0x0058A030 (:105)
void split_mempool::unlock_gpu()
{
    if (m_gpu_pool != nullptr)
    {
        m_gpu_pool->unlock();
    }
}

// 0x005AC180 (:117)
unsigned int split_mempool::mark_cpu()
{
    return current_pos;
}

// 0x0058A070 (:122)
unsigned int split_mempool::mark_gpu()
{
    if (m_gpu_pool == nullptr)
    {
        VLIB_CRITICAL_FAILURE();
        return 0;
    }
    return m_gpu_pool->mark();
}

// 0x005BC9F0 (:136)
bool split_mempool::restore_to_mark(unsigned int pos)
{
    if ((flags & 0x40) != 0)
    {
        keen::debug::nativePrintArgs("Split mempools should call restore_to_mark_cpu and restore_to_mark_gpu to ensure both memory types are restored.\n");
    }
    return restore_to_mark_cpu(pos);
}

// 0x005BCA30 (:145)
bool split_mempool::restore_to_mark_cpu(unsigned int pos)
{
    if (!assert_permission())
    {
        return false;
    }
    current_pos = pos;
    return true;
}

// 0x0058A0A0 (:150)
bool split_mempool::restore_to_mark_gpu(unsigned int pos)
{
    if (m_gpu_pool == nullptr)
    {
        return false;
    }
    return m_gpu_pool->restore_to_mark(pos);
}

// 0x005AC190 (:161)
void split_mempool::set_thread_ownership()
{
    // Only the GPU half (the CPU half's set_thread_ownership_cpu does nothing either).
    if (m_gpu_pool != nullptr)
    {
        m_gpu_pool->set_thread_ownership();
    }
}

// 0x005AC1A0 (:169)
void split_mempool::set_thread_ownership_cpu()
{
}

// 0x0058A0C0 (:174)
void split_mempool::set_thread_ownership_gpu()
{
    if (m_gpu_pool != nullptr)
    {
        m_gpu_pool->set_thread_ownership();
    }
}

// 0x0058A0D0 (:186)
unsigned int split_mempool::space_free_cpu() const
{
    return end_pos - current_pos;
}

// 0x0058A0E0 (:191)
unsigned int split_mempool::space_free_gpu() const
{
    return m_gpu_pool != nullptr ? m_gpu_pool->space_free() : 0;
}

// 0x0058A100 (:199)
unsigned int split_mempool::space_max_cpu() const
{
    return max_size;
}

// 0x0058A110 (:204)
unsigned int split_mempool::space_max_gpu() const
{
    return m_gpu_pool != nullptr ? m_gpu_pool->space_max() : 0;
}

// 0x0058A130 (:212)
unsigned int split_mempool::space_used_cpu() const
{
    return max_size - space_free();
}

// 0x0058A150 (:217)
unsigned int split_mempool::space_used_gpu() const
{
    return m_gpu_pool != nullptr ? m_gpu_pool->space_used() : 0;
}

// 0x005AC1B0 (:246)
bool split_mempool::release_bytes_cpu(unsigned int num_bytes)
{
    return restore_to_mark(current_pos - num_bytes);
}

// 0x0058A170 (:251)
bool split_mempool::release_bytes_gpu(unsigned int num_bytes)
{
    if (m_gpu_pool == nullptr)
    {
        return false;
    }
    return m_gpu_pool->release_bytes(num_bytes);
}
