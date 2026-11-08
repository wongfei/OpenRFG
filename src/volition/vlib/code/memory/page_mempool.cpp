// Original: volition/vlib/code/memory/page_mempool.cpp
//
// Allocator of page runs over a fixed buffer (decompiled). The buffer is cut into max_pages pages of page_size bytes.
// Every run (free or used) starts with a page_mempool_header: num_pages, then for a free run the index of the next free
// run (sorted by index, 0xFFFFFFFF ends the list) and for a used run the offset of the data from the header (8 plus the
// alignment padding). current_pos counts used pages, so space_used() is current_pos * page_size. alloc takes the first
// free run that fits (first fit), splitting off leading pages when the alignment is larger than a page; release_bytes
// puts the run back into the sorted free list and merges it with its neighbours. realloc grows a run in place into the
// free run right behind it, or fails (the caller then allocates and copies). The bump allocator interface (mark,
// restore_to_mark, alloc_all, pad_to_page, release_bytes(size)) is not supported.
//
// Notes (decompiled behaviour, kept as is):
// - clear() writes the first header through get_header(0) even when the pool has no pages (a null write).
// - The alignment padding is computed from the pool start, not from the run that is chosen, so it is only right for
//   alignments up to the page size (get_align_page_count handles the larger ones by skipping whole pages).

#include "volition/vlib/code/memory/page_mempool.h"
#include "volition/vlib/code/vlib_core/vlib_assert.h"

#include "keen/base/trace.hpp"

#include <string.h>

// 0x00563B30 (.h:58)
unsigned int page_mempool_header::data_offset()
{
    return align_offset;
}

// 0x005AB330 (:30)
page_mempool::page_mempool()
{
    page_size = 0;
    max_pages = 0;
    first_free_index = 0;
}

// 0x005BC3F0 (:39)
page_mempool::page_mempool(const void* mem, unsigned int mem_size, unsigned int _page_size, const char* pool_name, unsigned int align, unsigned char mp_flags)
{
    max_pages = 0;
    first_free_index = 0;
    // The first data block (after its 8-byte header) is aligned; the buffer is shortened by the skipped bytes.
    if (align > 1)
    {
        const unsigned int skip = (align - ((unsigned int)mem + 8) % align) % align;
        mem = (const unsigned char*)mem + skip;
        mem_size -= skip;
    }
    init_static_mempool(mem, mem_size, pool_name, align, mp_flags);
    flags |= 2;
    page_size = _page_size;
    max_pages = mem_size / _page_size;
    clear();
}

// 0x005AB460 (:48)
page_mempool::~page_mempool()
{
}

// 0x005AB470 (:61)
void page_mempool::init(const void* mem, unsigned int mem_size, unsigned int default_page_size, const char* pool_name, unsigned int align, unsigned char mp_flags)
{
    if (align > 1)
    {
        const unsigned int skip = (align - ((unsigned int)mem + 8) % align) % align;
        mem = (const unsigned char*)mem + skip;
        mem_size -= skip;
    }
    init_static_mempool(mem, mem_size, pool_name, align, mp_flags);
    flags |= 2;
    page_size = default_page_size;
    max_pages = mem_size / default_page_size;
    clear();
}

// 0x00589540 (:101)
unsigned int page_mempool::get_required_page_count(unsigned int size, unsigned int align, unsigned int& align_offset) const
{
    unsigned int total = size + 8;
    align_offset = 0;
    if ((align != 0 || (align = def_align) > 1) && align > 1)
    {
        unsigned int offset = (unsigned int)(pool + 8) % align;
        offset = offset == 0 ? 0 : align - offset;
        align_offset = offset % page_size;
        total += align_offset;
    }
    return (page_size - 1 + total) / page_size;
}

// 0x005895A0 (:128)
unsigned int page_mempool::get_align_page_count(page_mempool_header* page_header, unsigned int align) const
{
    // Whole pages to skip so that the data of a run starting at page_header is aligned (alignments above a page).
    if (align <= page_size)
    {
        return 0;
    }
    const unsigned int offset = (unsigned int)(page_header + 1) % align;
    if (offset == 0)
    {
        return 0;
    }
    return (align - offset) / page_size;
}

// 0x005895E0 (:139)
unsigned int page_mempool::get_page_index(const void* mem)
{
    if (pool < mem && mem < pool + max_size)
    {
        return ((unsigned int)mem - 8 - (unsigned int)pool) / page_size;
    }
    return 0xFFFFFFFF;
}

