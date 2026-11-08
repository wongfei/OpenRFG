// Original: volition/vlib/code/memory/object_pool.h
// Implementation: volition/vlib/code/memory/object_pool.inl (template code, included by this header)
//
// Fixed-capacity object pools with intrusive index lists (decompiled behaviour):
// - object_pool_server<T> owns the storage: `base` (T[capacity]), `links` (one index_link per object, a circular
//   doubly linked list in 16-bit indices) and `cat` (the category of each object, 0xFF when free). All free objects
//   are on the `avail` list. init() allocates the three arrays from a mempool; object_pool<T,N,K> embeds them instead.
// - object_pool_client<T,K> keeps K "in use" lists (categories). alloc(category) takes the head of `avail`, calls
//   T::init_cb on it when set, and appends it to in_use[category]; free(objp) does the reverse (T::release_cb) and
//   returns the next object of the same list, so freeing while iterating works. A spin lock guards both.
// - dynamic_object_pool<T,K> = server + client over memory from a mempool; object_pool<T,N,K> carries the memory
//   inline: memory_footprint = N * (sizeof(T) + sizeof(index_link) + 1) bytes (checked against several instances).
// Method addresses are for one instantiation, named in <>. Instances are very common (about 240 element types).
#pragma once

#include "volition/vlib/code/os/cs_wrapper.h"

class mempool_base;

// One circular list of objects, by index. Placement inferred.
// size 0x8
struct index_list
{
    int head; // 0x00
    int count; // 0x04
};

// Placement inferred.
// size 0x4
struct index_link
{
    unsigned int prev : 16; // 0x00:0
    unsigned int next : 16; // 0x00:16
};

// size 0x20
template<typename T>
class object_pool_server
{
public:
    void init(mempool_base* pool, unsigned int num_objects); // <split_memblk> 0x005B3E50 (object_pool.inl:31)
    void destroy(); // <split_memblk> 0x00595DA0 (object_pool.inl:57)
    int get_free() const;
    unsigned int get_capacity() const;
    bool can_alloc() const;
    bool contains(const T* objp) const; // <split_memblk> 0x00596BC0 (.h:62)
    int get_index(const T* objp) const; // <split_memblk> 0x005960B0 (.h:63)
    const T* get_entry(int) const;
    T* get_entry(int);
    int get_category(const T*) const;
    int get_category(int) const;
    object_pool_server(void (T::*init)(), void (T::*release)()); // <split_memblk> 0x00595DB0 (object_pool.inl:12)
    unsigned int bytes_allocated() const;
protected:
    void remove(index_list& list, int index); // <split_memblk> 0x00596730 (object_pool.inl:67)
    void append(index_list& list, int index); // <split_memblk> 0x00596760 (object_pool.inl:87)
    int grab(); // <split_memblk> 0x005967D0 (object_pool.inl:112)
    void drop(int index); // <split_memblk> 0x00596820 (object_pool.inl:131)

    T* base; // 0x00
    index_link* links; // 0x04
    unsigned char* cat; // 0x08
private:
    template<typename, int> friend class object_pool_client; // the client works on the server's lists (inferred)

    void (T::*init_cb)(); // 0x0C
    void (T::*release_cb)(); // 0x10
    unsigned int max_capacity; // 0x14
    index_list avail; // 0x18
};

// size 0x10 + 0xC * K, rounded up to the 8-byte alignment of thread_access, + 8
template<typename T, int K>
class object_pool_client
{
public:
    void init(object_pool_server<T>* servp); // <split_memblk,1> 0x00595880 (.h:114)
    int get_count(int) const;
    T* alloc(int category); // <split_memblk,1> 0x005B3F50 (object_pool.inl:160)
    T* free(T* objp); // <split_memblk,1> 0x005C0B00 (object_pool.inl:181), returns the next object in its list
    T* move(T*, int);
    void clear(int category); // <split_memblk,1> 0x005C1850 (object_pool.inl:324)
    void clear(); // <split_memblk,1> 0x005C6750 (object_pool.inl:314)
    const T* get_first(int) const;
    T* get_first(int category); // <split_memblk,1> 0x00596D70 (object_pool.inl:226)
    const T* get_last(int) const;
    T* get_last(int);
    const T* get_prev(const T*) const;
    T* get_prev(const T*);
    const T* get_next(const T*) const;
    T* get_next(const T* objp); // <split_memblk,1> 0x005B49A0 (object_pool.inl:238)
    object_pool_client(int __cs_debug_handle = 0); // <split_memblk,1> 0x005B36D0 (.h:142), default argument inferred
protected:
    void reset(); // <split_memblk,1> 0x00595DE0 (object_pool.inl:149)
public:
    ~object_pool_client(); // <split_memblk,1> 0x00589DC0

private:
    object_pool_server<T>* server; // 0x00
    index_list in_use[K]; // 0x04
public:
    cs_spinlock thread_access; // 0x10 for K == 1
    bool debug_flag; // 0x18 for K == 1
};

// Server and client over storage allocated from a mempool by init().
// size 0x20 + sizeof(object_pool_client<T,K>)
template<typename T, int K>
class dynamic_object_pool : public object_pool_server<T>, public object_pool_client<T, K>
{
public:
    void init(mempool_base& pool, unsigned int size); // <split_memblk,1> 0x005C0470 (.h:179)
    void destroy(); // <split_memblk,1> 0x005B3560 (.h:187)
    unsigned int get_count(int) const;
    dynamic_object_pool(void (T::*init_cb)() = nullptr, void (T::*release_cb)() = nullptr, int __cs_debug_handle = 0); // <split_memblk,1> 0x005C0490 (.h:200), default arguments inferred
    ~dynamic_object_pool(); // <split_memblk,1> 0x005ABF10
};

// Pool with inline storage for N objects. 16-byte aligned (not recorded in the PDB): object_pool members follow a
// 4-byte field at offset 0x10 (ai_defense_object_manager::targets, ai_search_object_manager::locations,
// ai_queued_rate_limiter::request_pool) and every global instance sits on a 16-byte boundary.
template<typename T, int N, int K>
class alignas(16) object_pool : public dynamic_object_pool<T, K>
{
public:
    object_pool(void (T::*init_cb)() = nullptr, void (T::*release_cb)() = nullptr, int __cs_debug_handle = 0); // <weapon,256,1> 0x00BE8F80 (.h:222), default arguments inferred
    ~object_pool(); // <weapon,256,1> 0x00BE0EE0

private:
    static const unsigned int memory_footprint; // <weapon,256,1> 0x0133AEB0
    unsigned char data[N * (sizeof(T) + sizeof(index_link) + sizeof(unsigned char))]; // 0x40 for K == 1
};

#include "volition/vlib/code/memory/object_pool.inl"
