// Original: volition/vlib/code/memory/object_pool.inl (template code, included by object_pool.h)
//
// Lists are circular and doubly linked through 16-bit indices; an empty list has count 0 (head is then meaningless).
// grab() takes the head of the free list, drop() appends to it, so released objects are reused last.
// The init/release callbacks are pointers to members of T and run on the object being taken or given back.
#pragma once

#include <new>

#include "volition/vlib/code/memory/mempool_base.h"
#include "volition/vlib/code/memory/vmemory.h"

// ---- object_pool_server<T>

// <split_memblk> 0x00595DB0 (:12)
template<typename T>
object_pool_server<T>::object_pool_server(void (T::*init)(), void (T::*release)())
{
    base = nullptr;
    links = nullptr;
    cat = nullptr;
    max_capacity = 0;
    avail.head = 0;
    avail.count = 0;
    init_cb = init;
    release_cb = release;
}

// <split_memblk> 0x005B3E50 (:31)
template<typename T>
void object_pool_server<T>::init(mempool_base* pool, unsigned int num_objects)
{
    base = (T*)pool->alloc(num_objects * sizeof(T), 0);
    links = (index_link*)pool->alloc(num_objects * sizeof(index_link), 0);
    cat = (unsigned char*)pool->alloc(num_objects, 0);
    max_capacity = num_objects;
    avail.head = 0;
    avail.count = 0;
    for (unsigned int i = 0; i < num_objects; ++i)
    {
        // Every element is constructed in place (seen in the <zone_file> instantiation, 0x0092F3B0).
        new (&base[i]) T;
        append(avail, (int)i);
        cat[i] = 0xFF;
    }
}

// <split_memblk> 0x00595DA0 (:57)
template<typename T>
void object_pool_server<T>::destroy()
{
}

template<typename T>
int object_pool_server<T>::get_free() const
{
    return avail.count;
}

template<typename T>
unsigned int object_pool_server<T>::get_capacity() const
{
    return max_capacity;
}

template<typename T>
bool object_pool_server<T>::can_alloc() const
{
    return avail.count > 0;
}

// <split_memblk> 0x00596BC0 (.h:62)
template<typename T>
bool object_pool_server<T>::contains(const T* objp) const
{
    return (unsigned int)(objp - base) < max_capacity;
}

// <split_memblk> 0x005960B0 (.h:63)
template<typename T>
int object_pool_server<T>::get_index(const T* objp) const
{
    return (int)(objp - base);
}

template<typename T>
const T* object_pool_server<T>::get_entry(int index) const
{
    return (unsigned int)index < max_capacity ? base + index : nullptr;
}

// <cfile> 0x005956E0 (.h:64)
template<typename T>
T* object_pool_server<T>::get_entry(int index)
{
    return (unsigned int)index < max_capacity ? base + index : nullptr;
}

template<typename T>
int object_pool_server<T>::get_category(const T* objp) const
{
    return cat[get_index(objp)];
}

template<typename T>
int object_pool_server<T>::get_category(int index) const
{
    return cat[index];
}

template<typename T>
unsigned int object_pool_server<T>::bytes_allocated() const
{
    return max_capacity * (sizeof(T) + sizeof(index_link) + sizeof(unsigned char));
}

// <split_memblk> 0x00596730 (:67)
template<typename T>
void object_pool_server<T>::remove(index_list& list, int index)
{
    const unsigned int next = links[index].next;
    const unsigned int prev = links[index].prev;
    links[next].prev = prev;
    links[prev].next = next;
    if (index == list.head)
    {
        list.head = (int)next;
    }
    --list.count;
}

// <split_memblk> 0x00596760 (:87)
template<typename T>
void object_pool_server<T>::append(index_list& list, int index)
{
    // Inserts before the head, i.e. at the end of the circular list.
    if (list.count == 0)
    {
        list.head = index;
        links[index].prev = (unsigned int)index;
        links[index].next = (unsigned int)index;
        ++list.count;
        return;
    }
    links[index].prev = links[list.head].prev;
    links[index].next = (unsigned int)list.head;
    links[links[list.head].prev].next = (unsigned int)index;
    links[list.head].prev = (unsigned int)index;
    ++list.count;
}

