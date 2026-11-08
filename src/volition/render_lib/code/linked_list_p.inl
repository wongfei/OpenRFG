// Original: volition/render_lib/code/linked_list_p.h (inline template members; included at the end of linked_list_p.h)
//
// Partial: construction, init, first, append, insert_before, insert_after, remove, concat, is_empty, get_elem_count and get_indexed. The list is circular through T::prev/T::next; m_head_p is the first
// element. The addresses are of the <object_effect::registered_expression_variable> instantiation.
#pragma once

// 0x007441A0 (.h:109)
template<typename T>
linked_list_p<T>::linked_list_p()
    : m_head_p(nullptr)
    , m_elem_count(0)
{
}

// 0x007441B0 (.h:110)
template<typename T>
linked_list_p<T>::~linked_list_p()
{
}

// 0x007441C0 (.h:112)
template<typename T>
void linked_list_p<T>::init()
{
    m_head_p = nullptr;
    m_elem_count = 0;
}

// 0x005D2ED0 (.h:113)
template<typename T>
T* linked_list_p<T>::first() const
{
    return m_head_p;
}

// 0x007441D0 (.h:116)
template<typename T>
void linked_list_p<T>::append(T* elem_p)
{
    if (m_head_p == nullptr)
    {
        elem_p->next = elem_p;
        elem_p->prev = elem_p;
        m_elem_count++;
        m_head_p = elem_p;
        return;
    }
    elem_p->prev = m_head_p->prev;
    elem_p->next = m_head_p;
    m_head_p->prev->next = elem_p;
    m_head_p->prev = elem_p;
    m_elem_count++;
}

// <rl_partition_sector_entry> 0x004D5E30
template<typename T>
void linked_list_p<T>::remove(T* elem_p)
{
    if (m_head_p == elem_p)
    {
        if (elem_p->next == elem_p)
        {
            elem_p->next = nullptr;
            elem_p->prev = nullptr;
            m_elem_count--;
            m_head_p = nullptr;
            return;
        }
        m_head_p = elem_p->next;
    }
    if (elem_p->prev != nullptr)
    {
        elem_p->prev->next = elem_p->next;
    }
    if (elem_p->next != nullptr)
    {
        elem_p->next->prev = elem_p->prev;
    }
    elem_p->next = nullptr;
    elem_p->prev = nullptr;
    m_elem_count--;
}

// Inlined everywhere.
template<typename T>
bool linked_list_p<T>::is_empty() const
{
    return m_elem_count == 0;
}

// Inlined everywhere.
template<typename T>
int linked_list_p<T>::get_elem_count() const
{
    return m_elem_count;
}

// <vfx_particle_parametric> 0x006E4F20, <vfx_particle_system_emitter_instance> 0x005D67C0
// The element at index from the head, or null past the end or for an empty list (index 0 of an empty list is null).
template<typename T>
T* linked_list_p<T>::get_indexed(int index) const
{
    T* elem_p = m_head_p;
    if (elem_p != nullptr)
    {
        for (int i = 0; i != index; i++)
        {
            elem_p = elem_p->next;
            if (elem_p == m_head_p || elem_p == nullptr)
            {
                return nullptr;
            }
        }
    }
    return elem_p;
}

// Inlined (<rl_renderable_particles_instance> in rl_renderable_particles_instance_group::add_instance): elem_p goes
// right before before_p (which must be in the list), becoming the head when before_p was.
template<typename T>
void linked_list_p<T>::insert_before(T* before_p, T* elem_p)
{
    before_p->prev->next = elem_p;
    elem_p->prev = before_p->prev;
    before_p->prev = elem_p;
    elem_p->next = before_p;
    if (m_head_p == before_p)
    {
        m_head_p = elem_p;
    }
    m_elem_count++;
}

// Inlined (<vfx_particle_system_emitter_instance> in vfx_effect::emit_node_emitters_spawn): elem_p goes right after
// after_p (which must be in the list).
template<typename T>
void linked_list_p<T>::insert_after(T* after_p, T* elem_p)
{
    elem_p->next = after_p->next;
    elem_p->prev = after_p;
    after_p->next->prev = elem_p;
    after_p->next = elem_p;
    m_elem_count++;
}

// <vfx_particle_iterative> 0x00CA97E0 (.h:141): moves every element of list_b to the end of this list.
template<typename T>
void linked_list_p<T>::concat(linked_list_p<T>& list_b)
{
    if (list_b.m_head_p == nullptr)
    {
        return;
    }
    m_elem_count += list_b.m_elem_count;
    if (m_head_p == nullptr)
    {
        m_head_p = list_b.m_head_p;
        list_b.m_head_p = nullptr;
        list_b.m_elem_count = 0;
        return;
    }
    m_head_p->prev->next = list_b.m_head_p;
    list_b.m_head_p->prev->next = m_head_p;
    T* const last_b = list_b.m_head_p->prev;
    list_b.m_head_p->prev = m_head_p->prev;
    m_head_p->prev = last_b;
    list_b.m_head_p = nullptr;
    list_b.m_elem_count = 0;
}
