// Original: volition/vlib/code/os/lockfree_queues.h
// Implementation: volition/vlib/code/os/lockfree_queues.cpp
//
// Intrusive lists used by the scheduler (job and trigger queues) and the utility node pool. Despite the names, both
// list_interface implementations carry a vlib mutex in the base (list_interface::m_mutex).
// lf_node<T> embeds a t_base_node in front of a value; lf_node_queue<T>/lf_node_stack<T> convert between the node,
// its link and its value. Instantiations in the binary: lf_node_queue<cpu_job|event_trigger|scheduling_trigger|
// unsigned int>, lf_node_stack<void*>.
// Method addresses of the templates are given for one instantiation (<cpu_job> for the queue, <void*> for the stack).
#pragma once

#include "volition/vlib/code/os/mutex.h"

#include <stddef.h>

// size 0x8
struct t_base_node
{
    void clear(); // 0x0058AF90 (.h:40)

    t_base_node* next; // 0x00
    t_base_node* prev; // 0x04
};

// size 0x3C
class list_interface
{
public:
    virtual ~list_interface(); // vtbl+0x00 0x004BB940 (.h:62)
    virtual void push(t_base_node*) = 0; // vtbl+0x04
    virtual t_base_node* pop() = 0; // vtbl+0x08
    virtual void clear() = 0; // vtbl+0x0C
    virtual int count() = 0; // vtbl+0x10
    list_interface(); // 0x004BB980

    // 0x00 vfptr
protected:
    mutex m_mutex; // 0x04
};

// size 0x44
class slist_lock_free_stack : public list_interface
{
public:
    virtual void push(t_base_node* pnode); // vtbl+0x04 0x005BD260 (.h:121)
    virtual t_base_node* pop(); // vtbl+0x08 0x005BD290 (.h:132)
    virtual void clear(); // vtbl+0x0C 0x005AD760 (.h:152)
    virtual int count(); // vtbl+0x10 0x005BD250 (.h:110)
    t_base_node* get_first(); // declared, never emitted
    t_base_node* get_next(t_base_node*); // declared, never emitted
    bool is_empty(); // declared, never emitted
    slist_lock_free_stack(); // 0x005BD200 (.h:103)
    virtual ~slist_lock_free_stack(); // vtbl+0x00 0x005AD740

    t_base_node m_head; // 0x3C
};

// FIFO with sentinel head and tail nodes (inferred from the layout).
// size 0x4C
class fober_lock_free_queue : public list_interface
{
public:
    virtual void push(t_base_node* pnode); // vtbl+0x04 0x004BB9B0 (.h:220)
    virtual t_base_node* pop(); // vtbl+0x08 0x004BB9F0 (.h:235)
    virtual void clear(); // vtbl+0x0C 0x004BBA40 (.h:256)
    virtual int count(); // vtbl+0x10 0x004E2A90 (.h:201)
    t_base_node* get_first(); // declared, never emitted
    t_base_node* get_next(t_base_node*); // declared, never emitted
    bool is_empty(); // declared, never emitted
    fober_lock_free_queue(); // 0x004E2A20 (.h:193)
    virtual ~fober_lock_free_queue(); // vtbl+0x00 0x004BB9A0 (.h:197)

private:
    t_base_node m_head; // 0x3C
    t_base_node m_tail; // 0x44
};

// size 0x8 + sizeof(T)
template<typename T>
struct lf_node
{
    lf_node(); // <cpu_job> 0x005AE0D0

    t_base_node base_node; // 0x00
    T value; // 0x08
};

// size 0x4C
template<typename T>
class lf_node_queue
{
public:
    void push_node(lf_node<T>* this_node); // <cpu_job> 0x00595770 (.h:270)
    lf_node<T>* pop_node(); // declared, never emitted
    void push(T* this_value); // <cpu_job> 0x005B3580 (.h:279), this_value points into an lf_node
    T* pop(); // <cpu_job> 0x005B35A0 (.h:284)
private:
    lf_node<T>* get_lf_node_from_base_node(t_base_node* this_base_node); // <cpu_job> 0x005958A0 (.h:293)
    lf_node<T>* get_lf_node_from_value(T* this_value); // <cpu_job> 0x005958B0 (.h:300)
public:
    lf_node_queue(); // <cpu_job> 0x005AE170
    ~lf_node_queue(); // <cpu_job> 0x005AE1C0

private:
    fober_lock_free_queue m_queue; // 0x00
};

