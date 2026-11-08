// Original: volition/vlib/code/memory/split_memmgr.cpp
//
// Partial: the console commands dump_dynamic_mem_usage / dump_dynamic_mem_stats (unused_dcf_*, not declared in the
// header) are not implemented. dump_mem_usage gathers the requested bytes per allocation name into a temporary table
// from Vlib_temp_mempool, sorts the names by extension and buckets the sizes by powers of two, then prints nothing in
// this build; it is a no-op here (it only runs when an allocation fails).
//
// Layout (decompiled): the tiny buckets take the end of the managed memory, the first bucket at the very end
// (tiny_buf = phys_buf + size - total_tiny_space after it); the large blocks cover the rest from phys_buf. An
// allocation goes to the first tiny bucket whose largest_alloc fits size + align and that can_alloc it, otherwise to
// the best-fit free block (can_alloc caches the block it found in cached_query for the next alloc). Pointers handed
// out are keyed in memblk_hash to find their block on free.
//
// Notes (decompiled behaviour, kept as is):
// - The tiny bucket array is allocated from the first bucket's hash_pool.
// - When a tiny bucket accepts a request but its alloc fails, the large blocks are tried.
// - space_free does not count alignment padding inside large blocks.

#include "volition/vlib/code/memory/split_memmgr.h"

#include "volition/vlib/code/memory/vmemory.h"

#include <new>
#include <string.h>

static split_memmgr* Mgr_root; // 0x01CE86F4, list of all split_memmgr instances (prev/next)

// 0x005CF7A0
split_memmgr::tiny_bucket::tiny_bucket()
{
}

// 0x005CB530 (:92)
split_memmgr::split_memmgr()
    : cs(0)
{
}

// 0x005C8000 (:98)
split_memmgr::split_memmgr(const char* _name)
    : cs(0)
{
    name = _name;
    if (Mgr_root == nullptr)
    {
        Mgr_root = this;
        next = this;
        prev = this;
        return;
    }
    // New managers go to the front of the list.
    split_memmgr* const old_root = Mgr_root;
    Mgr_root = this;
    old_root->prev->next = this;
    prev = old_root->prev;
    old_root->prev = this;
    next = old_root;
}

// 0x005C4400 (:107)
split_memmgr::~split_memmgr()
{
    if (Mgr_root == this && (Mgr_root = next) == this)
    {
        Mgr_root = nullptr;
    }
    else
    {
        if (prev != nullptr)
        {
            prev->next = next;
        }
        if (next != nullptr)
        {
            next->prev = prev;
        }
    }
    prev = nullptr;
    next = nullptr;
}

// 0x005CFFF0 (:130)
void split_memmgr::init(void* phys_buf, unsigned int size, int num_tiny_buckets, tiny_bucket_info* tiny_buckets_init, mempool& large_header_pool, unsigned int max_large_allocations, mempool& large_hash_pool, unsigned int large_hash_entries)
{
    mem_base = phys_buf;
    mem_size = size;
    mem_used = 0;
    mem_requested = 0;
    total_tiny_space = 0;
    if (num_tiny_buckets > 0)
    {
        num_buckets = num_tiny_buckets;
        buckets = (tiny_bucket*)tiny_buckets_init->hash_pool->alloc(num_tiny_buckets * sizeof(tiny_bucket), 0);
        for (int i = 0; i < num_tiny_buckets; i++)
        {
            const tiny_bucket_info& info = tiny_buckets_init[i];
            tiny_bucket* const bucket = &buckets[i];
            new (&bucket->tiny_pool) split_page_mempool();
            bucket->tiny_buf = (unsigned char*)phys_buf + size - info.size - total_tiny_space;
            total_tiny_space += info.size;
            bucket->tiny_size = info.size;
            bucket->tiny_page_size = info.page_size;
            bucket->largest_alloc = info.largest_alloc;

            char pool_name[32];
            strncpy(pool_name, name, sizeof(pool_name));
            pool_name[31] = '\0';
            const unsigned int room = 31 - (unsigned int)strlen(pool_name);
            strncat(pool_name, " tiny pool", room);
            pool_name[31] = '\0';
            bucket->tiny_pool.init(bucket->tiny_buf, bucket->tiny_size, bucket->tiny_page_size, pool_name, *info.hash_pool, info.num_hash_entries, 1, 0x20);
        }
    }

    memblk_pool.init(large_header_pool, max_large_allocations);
    memblk_hash.init(large_hash_pool, large_hash_entries);
    memblk_pool.thread_access.enter();
    for (split_memblk* blk = memblk_pool.get_first(0); blk != nullptr; blk = memblk_pool.free(blk))
    {
    }
    memblk_pool.thread_access.leave();
    memblk_hash.clear();

    // One free block covers the large-allocation area.
    tree_root = memblk_pool.alloc(0);
    const unsigned int large_size = size - total_tiny_space;
    tree_root->info.size = large_size;
    tree_root->info.usable_size = large_size;
    tree_root->mem_base = (char*)phys_buf;
    tree_root->mem_aligned = (char*)phys_buf;
    tree_root->info.requested_size = 0;
    tree_root->prev = nullptr;
    tree_root->next = nullptr;
    tree_root->top = nullptr;
    tree_root->left = nullptr;
    tree_root->right = nullptr;
    tree_root->track = nullptr;
    last_free = tree_root;
    cached_query = nullptr;
}

