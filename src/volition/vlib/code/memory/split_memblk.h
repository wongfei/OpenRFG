// Original: volition/vlib/code/memory/split_memblk.h
// Implementation: volition/vlib/code/memory/split_memblk.cpp
//
// Block of split_memmgr's large-allocation tree: a node in a binary tree keyed by size (left/right/top) and in a list
// (next/prev); the memory itself lives elsewhere (mem_base/mem_aligned), so blocks can describe GPU memory.
// Placement of split_meminfo inferred.
#pragma once

template<typename T, int K>
class dynamic_object_pool;

// size 0xC
struct split_meminfo
{
    unsigned int size; // 0x00
    unsigned int usable_size; // 0x04
    unsigned int requested_size; // 0x08
};

// size 0x2C
struct split_memblk
{
    split_memblk(); // 0x00589AA0 (:7)
    void init(unsigned int size, char* addr, split_memblk* parent); // 0x00589AD0 (:30)
    split_memblk* can_alloc(unsigned int size, unsigned int align); // 0x00589B10 (:55)
    split_memblk* alloc(unsigned int size, split_memblk*& root, dynamic_object_pool<split_memblk,1>& pool); // 0x005BC890 (:91)
    split_memblk* free(split_memblk*& root, dynamic_object_pool<split_memblk,1>& pool); // 0x005C4320 (:139)
    split_memblk* insert(split_memblk* root); // 0x00589B60 (:195)
    split_memblk* remove(split_memblk* root); // 0x00589BD0 (:234)

    split_meminfo info; // 0x00
    union
    {
        const char* track; // 0x0C
        int in_use; // 0x0C
    };
    split_memblk* next; // 0x10
    split_memblk* prev; // 0x14
    split_memblk* top; // 0x18
    split_memblk* left; // 0x1C
    split_memblk* right; // 0x20
    char* mem_base; // 0x24
    char* mem_aligned; // 0x28
};
