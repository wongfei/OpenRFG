/*
 * Original: library/modules/thirdparty/tlsf/tlsf.c (third party: Two-Level Segregated Fit allocator by Matthew Conte,
 * version 2.x; recovered from rfg.exe)
 *
 * Constants of this build (32-bit): ALIGN_SIZE 4, SL_INDEX_COUNT 32, FL_INDEX_MAX 30 (blocks up to 1 GB),
 * FL_INDEX_COUNT 24, SMALL_BLOCK_SIZE 128. sizeof(pool_t) is 0xC74 and tlsf_overhead() 0xC7C. The helpers are file
 * static; several used LTCG register arguments in the original (noted per function), which does not change behaviour.
 */

#include "tlsf.h"
#include "tlsfbits.h"

#include <stdio.h>
#include <string.h>

enum tlsf_public
{
    SL_INDEX_COUNT_LOG2 = 5,
};

enum tlsf_private
{
    ALIGN_SIZE_LOG2 = 2,
    ALIGN_SIZE = (1 << ALIGN_SIZE_LOG2),
    FL_INDEX_MAX = 30,
    SL_INDEX_COUNT = (1 << SL_INDEX_COUNT_LOG2),
    FL_INDEX_SHIFT = (SL_INDEX_COUNT_LOG2 + ALIGN_SIZE_LOG2),
    FL_INDEX_COUNT = (FL_INDEX_MAX - FL_INDEX_SHIFT + 1),
    SMALL_BLOCK_SIZE = (1 << FL_INDEX_SHIFT),
};

/* Physical block header. prev_phys_block is only valid when the previous block is free; it overlaps the last word of
 * the previous block. size holds two flags in its low bits: bit 0 free, bit 1 previous block free. */
typedef struct block_header_t
{
    struct block_header_t* prev_phys_block;
    size_t size;
    struct block_header_t* next_free;
    struct block_header_t* prev_free;
} block_header_t;

static const size_t block_header_free_bit = 1 << 0;
static const size_t block_header_prev_free_bit = 1 << 1;
static const size_t block_header_overhead = sizeof(size_t);
static const size_t block_start_offset = offsetof(block_header_t, size) + sizeof(size_t);
static const size_t block_size_min = sizeof(block_header_t) - sizeof(block_header_t*);
static const size_t block_size_max = (size_t)1 << FL_INDEX_MAX;

typedef struct pool_t
{
    block_header_t block_null; /* empty list sentinel */
    unsigned int fl_bitmap;
    unsigned int sl_bitmap[FL_INDEX_COUNT];
    block_header_t* blocks[FL_INDEX_COUNT][SL_INDEX_COUNT];
} pool_t;

/* 0x004010F0 (:179) */
static size_t block_size(const block_header_t* block)
{
    return block->size & ~(block_header_free_bit | block_header_prev_free_bit);
}

/* 0x00401100 (:184), LTCG (block in EAX) */
static void block_set_size(block_header_t* block, size_t size)
{
    const size_t oldsize = block->size;
    block->size = size | (oldsize & (block_header_free_bit | block_header_prev_free_bit));
}

/* 0x00401110 (:190) */
static int block_is_last(const block_header_t* block)
{
    return block_size(block) == 0;
}

/* 0x00401120 (:195) */
static int block_is_free(const block_header_t* block)
{
    return (int)(block->size & block_header_free_bit);
}

/* 0x00401130 (:200) */
static void block_set_free(block_header_t* block)
{
    block->size |= block_header_free_bit;
}

/* 0x00401140 (:205) */
static void block_set_used(block_header_t* block)
{
    block->size &= ~block_header_free_bit;
}

/* 0x00401150 (:210) */
static int block_is_prev_free(const block_header_t* block)
{
    return (int)(block->size & block_header_prev_free_bit);
}

/* 0x00401160 (:215) */
static void block_set_prev_free(block_header_t* block)
{
    block->size |= block_header_prev_free_bit;
}