// <split_memblk> 0x005967D0 (:112)
template<typename T>
int object_pool_server<T>::grab()
{
    if (avail.count < 1)
    {
        return -1;
    }
    const int index = avail.head;
    remove(avail, index);
    if (init_cb != nullptr)
    {
        (base[index].*init_cb)();
    }
    return index;
}

// <split_memblk> 0x00596820 (:131)
template<typename T>
void object_pool_server<T>::drop(int index)
{
    if ((unsigned int)index < max_capacity)
    {
        if (release_cb != nullptr)
        {
            (base[index].*release_cb)();
        }
        append(avail, index);
    }
}

// ---- object_pool_client<T, K>

// <split_memblk,1> 0x005B36D0 (.h:142)
template<typename T, int K>
object_pool_client<T, K>::object_pool_client(int __cs_debug_handle)
{
    server = nullptr;
    debug_flag = false;
    reset();
}

// <split_memblk,1> 0x00589DC0
template<typename T, int K>
object_pool_client<T, K>::~object_pool_client()
{
}

// <split_memblk,1> 0x00595880 (.h:114)
template<typename T, int K>
void object_pool_client<T, K>::init(object_pool_server<T>* servp)
{
    server = servp;
}

// <split_memblk,1> 0x00595DE0 (:149)
template<typename T, int K>
void object_pool_client<T, K>::reset()
{
    for (int i = 0; i < K; ++i)
    {
        in_use[i].head = 0;
        in_use[i].count = 0;
    }
}

template<typename T, int K>
int object_pool_client<T, K>::get_count(int category) const
{
    return in_use[category].count;
}

// <split_memblk,1> 0x005B3F50 (:160)
template<typename T, int K>
T* object_pool_client<T, K>::alloc(int category)
{
    csspinlock_scope lock(thread_access);
    const int index = server->grab();
    if (index < 0)
    {
        return nullptr;
    }
    server->append(in_use[category], index);
    server->cat[index] = (unsigned char)category;
    return server->base + index;
}

// <split_memblk,1> 0x005C0B00 (:181)
template<typename T, int K>
T* object_pool_client<T, K>::free(T* objp)
{
    // Returns the next object of the same list, so a list can be freed while walking it.
    csspinlock_scope lock(thread_access);
    T* next = get_next(objp);
    const int index = server->get_index(objp);
    const int category = server->cat[index];
    if (category != 0xFF)
    {
        server->remove(in_use[category], index);
        server->drop(index);
        server->cat[index] = 0xFF;
    }
    return next;
}

template<typename T, int K>
T* object_pool_client<T, K>::move(T* objp, int category)
{
    // Returns the next object of the list it left, as free() does, so a list can be emptied while walking it
    // (<action_script,4> 0x00B36510, <script_action,2> 0x00B36340).
    csspinlock_scope lock(thread_access);
    T* next = get_next(objp);
    const int index = server->get_index(objp);
    server->remove(in_use[server->cat[index]], index);
    server->append(in_use[category], index);
    server->cat[index] = (unsigned char)category;
    return next;
}

// <split_memblk,1> 0x005C1850 (:324)
template<typename T, int K>
void object_pool_client<T, K>::clear(int category)
{
    csspinlock_scope lock(thread_access);
    if (in_use[category].count != 0)
    {
        for (T* objp = server->base + in_use[category].head; objp != nullptr; objp = free(objp))
        {
        }
    }
}

// <split_memblk,1> 0x005C6750 (:314)
template<typename T, int K>
void object_pool_client<T, K>::clear()
{
    // Every category, as in <script_action,2> 0x00B3B5F0, <action_script,4> 0x00B3B640 and <script_timer,3> 0x00B3B7B0
    // (with K = 1, as in <split_memblk,1>, that is category 0 alone). Clearing category 0 only left each freed script's
    // completed actions allocated for good, so Script_actions could only fill up (inferred cause of a crash in
    // script_params_save on saving after a load: a full pool stops action_script::load early and the rest of the stream
    // loads as garbage scripts).
    for (int category = 0; category < K; category++)
    {
        clear(category);
    }
}

template<typename T, int K>
const T* object_pool_client<T, K>::get_first(int category) const
{
    return in_use[category].count != 0 ? server->base + in_use[category].head : nullptr;
}

