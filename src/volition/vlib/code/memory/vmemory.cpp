// Original: volition/vlib/code/memory/vmemory.cpp
//
// mempool allocates upwards from the start of its buffer (static_mempool_base) and downwards from end_pos
// (alloc_from_end). mempool_mark_scope restores a pool to the mark taken at construction; for a split pool (flag 0x40)
// it also restores the GPU half.
//
// The Vlib_* pool globals are defined here.

#include "volition/vlib/code/memory/vmemory.h"
#include "volition/vlib/code/memory/dynamic_mempool.h"
#include "volition/vlib/code/memory/split_mempool.h"
#include "volition/vlib/code/vlib_core/vlib_assert.h"

#include "keen/base/trace.hpp"

dynamic_mempool Vlib_mempool("vlib_mempool"); // 0x0163EA50
mempool_base* Vlib_scratch_pool_list[16]; // 0x01CD2908
int Vlib_scratch_pool_list_size; // 0x01CE86F8
mempool_base* Vlib_temp_mempool; // 0x01CE9904
page_pool* Vlib_temp_page_pool; // 0x01CE9908
__declspec(thread) mempool_base* Vlib_scratch_pool; // thread local

// 0x0058A7E0 (:17)
mempool::mempool()
{
    end_pos = 0;
}

// 0x0058A830 (:22)
mempool::mempool(const char* name)
    : static_mempool_base(name)
{
    end_pos = 0;
}

// 0x005AC6D0 (:35)
mempool::mempool(const void* mem, unsigned int mem_size, const char* pool_name, unsigned int align, unsigned char mp_flags)
    : static_mempool_base(mem, mem_size, pool_name, align, mp_flags)
{
    end_pos = mem_size;
}

// 0x0058A8A0 (:41)
mempool::~mempool()
{
}

// 0x0058A8B0 (:53)
void mempool::init(const void* mem, unsigned int mem_size, const char* pool_name, unsigned int align, unsigned char mp_flags)
{
    static_mempool_base::init_static_mempool(mem, mem_size, pool_name, align, mp_flags);
    end_pos = mem_size;
}

// 0x0058A8E0 (:70)
void mempool::init_from_mempool(mempool_base* src_pool, unsigned int mem_size, const char* pool_name, unsigned int align, unsigned char mp_flags)
{
    void* mem = src_pool->alloc(mem_size, align);
    if (mem == nullptr)
    {
        VLIB_CRITICAL_FAILURE();
        return;
    }
    init(mem, mem_size, pool_name, align, mp_flags);
}

// 0x0045B070 (.h:84)
unsigned int mempool::space_free() const
{
    return end_pos - current_pos;
}

// 0x0058A940 (:86)
unsigned int mempool::mark_at_end()
{
    return end_pos;
}

// 0x005AC710 (:95)
bool mempool::restore_to_mark_at_end(unsigned int pos)
{
    if (!assert_permission())
    {
        return false;
    }
    end_pos = pos;
    return true;
}

// Bytes to move an end allocation of size down to the alignment (the original computes it this way).
static inline unsigned int get_end_alignment_padding(const unsigned char* pool, unsigned int end_pos, unsigned int size, unsigned int align)
{
    const unsigned int misalignment = (unsigned int)(pool + (end_pos - size)) % align;
    if (misalignment == 0)
    {
        return 0;
    }
    const unsigned int up = align - misalignment;
    return up != 0 ? align - up : 0;
}

// 0x0058A950 (:121)
bool mempool::can_alloc_from_end(unsigned int size, unsigned int align)
{
    if (is_locked() || !is_owned() || ((flags & 1) != 0 && current_pos != 0))
    {
        return false;
    }
    if (align == 0)
    {
        align = def_align;
    }
    const unsigned int padding = get_end_alignment_padding(pool, end_pos, size, align);
    return end_pos - padding - size >= current_pos;
}

// 0x005AC730 (:158)
void* mempool::alloc_from_end(unsigned int size, unsigned int align)
{
    if (!assert_permission())
    {
        return nullptr;
    }
    if ((flags & 1) != 0 && current_pos != 0)
    {
        keen::debug::nativePrintArgs("Cannot alloc twice from SINGLE_ALLOC mempool");
        return nullptr;
    }
    if (align == 0)
    {
        align = def_align;
    }
    const unsigned int padding = get_end_alignment_padding(pool, end_pos, size, align);
    unsigned char* p = pool + end_pos - size - padding;
    if ((unsigned char*)(size_t)current_pos > p)
    {
        // The original compares the pointer against the offset current_pos (kept as is).
        return nullptr;
    }
    end_pos = (unsigned int)(p - pool);
    return p;
}

// 0x0058A9C0 (:208)
bool mempool::clear()
{
    return restore_to_mark(0) && restore_to_mark_at_end(max_size);
}

// 0x005AC7B0 (:215)
bool mempool::clear_front()
{
    if (!assert_permission())
    {
        return false;
    }
    current_pos = 0;
    return true;
}

// 0x005AC7D0 (:228)
bool mempool::clear_end()
{
    if (!assert_permission())
    {
        return false;
    }
    end_pos = max_size;
    return true;
}

// 0x0045B080 (.h:102)
bool mempool::instantiable()
{
    return true;
}

// 0x0058A9F0 (:239)
mempool_mark_scope::mempool_mark_scope(mempool_base* _pool)
{
    pool = _pool;
    if (pool == nullptr)
    {
        return;
    }
    mark = pool->mark();
    if ((pool->get_flags() & 0x40) != 0)
    {
        mempool* gpu_pool = static_cast<split_mempool*>(pool)->get_gpu_pool();
        if (gpu_pool == nullptr)
        {
            VLIB_CRITICAL_FAILURE();
            gpu_mark = 0;
            return;
        }
        gpu_mark = gpu_pool->mark();
    }
}

// 0x005BCAD0 (:249)
mempool_mark_scope::~mempool_mark_scope()
{
    if (pool == nullptr)
    {
        return;
    }
    if ((pool->get_flags() & 0x40) == 0)
    {
        pool->restore_to_mark(mark);
        return;
    }
    split_mempool* split_pool = static_cast<split_mempool*>(pool);
    split_pool->restore_to_mark_cpu(mark);
    if (split_pool->get_gpu_pool() != nullptr)
    {
        split_pool->get_gpu_pool()->restore_to_mark(gpu_mark);
    }
}

// 0x0058AA50 (:269)
void memory_global_status(memory_status* status)
{
    status->total_physical = 0x40000000;
    status->avail_physical = 0x40000000;
}

// 0x0058AA70 (:337)
void memory_scratch_thread_init(mempool_base* thread_scratch_pool)
{
    Vlib_scratch_pool_list[Vlib_scratch_pool_list_size] = thread_scratch_pool;
    Vlib_scratch_pool = thread_scratch_pool;
    ++Vlib_scratch_pool_list_size;
}
