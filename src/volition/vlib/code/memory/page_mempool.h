// Original: volition/vlib/code/memory/page_mempool.h
// Implementation: volition/vlib/code/memory/page_mempool.cpp
//
// Allocator that hands out runs of fixed-size pages from a buffer, with a page_mempool_header per run; supports free
// (release_bytes(void*)), realloc and shrink_to_fit.
#pragma once

#include "volition/vlib/code/memory/static_mempool.h"

// size 0x8
struct page_mempool_header
{
    unsigned int data_offset(); // 0x00563B30 (.h:58)

    unsigned int num_pages; // 0x00
    union
    {
        unsigned int next_index; // 0x04, free runs
        unsigned int align_offset; // 0x04, used runs
    };
};

// size 0x58
class page_mempool : public static_mempool_base
{
    unsigned int get_required_page_count(unsigned int size, unsigned int align, unsigned int& align_offset) const; // 0x00589540 (:101)
    unsigned int get_align_page_count(page_mempool_header* page_header, unsigned int align) const; // 0x005895A0 (:128)
    unsigned int get_page_index(const void* mem); // 0x005895E0 (:139)
    page_mempool_header* get_header(unsigned int page_index) const; // 0x00563B40 (.h:89)
public:
    page_mempool(const void* mem, unsigned int mem_size, unsigned int page_size, const char* pool_name, unsigned int align, unsigned char mp_flags); // 0x005BC3F0 (:39)
    page_mempool(); // 0x005AB330 (:30)
    virtual ~page_mempool(); // vtbl+0x40 0x005AB460 (:48)
    virtual void init(const void* mem, unsigned int mem_size, unsigned int default_page_size, const char* pool_name, unsigned int align, unsigned char mp_flags); // vtbl+0x50 0x005AB470 (:61)
    virtual void* get_base(); // vtbl+0x2C 0x005AB390 (.h:98)
    virtual unsigned int space_free() const; // vtbl+0x0C 0x005AB3A0 (.h:102)
    virtual unsigned int space_used() const; // vtbl+0x10 0x004B6760 (.h:103)
    virtual unsigned int space_max() const; // vtbl+0x14 0x004B6770 (.h:104)
    virtual bool can_alloc(unsigned int size, unsigned int align) const; // vtbl+0x18 0x005AB4D0 (:158)
    virtual void* alloc(unsigned int size, unsigned int align); // vtbl+0x1C 0x005AB5B0 (:196)
    virtual void* realloc(void* mem, unsigned int new_size); // vtbl+0x20 0x005AB770 (:299)
    virtual unsigned int size(const void* mem); // vtbl+0x54 0x00589610 (:393)
    virtual void shrink_to_fit(void* mem, unsigned int actual_size); // vtbl+0x58 0x00589680 (:413)
    virtual bool release_bytes(unsigned int num_bytes); // vtbl+0x38 0x005AB430 (.h:129)
    virtual void release_bytes(void* mem); // vtbl+0x5C 0x005897B0 (:488)
    virtual bool clear(); // vtbl+0x28 0x00589880 (:545)
    virtual bool debug_get_next_run(int& num_pages, bool& in_use); // vtbl+0x60 0x005898D0 (:564)
    virtual unsigned int mark(); // vtbl+0x30 0x005AB3B0 (.h:125)
    virtual bool restore_to_mark(unsigned int pos); // vtbl+0x34 0x005AB3D0 (.h:126)
    virtual void* alloc_all(unsigned int align); // vtbl+0x48 0x005AB3F0 (.h:127)
    virtual bool pad_to_page(unsigned int _page_size); // vtbl+0x3C 0x005AB410 (.h:128)
    virtual bool instantiable(); // vtbl+0x4C 0x005AB450 (.h:132)

private:
    unsigned int page_size; // 0x4C
    unsigned int max_pages; // 0x50
    unsigned int first_free_index; // 0x54
};