// <split_memblk,1> 0x00596D70 (:226)
template<typename T, int K>
T* object_pool_client<T, K>::get_first(int category)
{
    return in_use[category].count != 0 ? server->base + in_use[category].head : nullptr;
}

template<typename T, int K>
const T* object_pool_client<T, K>::get_last(int category) const
{
    return in_use[category].count != 0 ? server->base + server->links[in_use[category].head].prev : nullptr;
}

template<typename T, int K>
T* object_pool_client<T, K>::get_last(int category)
{
    return in_use[category].count != 0 ? server->base + server->links[in_use[category].head].prev : nullptr;
}

template<typename T, int K>
const T* object_pool_client<T, K>::get_prev(const T* objp) const
{
    return const_cast<object_pool_client<T, K>*>(this)->get_prev(objp);
}

template<typename T, int K>
T* object_pool_client<T, K>::get_prev(const T* objp)
{
    if (objp == nullptr || server == nullptr || !server->contains(objp))
    {
        return nullptr;
    }
    const int index = server->get_index(objp);
    if (index == in_use[server->cat[index]].head)
    {
        return nullptr;
    }
    return server->base + server->links[index].prev;
}

template<typename T, int K>
const T* object_pool_client<T, K>::get_next(const T* objp) const
{
    return const_cast<object_pool_client<T, K>*>(this)->get_next(objp);
}

// <split_memblk,1> 0x005B49A0 (:238)
template<typename T, int K>
T* object_pool_client<T, K>::get_next(const T* objp)
{
    if (objp == nullptr || server == nullptr || !server->contains(objp))
    {
        return nullptr;
    }
    const int index = server->get_index(objp);
    const unsigned int next = server->links[index].next;
    if ((int)next == in_use[server->cat[index]].head)
    {
        return nullptr;
    }
    return server->base + next;
}

// ---- dynamic_object_pool<T, K>

// <split_memblk,1> 0x005C0490 (.h:200)
template<typename T, int K>
dynamic_object_pool<T, K>::dynamic_object_pool(void (T::*init_cb)(), void (T::*release_cb)(), int __cs_debug_handle)
    : object_pool_server<T>(init_cb, release_cb), object_pool_client<T, K>(__cs_debug_handle)
{
    object_pool_client<T, K>::init(this);
}

// <split_memblk,1> 0x005ABF10
template<typename T, int K>
dynamic_object_pool<T, K>::~dynamic_object_pool()
{
}

// <split_memblk,1> 0x005C0470 (.h:179)
template<typename T, int K>
void dynamic_object_pool<T, K>::init(mempool_base& pool, unsigned int size)
{
    object_pool_server<T>::init(&pool, size);
    object_pool_client<T, K>::reset();
}

// <split_memblk,1> 0x005B3560 (.h:187)
template<typename T, int K>
void dynamic_object_pool<T, K>::destroy()
{
}

// <multi_badge_field,1> 0x009877E0: a negative category counts every object in use (capacity less the free ones)
template<typename T, int K>
unsigned int dynamic_object_pool<T, K>::get_count(int category) const
{
    if (category < 0)
    {
        return object_pool_server<T>::get_capacity() - (unsigned int)object_pool_server<T>::get_free();
    }
    return (unsigned int)object_pool_client<T, K>::get_count(category);
}

// ---- object_pool<T, N, K>

template<typename T, int N, int K>
const unsigned int object_pool<T, N, K>::memory_footprint = N * (sizeof(T) + sizeof(index_link) + sizeof(unsigned char));

// <weapon,256,1> 0x00BE8F80 (.h:222)
template<typename T, int N, int K>
object_pool<T, N, K>::object_pool(void (T::*init_cb)(), void (T::*release_cb)(), int __cs_debug_handle)
    : dynamic_object_pool<T, K>(init_cb, release_cb, __cs_debug_handle)
{
    // The storage is carved out of data[] through a temporary pool (align 1).
    mempool pool(data, sizeof(data), "object pool", 1, 0);
    dynamic_object_pool<T, K>::init(pool, N);
}

// <weapon,256,1> 0x00BE0EE0
template<typename T, int N, int K>
object_pool<T, N, K>::~object_pool()
{
}
