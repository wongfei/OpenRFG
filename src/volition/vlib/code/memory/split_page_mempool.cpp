// Original: volition/vlib/code/memory/split_page_mempool.cpp
//
// Page allocator whose run headers live in header_hash (keyed by address) instead of the managed memory. Free runs
// form a list sorted by address (first_free_page, linked through next_page); a used run's header holds the offset
// of the returned pointer into its first page (align_offset) and is keyed by that pointer, so release_bytes and
// size find it from the pointer the caller has.
//
// Notes (decompiled behaviour, kept as is):
// - alloc aligns the run start with the requested align (get_align_page_count) but computes the in-page offset from
//   max(align, def_align); new free-run headers get an uninitialised wasted_space in the original (0 here).
// - realloc never advances when the next free run lies above the block's end: the loop spins forever (checked in
//   the disassembly at 0x005CBBD5..0x005CBBE9).
// - shrink_to_fit links the freed tail behind the first free run even when it lies before it, writes through a
//   null pointer when there is no free run, and drops the tail when it lies after the last free run.
// - clear() with a full header hash leaves no free run but sets free_list_count to 1.
// - mark, restore_to_mark, alloc_all, pad_to_page and release_bytes(unsigned int) are not supported (log only).

#include "volition/vlib/code/memory/split_page_mempool.h"

#include "volition/vlib/code/memory/vmemory.h"
#include "volition/vlib/code/vlib_core/vlib_assert.h"

#include <string.h>

// 0x005CB890 (:28)
split_page_mempool::split_page_mempool()
{
    page_size = 0;
    max_pages = 0;
    free_list_count = 0;
}

// 0x005CBA90 (:37)
split_page_mempool::split_page_mempool(const void* mem, unsigned int mem_size, unsigned int page_size, const char* pool_name, mempool& hash_pool, unsigned int max_alloc, unsigned int align, unsigned char mp_flags)
{
    max_pages = 0;
    init(mem, mem_size, page_size, pool_name, hash_pool, max_alloc, align, mp_flags);
}

// 0x005CBB50 (:45)
split_page_mempool::~split_page_mempool()
{
    header_hash.clear();
}

// 0x005C8640 (:59)
void split_page_mempool::init(const void* mem, unsigned int mem_size, unsigned int default_page_size, const char* pool_name, mempool& hash_pool, unsigned int max_alloc, unsigned int align, unsigned char mp_flags)
{
    init_static_mempool(mem, mem_size, pool_name, align, mp_flags);
    header_hash.init(hash_pool, max_alloc);
    page_size = default_page_size;
    wasted_space = 0;
    max_pages = mem_size / default_page_size;
    max_page_addr = pool + (mem_size / default_page_size - 1) * default_page_size;
    clear();
}

// 0x0058A1A0 (:87)
unsigned int split_page_mempool::get_required_page_count(unsigned int size, unsigned int align, unsigned int& align_offset) const
{
    align_offset = 0;
    if (align < def_align)
    {
        align = def_align;
    }
    const unsigned int rest = (unsigned int)pool % align;
    align_offset = rest != 0 ? align - rest : 0;
    align_offset %= page_size;
    return (align_offset + page_size - 1 + size) / page_size;
}

// 0x0058A1F0 (:111)
unsigned int split_page_mempool::get_page_index(const split_page_mempool_header* header) const
{
    if (header == nullptr)
    {
        return 0xFFFFFFFF;
    }
    return (unsigned int)(header->page_addr - pool) / page_size;
}

// 0x0058A210 (:122)
unsigned int split_page_mempool::get_align_page_count(unsigned char* page_addr, unsigned int align) const
{
    if (align <= page_size)
    {
        return 0;
    }
    const unsigned int rest = (unsigned int)page_addr % align;
    if (rest == 0)
    {
        return 0;
    }
    return (align - rest) / page_size;
}

// 0x0058A250 (:139)
bool split_page_mempool::can_alloc(unsigned int size, unsigned int align) const
{
    unsigned int align_offset;
    const unsigned int pages = get_required_page_count(size, align, align_offset);
    for (split_page_mempool_header* header = first_free_page; header != nullptr; header = header->next_page)
    {
        if (get_align_page_count(header->page_addr, align) + pages <= header->num_pages)
        {
            return true;
        }
    }
    return false;
}