// 0x00563B40 (.h:89)
page_mempool_header* page_mempool::get_header(unsigned int page_index) const
{
    if (page_index < max_pages)
    {
        return (page_mempool_header*)(pool + page_size * page_index);
    }
    return nullptr;
}

// 0x005AB390 (.h:98)
void* page_mempool::get_base()
{
    return pool;
}

// 0x005AB3A0 (.h:102)
unsigned int page_mempool::space_free() const
{
    return (max_pages - current_pos) * page_size;
}

// 0x004B6760 (.h:103)
unsigned int page_mempool::space_used() const
{
    return current_pos * page_size;
}

// 0x004B6770 (.h:104)
unsigned int page_mempool::space_max() const
{
    return max_pages * page_size;
}

// 0x005AB4D0 (:158)
bool page_mempool::can_alloc(unsigned int size, unsigned int align) const
{
    if (!assert_permission())
    {
        return false;
    }
    unsigned int align_offset;
    const unsigned int num_pages = get_required_page_count(size, align, align_offset);
    for (unsigned int index = first_free_index; index != 0xFFFFFFFF;)
    {
        page_mempool_header* header = get_header(index);
        if (header == nullptr)
        {
            keen::debug::nativePrintArgs("[page_mempool] can_alloc: get_header failed for page_index %zu. allocator name: %s\n", index, name);
            return false;
        }
        if (get_align_page_count(header, align) + num_pages <= header->num_pages)
        {
            return true;
        }
        index = header->next_index;
    }
    return false;
}

// 0x005AB5B0 (:196)
void* page_mempool::alloc(unsigned int size, unsigned int align)
{
    if (!assert_permission())
    {
        return nullptr;
    }
    unsigned int align_offset;
    const unsigned int num_pages = get_required_page_count(size, align, align_offset);
    unsigned int index = first_free_index;
    page_mempool_header* prev = nullptr;
    if (index >= max_pages)
    {
        return nullptr;
    }
    for (;;)
    {
        page_mempool_header* header = get_header(index);
        if (header == nullptr)
        {
            keen::debug::nativePrintArgs("[page_mempool] alloc: (0) get_header failed for page_index: %zu. allocator name: %s\n", index, name);
            return nullptr;
        }
        const unsigned int skip = get_align_page_count(header, align);
        if (num_pages + skip <= header->num_pages)
        {
            page_mempool_header* run = header;
            if (skip != 0)
            {
                // The skipped pages stay a free run in front of the allocation.
                index += skip;
                run = get_header(index);
                if (run == nullptr)
                {
                    keen::debug::nativePrintArgs("[page_mempool] alloc: (1) get_header failed for page_index: %zu. allocator name: %s\n", index, name);
                    return nullptr;
                }
                run->num_pages = header->num_pages - skip;
                run->next_index = header->next_index;
                header->num_pages = skip;
                header->next_index = index;
                prev = header;
            }
            if (num_pages < run->num_pages)
            {
                // The rest of the run becomes a free run behind the allocation.
                const unsigned int next_index = index + num_pages;
                page_mempool_header* rest = get_header(next_index);
                if (rest == nullptr)
                {
                    keen::debug::nativePrintArgs("[page_mempool] alloc: (2) get_header failed for next_index: %zu. allocator name: %s\n", next_index, name);
                    return nullptr;
                }
                rest->num_pages = run->num_pages - num_pages;
                rest->next_index = run->next_index;
                run->next_index = next_index;
            }
            if (prev != nullptr)
            {
                prev->next_index = run->next_index;
            }
            if (index == first_free_index)
            {
                first_free_index = run->next_index;
            }
            void* mem = (unsigned char*)run + align_offset + 8;
            run->align_offset = (unsigned int)((unsigned char*)mem - (unsigned char*)run);
            run->num_pages = num_pages;
            current_pos += num_pages;
            const unsigned int used = space_used();
            peak_usage = used < peak_usage ? peak_usage : used;
            return mem;
        }
        index = header->next_index;
        prev = header;
        if (index >= max_pages)
        {
            return nullptr;
        }
    }
}

