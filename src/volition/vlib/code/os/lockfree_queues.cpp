// Original: volition/vlib/code/os/lockfree_queues.cpp
//
// Intrusive lists (decompiled; header comment in lockfree_queues.h). fober_lock_free_queue is a doubly linked list
// between a sentinel head and tail under the base mutex: push links at the head, pop unlinks before the tail (FIFO).
//
// slist_lock_free_stack is a singly linked stack under the base mutex (the prev links are kept but never followed);
// count always returns 0.
//
// list_interface, fober_lock_free_queue, slist_lock_free_stack, t_base_node and the utility node pool (512 shared
// lf_node<void*>, pushed onto a stack on first use).

#include "volition/vlib/code/os/lockfree_queues.h"

// ---- t_base_node (header inline)
// 0x0058AF90 (.h:40)
void t_base_node::clear()
{
    next = nullptr;
    prev = nullptr;
}

// ---- list_interface (header inlines)
// 0x004BB980
list_interface::list_interface()
{
}

// 0x004BB940 (.h:62)
list_interface::~list_interface()
{
}

// ---- fober_lock_free_queue (header inlines)
// 0x004E2A20 (.h:193)
fober_lock_free_queue::fober_lock_free_queue()
{
    clear(); // inlined
}

// 0x004BB9A0 (.h:197)
fober_lock_free_queue::~fober_lock_free_queue()
{
}

// 0x004E2A90 (.h:201)
int fober_lock_free_queue::count()
{
    m_mutex.lock(true);
    int num = 0;
    for (t_base_node* node = m_head.next; node != &m_tail; node = node->next)
    {
        num++;
    }
    m_mutex.unlock();
    return num;
}

// 0x004BB9B0 (.h:220)
void fober_lock_free_queue::push(t_base_node* pnode)
{
    m_mutex.lock(true);
    pnode->next = m_head.next;
    pnode->prev = &m_head;
    m_head.next->prev = pnode;
    m_head.next = pnode;
    m_mutex.unlock();
}

// 0x004BB9F0 (.h:235)
// The oldest node, or nullptr when empty.
t_base_node* fober_lock_free_queue::pop()
{
    t_base_node* pnode = nullptr;
    m_mutex.lock(true);
    if (m_tail.prev != &m_head)
    {
        pnode = m_tail.prev;
        m_tail.prev = pnode->prev;
        pnode->prev->next = &m_tail;
        pnode->next = nullptr;
        pnode->prev = nullptr;
    }
    m_mutex.unlock();
    return pnode;
}

// 0x004BBA40 (.h:256)
// Forgets the nodes without touching them (no lock).
void fober_lock_free_queue::clear()
{
    m_head.prev = nullptr;
    m_head.next = &m_tail;
    m_tail.prev = &m_head;
    m_tail.next = nullptr;
}

// ---- slist_lock_free_stack (header inlines)
// 0x005BD200 (.h:103)
slist_lock_free_stack::slist_lock_free_stack()
{
    clear(); // inlined
}

// 0x005AD740
slist_lock_free_stack::~slist_lock_free_stack()
{
}

// 0x005BD250 (.h:110)
// Not counted (see the notes).
int slist_lock_free_stack::count()
{
    return 0;
}

// 0x005BD260 (.h:121)
void slist_lock_free_stack::push(t_base_node* pnode)
{
    m_mutex.lock(true);
    pnode->next = m_head.next;
    pnode->prev = &m_head;
    m_head.next = pnode;
    m_mutex.unlock();
}

// 0x005BD290 (.h:132)
// The newest node, or nullptr when empty.
t_base_node* slist_lock_free_stack::pop()
{
    m_mutex.lock(true);
    t_base_node* pnode = m_head.next;
    if (pnode != nullptr)
    {
        m_head.next = pnode->next;
        if (pnode->next != nullptr)
        {
            pnode->next->prev = &m_head;
        }
        pnode->next = nullptr;
        pnode->prev = nullptr;
    }
    m_mutex.unlock();
    return pnode;
}

// 0x005AD760 (.h:152)
void slist_lock_free_stack::clear()
{
    m_mutex.lock(true);
    m_head.next = nullptr;
    m_head.prev = nullptr;
    m_mutex.unlock();
}

// ---- utility node pool
lf_node_stack<void*> Utility_node_stack; // 0x0163E6D8
bool Utility_node_stack_initialized = false; // 0x01CE872E
// Magic number: 512 shared nodes.
lf_node<void*> Utility_nodes[512]; // 0x01CE2778

// 0x005BD370 (:64)
// A free node, or nullptr when all 512 are in use.
lf_node<void*>* get_utility_lockfree_node()
{
    if (!Utility_node_stack_initialized)
    {
        for (lf_node<void*>* pnode = Utility_nodes; pnode < Utility_nodes + 512; pnode++)
        {
            Utility_node_stack.push_node(pnode);
        }
        Utility_node_stack_initialized = true;
    }
    return Utility_node_stack.pop_node();
}

// 0x005AD7B0 (:80)
void free_utility_lockfree_node(lf_node<void*>* pnode)
{
    if (pnode != nullptr)
    {
        Utility_node_stack.push_node(pnode);
    }
}
