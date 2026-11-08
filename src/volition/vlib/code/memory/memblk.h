// Original: volition/vlib/code/memory/memblk.h
// Implementation: volition/vlib/code/memory/memblk.cpp
//
// Block of a binary-tree (best fit) allocator: the header precedes the memory (mem_base). Placement of meminfo inferred.
#pragma once

// size 0xC
struct meminfo
{
    unsigned int size; // 0x00
    unsigned int usable_size; // 0x04
    unsigned int requested_size; // 0x08
};

// size 0x28
struct memblk
{
    memblk(unsigned int size, memblk* parent); // 0x00589150 (:5)
    memblk* can_alloc(unsigned int size); // 0x00589180 (:28)
    memblk* alloc(unsigned int size, memblk*& root); // 0x005AB0F0 (:61)
    memblk* free(memblk*& root); // 0x005AB190 (:102)
    memblk* insert(memblk* root); // 0x005891D0 (:150)
    memblk* remove(memblk* root); // 0x00589240 (:189)

    meminfo info; // 0x00
    union
    {
        const char* track; // 0x0C
        int in_use; // 0x0C
    };
    memblk* next; // 0x10
    memblk* prev; // 0x14
    memblk* top; // 0x18
    memblk* left; // 0x1C
    memblk* right; // 0x20
    void* reserved; // 0x24
    char mem_base[0]; // 0x28
};