// 0x005AB770 (:299)
void* page_mempool::realloc(void* mem, unsigned int new_size)
{
    if (mem == nullptr)
    {
        return nullptr;
    }
    const unsigned int page_index = get_page_index(mem);
    page_mempool_header* header = get_header(page_index);
    if (header == nullptr)
    {
        keen::debug::nativePrintArgs("[page_mempool] realloc: (0) get_header failed for page_index: %zu. allocator name: %s\n", page_index, name);
        return nullptr;
    }
    const unsigned int required = (header->align_offset + page_size - 1 + new_size) / page_size;
    if (required <= header->num_pages)
    {
        return mem;
    }
    // Only the free run directly behind this one can be used: find it and its predecessor in the free list.
    const unsigned int extra = required - header->num_pages;
    const unsigned int next_index = page_index + header->num_pages;
    page_mempool_header* prev = nullptr;
    for (unsigned int cur_index = first_free_index; cur_index != next_index;)
    {
        if (next_index < cur_index)
        {
            return nullptr;
        }
        prev = get_header(cur_index);
        if (prev == nullptr)
        {
            keen::debug::nativePrintArgs("[page_mempool] realloc: (3) get_header failed for cur_index: %zu. allocator name: %s\n", cur_index, name);
            return nullptr;
        }
        cur_index = prev->next_index;
    }
    page_mempool_header* next = get_header(next_index);
    if (next == nullptr)
    {
        keen::debug::nativePrintArgs("[page_mempool] realloc: (1) get_header failed for next_index: %zu. allocator name: %s\n", next_index, name);
        return nullptr;
    }
    if (next->num_pages < extra)
    {
        return nullptr;
    }
    header->num_pages += extra;
    next->num_pages -= extra;
    current_pos += extra;
    const unsigned int used = space_used();
    peak_usage = used < peak_usage ? peak_usage : used;
    unsigned int* link = prev != nullptr ? &prev->next_index : &first_free_index;
    if (next->num_pages == 0)
    {
        *link = next->next_index;
        return mem;
    }
    // The shortened free run moves behind the grown allocation.
    *link += extra;
    page_mempool_header* moved = get_header(*link);
    if (moved == nullptr)
    {
        keen::debug::nativePrintArgs("[page_mempool] realloc: (2) get_header failed for use_index: %zu. allocator name: %s\n", *link, name);
        return nullptr;
    }
    moved->num_pages = next->num_pages;
    moved->next_index = next->next_index;
    return mem;
}

// 0x00589610 (:393)
unsigned int page_mempool::size(const void* mem)
{
    if (mem == nullptr)
    {
        return 0;
    }
    const unsigned int page_index = get_page_index(mem);
    page_mempool_header* header = get_header(page_index);
    if (header == nullptr)
    {
        keen::debug::nativePrintArgs("[page_mempool] size: (0) get_header failed for page_index: %zu. allocator name: %s\n", page_index, name);
        return 0;
    }
    return header->num_pages * page_size - header->align_offset;
}

// 0x00589680 (:413)
void page_mempool::shrink_to_fit(void* mem, unsigned int actual_size)
{
    if (mem == nullptr)
    {
        return;
    }
    unsigned int page_index = get_page_index(mem);
    page_mempool_header* header = get_header(page_index);
    if (header == nullptr)
    {
        keen::debug::nativePrintArgs("[page_mempool] shrink_to_fit: (0) get_header failed for page_index: %zu. allocator name: %s\n", page_index, name);
        return;
    }
    const unsigned int num_pages = (header->align_offset + page_size - 1 + actual_size) / page_size;
    const unsigned int freed = header->num_pages - num_pages;
    if (freed == 0)
    {
        return;
    }
    header->num_pages = num_pages;
    current_pos -= freed;
    page_index += header->num_pages;
    page_mempool_header* free_run = get_header(page_index);
    if (free_run == nullptr)
    {
        keen::debug::nativePrintArgs("[page_mempool] shrink_to_fit: (1) get_header failed for page_index: %zu. allocator name: %s\n", page_index, name);
        return;
    }
    // Insert the freed pages into the sorted free list and merge them with the following free run.
    unsigned int* link = &first_free_index;
    for (unsigned int cur_index = first_free_index; cur_index <= page_index; cur_index = *link)
    {
        page_mempool_header* cur = get_header(*link);
        if (cur == nullptr)
        {
            return;
        }
        link = &cur->next_index;
    }
    free_run->num_pages = freed;
    free_run->next_index = *link;
    *link = page_index;
    if (page_index + free_run->num_pages != free_run->next_index)
    {
        return;
    }
    page_mempool_header* next = get_header(free_run->next_index);
    if (next == nullptr)
    {
        keen::debug::nativePrintArgs("[page_mempool] shrink_to_fit: (2) get_header failed for page_header->next_index: %zu. allocator name: %s\n", free_run->next_index, name);
        return;
    }
    free_run->num_pages = next->num_pages + freed;
    free_run->next_index = next->next_index;
}

