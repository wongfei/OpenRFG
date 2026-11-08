// OpenRFG helper (not in rfg.pdb): the circular doubly linked lists interface_lib inlines everywhere (factory free/active
// lists, name and handle hash chains, documents). A list is a head pointer; an empty list is null; a single node links
// to itself. Removal clears the node's links. rfg.exe expands these as macros at each use.
#pragma once

template<typename T>
inline void vint_dlist_add(T*& head, T* item, T* T::*next, T* T::*prev)
{
    if (head == nullptr)
    {
        item->*next = item;
        item->*prev = item;
        head = item;
        return;
    }
    // add at the tail (before the head)
    item->*prev = head->*prev;
    item->*next = head;
    (head->*prev)->*next = item;
    head->*prev = item;
}

template<typename T>
inline void vint_dlist_remove(T*& head, T* item, T* T::*next, T* T::*prev)
{
    if (head == item && (head = item->*next) == item)
    {
        head = nullptr;
    }
    else
    {
        (item->*prev)->*next = item->*next;
        (item->*next)->*prev = item->*prev;
    }
    item->*prev = nullptr;
    item->*next = nullptr;
}
