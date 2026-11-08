// Original: volition/render_lib/code/pool_list.h (inline template members; included at the end of pool_list.h)
//
// Partial: the members the object pools use. The addresses are of the <rl_light*> instantiation.
#pragma once

#include "volition/vlib/code/vlib_core/vlib_assert.h"

// <rl_light*> 0x00502930 (.h:21)
template<typename T>
pool_list<T>::pool_list()
    : m_init_dynamic(false)
    , m_min_used(0xFFFFFFFF)
{
    this->num = 0;
    this->array_size = 0;
    // Magic number: an invalid element pointer until init or init_dynamic.
    this->elt = (T*)0xFFFFFFFF;
}

// <rl_light*> 0x00502950 (.h:28)
template<typename T>
pool_list<T>::~pool_list()
{
    if (m_init_dynamic)
    {
        delete[] this->elt;
    }
}

// <rl_light*> 0x00505940 (.h:40)
template<typename T>
T pool_list<T>::remove_unsorted_return(int i)
{
    T result = this->elt[i];
    if (i < this->num - 1)
    {
        this->elt[i] = this->elt[this->num - 1];
    }
    this->num--;
    return result;
}

// <rl_light*> 0x0051E030 (.h:48)
template<typename T>
T pool_list<T>::remove_back()
{
    // m_min_used is the low-water mark of the pool.
    if ((unsigned int)(this->num - 1) < m_min_used)
    {
        m_min_used = (unsigned int)(this->num - 1);
    }
    this->num--;
    return this->elt[this->num];
}

// <rl_light*> 0x00502980 (.h:62)
template<typename T>
void pool_list<T>::clear()
{
    this->num = 0;
}

// <rl_light*> 0x00502990 (.h:67)
template<typename T>
void pool_list<T>::init(T* data_ptr, int size, int num_existing_elements)
{
    if (size < num_existing_elements)
    {
        VLIB_CRITICAL_FAILURE();
        num_existing_elements = size;
    }
    this->num = num_existing_elements;
    this->array_size = size;
    this->elt = data_ptr;
}

// <rl_light*> 0x005029D0 (.h:80)
template<typename T>
void pool_list<T>::init_dynamic(int count)
{
    T* elements = new T[count];
    int num_elements = 0;
    if (count < 0)
    {
        VLIB_CRITICAL_FAILURE();
        num_elements = count;
    }
    this->array_size = count;
    this->elt = elements;
    this->num = num_elements;
    m_init_dynamic = true;
}

// <rl_light*> 0x004D51A0 (.h:88)
template<typename T>
T& pool_list<T>::operator[](int i)
{
    return this->elt[i];
}

// (inlined in rl_visibility_manager)
template<typename T>
const T& pool_list<T>::operator[](int i) const
{
    return this->elt[i];
}