// 0x005C8700 (:163)
void* split_page_mempool::alloc(unsigned int size, unsigned int align)
{
    if (size == 0)
    {
        return nullptr;
    }
    unsigned int align_offset;
    const unsigned int pages = get_required_page_count(size, align, align_offset);

    split_page_mempool_header** link = &first_free_page;
    for (split_page_mempool_header* header = first_free_page; header != nullptr; link = &header->next_page, header = header->next_page)
    {
        const unsigned int align_pages = get_align_page_count(header->page_addr, align);
        if (align_pages + pages > header->num_pages)
        {
            continue;
        }

        free_list_count--;
        if (align_pages != 0)
        {
            // The leading pages stay a free run; the allocation starts at the next aligned page.
            split_page_mempool_header rest;
            rest.wasted_space = 0;
            rest.num_pages = header->num_pages - align_pages;
            rest.page_addr = header->page_addr + page_size * align_pages;
            rest.next_page = header->next_page;
            link = &header->next_page;
            *link = header_hash.insert_and_get_ptr((unsigned int)rest.page_addr, rest);
            free_list_count++;
            if (free_list_max < free_list_count)
            {
                free_list_max = free_list_count;
            }
            header->num_pages = align_pages;
            header = *link;
        }
        if (pages < header->num_pages)
        {
            split_page_mempool_header rest;
            rest.wasted_space = 0;
            rest.num_pages = header->num_pages - pages;
            rest.page_addr = header->page_addr + page_size * pages;
            rest.next_page = header->next_page;
            header->next_page = header_hash.insert_and_get_ptr((unsigned int)rest.page_addr, rest);
            free_list_count++;
            if (free_list_max < free_list_count)
            {
                free_list_max = free_list_count;
            }
        }

        *link = header->next_page;
        header->num_pages = pages;
        header->align_offset = align_offset;
        current_pos += pages;
        const unsigned int waste = page_size * pages - size;
        header->wasted_space = waste;
        wasted_space += waste;
        const unsigned int used = space_used();
        peak_usage = used < peak_usage ? peak_usage : used;

        unsigned char* const page_addr = header->page_addr;
        if (align_offset != 0)
        {
            // Re-key the header by the pointer handed out.
            header_hash.insert((unsigned int)(page_addr + align_offset), *header);
            header_hash.remove((unsigned int)header->page_addr);
        }
        return page_addr + align_offset;
    }
    return nullptr;
}

// 0x005CBB70 (:260)
void* split_page_mempool::realloc(void* mem, unsigned int new_size)
{
    if (mem == nullptr)
    {
        return nullptr;
    }
    split_page_mempool_header* header = header_hash.get_ptr((unsigned int)mem);
    if (header == nullptr)
    {
        return nullptr;
    }
    const unsigned int needed = header->align_offset + new_size;
    const unsigned int pages = (page_size - 1 + needed) / page_size;
    const unsigned int cur_pages = header->num_pages;
    if (pages <= cur_pages)
    {
        return mem;
    }

    const unsigned char* const end = header->page_addr + cur_pages * page_size;
    const unsigned int extra = pages - cur_pages;
    split_page_mempool_header** link = &first_free_page;
    split_page_mempool_header* free_run = first_free_page;
    while (free_run != nullptr)
    {
        if (free_run->page_addr > end)
        {
            continue; // see the file comment
        }
        if (free_run->page_addr != end)
        {
            link = &free_run->next_page;
            free_run = free_run->next_page;
            continue;
        }

        if (free_run->num_pages < extra)
        {
            return nullptr;
        }
        header->num_pages = cur_pages + extra;
        free_run->num_pages -= extra;
        current_pos += extra;
        wasted_space -= header->wasted_space;
        const unsigned int waste = page_size * header->num_pages - needed;
        header->wasted_space = waste;
        wasted_space += waste;
        const unsigned int used = space_used();
        peak_usage = used < peak_usage ? peak_usage : used;
        if (free_run->num_pages == 0)
        {
            *link = free_run->next_page;
            free_list_count--;
            header_hash.remove((unsigned int)free_run->page_addr);
            return mem;
        }
        split_page_mempool_header rest;
        rest.wasted_space = 0;
        rest.num_pages = free_run->num_pages;
        rest.page_addr = header->page_addr + page_size * header->num_pages;
        rest.next_page = free_run->next_page;
        *link = header_hash.insert_and_get_ptr((unsigned int)rest.page_addr, rest);
        header_hash.remove((unsigned int)free_run->page_addr);
        return mem;
    }
    return nullptr;
}

