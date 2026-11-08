// Original: volition/render_lib/code/linked_list_p.h
//
// Seam only (Tier 6): the intrusive list template of render_lib, used by object_effect for its expression variables.
#pragma once

// size 0x8
// Intrusive doubly linked list of T (T provides get_prev/set_prev/get_next/set_next). Methods are in
// linked_list_p.h (instantiations listed by VA).
template<typename T>
class linked_list_p
{
public:
    class iterator;

    linked_list_p(); // <object_effect::registered_expression_variable> 0x007441A0 (.h:109)
    ~linked_list_p(); // <object_effect::registered_expression_variable> 0x007441B0 (.h:110)
    void init(); // <object_effect::registered_expression_variable> 0x007441C0 (.h:112)
    T* first() const; // <object_effect::registered_expression_variable> 0x005D2ED0 (.h:113)
    T* last() const;
    void append(T* elem_p); // <object_effect::registered_expression_variable> 0x007441D0 (.h:116)
    void insert_front(T*);
    void insert_before(T*, T*);
    void insert_after(T*, T*);
    void remove(T*);
    void concat(linked_list_p<T>&);
    bool is_empty() const;
    int get_elem_count() const;
    T* get_indexed(int) const;
    iterator begin() const;
    iterator end() const;
    bool validate_list();

private:
    T* m_head_p; // 0x00
    int m_elem_count; // 0x04
};

#include "volition/render_lib/code/linked_list_p.inl"
