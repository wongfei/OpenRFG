// Original: volition/stream2/code/stream2_allocator.h
// Implementation: volition/stream2/code/stream2_allocator.cpp
//
// Allocators that give streamed containers their memory. Every container type names one by index
// (stream2_container_type_reg::allocator_type, game_stream_allocators in game_stream2_prims.h); the instances are
// registered in _Stream_allocators by game_stream2_register_allocators. All requests come in pairs: CPU memory and an
// optional GPU (split) part, each with its own alignment.
// - stream2_dynamic_allocator: general heap on two split_memmgr (Level_memmgr for the world). frag_fail is set when an
//   allocation fails although enough memory is free, and stream_grid::do_fragmentation_handling reacts to it.
// - stream2_mempool_allocator: bump allocation from one mempool (its GPU half for split pools); preload containers.
// - stream2_stack_allocator: like the mempool allocator but frees in any order, keeping up to 16 memory_marks and
//   rolling the pool back once the topmost marks are released (process_restore_requests).
// - stream2_slot_allocator: fixed-size slots grouped in brackets (bracket_info: pairs of CPU/GPU sizes and counts);
//   each slot is a pair of mempools, so a container that fits a bracket always gets a slot (humans, vehicles, weapons,
//   layers, voice personas).
// - stream2_system_allocator: the plain heap (interface stack allocator).
// memory_mark: one stack allocation (pool marks and data pointers); released marks are rolled back in order.
#pragma once

#include "volition/vlib/code/memory/vmemory.h"
#include "volition/vlib/code/util/varray.h"

class split_memmgr;

// size 0x4
class stream2_allocator
{
public:
    virtual ~stream2_allocator(); // vtbl+0x00 0x004A9B30 (.h:18)
    virtual bool can_alloc(int, int, int, int) = 0; // vtbl+0x04
    virtual bool can_alloc(int, int) = 0; // vtbl+0x08
    virtual void max_alloc(int&, int&) = 0; // vtbl+0x0C
    virtual int max_alloc() = 0; // vtbl+0x10
    virtual void alloc(int, int, unsigned char*&, int, int, unsigned char*&, const char*) = 0; // vtbl+0x14
    virtual unsigned char* alloc(int, int, const char*) = 0; // vtbl+0x18
    virtual void free(unsigned char*, unsigned char*) = 0; // vtbl+0x1C
    virtual void free(unsigned char*) = 0; // vtbl+0x20
    virtual void set_thread_ownership() = 0; // vtbl+0x24
    virtual bool using_split_mempools() = 0; // vtbl+0x28
    stream2_allocator(); // 0x004A9FE0

    // 0x00 vfptr
};

// size 0x14
struct memory_mark
{
    memory_mark(unsigned int, unsigned int, unsigned char*, unsigned char*);
    memory_mark(); // 0x004A9B60 (.h:52)

    unsigned int cpu_mark; // 0x00
    unsigned int gpu_mark; // 0x04
    unsigned char* cpu_data; // 0x08
    unsigned char* gpu_data; // 0x0C
    bool released; // 0x10
};

// size 0x14
class stream2_dynamic_allocator : public stream2_allocator
{
public:
    stream2_dynamic_allocator(); // 0x004AC550 (:24)
    void init(split_memmgr* _cpu_memmgr, split_memmgr* _gpu_memmgr); // 0x004A9FF0 (:32)
    virtual ~stream2_dynamic_allocator(); // vtbl+0x00 0x004AC570 (.h:96)
    bool get_frag_fail(); // 0x00919C40 (.h:98)
    void set_frag_fail(bool f); // 0x004A9B80 (.h:99)
    virtual bool can_alloc(int cpu_bytes, int cpu_align, int gpu_bytes, int gpu_align); // vtbl+0x04 0x004AC5A0 (:39)
    virtual bool can_alloc(int bytes, int align); // vtbl+0x08 0x004AA010 (:53)
    virtual void max_alloc(int& cpu_max, int& gpu_max); // vtbl+0x0C 0x004AA020 (:58)
    virtual int max_alloc(); // vtbl+0x10 0x004AA050 (:69)
    virtual void alloc(int cpu_bytes, int cpu_align, unsigned char*& cpu_alloc, int gpu_bytes, int gpu_align, unsigned char*& gpu_alloc, const char* debug_name); // vtbl+0x14 0x004AA060 (:74)
    virtual unsigned char* alloc(int bytes, int align, const char* debug_name); // vtbl+0x18 0x004AA110 (:168)
    virtual void free(unsigned char* data); // vtbl+0x20 0x004AA160 (:185)
    virtual void free(unsigned char* cpu_data, unsigned char* gpu_data); // vtbl+0x1C 0x004AA130 (:173)
    virtual void set_thread_ownership(); // vtbl+0x24 0x004AA190 (:204)
    virtual bool using_split_mempools(); // vtbl+0x28 0x004AA170 (:190)
    virtual unsigned int get_gpu_allocated(); // vtbl+0x2C 0x004AA180 (:199)

private:
    unsigned int gpu_allocated; // 0x04
    split_memmgr* cpu_memmgr; // 0x08
    split_memmgr* gpu_memmgr; // 0x0C
    bool frag_fail; // 0x10
};

