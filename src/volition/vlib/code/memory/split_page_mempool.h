// Original: volition/vlib/code/memory/split_page_mempool.h
// Implementation: volition/vlib/code/memory/split_page_mempool.cpp
//
// page_mempool variant that keeps its run headers outside the managed memory, in a hash table keyed by address
// (header_hash), so it can manage memory the CPU should not touch. Used for split_memmgr's tiny buckets.
#pragma once

#include "volition/vlib/code/memory/static_mempool.h"
#include "volition/vlib/code/util/hash_table.h"

class mempool;

// size 0x10
struct split_page_mempool_header
{
    unsigned int wasted_space; // 0x00
    unsigned int num_pages; // 0x04
    unsigned char* page_addr; // 0x08
    union
    {
        split_page_mempool_header* next_page; // 0x0C, free runs
        unsigned int align_offset; // 0x0C, used runs
    };
};

// size 0xC0
class split_page_mempool : public static_mempool_base
{
    unsigned int get_required_page_count(unsigned int size, unsigned int align, unsigned int& align_offset) const; // 0x0058A1A0 (:87)
    unsigned int get_align_page_count(unsigned char* page_addr, unsigned int align) const; // 0x0058A210 (:122)
    unsigned int get_page_index(const split_page_mempool_header* header) const; // 0x0058A1F0 (:111)
    split_page_mempool_header* get_header(unsigned int page_index); // 0x005C88E0 (:553)
public:
    split_page_mempool(const void* mem, unsigned int mem_size, unsigned int page_size, const char* pool_name, mempool& hash_pool, unsigned int max_alloc, unsigned int align, unsigned char mp_flags); // 0x005CBA90 (:37)
    split_page_mempool(); // 0x005CB890 (:28)
    virtual ~split_page_mempool(); // vtbl+0x40 0x005CBB50 (:45)
    virtual void init(const void* mem, unsigned int mem_size, unsigned int default_page_size, const char* pool_name, mempool& hash_pool, unsigned int max_alloc, unsigned int align, unsigned char mp_flags); // vtbl+0x50 0x005C8640 (:59)
    virtual void* get_base(); // vtbl+0x2C 0x005CB920 (.h:108)
    virtual unsigned int space_free() const; // vtbl+0x0C 0x005CB930 (.h:112)
    virtual unsigned int space_used() const; // vtbl+0x10 0x005CB940 (.h:113)
    virtual unsigned int space_max() const; // vtbl+0x14 0x005CB950 (.h:114)
    virtual unsigned int get_page_size() const; // vtbl+0x54 0x005CB960 (.h:115)
    virtual unsigned int free_list_size() const; // vtbl+0x58 0x005CB970 (.h:116)
    virtual unsigned int free_list_peak() const; // vtbl+0x5C 0x005CB980 (.h:117)
    virtual unsigned int space_wasted() const; // vtbl+0x60 0x005CB990 (.h:120)
    virtual unsigned int space_hash() const; // vtbl+0x64 0x005CBA70 (.h:122)
    virtual bool can_alloc(unsigned int size, unsigned int align) const; // vtbl+0x18 0x0058A250 (:139)
    virtual void* alloc(unsigned int size, unsigned int align); // vtbl+0x1C 0x005C8700 (:163)
    virtual void* realloc(void* mem, unsigned int new_size); // vtbl+0x20 0x005CBB70 (:260)
    virtual unsigned int size(const void* mem); // vtbl+0x68 0x005CBD00 (:359)
    virtual bool is_aligned(const void* mem); // vtbl+0x6C 0x005CBCD0 (:343)
    virtual void shrink_to_fit(void* mem, unsigned int actual_size); // vtbl+0x70 0x005CBD30 (:375)
    virtual bool release_bytes(unsigned int num_bytes); // vtbl+0x38 0x005CBA20 (.h:148)
    virtual void release_bytes(void* mem); // vtbl+0x74 0x005CBE80 (:453)
    virtual bool clear(); // vtbl+0x28 0x005CC000 (:561)
    void debug_reset_runs(); // 0x0058A2E0 (:583)
    bool debug_get_next_run(int& num_pages, bool& in_use); // 0x005C88F0 (:591)
    virtual unsigned int mark(); // vtbl+0x30 0x005CB9A0 (.h:144)
    virtual bool restore_to_mark(unsigned int pos); // vtbl+0x34 0x005CB9C0 (.h:145)
    virtual void* alloc_all(unsigned int align); // vtbl+0x48 0x005CB9E0 (.h:146)
    virtual bool pad_to_page(unsigned int page_sizev); // vtbl+0x3C 0x005CBA00 (.h:147)
    virtual bool instantiable(); // vtbl+0x4C 0x005CBA40 (.h:151)

private:
    unsigned int page_size; // 0x4C
    unsigned int max_pages; // 0x50
    unsigned char* max_page_addr; // 0x54
    split_page_mempool_header* first_free_page; // 0x58
    unsigned int free_list_count; // 0x5C
    unsigned int free_list_max; // 0x60
    unsigned int dbg_cur_index; // 0x64
    unsigned int dbg_next_free_block; // 0x68
    dynamic_uint_hash_table<split_page_mempool_header> header_hash; // 0x70
    unsigned int wasted_space; // 0xB8
};
