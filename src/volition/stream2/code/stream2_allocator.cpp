// Original: volition/stream2/code/stream2_allocator.cpp
//
// Notes (decompiled behaviour, kept as is):
// - stream2_dynamic_allocator uses cpu_memmgr for both halves (gpu_memmgr is never used); its max_alloc reports the
//   CPU manager's largest block for the GPU side too. alloc fails without touching cpu_alloc when the request is
//   larger than the free space or the largest block, and sets frag_fail on every failure.
// - stream2_mempool_allocator and stream2_stack_allocator place the GPU part right after the CPU part (aligned to
//   gpu_align, the larger alignment for the block) when the pool has no separate GPU pool; free does nothing for
//   the mempool allocator.
// - stream2_stack_allocator::alloc(bytes) records the mark in cpu_mark and the data in cpu_data (gpu_mark and
//   gpu_data stay 0). free(data1, data2) on a single pool calls free(data1).
// - stream2_slot_allocator ignores the GPU sizes of bracket_info (gpu_alloc_cb is only checked for null): each slot
//   is one mempool of cpu_align + gpu_align + cpu_size bytes from the cpu_alloc_cb block, and alloc hands out the
//   whole slot (alloc_all) with the GPU part placed after the CPU part. Only the slot's cpu_pool is constructed. The
//   single-pointer can_alloc/max_alloc/alloc/free and the 5-argument init are not supported (log only).
// - stream2_system_allocator always allocates the GPU part, even when gpu_bytes is 0.

#include "volition/stream2/code/stream2_allocator.h"

#include "volition/vlib/code/memory/split_memmgr.h"
#include "volition/vlib/code/memory/split_mempool.h"
#include "volition/vlib/code/vlib_core/vlib_assert.h"

#include <malloc.h>
#include <new>
#include <stdio.h>

stream2_allocator* _Stream_allocators[255]; // 0x01AC3740

// Size of the CPU part plus padding to gpu_align plus the GPU part (one block for both).
static int combined_size(int cpu_bytes, int gpu_bytes, int gpu_align)
{
    if (gpu_bytes <= 0)
    {
        return cpu_bytes;
    }
    const int rest = cpu_bytes % gpu_align;
    return cpu_bytes + (rest == 0 ? 0 : gpu_align - rest) + gpu_bytes;
}

// GPU part of a combined block: after the CPU part, aligned to gpu_align.
static unsigned char* combined_gpu_part(unsigned char* cpu_data, int cpu_bytes, int gpu_align)
{
    const int rest = cpu_bytes % gpu_align;
    return cpu_data + cpu_bytes + (rest == 0 ? 0 : gpu_align - rest);
}

// 0x004A9FE0
stream2_allocator::stream2_allocator()
{
}

// 0x004A9B30 (.h:18)
stream2_allocator::~stream2_allocator()
{
}

// 0x004A9B60 (.h:52)
memory_mark::memory_mark()
{
    cpu_mark = 0;
    gpu_mark = 0;
    cpu_data = nullptr;
    gpu_data = nullptr;
    released = false;
}

// (inline, no address)
memory_mark::memory_mark(unsigned int _cpu_mark, unsigned int _gpu_mark, unsigned char* _cpu_data, unsigned char* _gpu_data)
{
    cpu_mark = _cpu_mark;
    gpu_mark = _gpu_mark;
    cpu_data = _cpu_data;
    gpu_data = _gpu_data;
    released = false;
}

// ---- stream2_dynamic_allocator

// 0x004AC550 (:24)
stream2_dynamic_allocator::stream2_dynamic_allocator()
{
    gpu_allocated = 0;
    cpu_memmgr = nullptr;
    gpu_memmgr = nullptr;
    frag_fail = false;
}

// 0x004A9FF0 (:32)
void stream2_dynamic_allocator::init(split_memmgr* _cpu_memmgr, split_memmgr* _gpu_memmgr)
{
    cpu_memmgr = _cpu_memmgr;
    gpu_memmgr = _gpu_memmgr;
    frag_fail = false;
}

// 0x004AC570 (.h:96)
stream2_dynamic_allocator::~stream2_dynamic_allocator()
{
}

// 0x00919C40 (.h:98)
bool stream2_dynamic_allocator::get_frag_fail()
{
    return frag_fail;
}

