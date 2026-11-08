// Original: volition/vlib/code/memory/mempool_base.h
// Implementation: volition/vlib/code/memory/mempool_base.cpp
//
// Abstract allocator interface of vlib (decompiled behaviour):
// - Every pool has a 31-character name, flags and an owning thread. alloc/realloc/clear/restore_to_mark of the
//   implementations first call assert_permission(): the call fails (and logs) when the pool is lock()ed or when the
//   calling thread does not own it (is_owned). keen::rfg::Application::updateSimulation claims the pools the sim thread
//   uses with set_thread_ownership().
// - Flags (mp_flags): 0x01 single allocation (a second alloc is refused), 0x40 split pool with a GPU sub-pool
//   (split_mempool::m_gpu_pool, set by rfg_split_mempool). Other bits not identified yet.
// - Most pools are bump allocators with mark()/restore_to_mark() for scoped temporary memory (mempool_mark_scope).
#pragma once

#include <stddef.h>

// size 0x38
class mempool_base
{
protected:
    bool assert_permission() const; // 0x005AB260 (:141)
public:
    virtual void set_thread_ownership() = 0; // vtbl+0x00
    const char* get_name() const; // 0x00563AF0 (.h:77)
    unsigned char get_flags() const; // 0x004A9B20 (.h:78)
    virtual bool is_owned() const = 0; // vtbl+0x04
    void lock(); // 0x005894F0 (:219)
    void unlock(); // 0x00589500 (:229)
    bool is_locked() const; // 0x00589530 (:244)
    virtual bool is_dynamic() const = 0; // vtbl+0x08
    virtual unsigned int space_free() const = 0; // vtbl+0x0C
    virtual unsigned int space_used() const = 0; // vtbl+0x10
    virtual unsigned int space_max() const = 0; // vtbl+0x14
    unsigned int peak_used() const; // 0x007B98C0 (.h:115)
    void reset_peak_used();
    virtual bool can_alloc(unsigned int, unsigned int) const = 0; // vtbl+0x18
    virtual void* alloc(unsigned int, unsigned int) = 0; // vtbl+0x1C
    virtual void* realloc(void*, unsigned int) = 0; // vtbl+0x20
    virtual bool contains_address(const void*) const = 0; // vtbl+0x24
    virtual bool clear() = 0; // vtbl+0x28
    virtual void* get_base() = 0; // vtbl+0x2C
    virtual unsigned int mark() = 0; // vtbl+0x30
    virtual bool restore_to_mark(unsigned int) = 0; // vtbl+0x34
    virtual bool release_bytes(unsigned int) = 0; // vtbl+0x38
    virtual bool pad_to_page(unsigned int) = 0; // vtbl+0x3C
    void base_init(const char* pool_name, unsigned char mp_flags); // 0x00589430 (:28)
    void destroy(); // 0x00589480 (:74)
    mempool_base(const char* pool_name, unsigned char mp_flags); // 0x00589490 (:97)
    virtual ~mempool_base(); // vtbl+0x40 0x005894E0 (:102)

    // 0x00 vfptr
private:
    volatile bool locked; // 0x04
    volatile unsigned int lock_id; // 0x08
protected:
    char name[32]; // 0x0C
    unsigned char flags; // 0x2C
    volatile unsigned int thread_id; // 0x30
    unsigned int peak_usage; // 0x34
};

// Allocation from a pool; pool memory is never freed individually (the matching deletes do nothing and only exist
// for exceptions thrown by constructors). Inline in the original header.
void* operator new(size_t size, mempool_base& pool); // 0x00705120 (.h:161)
void* operator new[](size_t size, mempool_base& pool); // 0x0045AB80 (.h:170)
inline void operator delete(void*, mempool_base&)
{
}
inline void operator delete[](void*, mempool_base&)
{
}
