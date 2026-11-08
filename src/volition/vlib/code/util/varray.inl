// Original: volition/vlib/code/util/varray.h (inline template members; included at the end of varray.h)
//
// Partial: the constructors (farray's copy too), destructors, init, add, copy, find, contains, remove, remove_matching,
// remove_unsorted, remove_all, resize, size, capacity,
// full and operator[](int) of base_array and farray. The rest is not implemented yet.
#pragma once

#include "volition/vlib/code/math/rand.h"
#include "volition/vlib/code/vlib_core/vlib_assert.h"

// <int> 0x007054F0
template<typename T>
base_array<T>::base_array()
{
    // elt, array_size and num are left to the derived class (farray sets them).
}

// <int> 0x00707680 (.h:94)
template<typename T>
base_array<T>::~base_array()
{
}

// <int> 0x00707690 (.h:98)
template<typename T>
void base_array<T>::init(T* data_ptr, int size, int num_existing_elements)
{
    if (size < num_existing_elements)
    {
        VLIB_CRITICAL_FAILURE();
        num_existing_elements = size;
    }
    num = num_existing_elements;
    array_size = size;
    elt = data_ptr;
}

// <int> 0x0070AC60 (.h:156)
template<typename T>
int base_array<T>::add(const T& t)
{
    const int index = num++;
    if (num > array_size)
    {
        VLIB_CRITICAL_FAILURE();
        num = index;
        return -1;
    }
    elt[index] = t;
    return index;
}

// <int> 0x008A2650 (.h:248)
template<typename T>
int base_array<T>::contains(const T& t) const
{
    // The number of elements equal to t.
    int count = 0;
    for (int i = 0; i < num; i++)
    {
        if (elt[i] == t)
        {
            count++;
        }
    }
    return count;
}

// Written through find; rfg.exe has its own instances of it, for example 0x00778A90.
template<typename T>
bool base_array<T>::is_member(const T& t) const
{
    return find(t, false) != -1;
}

// <int> 0x008A3690 (.h:261)
template<typename T>
int base_array<T>::find(const T& t, bool from_end) const
{
    if (from_end)
    {
        for (int i = num - 1; i >= 0; i--)
        {
            if (elt[i] == t)
            {
                return i;
            }
        }
        return -1;
    }
    for (int i = 0; i < num; i++)
    {
        if (elt[i] == t)
        {
            return i;
        }
    }
    return -1;
}

// <int> 0x00987550 (.h:283)
// A random element (the global rand, int_rand_range); false when the array is empty.
template<typename T>
bool base_array<T>::get_random_element(T& returned_value) const
{
    if (num <= 0)
    {
        return false;
    }
    int index = 0;
    if (num != 1)
    {
        index = int_rand_range(0, num - 1);
        if (index < 0)
        {
            return false;
        }
    }
    if (index >= num)
    {
        return false;
    }
    returned_value = elt[index];
    return true;
}

// <int> 0x008A2680 (.h:320)
template<typename T>
void base_array<T>::remove(int i)
{
    num--;
    for (; i < num; i++)
    {
        elt[i] = elt[i + 1];
    }
}

// <int> 0x0077B640 (.h:127): the other array's elements added in order (past the capacity: the critical message, the
// rest dropped)
template<typename T>
void base_array<T>::copy(const base_array<T>& other_array)
{
    num = 0;
    for (int i = 0; i < other_array.num; i++)
    {
        add(other_array.elt[i]);
    }
}

// <int> 0x008BC090 (.h:306): every element equal to t, the order kept
template<typename T>
void base_array<T>::remove_matching(const T& t)
{
    int i = 0;
    while (i < num)
    {
        if (elt[i] == t)
        {
            remove(i);
        }
        else
        {
            i++;
        }
    }
}

// <int> 0x00B6F780 (.h:335)
template<typename T>
bool base_array<T>::remove_unsorted(int i)
{
    if (i < num - 1)
    {
        elt[i] = elt[num - 1];
        num--;
        return true;
    }
    num--;
    return false;
}

// Inlined everywhere (no instantiation recorded); the behaviour is world::free_object's: a size above the capacity is
// refused with a critical failure.
template<typename T>
bool base_array<T>::resize(int n)
{
    if (n > array_size)
    {
        VLIB_CRITICAL_FAILURE();
        return false;
    }
    num = n;
    return true;
}

// (inlined in rl_scene_renderer::render)
template<typename T>
void base_array<T>::clear()
{
    num = 0;
}

// <int> 0x00762500 (.h:347)
template<typename T>
void base_array<T>::remove_all()
{
    num = 0;
}

// Inlined everywhere (rl_visibility_manager): the next slot, constructed by the caller; null with a critical failure
// when the array is full.
template<typename T>
T* base_array<T>::add_in_place()
{
    const int index = num;
    if (array_size < index + 1)
    {
        VLIB_CRITICAL_FAILURE();
        return nullptr;
    }
    num = index + 1;
    return &elt[index];
}

// <human_info> 0x00A74100: the next slot and its index (constructed by the caller); null with a critical failure when
// the array is full (the index is set anyway).
template<typename T>
T* base_array<T>::add_in_place(int& array_index)
{
    array_index = num;
    if (array_size < num + 1)
    {
        VLIB_CRITICAL_FAILURE();
        return nullptr;
    }
    num++;
    return &elt[array_index];
}