// 0x004A9B80 (.h:99)
void stream2_dynamic_allocator::set_frag_fail(bool f)
{
    frag_fail = f;
}

// 0x004AC5A0 (:39)
bool stream2_dynamic_allocator::can_alloc(int cpu_bytes, int cpu_align, int gpu_bytes, int gpu_align)
{
    const int bytes = combined_size(cpu_bytes, gpu_bytes, gpu_align);
    return cpu_memmgr->can_alloc((unsigned int)bytes, (unsigned int)(gpu_align < cpu_align ? cpu_align : gpu_align));
}

// 0x004AA010 (:53)
bool stream2_dynamic_allocator::can_alloc(int bytes, int align)
{
    return cpu_memmgr->can_alloc((unsigned int)bytes, (unsigned int)align);
}

// 0x004AA020 (:58)
void stream2_dynamic_allocator::max_alloc(int& cpu_max, int& gpu_max)
{
    cpu_max = (int)cpu_memmgr->largest_allocation_possible();
    gpu_max = (int)cpu_memmgr->largest_allocation_possible();
}

// 0x004AA050 (:69)
int stream2_dynamic_allocator::max_alloc()
{
    return (int)cpu_memmgr->largest_allocation_possible();
}

// 0x004AA060 (:74)
void stream2_dynamic_allocator::alloc(int cpu_bytes, int cpu_align, unsigned char*& cpu_alloc, int gpu_bytes, int gpu_align, unsigned char*& gpu_alloc, const char* debug_name)
{
    if (cpu_bytes > (int)cpu_memmgr->space_free() || cpu_bytes > (int)cpu_memmgr->largest_allocation_possible())
    {
        frag_fail = true;
        return;
    }
    cpu_alloc = (unsigned char*)cpu_memmgr->alloc((unsigned int)cpu_bytes, debug_name, (unsigned int)cpu_align);
    if (cpu_alloc == nullptr)
    {
        frag_fail = true;
        return;
    }
    if (gpu_bytes <= 0)
    {
        gpu_alloc = nullptr;
        return;
    }
    if (gpu_bytes <= (int)cpu_memmgr->space_free() && gpu_bytes <= (int)cpu_memmgr->largest_allocation_possible())
    {
        gpu_alloc = (unsigned char*)cpu_memmgr->alloc((unsigned int)gpu_bytes, debug_name, (unsigned int)gpu_align);
        if (gpu_alloc != nullptr)
        {
            gpu_allocated += (unsigned int)gpu_bytes;
            return;
        }
    }
    frag_fail = true;
}

// 0x004AA110 (:168)
unsigned char* stream2_dynamic_allocator::alloc(int bytes, int align, const char* debug_name)
{
    return (unsigned char*)cpu_memmgr->alloc((unsigned int)bytes, debug_name, (unsigned int)align);
}

// 0x004AA160 (:185)
void stream2_dynamic_allocator::free(unsigned char* data)
{
    cpu_memmgr->free(data);
}

// 0x004AA130 (:173)
void stream2_dynamic_allocator::free(unsigned char* cpu_data, unsigned char* gpu_data)
{
    cpu_memmgr->free(cpu_data);
    if (gpu_data != nullptr)
    {
        cpu_memmgr->free(gpu_data);
    }
}

// 0x004AA190 (:204)
void stream2_dynamic_allocator::set_thread_ownership()
{
}

// 0x004AA170 (:190)
bool stream2_dynamic_allocator::using_split_mempools()
{
    return false;
}

// 0x004AA180 (:199)
unsigned int stream2_dynamic_allocator::get_gpu_allocated()
{
    return gpu_allocated;
}

// ---- stream2_mempool_allocator

// 0x005D4770
stream2_mempool_allocator::stream2_mempool_allocator()
{
}

// 0x005D0A90 (.h:119)
stream2_mempool_allocator::~stream2_mempool_allocator()
{
}

// 0x004AA1A0 (:215)
void stream2_mempool_allocator::init(mempool_base* _pool)
{
    cpu_pool = _pool;
    gpu_pool = (_pool->get_flags() & 0x40) != 0 ? ((split_mempool*)_pool)->get_gpu_pool() : _pool;
}

// 0x004AA1C0 (:226)
bool stream2_mempool_allocator::can_alloc(int bytes, int align)
{
    return cpu_pool->can_alloc((unsigned int)bytes, (unsigned int)align);
}