/* 0x00401170 (:220) */
static void block_set_prev_used(block_header_t* block)
{
    block->size &= ~block_header_prev_free_bit;
}

/* 0x00401180 (:225) */
static block_header_t* block_from_ptr(const void* ptr)
{
    return (block_header_t*)((unsigned char*)ptr - block_start_offset);
}

/* 0x00401190 (:231) */
static void* block_to_ptr(const block_header_t* block)
{
    return (void*)((unsigned char*)block + block_start_offset);
}

/* 0x004011A0 (:238), LTCG (ptr EAX, size ECX) */
static block_header_t* offset_to_block(const void* ptr, size_t size)
{
    return (block_header_t*)((unsigned char*)ptr + size);
}

/* 0x004011B0 (:244) */
static block_header_t* block_prev(const block_header_t* block)
{
    return block->prev_phys_block;
}

/* 0x004011C0 (:250) */
static block_header_t* block_next(const block_header_t* block)
{
    return offset_to_block(block_to_ptr(block), block_size(block) - block_header_overhead);
}

/* 0x004011D0 (:259) */
static block_header_t* block_link_next(block_header_t* block)
{
    block_header_t* next = block_next(block);
    next->prev_phys_block = block;
    return next;
}

/* 0x004011E0 (:266) */
static void block_mark_as_free(block_header_t* block)
{
    block_header_t* next = block_link_next(block);
    block_set_prev_free(next);
    block_set_free(block);
}

/* 0x00401200 (:274) */
static void block_mark_as_used(block_header_t* block)
{
    block_header_t* next = block_next(block);
    block_set_prev_used(next);
    block_set_used(block);
}

/* 0x00401220 (:281), LTCG (x EAX, align ECX) */
static size_t align_up(size_t x, size_t align)
{
    return (x + (align - 1)) & ~(align - 1);
}

/* 0x00401230 (:287), LTCG (x EAX, align ECX) */
static size_t align_down(size_t x, size_t align)
{
    return x - (x & (align - 1));
}

/* 0x00401240 (:293), LTCG (ptr EAX, align ECX) */
static void* align_ptr(const void* ptr, size_t align)
{
    return (void*)(((size_t)ptr + (align - 1)) & ~(align - 1));
}

/* 0x00401250 (:305), LTCG (size ECX, align EDX). Rounds a request up to align and the minimum block size; 0 for
 * requests of 0 or of 1 GB and more. */
static size_t adjust_request_size(size_t size, size_t align)
{
    size_t adjust = 0;
    if (size && size < block_size_max)
    {
        const size_t aligned = align_up(size, align);
        adjust = aligned < block_size_min ? block_size_min : aligned;
    }
    return adjust;
}

/* 0x00401280 (:321), LTCG (size EAX, fli EDI, sli ESI) */
static void mapping_insert(size_t size, int* fli, int* sli)
{
    int fl, sl;
    if (size < SMALL_BLOCK_SIZE)
    {
        fl = 0;
        sl = (int)size / (SMALL_BLOCK_SIZE / SL_INDEX_COUNT);
    }
    else
    {
        fl = tlsf_fls((unsigned int)size);
        sl = (int)(size >> (fl - SL_INDEX_COUNT_LOG2)) ^ (1 << SL_INDEX_COUNT_LOG2);
        fl -= (FL_INDEX_SHIFT - 1);
    }
    *fli = fl;
    *sli = sl;
}

/* 0x004012B0 (:341), LTCG (size EAX, fli EDI, sli ESI). Rounds up to the next list so any block found fits. */
static void mapping_search(size_t size, int* fli, int* sli)
{
    if (size >= (1 << SL_INDEX_COUNT_LOG2))
    {
        const size_t round = (1 << (tlsf_fls((unsigned int)size) - SL_INDEX_COUNT_LOG2)) - 1;
        size += round;
    }
    mapping_insert(size, fli, sli);
}