// 0x005CBCD0 (:343)
bool split_page_mempool::is_aligned(const void* mem)
{
    if (mem != nullptr)
    {
        const split_page_mempool_header* header = header_hash.get_ptr((unsigned int)mem);
        if (header != nullptr)
        {
            return header->align_offset == 0;
        }
    }
    return false;
}

// 0x005CBD00 (:359)
unsigned int split_page_mempool::size(const void* mem)
{
    if (mem != nullptr)
    {
        const split_page_mempool_header* header = header_hash.get_ptr((unsigned int)mem);
        if (header != nullptr)
        {
            return page_size * header->num_pages;
        }
    }
    return 0;
}

// 0x005CBD30 (:375)
void split_page_mempool::shrink_to_fit(void* mem, unsigned int actual_size)
{
    if (mem == nullptr)
    {
        return;
    }
    split_page_mempool_header* header = header_hash.get_ptr((unsigned int)mem);
    if (header == nullptr)
    {
        return;
    }
    const unsigned int needed = header->align_offset + actual_size;
    const unsigned int pages = (page_size - 1 + needed) / page_size;
    unsigned int freed = header->num_pages - pages;
    if (freed == 0)
    {
        return;
    }
    header->num_pages = pages;
    current_pos -= freed;
    wasted_space -= header->wasted_space;
    const unsigned int waste = page_size * header->num_pages - needed;
    header->wasted_space = waste;
    wasted_space += waste;

    const unsigned int end_index = (unsigned int)(header->page_addr - pool) / page_size + header->num_pages;
    split_page_mempool_header* prev = first_free_page;
    split_page_mempool_header* cur = first_free_page;
    unsigned int cur_index = get_page_index(cur);
    do
    {
        split_page_mempool_header* run = cur;
        if (end_index < cur_index)
        {
            if (run != nullptr && end_index + freed == cur_index)
            {
                // The freed tail touches the next free run: take it over.
                freed += run->num_pages;
                split_page_mempool_header* const next = run->next_page;
                header_hash.remove((unsigned int)run->page_addr);
                free_list_count--;
                run = next;
            }
            split_page_mempool_header tail;
            tail.wasted_space = 0;
            tail.num_pages = freed;
            tail.page_addr = header->page_addr + page_size * header->num_pages;
            tail.next_page = run;
            prev->next_page = header_hash.insert_and_get_ptr((unsigned int)tail.page_addr, tail);
            free_list_count++;
            if (free_list_max < free_list_count)
            {
                free_list_max = free_list_count;
            }
            return;
        }
        if (run == nullptr)
        {
            return;
        }
        cur = run->next_page;
        cur_index = get_page_index(cur);
        prev = run;
    } while (cur != nullptr);
}

// 0x005CBE80 (:453)
void split_page_mempool::release_bytes(void* mem)
{
    if (mem == nullptr)
    {
        return;
    }
    split_page_mempool_header* header = header_hash.get_ptr((unsigned int)mem);
    if (header == nullptr)
    {
        return;
    }
    unsigned char* const key = header->page_addr;
    if (header->align_offset != 0)
    {
        // Move the header back under its page address.
        const split_page_mempool_header copy = *header;
        header = header_hash.insert_and_get_ptr((unsigned int)key, copy);
        header_hash.remove((unsigned int)mem);
    }
    current_pos -= header->num_pages;
    wasted_space -= header->wasted_space;

    // The last free run that starts before the next run above key (free runs are sorted by address).
    split_page_mempool_header* before = first_free_page;
    split_page_mempool_header* next = first_free_page;
    unsigned char* before_addr;
    for (;;)
    {
        before = next;
        if (before == nullptr)
        {
            before_addr = key + page_size;
            break;
        }
        next = before->next_page;
        before_addr = before->page_addr;
        const unsigned char* const limit = next != nullptr ? next->page_addr : max_page_addr;
        if (limit > key)
        {
            break;
        }
    }

    split_page_mempool_header** link;
    if (key < before_addr)
    {
        header->next_page = first_free_page;
        link = &first_free_page;
    }
    else
    {
        link = &before->next_page;
        header->next_page = before->next_page;
    }

    split_page_mempool_header* const after = header->next_page;
    if (after != nullptr && key + page_size * header->num_pages == after->page_addr)
    {
        header->num_pages += after->num_pages;
        header->next_page = after->next_page;
        header_hash.remove((unsigned int)after->page_addr);
        free_list_count--;
    }
    if (before != nullptr && before_addr + page_size * before->num_pages == key)
    {
        before->num_pages += header->num_pages;
        before->next_page = header->next_page;
        header_hash.remove((unsigned int)key);
        return;
    }
    *link = header;
    free_list_count++;
    if (free_list_max < free_list_count)
    {
        free_list_max = free_list_count;
    }
}