// 0x004AC5F0 (:231)
bool stream2_mempool_allocator::can_alloc(int cpu_bytes, int cpu_align, int gpu_bytes, int gpu_align)
{
    if (cpu_pool == gpu_pool)
    {
        const int bytes = combined_size(cpu_bytes, gpu_bytes, gpu_align);
        return cpu_pool->can_alloc((unsigned int)bytes, (unsigned int)(cpu_align <= gpu_align ? gpu_align : cpu_align));
    }
    return cpu_pool->can_alloc((unsigned int)cpu_bytes, (unsigned int)cpu_align) && gpu_pool->can_alloc((unsigned int)gpu_bytes, (unsigned int)gpu_align);
}

// 0x004AA1D0 (:244)
int stream2_mempool_allocator::max_alloc()
{
    return (int)cpu_pool->space_free();
}

// 0x004AA1E0 (:249)
void stream2_mempool_allocator::max_alloc(int& cpu_max, int& gpu_max)
{
    cpu_max = (int)cpu_pool->space_free();
    gpu_max = (int)gpu_pool->space_free();
}

// 0x004AA210 (:255)
unsigned char* stream2_mempool_allocator::alloc(int bytes, int align, const char* debug_name)
{
    return (unsigned char*)cpu_pool->alloc((unsigned int)bytes, (unsigned int)align);
}

// 0x004AC680 (:263)
void stream2_mempool_allocator::alloc(int cpu_bytes, int cpu_align, unsigned char*& cpu_alloc, int gpu_bytes, int gpu_align, unsigned char*& gpu_alloc, const char* debug_name)
{
    if (cpu_pool != gpu_pool)
    {
        cpu_alloc = (unsigned char*)cpu_pool->alloc((unsigned int)cpu_bytes, (unsigned int)cpu_align);
        gpu_alloc = gpu_bytes > 0 ? (unsigned char*)gpu_pool->alloc((unsigned int)gpu_bytes, (unsigned int)gpu_align) : nullptr;
        return;
    }
    const int bytes = combined_size(cpu_bytes, gpu_bytes, gpu_align);
    unsigned char* const data = (unsigned char*)cpu_pool->alloc((unsigned int)bytes, (unsigned int)(cpu_align <= gpu_align ? gpu_align : cpu_align));
    cpu_alloc = data;
    gpu_alloc = gpu_bytes > 0 ? combined_gpu_part(data, cpu_bytes, gpu_align) : nullptr;
}

// 0x004AA230 (:290)
void stream2_mempool_allocator::free(unsigned char* data)
{
}

// 0x004AA240 (:296)
void stream2_mempool_allocator::free(unsigned char* data1, unsigned char* data2)
{
}

// 0x004AA250 (:302)
bool stream2_mempool_allocator::using_split_mempools()
{
    return gpu_pool != nullptr && gpu_pool != cpu_pool;
}

// 0x004AA270 (:307)
void stream2_mempool_allocator::set_thread_ownership()
{
    if (cpu_pool != nullptr)
    {
        cpu_pool->set_thread_ownership();
    }
    if (cpu_pool != gpu_pool && gpu_pool != nullptr)
    {
        gpu_pool->set_thread_ownership();
    }
}

// ---- stream2_stack_allocator

// 0x004AC770 (:324)
void stream2_stack_allocator::init(mempool_base* _pool)
{
    cpu_pool = _pool;
    marks.remove_all();
    gpu_pool = (_pool->get_flags() & 0x40) != 0 ? ((split_mempool*)_pool)->get_gpu_pool() : _pool;
}

// (implicit, vtbl+0x00)
stream2_stack_allocator::~stream2_stack_allocator()
{
}

// 0x004AA2A0 (:337)
bool stream2_stack_allocator::can_alloc(int bytes, int align)
{
    return cpu_pool->can_alloc((unsigned int)bytes, (unsigned int)align);
}