/* 0x00401300 (:351), LTCG (pool EDX, fli ESI, sli EDI) */
static block_header_t* search_suitable_block(pool_t* pool, int* fli, int* sli)
{
    int fl = *fli;
    int sl = *sli;
    unsigned int sl_map = pool->sl_bitmap[fl] & (~0u << sl);
    if (!sl_map)
    {
        const unsigned int fl_map = pool->fl_bitmap & (~0u << (fl + 1));
        if (!fl_map)
        {
            return 0;
        }
        fl = tlsf_ffs(fl_map);
        *fli = fl;
        sl_map = pool->sl_bitmap[fl];
    }
    sl = tlsf_ffs(sl_map);
    *sli = sl;
    return pool->blocks[fl][sl];
}

/* 0x00401350 (:384), LTCG (pool EDX, block ESI, fl in the first stack slot, sl ECX) */
static void remove_free_block(pool_t* pool, block_header_t* block, int fl, int sl)
{
    block_header_t* prev = block->prev_free;
    block_header_t* next = block->next_free;
    next->prev_free = prev;
    prev->next_free = next;

    if (pool->blocks[fl][sl] == block)
    {
        pool->blocks[fl][sl] = next;
        if (next == &pool->block_null)
        {
            pool->sl_bitmap[fl] &= ~(1u << sl);
            if (!pool->sl_bitmap[fl])
            {
                pool->fl_bitmap &= ~(1u << fl);
            }
        }
    }
}

/* 0x004013A0 (:413), LTCG (block ECX, fl EDX) */
static void insert_free_block(pool_t* pool, block_header_t* block, int fl, int sl)
{
    block_header_t* current = pool->blocks[fl][sl];
    block->next_free = current;
    block->prev_free = &pool->block_null;
    current->prev_free = block;
    pool->blocks[fl][sl] = block;
    pool->fl_bitmap |= (1u << fl);
    pool->sl_bitmap[fl] |= (1u << sl);
}

/* 0x004013E0 (:434), LTCG (block EDI) */
static void block_remove(pool_t* pool, block_header_t* block)
{
    int fl, sl;
    mapping_insert(block_size(block), &fl, &sl);
    remove_free_block(pool, block, fl, sl);
}

/* 0x00401460 (:442), LTCG (pool EDI) */
static void block_insert(pool_t* pool, block_header_t* block)
{
    int fl, sl;
    mapping_insert(block_size(block), &fl, &sl);
    insert_free_block(pool, block, fl, sl);
}

/* 0x004014D0 (:449), LTCG (block EAX, size EDX) */
static int block_can_split(block_header_t* block, size_t size)
{
    return block_size(block) >= sizeof(block_header_t) + size;
}

/* 0x004014E0 (:455), LTCG (block ECX, size EDX) */
static block_header_t* block_split(block_header_t* block, size_t size)
{
    block_header_t* remaining = offset_to_block(block_to_ptr(block), size - block_header_overhead);
    const size_t remain_size = block_size(block) - (size + block_header_overhead);
    block_set_size(remaining, remain_size);
    block_set_size(block, size);
    block_mark_as_free(remaining);
    return remaining;
}

/* 0x00401520 (:477), LTCG (prev EAX, block ECX) */
static block_header_t* block_absorb(block_header_t* prev, block_header_t* block)
{
    prev->size += block_size(block) + block_header_overhead;
    block_link_next(prev);
    return prev;
}

/* 0x00401540 (:487), LTCG */
static block_header_t* block_merge_prev(pool_t* pool, block_header_t* block)
{
    if (block_is_prev_free(block))
    {
        block_header_t* prev = block_prev(block);
        block_remove(pool, prev);
        block = block_absorb(prev, block);
    }
    return block;
}

/* 0x00401570 (:502), LTCG */
static block_header_t* block_merge_next(pool_t* pool, block_header_t* block)
{
    block_header_t* next = block_next(block);
    if (block_is_free(next))
    {
        block_remove(pool, next);
        block = block_absorb(block, next);
    }
    return block;
}