// 0x005BC9D0 (:210)
void split_memmgr::destroy()
{
}

// 0x00589DE0 (:220)
const char* split_memmgr::get_name()
{
    return name;
}

// 0x005ABF30 (:226)
bool split_memmgr::can_alloc(unsigned int size, unsigned int align)
{
    cs.enter();
    cached_query = nullptr;
    if (align < 4)
    {
        align = 4;
    }
    for (int i = 0; i < num_buckets; i++)
    {
        if (size + align <= buckets[i].largest_alloc && buckets[i].tiny_pool.can_alloc(size + align, 0))
        {
            cs.leave();
            return true;
        }
    }

    // last_free is the root of the free tree.
    split_memblk* found = last_free;
    const unsigned int root_size = found->info.size;
    if ((((unsigned int)found->mem_base & (align - 1)) == 0 && size <= root_size) || size + align <= root_size)
    {
        if (found->left != nullptr)
        {
            split_memblk* const smaller = found->left->can_alloc(size, align);
            if (smaller != nullptr && smaller->info.size < root_size)
            {
                found = smaller;
            }
        }
    }
    else
    {
        found = found->right != nullptr ? found->right->can_alloc(size, align) : nullptr;
    }
    cached_query = found;
    cs.leave();
    return found != nullptr;
}

// 0x00589DF0 (:258)
bool split_memmgr::is_free()
{
    return tree_root->track == nullptr && tree_root->next == nullptr;
}

// 0x005CB5C0 (:264)
void* split_memmgr::alloc(unsigned int size, const char* debug_name, unsigned int align)
{
    cs.enter();
    if (size == 0)
    {
        cs.leave();
        return nullptr;
    }
    if (align < 4)
    {
        align = 4;
    }
    for (int i = 0; i < num_buckets; i++)
    {
        tiny_bucket& bucket = buckets[i];
        if (size + align <= bucket.largest_alloc && bucket.tiny_pool.can_alloc(size + align, 0))
        {
            void* const mem = bucket.tiny_pool.alloc(size, align);
            if (mem != nullptr)
            {
                cs.leave();
                return mem;
            }
            break;
        }
    }

    split_memblk* blk = cached_query;
    if (blk == nullptr || ((((unsigned int)blk->mem_base & (align - 1)) != 0 || blk->info.size < size) && blk->info.size < size + align))
    {
        blk = last_free->can_alloc(size, align);
    }
    cached_query = nullptr;
    if (blk == nullptr)
    {
        dump_mem_usage();
        cs.leave();
        return nullptr;
    }

    char* mem = blk->mem_base;
    unsigned int alloc_size = size;
    const unsigned int misalign = (align - 1) & (unsigned int)mem;
    if (misalign != 0)
    {
        mem += align - misalign;
        blk->mem_aligned = mem;
        alloc_size = size - misalign + align;
    }
    blk->alloc(alloc_size, last_free, memblk_pool);
    blk->info.requested_size = size;
    mem_requested += size;
    mem_used += blk->info.size;
    blk->info.usable_size = (unsigned int)(blk->mem_base + blk->info.size - mem);
    if (debug_name == nullptr)
    {
        debug_name = "untracked";
    }
    blk->track = debug_name;
    if (!memblk_hash.insert((unsigned int)mem, blk))
    {
        dump_mem_usage();
    }
    cs.leave();
    return mem;
}

// 0x005C80E0 (:371)
void split_memmgr::free(void* p)
{
    cs.enter();
    if (p == nullptr)
    {
        cs.leave();
        return;
    }
    for (int i = 0; i < num_buckets; i++)
    {
        tiny_bucket& bucket = buckets[i];
        if (bucket.tiny_buf <= p && p < (unsigned char*)bucket.tiny_buf + bucket.tiny_size)
        {
            bucket.tiny_pool.release_bytes(p);
            cs.leave();
            return;
        }
    }
    split_memblk* const blk = memblk_hash.get_and_remove((unsigned int)p);
    if (blk != nullptr)
    {
        mem_used -= blk->info.size;
        mem_requested -= blk->info.requested_size;
        blk->free(last_free, memblk_pool);
    }
    cs.leave();
}

