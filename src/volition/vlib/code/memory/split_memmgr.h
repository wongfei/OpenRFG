// Original: volition/vlib/code/memory/split_memmgr.h
// Implementation: volition/vlib/code/memory/split_memmgr.cpp
//
// General allocator for level data (Level_memmgr in rfg.h): small sizes go to "tiny buckets" (split_page_mempools of
// fixed page sizes), larger ones to a best-fit tree of split_memblk headers kept outside the managed memory
// (memblk_pool, memblk_hash by address). All instances are linked into a global list (Mgr_root) for the
// dump_dynamic_mem_* console commands. Placement of tiny_bucket_info inferred.
#pragma once

#include "volition/vlib/code/memory/split_memblk.h"
#include "volition/vlib/code/memory/split_page_mempool.h"
#include "volition/vlib/code/memory/object_pool.h"
#include "volition/vlib/code/util/hash_table.h"
#include "volition/vlib/code/os/cs_wrapper.h"

class mempool;

// size 0x14
struct tiny_bucket_info
{
    unsigned int size; // 0x00
    unsigned int page_size; // 0x04
    unsigned int largest_alloc; // 0x08
    mempool* hash_pool; // 0x0C
    unsigned int num_hash_entries; // 0x10
};

// size 0x100
class split_memmgr
{
public:
    // size 0xD0
    struct tiny_bucket
    {
        tiny_bucket(); // 0x005CF7A0

        void* tiny_buf; // 0x00
        unsigned int tiny_size; // 0x04
        unsigned int tiny_page_size; // 0x08
        unsigned int largest_alloc; // 0x0C
        split_page_mempool tiny_pool; // 0x10
    };

    split_memmgr(const char* _name); // 0x005C8000 (:98)
    split_memmgr(); // 0x005CB530 (:92)
    ~split_memmgr(); // 0x005C4400 (:107)
    void init(void* phys_buf, unsigned int size, int num_tiny_buckets, tiny_bucket_info* tiny_buckets_init, mempool& large_header_pool, unsigned int max_large_allocations, mempool& large_hash_pool, unsigned int large_hash_entries); // 0x005CFFF0 (:130)
    void destroy(); // 0x005BC9D0 (:210)
    const char* get_name(); // 0x00589DE0 (:220)
    bool can_alloc(unsigned int size, unsigned int align); // 0x005ABF30 (:226)
    bool is_free(); // 0x00589DF0 (:258)
    void* alloc(unsigned int size, const char* debug_name, unsigned int align); // 0x005CB5C0 (:264)
    void free(void* p); // 0x005C80E0 (:371)
    void clear(); // 0x005CB740 (:435)
    split_meminfo get_info(void* p); // 0x005C8180 (:474)
    void* large_mem_base(); // 0x007B98D0 (.h:151)
    unsigned int space_max(); // 0x00589E10 (:490)
    unsigned int space_used(); // 0x005AC020 (:496)
    unsigned int space_free(); // 0x005AC030 (:502)
    unsigned int space_wasted(); // 0x005AC050 (:508)
    unsigned int space_requested(); // 0x005AC070 (:518)
    unsigned int largest_allocation_possible(); // 0x005AC080 (:524)
    unsigned int tiny_space_used(); // 0x00589E20 (:541)
    unsigned int tiny_space_free(); // 0x00589E70 (:551)
    unsigned int tiny_space_max(); // 0x007B98E0 (.h:174)
    unsigned int tiny_space_wasted(); // 0x00589EC0 (:562)
    void dump_mem_usage(); // 0x005C81D0 (:615)
    void dump_mem_stats(); // 0x00589F10 (:717)
    const split_memblk* next_debug_blk(const split_memblk* blk); // 0x00589F20 (:805)
    int debug_num_tiny_pools(); // 0x007B98F0 (.h:191)
    split_page_mempool& debug_tiny_pool(int n); // 0x007B9900 (.h:192)

private:
    cs_wrapper cs; // 0x00
protected:
    const char* name; // 0x3C
    void* mem_base; // 0x40
    unsigned int mem_size; // 0x44
    unsigned int mem_used; // 0x48
    unsigned int mem_requested; // 0x4C
    unsigned int tiny_requested; // 0x50
private:
    split_memblk* tree_root; // 0x54
    split_memblk* cached_query; // 0x58
    split_memblk* last_free; // 0x5C
    dynamic_object_pool<split_memblk,1> memblk_pool; // 0x60
    dynamic_uint_hash_table<split_memblk*> memblk_hash; // 0xA0
    tiny_bucket* buckets; // 0xE8
    int num_buckets; // 0xEC
    unsigned int total_tiny_space; // 0xF0
public:
    split_memmgr* prev; // 0xF4
    split_memmgr* next; // 0xF8
};