// size 0x44
template<typename T>
class lf_node_stack
{
public:
    void push_node(lf_node<T>* this_node); // <void*> 0x00595760 (.h:316)
    lf_node<T>* pop_node(); // <void*> 0x005B3570 (.h:318)
    void push(T*); // declared, never emitted
    T* pop(); // declared, never emitted
private:
    lf_node<T>* get_lf_node_from_base_node(t_base_node* this_base_node); // <void*> 0x00595890 (.h:338)
    lf_node<T>* get_lf_node_from_value(T*); // declared, never emitted
public:
    lf_node_stack(); // <void*> 0x005BD320
    ~lf_node_stack(); // <void*> 0x005AD790

private:
    slist_lock_free_stack m_stack; // 0x00
};

// Shared pool of lf_node<void*> for ad-hoc lists.
lf_node<void*>* get_utility_lockfree_node(); // 0x005BD370 (:64)
void free_utility_lockfree_node(lf_node<void*>* pnode); // 0x005AD7B0 (:80)

// ---- template members (inline in the original header)

// <cpu_job> 0x005AE0D0
// The value is default constructed (cpu_job's constructor in that instantiation); the link is left as is.
template<typename T>
lf_node<T>::lf_node()
{
}

// <cpu_job> 0x005AE170
template<typename T>
lf_node_queue<T>::lf_node_queue()
{
}

// <cpu_job> 0x005AE1C0
template<typename T>
lf_node_queue<T>::~lf_node_queue()
{
}

// <cpu_job> 0x00595770 (.h:270)
template<typename T>
void lf_node_queue<T>::push_node(lf_node<T>* this_node)
{
    m_queue.push(&this_node->base_node);
}

// <cpu_job> 0x005B3580 (.h:279)
template<typename T>
void lf_node_queue<T>::push(T* this_value)
{
    m_queue.push(&get_lf_node_from_value(this_value)->base_node);
}

// <cpu_job> 0x005B35A0 (.h:284)
// The value of the oldest node, or nullptr when empty.
template<typename T>
T* lf_node_queue<T>::pop()
{
    t_base_node* pnode = m_queue.pop();
    if (pnode == nullptr)
    {
        return nullptr;
    }
    return &get_lf_node_from_base_node(pnode)->value;
}

// <cpu_job> 0x005958A0 (.h:293)
template<typename T>
lf_node<T>* lf_node_queue<T>::get_lf_node_from_base_node(t_base_node* this_base_node)
{
    return (lf_node<T>*)this_base_node; // base_node is the first member
}

// <cpu_job> 0x005958B0 (.h:300)
template<typename T>
lf_node<T>* lf_node_queue<T>::get_lf_node_from_value(T* this_value)
{
    return (lf_node<T>*)((unsigned char*)this_value - offsetof(lf_node<T>, value));
}

// <void*> 0x005BD320
template<typename T>
lf_node_stack<T>::lf_node_stack()
{
}

// <void*> 0x005AD790
template<typename T>
lf_node_stack<T>::~lf_node_stack()
{
}

// <void*> 0x00595760 (.h:316)
template<typename T>
void lf_node_stack<T>::push_node(lf_node<T>* this_node)
{
    m_stack.push(&this_node->base_node);
}

// <void*> 0x005B3570 (.h:318)
// The newest node, or nullptr when empty.
template<typename T>
lf_node<T>* lf_node_stack<T>::pop_node()
{
    return get_lf_node_from_base_node(m_stack.pop());
}

// <void*> 0x00595890 (.h:338)
template<typename T>
lf_node<T>* lf_node_stack<T>::get_lf_node_from_base_node(t_base_node* this_base_node)
{
    return (lf_node<T>*)this_base_node; // base_node is the first member
}