// 0x004AC790 (:342)
bool stream2_stack_allocator::can_alloc(int cpu_bytes, int cpu_align, int gpu_bytes, int gpu_align)
{
    if (cpu_pool == gpu_pool)
    {
        const int bytes = combined_size(cpu_bytes, gpu_bytes, gpu_align);
        return cpu_pool->can_alloc((unsigned int)bytes, (unsigned int)(cpu_align <= gpu_align ? gpu_align : cpu_align));
    }
    return cpu_pool->can_alloc((unsigned int)cpu_bytes, (unsigned int)cpu_align) && gpu_pool->can_alloc((unsigned int)gpu_bytes, (unsigned int)gpu_align);
}

// 0x004AA2B0 (:355)
int stream2_stack_allocator::max_alloc()
{
    return (int)cpu_pool->space_free();
}

// 0x004AA2C0 (:360)
void stream2_stack_allocator::max_alloc(int& cpu_max, int& gpu_max)
{
    cpu_max = (int)cpu_pool->space_free();
    gpu_max = (int)gpu_pool->space_free();
}

// 0x004AF0E0 (:366)
unsigned char* stream2_stack_allocator::alloc(int bytes, int align, const char* debug_name)
{
    const unsigned int mark = cpu_pool->mark();
    unsigned char* const data = (unsigned char*)cpu_pool->alloc((unsigned int)bytes, (unsigned int)align);
    marks.add(memory_mark(mark, 0, data, nullptr));
    return data;
}

// 0x004AF180 (:394)
void stream2_stack_allocator::alloc(int cpu_bytes, int cpu_align, unsigned char*& cpu_alloc, int gpu_bytes, int gpu_align, unsigned char*& gpu_alloc, const char* debug_name)
{
    unsigned int cpu_mark;
    unsigned int gpu_mark = 0;
    if (cpu_pool == gpu_pool)
    {
        cpu_mark = cpu_pool->mark();
        const int bytes = combined_size(cpu_bytes, gpu_bytes, gpu_align);
        unsigned char* const data = (unsigned char*)cpu_pool->alloc((unsigned int)bytes, (unsigned int)(cpu_align <= gpu_align ? gpu_align : cpu_align));
        cpu_alloc = data;
        gpu_alloc = gpu_bytes > 0 ? combined_gpu_part(data, cpu_bytes, gpu_align) : nullptr;
    }
    else
    {
        cpu_mark = cpu_pool->mark();
        gpu_mark = gpu_pool->mark();
        cpu_alloc = (unsigned char*)cpu_pool->alloc((unsigned int)cpu_bytes, (unsigned int)cpu_align);
        gpu_alloc = gpu_bytes > 0 ? (unsigned char*)gpu_pool->alloc((unsigned int)gpu_bytes, (unsigned int)gpu_align) : nullptr;
    }
    marks.add(memory_mark(cpu_mark, gpu_mark, cpu_alloc, gpu_alloc));
}

// 0x004AF2F0 (:447)
void stream2_stack_allocator::free(unsigned char* data)
{
    for (int i = marks.size() - 1; i >= 0; i--)
    {
        if (marks[i].cpu_data == data)
        {
            marks[i].released = true;
            process_restore_requests();
            return;
        }
    }
}

// 0x004AF330 (:466)
void stream2_stack_allocator::free(unsigned char* data1, unsigned char* data2)
{
    if (cpu_pool == gpu_pool)
    {
        free(data1);
        return;
    }
    for (int i = marks.size() - 1; i >= 0; i--)
    {
        if (marks[i].cpu_data == data1)
        {
            marks[i].released = true;
            process_restore_requests();
            return;
        }
    }
}

// 0x004AA2F0 (:494)
bool stream2_stack_allocator::using_split_mempools()
{
    return gpu_pool != nullptr && gpu_pool != cpu_pool;
}

// 0x004AA310 (:499)
void stream2_stack_allocator::set_thread_ownership()
{
    if (cpu_pool != nullptr)
    {
        cpu_pool->set_thread_ownership();
    }
    if (cpu_pool != gpu_pool && gpu_pool != nullptr)
    {
        gpu_pool->set_thread_ownership();
    }
}

// 0x004AC820 (:536)
void stream2_stack_allocator::process_restore_requests()
{
    // Roll the pools back over the released marks at the top of the stack.
    for (int i = marks.size() - 1; i >= 0; i--)
    {
        if (!marks[i].released)
        {
            return;
        }
        if ((cpu_pool->get_flags() & 0x40) == 0)
        {
            cpu_pool->restore_to_mark(marks[i].cpu_mark);
        }
        else
        {
            ((split_mempool*)cpu_pool)->restore_to_mark_cpu(marks[i].cpu_mark);
        }
        if (cpu_pool != gpu_pool)
        {
            gpu_pool->restore_to_mark(marks[i].gpu_mark);
        }
        marks.remove(i);
    }
}