/* 0x004015B0 (:518), LTCG (block EDX, size EAX, pool EDI) */
static void block_trim_free(pool_t* pool, block_header_t* block, size_t size)
{
    if (block_can_split(block, size))
    {
        block_header_t* remaining_block = block_split(block, size);
        block_link_next(block);
        block_set_prev_free(remaining_block);
        block_insert(pool, remaining_block);
    }
}

/* 0x00401620 (:531), LTCG (block EDX, size ECX) */
static void block_trim_used(pool_t* pool, block_header_t* block, size_t size)
{
    if (block_can_split(block, size))
    {
        block_header_t* remaining_block = block_split(block, size);
        block_set_prev_used(remaining_block);
        remaining_block = block_merge_next(pool, remaining_block);
        block_insert(pool, remaining_block);
    }
}

/* 0x004016B0 (:545), LTCG (block EAX, size EDX) */
static block_header_t* block_trim_free_leading(pool_t* pool, block_header_t* block, size_t size)
{
    block_header_t* remaining_block = block;
    if (block_can_split(block, size))
    {
        remaining_block = block_split(block, size - block_header_overhead);
        block_set_prev_free(remaining_block);
        block_link_next(block);
        block_insert(pool, block);
    }
    return remaining_block;
}

/* 0x00401720 (:561), LTCG (size EAX, pool EDI) */
static block_header_t* block_locate_free(pool_t* pool, size_t size)
{
    int fl = 0, sl = 0;
    block_header_t* block = 0;
    if (size)
    {
        mapping_search(size, &fl, &sl);
        block = search_suitable_block(pool, &fl, &sl);
    }
    if (block)
    {
        remove_free_block(pool, block, fl, sl);
    }
    return block;
}

/* 0x00401820 (:581) */
static void* block_prepare_used(pool_t* pool, block_header_t* block, size_t size)
{
    void* p = 0;
    if (block)
    {
        block_trim_free(pool, block, size);
        block_mark_as_used(block);
        p = block_to_ptr(block);
    }
    return p;
}

/* 0x00401850 (:594) */
static void pool_construct(pool_t* pool)
{
    int i, j;
    pool->block_null.next_free = &pool->block_null;
    pool->block_null.prev_free = &pool->block_null;
    pool->fl_bitmap = 0;
    for (i = 0; i < FL_INDEX_COUNT; ++i)
    {
        pool->sl_bitmap[i] = 0;
        for (j = 0; j < SL_INDEX_COUNT; ++j)
        {
            pool->blocks[i][j] = &pool->block_null;
        }
    }
}

typedef struct integrity_t
{
    int prev_status;
    int status;
} integrity_t;

/* 0x004018E0 (:624) */
static void integrity_walker(void* ptr, size_t size, int used, void* user)
{
    block_header_t* block = block_from_ptr(ptr);
    integrity_t* integ = (integrity_t*)user;
    const int this_prev_status = block_is_prev_free(block) ? 1 : 0;
    const int this_status = block_is_free(block) ? 1 : 0;
    const size_t this_block_size = block_size(block);
    int status = 0;
    (void)used;
    if (integ->prev_status != this_prev_status)
    {
        status--;
    }
    if (size != this_block_size)
    {
        status--;
    }
    integ->prev_status = this_status;
    integ->status += status;
}