// 0x005C88E0 (:553)
split_page_mempool_header* split_page_mempool::get_header(unsigned int page_index)
{
    return header_hash.get_ptr(page_index);
}

// 0x005CC000 (:561)
bool split_page_mempool::clear()
{
    memset(pool, 0, max_size);
    header_hash.clear();
    unsigned char* const key = pool;
    current_pos = 0;
    split_page_mempool_header first;
    first.wasted_space = 0;
    first.num_pages = max_pages;
    first.page_addr = key;
    first.next_page = nullptr;
    first_free_page = header_hash.insert_and_get_ptr((unsigned int)key, first);
    free_list_count = 1;
    return true;
}

// 0x0058A2E0 (:583)
void split_page_mempool::debug_reset_runs()
{
    dbg_cur_index = 0;
    dbg_next_free_block = 0xFFFFFFFF;
}

// 0x005C88F0 (:591)
bool split_page_mempool::debug_get_next_run(int& num_pages, bool& in_use)
{
    if (max_pages <= dbg_cur_index)
    {
        dbg_cur_index = 0;
        return false;
    }
    if (dbg_next_free_block == 0xFFFFFFFF)
    {
        dbg_next_free_block = get_page_index(first_free_page);
    }
    const split_page_mempool_header* header = header_hash.get_ptr((unsigned int)(pool + page_size * dbg_cur_index));
    if (dbg_next_free_block == dbg_cur_index)
    {
        num_pages = (int)header->num_pages;
        in_use = false;
        dbg_next_free_block = header->next_page != nullptr ? get_page_index(header->next_page) : 0xFFFFFFFF;
    }
    else if (dbg_next_free_block < dbg_cur_index)
    {
        num_pages = (int)(max_pages - dbg_cur_index);
        in_use = true;
    }
    else
    {
        num_pages = (int)(dbg_next_free_block - dbg_cur_index);
        in_use = true;
    }
    dbg_cur_index += num_pages;
    return true;
}

// 0x005CB920 (.h:108)
void* split_page_mempool::get_base()
{
    return pool;
}

// 0x005CB930 (.h:112)
unsigned int split_page_mempool::space_free() const
{
    return (max_pages - current_pos) * page_size;
}

// 0x005CB940 (.h:113)
unsigned int split_page_mempool::space_used() const
{
    return current_pos * page_size;
}

// 0x005CB950 (.h:114)
unsigned int split_page_mempool::space_max() const
{
    return max_pages * page_size;
}

// 0x005CB960 (.h:115)
unsigned int split_page_mempool::get_page_size() const
{
    return page_size;
}

// 0x005CB970 (.h:116)
unsigned int split_page_mempool::free_list_size() const
{
    return free_list_count;
}

// 0x005CB980 (.h:117)
unsigned int split_page_mempool::free_list_peak() const
{
    return free_list_max;
}

// 0x005CB990 (.h:120)
unsigned int split_page_mempool::space_wasted() const
{
    return wasted_space;
}

// 0x005CBA70 (.h:122)
unsigned int split_page_mempool::space_hash() const
{
    return header_hash.bytes_allocated();
}

// 0x005CB9A0 (.h:144)
unsigned int split_page_mempool::mark()
{
    VLIB_CRITICAL_FAILURE();
    return 0;
}

// 0x005CB9C0 (.h:145)
bool split_page_mempool::restore_to_mark(unsigned int pos)
{
    VLIB_CRITICAL_FAILURE();
    return false;
}

// 0x005CB9E0 (.h:146)
void* split_page_mempool::alloc_all(unsigned int align)
{
    VLIB_CRITICAL_FAILURE();
    return nullptr;
}

// 0x005CBA00 (.h:147)
bool split_page_mempool::pad_to_page(unsigned int page_sizev)
{
    VLIB_CRITICAL_FAILURE();
    return false;
}

// 0x005CBA20 (.h:148)
bool split_page_mempool::release_bytes(unsigned int num_bytes)
{
    VLIB_CRITICAL_FAILURE();
    return false;
}

// 0x005CBA40 (.h:151)
bool split_page_mempool::instantiable()
{
    return true;
}
