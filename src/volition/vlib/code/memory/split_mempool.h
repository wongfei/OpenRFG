// Original: volition/vlib/code/memory/split_mempool.h
// Implementation: volition/vlib/code/memory/split_mempool.cpp
//
// A mempool (the CPU half) plus an optional GPU mempool (m_gpu_pool), for resources with a CPU part and a GPU part.
// The *_cpu functions act on this pool, *_gpu on m_gpu_pool; clear/restore_to_mark/set_thread_ownership act on both.
#pragma once

#include "volition/vlib/code/memory/vmemory.h"

// size 0x54
class split_mempool : public mempool
{
public:
    split_mempool(const void* mem, unsigned int mem_size, const char* pool_name, unsigned int align, unsigned char mp_flags); // 0x005B5000 (.h:35)
    split_mempool(); // 0x005D0AD0 (.h:33)
    void attach_gpu_pool(mempool* gpu); // 0x00589F40 (:16)
    mempool* get_gpu_pool(); // 0x004A9BA0 (.h:42)
    bool can_alloc_split(unsigned int cpu_size, unsigned int gpu_size, unsigned int cpu_align, unsigned int gpu_align); // 0x00589F50 (:23)
    void* alloc_cpu(unsigned int size, unsigned int align); // 0x005BC9E0 (:48)
    void* alloc_gpu(unsigned int size, unsigned int align); // 0x00589FA0 (:53)
    virtual bool clear(); // vtbl+0x28 0x005C8590 (:62)
    bool clear_cpu(); // 0x005C4500 (:70)
    bool clear_gpu(); // 0x00589FB0 (:75)
    void lock_cpu(); // 0x00589FD0 (:85)
    void lock_gpu(); // 0x00589FE0 (:90)
    void unlock_cpu(); // 0x0058A000 (:100)
    void unlock_gpu(); // 0x0058A030 (:105)
    unsigned int mark_cpu(); // 0x005AC180 (:117)
    unsigned int mark_gpu(); // 0x0058A070 (:122)
    virtual bool restore_to_mark(unsigned int pos); // vtbl+0x34 0x005BC9F0 (:136)
    bool restore_to_mark_cpu(unsigned int pos); // 0x005BCA30 (:145)
    bool restore_to_mark_gpu(unsigned int pos); // 0x0058A0A0 (:150)
    virtual void set_thread_ownership(); // vtbl+0x00 0x005AC190 (:161)
    void set_thread_ownership_cpu(); // 0x005AC1A0 (:169)
    void set_thread_ownership_gpu(); // 0x0058A0C0 (:174)
    unsigned int space_free_cpu() const; // 0x0058A0D0 (:186)
    unsigned int space_free_gpu() const; // 0x0058A0E0 (:191)
    unsigned int space_max_cpu() const; // 0x0058A100 (:199)
    unsigned int space_max_gpu() const; // 0x0058A110 (:204)
    unsigned int space_used_cpu() const; // 0x0058A130 (:212)
    unsigned int space_used_gpu() const; // 0x0058A150 (:217)
    unsigned int space_used_highwater_cpu() const;
    unsigned int space_used_highwater_gpu() const;
    bool release_bytes_cpu(unsigned int num_bytes); // 0x005AC1B0 (:246)
    bool release_bytes_gpu(unsigned int num_bytes); // 0x0058A170 (:251)
    virtual ~split_mempool(); // vtbl+0x40 0x00597520

protected:
    mempool* m_gpu_pool; // 0x50
};
