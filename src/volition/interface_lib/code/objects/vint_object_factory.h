// Original: volition/interface_lib/code/objects/vint_object_factory.h
//
// The object factory: vint_object_factory_imp<T> creates and pools one element or animation class; each class
// registers a Factory_<class> instance.
#pragma once


class vint_object_base;

// size 0x4
class vint_object_factory
{
public:
    virtual ~vint_object_factory(); // vtbl+0x00 0x0045BD90 (.h:19)
    virtual vint_object_base* allocate() = 0; // vtbl+0x04
    virtual void release(vint_object_base*) = 0; // vtbl+0x08
    virtual unsigned int get_index(const vint_object_base*) const = 0; // vtbl+0x0C
    virtual vint_object_base* get_at_index(unsigned int) const = 0; // vtbl+0x10
    virtual unsigned int for_each_active(bool (*)(vint_object_base*)) = 0; // vtbl+0x14
    vint_object_factory(); // 0x004673E0

    // 0x00 vfptr
};

// size (<vint_element_map>) 0x18
template<typename T>
class vint_object_factory_imp : public vint_object_factory
{
public:
    vint_object_factory_imp(); // <vint_element_map> 0x004707E0 (.h:42)
    virtual ~vint_object_factory_imp(); // vtbl+0x00 <vint_element_map> 0x00467350 (.h:43)
    bool init(unsigned int _max_objects); // <vint_element_map> 0x0047D2B0 (.h:93)
    virtual vint_object_base* allocate(); // vtbl+0x04 <vint_element_map> 0x00470820 (.h:116)
    virtual void release(vint_object_base* _obj); // vtbl+0x08 <vint_element_map> 0x00470890 (.h:140)
    virtual unsigned int get_index(const vint_object_base* _obj) const; // vtbl+0x0C <vint_element_map> 0x00470910 (.h:155)
    virtual vint_object_base* get_at_index(unsigned int _index) const; // vtbl+0x10 <vint_element_map> 0x00470950 (.h:165)
    virtual unsigned int for_each_active(bool (*cb)(vint_object_base*)); // vtbl+0x14 <vint_element_map> 0x00470970 (.h:74)

private:
    vint_object_base* m_avail; // 0x04
    vint_object_base* m_active; // 0x08
    unsigned int m_object_count; // 0x0C
    unsigned int m_max_objects; // 0x10
    T* m_array; // 0x14
};

// ---- vint_object_factory_imp's members (UI.md UI2): the objects are an array from Vint_perm_mempool (pool operator
// new[], with the count before the array); the free and active lists are circular lists through the objects'
// factory_next/factory_prev.

#include "volition/interface_lib/code/internal/vint_dlist.h"
#include "volition/interface_lib/code/internal/vint_memory.h"
#include "volition/interface_lib/code/objects/vint_object_base.h"
#include "volition/vlib/code/memory/mempool_base.h"
#include "library/modules/runtime/base/include/keen/base/trace.hpp"

#include <new>

// .h:42
template<typename T>
vint_object_factory_imp<T>::vint_object_factory_imp()
{
    m_array = nullptr;
    m_active = nullptr;
    m_avail = nullptr;
    m_object_count = 0;
}

// .h:43
template<typename T>
vint_object_factory_imp<T>::~vint_object_factory_imp()
{
}

// .h:74
template<typename T>
unsigned int vint_object_factory_imp<T>::for_each_active(bool (*cb)(vint_object_base*))
{
    if (cb == nullptr)
    {
        return 0;
    }
    unsigned int count = 0;
    vint_object_base* obj = m_active;
    if (obj == nullptr)
    {
        return 0;
    }
    // stops at the first object the callback returns false for
    while (cb(obj))
    {
        obj = obj->factory_next;
        ++count;
        if (obj == m_active || obj == nullptr)
        {
            break;
        }
    }
    return count;
}

// .h:93
template<typename T>
bool vint_object_factory_imp<T>::init(unsigned int _max_objects)
{
    if (Vint_perm_mempool == nullptr || m_array != nullptr)
    {
        return false;
    }
    // pool operator new[]: the element count in the 4 bytes before the array
    unsigned int* block = (unsigned int*)Vint_perm_mempool->alloc(_max_objects * sizeof(T) + 4, 0);
    T* array = nullptr;
    if (block == nullptr)
    {
        keen::debug::nativePrintArgs("Pool operator new out of memory\n");
    }
    else
    {
        *block = _max_objects;
        array = (T*)(block + 1);
        for (unsigned int i = 0; i < _max_objects; ++i)
        {
            new (&array[i]) T();
        }
    }
    m_array = array;
    if (array == nullptr)
    {
        return false;
    }
    m_object_count = 0;
    m_max_objects = _max_objects;
    m_avail = nullptr;
    m_active = nullptr;
    for (unsigned int i = 0; i < m_max_objects; ++i)
    {
        vint_dlist_add<vint_object_base>(m_avail, &m_array[i], &vint_object_base::factory_next, &vint_object_base::factory_prev);
    }
    return true;
}

// .h:116
template<typename T>
vint_object_base* vint_object_factory_imp<T>::allocate()
{
    vint_object_base* obj = m_avail;
    if (obj != nullptr)
    {
        vint_dlist_remove(m_avail, obj, &vint_object_base::factory_next, &vint_object_base::factory_prev);
        vint_dlist_add(m_active, obj, &vint_object_base::factory_next, &vint_object_base::factory_prev);
        ++m_object_count;
    }
    return obj;
}

// .h:140
template<typename T>
void vint_object_factory_imp<T>::release(vint_object_base* _obj)
{
    if (_obj == nullptr)
    {
        return;
    }
    vint_dlist_remove(m_active, _obj, &vint_object_base::factory_next, &vint_object_base::factory_prev);
    vint_dlist_add(m_avail, _obj, &vint_object_base::factory_next, &vint_object_base::factory_prev);
    --m_object_count;
}

// .h:155
template<typename T>
unsigned int vint_object_factory_imp<T>::get_index(const vint_object_base* _obj) const
{
    // Original: the byte distance is divided before the range check, as a signed value.
    unsigned int index = (unsigned int)(((const char*)_obj - (const char*)m_array) / (int)sizeof(T));
    if ((const void*)_obj < (const void*)m_array || index >= m_max_objects)
    {
        return 0xFFFFFFFF;
    }
    return index;
}

// .h:165
template<typename T>
vint_object_base* vint_object_factory_imp<T>::get_at_index(unsigned int _index) const
{
    if (m_array != nullptr && _index < m_max_objects)
    {
        return &m_array[_index];
    }
    return nullptr;
}