// 0x005CB740 (:435)
void split_memmgr::clear()
{
    cs.enter();
    mem_used = 0;
    mem_requested = 0;
    for (int i = 0; i < num_buckets; i++)
    {
        buckets[i].tiny_pool.clear();
    }
    memblk_pool.thread_access.enter();
    for (split_memblk* blk = memblk_pool.get_first(0); blk != nullptr; blk = memblk_pool.free(blk))
    {
    }
    memblk_pool.thread_access.leave();
    memblk_hash.clear();

    tree_root = memblk_pool.alloc(0);
    const unsigned int large_size = mem_size - total_tiny_space;
    tree_root->info.size = large_size;
    tree_root->info.usable_size = large_size;
    tree_root->info.requested_size = 0;
    tree_root->prev = nullptr;
    tree_root->next = nullptr;
    tree_root->top = nullptr;
    tree_root->left = nullptr;
    tree_root->right = nullptr;
    tree_root->track = nullptr;
    tree_root->mem_base = (char*)mem_base;
    tree_root->mem_aligned = (char*)mem_base;
    last_free = tree_root;
    cached_query = nullptr;
    cs.leave();
}

// 0x005C8180 (:474)
split_meminfo split_memmgr::get_info(void* p)
{
    split_meminfo info;
    const split_memblk* const blk = memblk_hash.get((unsigned int)p);
    if (blk != nullptr)
    {
        info = blk->info;
    }
    else
    {
        info.size = 0;
        info.usable_size = 0;
        info.requested_size = 0;
    }
    return info;
}

// 0x007B98D0 (.h:151)
void* split_memmgr::large_mem_base()
{
    return mem_base;
}

// 0x00589E10 (:490)
unsigned int split_memmgr::space_max()
{
    return mem_size;
}

// 0x005AC020 (:496)
unsigned int split_memmgr::space_used()
{
    return tiny_space_used() + mem_used;
}

// 0x005AC030 (:502)
unsigned int split_memmgr::space_free()
{
    return tiny_space_free() + (mem_size - total_tiny_space - mem_used);
}

// 0x005AC050 (:508)
unsigned int split_memmgr::space_wasted()
{
    return tiny_space_wasted() + (mem_used - mem_requested);
}

// 0x005AC070 (:518)
unsigned int split_memmgr::space_requested()
{
    return tiny_space_used() + mem_requested;
}

// 0x005AC080 (:524)
unsigned int split_memmgr::largest_allocation_possible()
{
    cs.enter();
    split_memblk* blk = last_free;
    if (blk == nullptr)
    {
        cs.leave();
        return 0;
    }
    while (blk->right != nullptr)
    {
        blk = blk->right;
    }
    const unsigned int size = blk->info.usable_size;
    cs.leave();
    return size;
}

// 0x00589E20 (:541)
unsigned int split_memmgr::tiny_space_used()
{
    unsigned int total = 0;
    for (int i = 0; i < num_buckets; i++)
    {
        total += buckets[i].tiny_pool.space_used();
    }
    return total;
}

// 0x00589E70 (:551)
unsigned int split_memmgr::tiny_space_free()
{
    unsigned int total = 0;
    for (int i = 0; i < num_buckets; i++)
    {
        total += buckets[i].tiny_pool.space_free();
    }
    return total;
}

// 0x007B98E0 (.h:174)
unsigned int split_memmgr::tiny_space_max()
{
    return total_tiny_space;
}

// 0x00589EC0 (:562)
unsigned int split_memmgr::tiny_space_wasted()
{
    unsigned int total = 0;
    for (int i = 0; i < num_buckets; i++)
    {
        total += buckets[i].tiny_pool.space_wasted();
    }
    return total;
}

// 0x005C81D0 (:615)
void split_memmgr::dump_mem_usage()
{
    // See the file comment: the release build gathers statistics and prints nothing.
}

// 0x00589F10 (:717)
void split_memmgr::dump_mem_stats()
{
}

// 0x00589F20 (:805)
const split_memblk* split_memmgr::next_debug_blk(const split_memblk* blk)
{
    if (blk == nullptr)
    {
        return tree_root;
    }
    return blk->next;
}

// 0x007B98F0 (.h:191)
int split_memmgr::debug_num_tiny_pools()
{
    return num_buckets;
}

// 0x007B9900 (.h:192)
split_page_mempool& split_memmgr::debug_tiny_pool(int n)
{
    return buckets[n].tiny_pool;
}
