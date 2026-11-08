// Original: volition/vlib/code/memory/page_pool.h
// Implementation: volition/vlib/code/memory/page_pool.cpp
//
// A budget of 4 KB pages shared by virtual_mempools (decompiled): init rounds the size up to pages; alloc_pages and
// release_pages only count (pages_free), except with keep_allocated, where the pages come from one block reserved at
// init. 128 virtual_mempool records are kept inline: get_virtual_mempool takes one from the free list and initialises
// it, release_virtual_mempool returns it. Example: Common_page_pool (rfg.h), handed to scheduler_init and xml.
#pragma once

#include "volition/vlib/code/memory/virtual_mempool.h"
#include "volition/vlib/code/os/cs_wrapper.h"

class mempool_base;

// size 0x30F8
class page_pool
{
public:
    page_pool(const char* _name, unsigned int _size, bool _keep_allocated); // 0x005BC590 (:52)
    page_pool(); // 0x005BC4B0 (:48)
    bool init(const char* _name, unsigned int _size, bool _keep_allocated); // 0x005AB930 (:61)
    ~page_pool(); // 0x005BC670 (:120)
    bool destroy(); // 0x005ABA20 (:125)
    virtual_mempool* get_virtual_mempool(const char* namev, unsigned int max_size); // 0x005ABBC0 (:263)
    bool release_virtual_mempool(virtual_mempool* mempool); // 0x005ABCB0 (:290)
    bool alloc_pages(unsigned int num_pages); // 0x005ABAA0 (:167)
    bool release_pages(unsigned int num_pages); // 0x005ABB40 (:215)
    unsigned int space_free() const; // 0x00589970 (:321)
    unsigned int space_used() const;
    unsigned int space_max() const; // 0x00563B20 (.h:64)
    bool link_to_mempool(mempool_base* pool); // 0x00589980 (:327)
    bool unlink_from_mempool(mempool_base* pool); // 0x005899A0 (:350)
    const char* get_name() const;

    cs_wrapper pp_cs; // 0x00
    unsigned int size; // 0x3C
    unsigned int pages; // 0x40
    unsigned int pages_free; // 0x44
    unsigned int peak_usage_size; // 0x48
    unsigned int virtual_mempool_count; // 0x4C
    char name[32]; // 0x50
    bool keep_allocated; // 0x70
    unsigned char* keep_allocated_memory; // 0x74
    cs_wrapper keep_allocated_cs; // 0x78
private:
    virtual_mempool* virtual_mempool_free_list; // 0xB4
    virtual_mempool* virtual_mempool_used_list; // 0xB8
    virtual_mempool virtual_mempool_pool[128]; // 0xBC
    cs_wrapper virtual_mempool_record_cs; // 0x30BC
};