// 0x005897B0 (:488)
void page_mempool::release_bytes(void* mem)
{
    if (mem == nullptr)
    {
        return;
    }
    const unsigned int page_index = get_page_index(mem);
    page_mempool_header* header = get_header(page_index);
    if (header == nullptr)
    {
        return;
    }
    current_pos -= header->num_pages;
    // prev: the last free run whose successor lies behind page_index (none when the free list is empty).
    unsigned int prev_index = first_free_index;
    page_mempool_header* prev = nullptr;
    for (unsigned int cur_index = first_free_index; (prev_index = cur_index) < max_pages;)
    {
        prev = get_header(prev_index);
        if (prev == nullptr)
        {
            break;
        }
        cur_index = prev->next_index;
        if (page_index < cur_index)
        {
            break;
        }
        prev = nullptr;
    }
    if (page_index < prev_index)
    {
        header->next_index = first_free_index;
        first_free_index = page_index;
    }
    else
    {
        header->next_index = prev->next_index;
        prev->next_index = page_index;
    }
    if (header->num_pages + page_index == header->next_index)
    {
        page_mempool_header* next = get_header(header->next_index);
        header->num_pages += next->num_pages;
        header->next_index = next->next_index;
    }
    if (prev != nullptr && prev->num_pages + prev_index == page_index)
    {
        prev->num_pages += header->num_pages;
        prev->next_index = header->next_index;
    }
}

// 0x00589880 (:545)
bool page_mempool::clear()
{
    memset(pool, 0, max_size);
    current_pos = 0;
    first_free_index = 0;
    // One free run over all pages (a null write when there are none, see the file comment).
    page_mempool_header* header = get_header(0);
    header->num_pages = max_pages;
    header->next_index = 0xFFFFFFFF;
    return true;
}

// 0x005898D0 (:564)
bool page_mempool::debug_get_next_run(int& num_pages, bool& in_use)
{
    // Walks the runs one call at a time; the position is kept in function statics (0x01D579A0, 0x0163EB14).
    static unsigned int cur_index = 0;
    static unsigned int next_free_block = 0xFFFFFFFF;
    if (cur_index >= max_pages)
    {
        cur_index = 0;
        return false;
    }
    if (next_free_block == 0xFFFFFFFF)
    {
        next_free_block = first_free_index;
    }
    if (next_free_block == cur_index)
    {
        page_mempool_header* header = (page_mempool_header*)(pool + page_size * cur_index);
        num_pages = header->num_pages;
        in_use = false;
        next_free_block = header->next_index;
    }
    else if (next_free_block < cur_index)
    {
        num_pages = max_pages - cur_index;
        in_use = true;
    }
    else
    {
        num_pages = next_free_block - cur_index;
        in_use = true;
    }
    cur_index += num_pages;
    return true;
}

// 0x005AB3B0 (.h:125)
unsigned int page_mempool::mark()
{
    VLIB_CRITICAL_FAILURE();
    return 0;
}

// 0x005AB3D0 (.h:126)
bool page_mempool::restore_to_mark(unsigned int pos)
{
    VLIB_CRITICAL_FAILURE();
    return false;
}

// 0x005AB3F0 (.h:127)
void* page_mempool::alloc_all(unsigned int align)
{
    VLIB_CRITICAL_FAILURE();
    return nullptr;
}

// 0x005AB410 (.h:128)
bool page_mempool::pad_to_page(unsigned int _page_size)
{
    VLIB_CRITICAL_FAILURE();
    return false;
}

// 0x005AB430 (.h:129)
bool page_mempool::release_bytes(unsigned int num_bytes)
{
    VLIB_CRITICAL_FAILURE();
    return false;
}

// 0x005AB450 (.h:132)
bool page_mempool::instantiable()
{
    return true;
}