/* 0x00401D80 (:640). Returns 0 when the heap is consistent, otherwise the negated number of problems. */
int tlsf_check_heap(tlsf_pool tlsf)
{
    int i, j;
    pool_t* pool = (pool_t*)tlsf;
    int status = 0;
    integrity_t integ = { 0, 0 };
    tlsf_walk_heap(tlsf, integrity_walker, &integ);
    status = integ.status;

    for (i = 0; i < FL_INDEX_COUNT; ++i)
    {
        for (j = 0; j < SL_INDEX_COUNT; ++j)
        {
            const unsigned int fl_map = pool->fl_bitmap & (1u << i);
            const unsigned int sl_list = pool->sl_bitmap[i];
            const unsigned int sl_map = sl_list & (1u << j);
            const block_header_t* block = pool->blocks[i][j];

            /* Check that first- and second-level lists agree. */
            if (!fl_map)
            {
                if (sl_map)
                {
                    status--;
                }
            }
            if (!sl_map)
            {
                if (block != &pool->block_null)
                {
                    status--;
                }
                continue;
            }

            /* Check that there is at least one free block. */
            if (!sl_list)
            {
                status--;
            }
            if (block == &pool->block_null)
            {
                status--;
            }
            while (block != &pool->block_null)
            {
                int fli, sli;
                if (!block_is_free(block))
                {
                    status--;
                }
                if (block_is_prev_free(block))
                {
                    status--;
                }
                if (block_is_free(block_next(block)))
                {
                    status--;
                }
                if (!block_is_prev_free(block_next(block)))
                {
                    status--;
                }
                if (block_size(block) < block_size_min)
                {
                    status--;
                }
                mapping_insert(block_size(block), &fli, &sli);
                if (fli != i || sli != j)
                {
                    status--;
                }
                block = block->next_free;
            }
        }
    }
    return status;
}

/* 0x00401920 (:699) */
static void default_walker(void* ptr, size_t size, int used, void* user)
{
    (void)user;
    printf("\t%p %s size: %x (%p)\n", ptr, used ? "used" : "free", (unsigned int)size, (void*)block_from_ptr(ptr));
}

/* 0x00401950 (:705) */
void tlsf_walk_heap(tlsf_pool pool, tlsf_walker walker, void* user)
{
    tlsf_walker heap_walker = walker ? walker : default_walker;
    block_header_t* block = offset_to_block(pool, sizeof(pool_t) - block_header_overhead);
    while (block && !block_is_last(block))
    {
        heap_walker(block_to_ptr(block), block_size(block), !block_is_free(block), user);
        block = block_next(block);
    }
}

/* 0x004019A0 (:722) */
size_t tlsf_block_size(void* ptr)
{
    size_t size = 0;
    if (ptr)
    {
        const block_header_t* block = block_from_ptr(ptr);
        size = block_size(block);
    }
    return size;
}

/* 0x004019C0 (:738) */
size_t tlsf_overhead(void)
{
    const size_t pool_overhead = sizeof(pool_t) + 2 * block_header_overhead;
    return pool_overhead;
}

/* 0x004019D0 (:748) */
tlsf_pool tlsf_create(void* mem, size_t bytes)
{
    block_header_t* block;
    block_header_t* next;
    const size_t pool_overhead = tlsf_overhead();
    const size_t pool_bytes = align_down(bytes - pool_overhead, ALIGN_SIZE);
    pool_t* pool = (pool_t*)mem;

    if (pool_bytes < block_size_min || pool_bytes > block_size_max)
    {
        printf("tlsf_create: Pool size must be between %u and %u bytes.\n", (unsigned int)(pool_overhead + block_size_min), (unsigned int)(pool_overhead + block_size_max));
        return 0;
    }

    /* Construct a valid pool object. */
    pool_construct(pool);

    /* Create the main free block. Offset the start of the block slightly so that the prev_phys_block field falls
     * inside of the pool structure - it will never be used. */
    block = offset_to_block((void*)pool, sizeof(pool_t) - block_header_overhead);
    block_set_size(block, pool_bytes);
    block_set_free(block);
    block_set_prev_used(block);
    block_insert(pool, block);

    /* Split the block to create a zero-size pool sentinel block. */
    next = block_link_next(block);
    block_set_size(next, 0);
    block_set_used(next);
    block_set_prev_free(next);
    return mem;
}

/* 0x00401A50 (:818) */
void tlsf_destroy(tlsf_pool pool)
{
    /* Nothing to do. */
    (void)pool;
}