// size 0xC
class stream2_mempool_allocator : public stream2_allocator
{
public:
    void init(mempool_base* _pool); // 0x004AA1A0 (:215)
    virtual ~stream2_mempool_allocator(); // vtbl+0x00 0x005D0A90 (.h:119)
    virtual bool can_alloc(int cpu_bytes, int cpu_align, int gpu_bytes, int gpu_align); // vtbl+0x04 0x004AC5F0 (:231)
    virtual bool can_alloc(int bytes, int align); // vtbl+0x08 0x004AA1C0 (:226)
    virtual void max_alloc(int& cpu_max, int& gpu_max); // vtbl+0x0C 0x004AA1E0 (:249)
    virtual int max_alloc(); // vtbl+0x10 0x004AA1D0 (:244)
    virtual void alloc(int cpu_bytes, int cpu_align, unsigned char*& cpu_alloc, int gpu_bytes, int gpu_align, unsigned char*& gpu_alloc, const char* debug_name); // vtbl+0x14 0x004AC680 (:263)
    virtual unsigned char* alloc(int bytes, int align, const char* debug_name); // vtbl+0x18 0x004AA210 (:255)
    virtual void free(unsigned char* data1, unsigned char* data2); // vtbl+0x1C 0x004AA240 (:296)
    virtual void free(unsigned char* data); // vtbl+0x20 0x004AA230 (:290)
    virtual void set_thread_ownership(); // vtbl+0x24 0x004AA270 (:307)
    virtual bool using_split_mempools(); // vtbl+0x28 0x004AA250 (:302)
    stream2_mempool_allocator(); // 0x005D4770

private:
    mempool_base* cpu_pool; // 0x04
    mempool_base* gpu_pool; // 0x08
};

// size 0x15C
class stream2_stack_allocator : public stream2_allocator
{
public:
    void init(mempool_base* _pool); // 0x004AC770 (:324)
    virtual ~stream2_stack_allocator(); // vtbl+0x00
    virtual bool can_alloc(int cpu_bytes, int cpu_align, int gpu_bytes, int gpu_align); // vtbl+0x04 0x004AC790 (:342)
    virtual bool can_alloc(int bytes, int align); // vtbl+0x08 0x004AA2A0 (:337)
    virtual void max_alloc(int& cpu_max, int& gpu_max); // vtbl+0x0C 0x004AA2C0 (:360)
    virtual int max_alloc(); // vtbl+0x10 0x004AA2B0 (:355)
    virtual void alloc(int cpu_bytes, int cpu_align, unsigned char*& cpu_alloc, int gpu_bytes, int gpu_align, unsigned char*& gpu_alloc, const char* debug_name); // vtbl+0x14 0x004AF180 (:394)
    virtual unsigned char* alloc(int bytes, int align, const char* debug_name); // vtbl+0x18 0x004AF0E0 (:366)
    virtual void free(unsigned char* data1, unsigned char* data2); // vtbl+0x1C 0x004AF330 (:466)
    virtual void free(unsigned char* data); // vtbl+0x20 0x004AF2F0 (:447)
    virtual void set_thread_ownership(); // vtbl+0x24 0x004AA310 (:499)
    virtual bool using_split_mempools(); // vtbl+0x28 0x004AA2F0 (:494)
private:
    void process_restore_requests(); // 0x004AC820 (:536)

    mempool_base* cpu_pool; // 0x04
    mempool_base* gpu_pool; // 0x08
    farray<memory_mark,16> marks; // 0x0C
};

// size 0x24
class stream2_slot_allocator : public stream2_allocator
{
public:
    // size 0xA8
    struct slot
    {
        mempool cpu_pool; // 0x00
        mempool gpu_pool; // 0x50
        unsigned short next; // 0xA0
        unsigned short prev; // 0xA2
        int bindex; // 0xA4
    };