// ---- stream2_slot_allocator

// Slots of a bracket form circular lists by index (slot_free_list / slot_used_list are the heads).
static unsigned short slot_index(const stream2_slot_allocator::bracket& b, const stream2_slot_allocator::slot* s)
{
    return (unsigned short)(s - b.slots);
}

static void slot_list_append(stream2_slot_allocator::bracket& b, unsigned short& head, stream2_slot_allocator::slot* s)
{
    const unsigned short index = slot_index(b, s);
    if (head == 0xFFFF)
    {
        s->next = index;
        s->prev = index;
        head = index;
        return;
    }
    s->prev = b.slots[head].prev;
    s->next = head;
    b.slots[b.slots[head].prev].next = index;
    b.slots[head].prev = index;
}

static void slot_list_remove(stream2_slot_allocator::bracket& b, unsigned short& head, stream2_slot_allocator::slot* s)
{
    const unsigned short index = slot_index(b, s);
    if (head == index)
    {
        if (s->next == index)
        {
            s->next = 0xFFFF;
            s->prev = 0xFFFF;
            head = 0xFFFF;
            return;
        }
        head = s->next;
    }
    b.slots[s->prev].next = s->next;
    b.slots[s->next].prev = s->prev;
    s->next = 0xFFFF;
    s->prev = 0xFFFF;
}

// 0x005D47A0
stream2_slot_allocator::stream2_slot_allocator()
{
}

// 0x005D47B0
stream2_slot_allocator::~stream2_slot_allocator()
{
}

// 0x004AA340 (:561)
bool stream2_slot_allocator::can_alloc(int bytes, int _align)
{
    VLIB_CRITICAL_FAILURE();
    return false;
}

// 0x004AF390 (:570)
bool stream2_slot_allocator::can_alloc(int cpu_bytes, int _cpu_align, int gpu_bytes, int _gpu_align)
{
    if (_cpu_align <= cpu_align && _gpu_align <= gpu_align)
    {
        const int index = get_bracket_index(cpu_bytes, gpu_bytes);
        if (index >= 0 && brackets[index].slot_free_list != 0xFFFF)
        {
            return true;
        }
    }
    return false;
}

// 0x004AA360 (:589)
int stream2_slot_allocator::max_alloc()
{
    VLIB_CRITICAL_FAILURE();
    return 0;
}

// 0x004AC8D0 (:596)
void stream2_slot_allocator::max_alloc(int& cpu_max, int& gpu_max)
{
    const bracket& last = brackets[brackets.size() - 1];
    cpu_max = last.cpu_bracket_size - cpu_align;
    gpu_max = last.gpu_bracket_size - gpu_align;
}

// 0x004AA380 (:609)
bool stream2_slot_allocator::init(unsigned char* (*alloc_cb)(int, int), int* bracket_info, int num_brackets, int _align, const char* name)
{
    VLIB_CRITICAL_FAILURE();
    return false;
}

