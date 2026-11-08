// Original: volition/render_lib/code/rl_callback_widget.cpp
//
// rl_callback_widget_list: a circular list of callback widgets (the on-death callbacks of renderable instances). The
// header-inline members of rl_callback_widget.h are defined here as well.

#include "volition/render_lib/code/rl_callback_widget.h"

// 0x004BA4F0 (.h:32)
rl_callback_widget_list::rl_callback_widget_list()
{
    m_head = nullptr;
}

// Not in the binary: the unlink that the list functions inline (linked_list_p::remove).
void rl_callback_widget_list::unlink(rl_callback_widget* widget_p)
{
    if (m_head == widget_p)
    {
        if (widget_p->next == widget_p)
        {
            widget_p->next = nullptr;
            widget_p->prev = nullptr;
            m_head = nullptr;
            return;
        }
        m_head = widget_p->next;
    }
    if (widget_p->prev != nullptr)
    {
        widget_p->prev->next = widget_p->next;
    }
    if (widget_p->next != nullptr)
    {
        widget_p->next->prev = widget_p->prev;
    }
    widget_p->next = nullptr;
    widget_p->prev = nullptr;
}

// 0x004BA8B0 (:15)
// Unlinks all widgets (without calling them).
rl_callback_widget_list::~rl_callback_widget_list()
{
    rl_callback_widget* widget = m_head;
    while (widget != nullptr)
    {
        rl_callback_widget* following = widget->next != m_head ? widget->next : nullptr;
        unlink(widget);
        widget = following;
    }
}

// 0x004BA910 (:26)
// Appends at the end of the circular list.
void rl_callback_widget_list::add_widget(rl_callback_widget* widget_p)
{
    if (widget_p == nullptr)
    {
        return;
    }
    if (m_head == nullptr)
    {
        widget_p->next = widget_p;
        widget_p->prev = widget_p;
        m_head = widget_p;
        return;
    }
    widget_p->prev = m_head->prev;
    widget_p->next = m_head;
    m_head->prev->next = widget_p;
    m_head->prev = widget_p;
}

// 0x004BA950 (:38)
void rl_callback_widget_list::remove_widget(rl_callback_widget* widget_p)
{
    if (widget_p != nullptr)
    {
        unlink(widget_p);
    }
}

// 0x004BA9A0 (:61)
// Calls each widget; the next widget is taken before the call.
void rl_callback_widget_list::do_callbacks()
{
    rl_callback_widget* widget = m_head;
    while (widget != nullptr)
    {
        rl_callback_widget* next = widget->next;
        rl_callback_widget* head = m_head;
        widget->on_callback();
        widget = next != head ? next : nullptr;
    }
}

// 0x006E13A0 (.h:16)
// Notes (decompiled behaviour, kept as is): the links are left uninitialized.
rl_callback_widget::rl_callback_widget()
{
}

// 0x006E13C0 (.h:17)
rl_callback_widget::~rl_callback_widget()
{
}