    // size 0x10
    struct bracket
    {
        int cpu_bracket_size; // 0x00
        int gpu_bracket_size; // 0x04
        slot* slots; // 0x08
        unsigned short slot_free_list; // 0x0C
        unsigned short slot_used_list; // 0x0E
    };

    bool init(unsigned char* (*cpu_alloc_cb)(int, int), int cpu_alignment, unsigned char* (*gpu_alloc_cb)(int, int), int gpu_alignment, int* bracket_info, int num_brackets, const char* name); // 0x004AC900 (:624)
    bool init(unsigned char* (*alloc_cb)(int, int), int* bracket_info, int num_brackets, int _align, const char* name); // 0x004AA380 (:609)
    virtual bool can_alloc(int cpu_bytes, int _cpu_align, int gpu_bytes, int _gpu_align); // vtbl+0x04 0x004AF390 (:570)
    virtual bool can_alloc(int bytes, int _align); // vtbl+0x08 0x004AA340 (:561)
    virtual void max_alloc(int& cpu_max, int& gpu_max); // vtbl+0x0C 0x004AC8D0 (:596)
    virtual int max_alloc(); // vtbl+0x10 0x004AA360 (:589)
    virtual void alloc(int cpu_bytes, int _cpu_align, unsigned char*& cpu_alloc, int gpu_bytes, int _gpu_align, unsigned char*& gpu_alloc, const char* debug_name); // vtbl+0x14 0x004AF3E0 (:798)
    virtual unsigned char* alloc(int bytes, int _align, const char* debug_name); // vtbl+0x18 0x004AA3A0 (:880)
    virtual void free(unsigned char* cpu_buf, unsigned char* gpu_buf); // vtbl+0x1C 0x004ACD60 (:891)
    virtual void free(unsigned char* buf); // vtbl+0x20 0x004AA3C0 (:934)
    virtual void set_thread_ownership(); // vtbl+0x24 0x004ACFE0 (:947)
    virtual bool using_split_mempools(); // vtbl+0x28 0x004AA3E0 (:942)
    int get_cpu_allocated();
    int get_gpu_allocated();
    int get_total_bytes_allocated();
    void destroy(); // 0x004AD0E0 (:994)
private:
    int get_bracket_index(int cpu_bytes, int gpu_bytes); // 0x004AD0A0 (:971)
public:
    stream2_slot_allocator(); // 0x005D47A0
    virtual ~stream2_slot_allocator(); // vtbl+0x00 0x005D47B0

private:
    int cpu_align; // 0x04
    int cpu_total_allocated; // 0x08
    int gpu_align; // 0x0C
    int gpu_vram_allocated; // 0x10
    base_array<bracket> brackets; // 0x14
};

// size 0x4
class stream2_system_allocator : public stream2_allocator
{
public:
    virtual bool can_alloc(int, int); // vtbl+0x08 0x008A0050 (.h:237)
    virtual bool can_alloc(int, int, int, int); // vtbl+0x04 0x00883360 (.h:236)
    virtual void max_alloc(int& cpu_max, int& gpu_max); // vtbl+0x0C 0x008A0070 (.h:239)
    virtual int max_alloc(); // vtbl+0x10 0x008A0060 (.h:238)
    virtual void alloc(int cpu_bytes, int cpu_align, unsigned char*& cpu_alloc, int gpu_bytes, int gpu_align, unsigned char*& gpu_alloc, const char* debug_name); // vtbl+0x14 0x00883370 (.h:241)
    virtual unsigned char* alloc(int bytes, int align, const char* debug_name); // vtbl+0x18 0x008A0090 (.h:240)
    virtual void free(unsigned char* cpu_data, unsigned char* gpu_data); // vtbl+0x1C 0x008833B0 (.h:243)
    virtual void free(unsigned char* data); // vtbl+0x20 0x008A00B0 (.h:242)
    virtual void set_thread_ownership(); // vtbl+0x24 0x008833E0 (.h:244)
    virtual bool using_split_mempools(); // vtbl+0x28 0x008A00D0 (.h:245)
    stream2_system_allocator(); // 0x008A0040
    virtual ~stream2_system_allocator(); // vtbl+0x00 0x008A00E0

};

extern stream2_allocator* _Stream_allocators[255]; // 0x01AC3740

bool stream2_register_allocator(int allocator_index, stream2_allocator* allocator); // 0x004AA3F0 (:1028)