// 0x004AC900 (:624)
bool stream2_slot_allocator::init(unsigned char* (*cpu_alloc_cb)(int, int), int cpu_alignment, unsigned char* (*gpu_alloc_cb)(int, int), int gpu_alignment, int* bracket_info, int num_brackets, const char* name)
{
    gpu_vram_allocated = 0;
    cpu_total_allocated = 0;
    if (cpu_alloc_cb == nullptr || gpu_alloc_cb == nullptr)
    {
        return false;
    }
    cpu_align = cpu_alignment < 5 ? 4 : cpu_alignment;
    gpu_align = gpu_alignment < 5 ? 4 : gpu_alignment;

    // One block: the bracket array, the slot headers, then each slot's memory (aligned to cpu_align).
    const int info_count = num_brackets * 3;
    const int header_size = num_brackets * (int)sizeof(bracket);
    int total = header_size;
    for (int i = 0; i < info_count; i += 3)
    {
        total += bracket_info[i + 2] * (int)sizeof(slot);
    }
    for (int i = 0; i < info_count; i += 3)
    {
        for (int n = 0; n < bracket_info[i + 2]; n++)
        {
            const int rest = total % cpu_align;
            total += (rest == 0 ? 0 : cpu_align - rest) + cpu_align + gpu_align + bracket_info[i];
        }
    }
    unsigned char* const block = cpu_alloc_cb(total, cpu_align);
    cpu_total_allocated = total;
    if (block == nullptr)
    {
        return false;
    }

    brackets.init((bracket*)block, num_brackets, num_brackets);
    int offset = header_size;
    for (int b = 0; b < num_brackets; b++)
    {
        bracket& br = brackets[b];
        const int count = bracket_info[b * 3 + 2];
        br.slots = (slot*)(block + offset);
        offset += count * (int)sizeof(slot);
        br.cpu_bracket_size = cpu_align + gpu_align + bracket_info[b * 3];
        br.gpu_bracket_size = 0;
        br.slot_free_list = 0xFFFF;
        br.slot_used_list = 0xFFFF;
        for (int n = 0; n < count; n++)
        {
            slot_list_append(br, br.slot_free_list, &br.slots[n]);
        }
    }
    for (int b = 0; b < num_brackets; b++)
    {
        bracket& br = brackets[b];
        const int count = bracket_info[b * 3 + 2];
        const int cpu_size = bracket_info[b * 3];
        for (int n = 0; n < count; n++)
        {
            char pool_name[32];
            sprintf(pool_name, "%s_%d_%d", name, b, n);
            slot* const s = &br.slots[n];
            s->bindex = b;
            const int rest = offset % cpu_align;
            const int pad = rest == 0 ? 0 : cpu_align - rest;
            new (&s->cpu_pool) mempool(block + offset + pad, (unsigned int)(gpu_align + cpu_align + cpu_size), pool_name, (unsigned int)cpu_align, 0x20);
            offset += pad + cpu_align + gpu_align + cpu_size;
        }
    }
    return true;
}

// 0x004AF3E0 (:798)
void stream2_slot_allocator::alloc(int cpu_bytes, int _cpu_align, unsigned char*& cpu_alloc, int gpu_bytes, int _gpu_align, unsigned char*& gpu_alloc, const char* debug_name)
{
    // The first bracket that fits both parts; the whole slot is handed out.
    for (int i = 0; i < brackets.size(); i++)
    {
        bracket& br = brackets[i];
        if (gpu_bytes + cpu_bytes <= br.cpu_bracket_size)
        {
            if (br.slot_free_list == 0xFFFF)
            {
                break;
            }
            slot* const s = &br.slots[br.slot_free_list];
            slot_list_remove(br, br.slot_free_list, s);
            slot_list_append(br, br.slot_used_list, s);
            unsigned char* const data = (unsigned char*)s->cpu_pool.alloc_all(0);
            cpu_alloc = data;
            gpu_alloc = gpu_bytes > 0 ? combined_gpu_part(data, cpu_bytes, _gpu_align) : nullptr;
            return;
        }
    }
    cpu_alloc = nullptr;
    gpu_alloc = nullptr;
}

// 0x004AA3A0 (:880)
unsigned char* stream2_slot_allocator::alloc(int bytes, int _align, const char* debug_name)
{
    VLIB_CRITICAL_FAILURE();
    return nullptr;
}

// 0x004ACD60 (:891)
void stream2_slot_allocator::free(unsigned char* cpu_buf, unsigned char* gpu_buf)
{
    for (int i = 0; i < brackets.size(); i++)
    {
        bracket& br = brackets[i];
        if (br.slot_used_list == 0xFFFF)
        {
            continue;
        }
        slot* s = &br.slots[br.slot_used_list];
        for (;;)
        {
            if (s->cpu_pool.get_base() == cpu_buf)
            {
                s->cpu_pool.clear();
                bracket& owner = brackets[s->bindex];
                slot_list_remove(owner, owner.slot_used_list, s);
                slot_list_append(owner, owner.slot_free_list, s);
                return;
            }
            if (s->next == br.slot_used_list)
            {
                break;
            }
            s = &br.slots[s->next];
        }
    }
}

// 0x004AA3C0 (:934)
void stream2_slot_allocator::free(unsigned char* buf)
{
    VLIB_CRITICAL_FAILURE();
}