/* 0x00401A60 (:824) */
void* tlsf_malloc(tlsf_pool tlsf, size_t size)
{
    pool_t* pool = (pool_t*)tlsf;
    const size_t adjust = adjust_request_size(size, ALIGN_SIZE);
    block_header_t* block = block_locate_free(pool, adjust);
    return block_prepare_used(pool, block, adjust);
}

/* 0x00401AC0 (:832) */
void* tlsf_memalign(tlsf_pool tlsf, size_t align, size_t size)
{
    pool_t* pool = (pool_t*)tlsf;
    const size_t adjust = adjust_request_size(size, ALIGN_SIZE);

    /* We must allocate an additional minimum block size bytes so that if our free block will leave an alignment gap
     * which is smaller, we can trim a leading free block and release it back to the heap. */
    const size_t gap_minimum = sizeof(block_header_t);
    const size_t size_with_gap = adjust_request_size(adjust + align + gap_minimum, align);

    /* If alignment is less than or equals base alignment, we're done. */
    const size_t aligned_size = (align <= ALIGN_SIZE) ? adjust : size_with_gap;

    block_header_t* block = block_locate_free(pool, aligned_size);

    if (block)
    {
        void* ptr = block_to_ptr(block);
        void* aligned = align_ptr(ptr, align);
        size_t gap = (size_t)((unsigned char*)aligned - (unsigned char*)ptr);

        /* If gap size is too small, offset to next aligned boundary. */
        if (gap && gap < gap_minimum)
        {
            const size_t gap_remain = gap_minimum - gap;
            const size_t offset = gap_remain > align ? gap_remain : align;
            const void* next_aligned = (void*)((unsigned char*)aligned + offset);
            aligned = align_ptr(next_aligned, align);
            gap = (size_t)((unsigned char*)aligned - (unsigned char*)ptr);
        }

        if (gap)
        {
            block = block_trim_free_leading(pool, block, gap);
        }
    }

    return block_prepare_used(pool, block, adjust);
}

/* 0x00401BB0 (:886) */
void tlsf_free(tlsf_pool tlsf, void* ptr)
{
    /* Don't attempt to free a NULL pointer. */
    if (ptr)
    {
        pool_t* pool = (pool_t*)tlsf;
        block_header_t* block = block_from_ptr(ptr);
        block_mark_as_free(block);
        block = block_merge_prev(pool, block);
        block = block_merge_next(pool, block);
        block_insert(pool, block);
    }
}

/* 0x00401C50 (:913). realloc(0, size) allocates, realloc(ptr, 0) frees; a block is grown in place when the next block
 * is free and large enough, otherwise moved. */
void* tlsf_realloc(tlsf_pool tlsf, void* ptr, size_t size)
{
    pool_t* pool = (pool_t*)tlsf;
    void* p = 0;

    if (ptr && size == 0)
    {
        tlsf_free(tlsf, ptr);
    }
    else if (!ptr)
    {
        p = tlsf_malloc(tlsf, size);
    }
    else
    {
        block_header_t* block = block_from_ptr(ptr);
        block_header_t* next = block_next(block);

        const size_t cursize = block_size(block);
        const size_t combined = cursize + block_size(next) + block_header_overhead;
        const size_t adjust = adjust_request_size(size, ALIGN_SIZE);

        if (adjust > cursize && (!block_is_free(next) || adjust > combined))
        {
            p = tlsf_malloc(tlsf, size);
            if (p)
            {
                const size_t minsize = cursize < size ? cursize : size;
                memcpy(p, ptr, minsize);
                tlsf_free(tlsf, ptr);
            }
        }
        else
        {
            /* Do we need to expand to the next block? */
            if (adjust > cursize)
            {
                block_merge_next(pool, block);
                block_mark_as_used(block);
            }

            /* Trim the resulting block and return the original pointer. */
            block_trim_used(pool, block, adjust);
            p = ptr;
        }
    }

    return p;
}