// <int> 0x008BC020 (.h:237): the index of an equal element, else the index of t added (-1 when full).
template<typename T>
int base_array<T>::add_unique(const T& t)
{
    for (int i = 0; i < num; i++)
    {
        if (elt[i] == t)
        {
            return i;
        }
    }
    return add(t);
}

// The element storage.
template<typename T>
T* base_array<T>::elements()
{
    return elt;
}

// <int> 0x007076D0 (.h:360)
template<typename T>
int base_array<T>::size() const
{
    return num;
}

// <int> 0x007076E0 (.h:366)
template<typename T>
int base_array<T>::capacity() const
{
    return array_size;
}

// <int> 0x008A26B0 (.h:372)
template<typename T>
bool base_array<T>::full() const
{
    return array_size <= num;
}

// <int> 0x00766380 (.h:409)
template<typename T>
const T& base_array<T>::operator[](int i) const
{
    return elt[i];
}

// <int> 0x007076F0 (.h:399)
template<typename T>
T& base_array<T>::operator[](int i)
{
    return elt[i];
}

// <int,16> 0x00778DC0 (.h:512)
template<typename T, int N>
farray<T, N>::farray()
{
    this->num = 0;
    this->array_size = N;
    this->elt = data;
}

// <human_info_inventory_info,15> 0x007EF130 (.h:517). Original: a copy of itself leaves the array uninitialized.
template<typename T, int N>
farray<T, N>::farray(const farray<T, N>& farray_copy)
{
    if (&farray_copy == this)
    {
        return;
    }
    int num_elements = farray_copy.num;
    if (num_elements > N)
    {
        VLIB_CRITICAL_FAILURE();
        num_elements = N;
    }
    this->num = num_elements;
    this->array_size = N;
    this->elt = data;
    this->copy(farray_copy);
}

// <rl_light*> 0x004D6D70
// Quicksort with the first element as the pivot (cmple: less or equal); recurses on the left part, loops on the
// right part.
template<typename T>
void base_array<T>::quick_sort(T* a, int orig_left, int orig_right, bool (*cmple)(const T&, const T&))
{
    while (orig_left < orig_right)
    {
        int last = orig_left;
        for (int i = orig_left + 1; i <= orig_right; i++)
        {
            if (cmple(a[i], a[orig_left]) && i != ++last)
            {
                const T swap = a[i];
                a[i] = a[last];
                a[last] = swap;
            }
        }
        if (orig_left != last)
        {
            const T swap = a[orig_left];
            a[orig_left] = a[last];
            a[last] = swap;
        }
        quick_sort(a, orig_left, last - 1, cmple);
        orig_left = last + 1;
    }
}

// (inlined in rl_scene_renderer::render_lighting_passes)
template<typename T>
void base_array<T>::sort(bool (*cmple)(const T&, const T&))
{
    quick_sort(elt, 0, num - 1, cmple);
}

// <int,16> 0x00762A40 (.h:523)
template<typename T, int N>
farray<T, N>::~farray()
{
}

// <int,16> 0x008A26C0 (.h:548)
template<typename T, int N>
const T& farray<T, N>::operator[](int i) const
{
    return this->elt[i];
}

// <int,16> 0x00762A50 (.h:538)
template<typename T, int N>
T& farray<T, N>::operator[](int i)
{
    return this->elt[i];
}

// <int,16> 0x008A26D0 (.h:558)
template<typename T, int N>
T& farray<T, N>::operator[](unsigned int i)
{
    return this->elt[i];
}

// ---- circ_array (P4)

// <int,30> 0x00B6F6B0 (.h:623)
template<typename T, int N>
circ_array<T, N>::circ_array()
    : num(0), offset(0)
{
}

// <int,30> 0x00B6F6C0 (.h:630)
template<typename T, int N>
circ_array<T, N>::~circ_array()
{
}

// <int,30> 0x00B6F6D0 (.h:635)
template<typename T, int N>
int circ_array<T, N>::size() const
{
    return num;
}

// <int,30> 0x00B6F6E0 (.h:642): the i-th newest element.
template<typename T, int N>
const T& circ_array<T, N>::operator[](int i) const
{
    return data[(offset - i + N) % N];
}

template<typename T, int N>
T& circ_array<T, N>::operator[](int i)
{
    return data[((offset - i) + N) % N];
}

// <int,30> 0x00B6F700 (.h:656): the new element becomes the newest; a full array overwrites the oldest. Returns its
// slot (VFX.md FX7: the player's blood decal queue).
template<typename T, int N>
void circ_array<T, N>::remove_all()
{
    num = 0;
    offset = 0;
}

template<typename T, int N>
int circ_array<T, N>::add(const T& t)
{
    if (num == 0)
    {
        data[0] = t;
        num = 1;
        return offset;
    }
    if (num < N)
    {
        offset++;
        num++;
        data[offset] = t;
        return offset;
    }
    offset = (offset + 1) % N;
    data[offset] = t;
    return offset;
}