// 0x004AA3E0 (:942)
bool stream2_slot_allocator::using_split_mempools()
{
    return true;
}

// 0x004ACFE0 (:947)
void stream2_slot_allocator::set_thread_ownership()
{
    for (int i = 0; i < brackets.size(); i++)
    {
        bracket& br = brackets[i];
        const unsigned short heads[2] = { br.slot_used_list, br.slot_free_list };
        for (int list = 0; list < 2; list++)
        {
            if (heads[list] == 0xFFFF)
            {
                continue;
            }
            slot* s = &br.slots[heads[list]];
            for (;;)
            {
                s->cpu_pool.set_thread_ownership();
                if (s->next == heads[list])
                {
                    break;
                }
                s = &br.slots[s->next];
            }
        }
    }
}

// 0x004AD0A0 (:971)
int stream2_slot_allocator::get_bracket_index(int cpu_bytes, int gpu_bytes)
{
    for (int i = 0; i < brackets.size(); i++)
    {
        if (gpu_bytes + cpu_bytes <= brackets[i].cpu_bracket_size)
        {
            return i;
        }
    }
    return -1;
}

// 0x004AD0E0 (:994)
void stream2_slot_allocator::destroy()
{
    for (int i = 0; i < brackets.size(); i++)
    {
        bracket& br = brackets[i];
        const unsigned short heads[2] = { br.slot_free_list, br.slot_used_list };
        for (int list = 0; list < 2; list++)
        {
            if (heads[list] == 0xFFFF)
            {
                continue;
            }
            slot* s = &br.slots[heads[list]];
            for (;;)
            {
                s->cpu_pool.destroy();
                if (s->next == heads[list])
                {
                    break;
                }
                s = &br.slots[s->next];
            }
        }
    }
    brackets.init(nullptr, 0, 0);
}

// ---- stream2_system_allocator (inline in the header)

// 0x008A0040
stream2_system_allocator::stream2_system_allocator()
{
}

// 0x008A00E0
stream2_system_allocator::~stream2_system_allocator()
{
}

// 0x008A0050 (.h:237)
bool stream2_system_allocator::can_alloc(int, int)
{
    return true;
}

// 0x00883360 (.h:236)
bool stream2_system_allocator::can_alloc(int, int, int, int)
{
    return true;
}

// 0x008A0070 (.h:239)
void stream2_system_allocator::max_alloc(int& cpu_max, int& gpu_max)
{
    cpu_max = 0x7FFFFFFF;
    gpu_max = 0x7FFFFFFF;
}

// 0x008A0060 (.h:238)
int stream2_system_allocator::max_alloc()
{
    return 0x7FFFFFFF;
}

// 0x00883370 (.h:241)
void stream2_system_allocator::alloc(int cpu_bytes, int cpu_align, unsigned char*& cpu_alloc, int gpu_bytes, int gpu_align, unsigned char*& gpu_alloc, const char* debug_name)
{
    cpu_alloc = (unsigned char*)_aligned_malloc((size_t)cpu_bytes, (size_t)cpu_align);
    gpu_alloc = (unsigned char*)_aligned_malloc((size_t)gpu_bytes, (size_t)gpu_align);
}

// 0x008A0090 (.h:240)
unsigned char* stream2_system_allocator::alloc(int bytes, int align, const char* debug_name)
{
    return (unsigned char*)_aligned_malloc((size_t)bytes, (size_t)align);
}

// 0x008833B0 (.h:243)
void stream2_system_allocator::free(unsigned char* cpu_data, unsigned char* gpu_data)
{
    if (cpu_data != nullptr)
    {
        _aligned_free(cpu_data);
    }
    if (gpu_data != nullptr)
    {
        _aligned_free(gpu_data);
    }
}

// 0x008A00B0 (.h:242)
void stream2_system_allocator::free(unsigned char* data)
{
    if (data != nullptr)
    {
        _aligned_free(data);
    }
}

// 0x008833E0 (.h:244)
void stream2_system_allocator::set_thread_ownership()
{
}

// 0x008A00D0 (.h:245)
bool stream2_system_allocator::using_split_mempools()
{
    return false;
}

// 0x004AA3F0 (:1028)
bool stream2_register_allocator(int allocator_index, stream2_allocator* allocator)
{
    _Stream_allocators[allocator_index] = allocator;
    return true;
}
