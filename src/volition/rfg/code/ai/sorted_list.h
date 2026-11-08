// Original: volition/rfg/code/ai/sorted_list.h
//
// sorted_list<T, N, K>: a fixed list of N elements kept sorted by a key of type K (ascending or descending), on top of
// sorted_list_base<T, K> which works on an external element buffer; the _with_identity variants add unique insertion.
// Used for cover, navpoint, guard group and turret candidates and for bullet hit lists (weapons).
#pragma once

// size (<weapon_fire_info,float>) 0x60
template<typename T, typename K>
struct sorted_list_element
{
    sorted_list_element(); // <weapon_fire_info,float> 0x00A22CF0

    T data; // 0x00
    K val; // 0x54
    sorted_list_element<T, K>* prev; // 0x58
    sorted_list_element<T, K>* next; // 0x5C
};

// size (<weapon_fire_info,float>) 0x10
template<typename T, typename K>
class sorted_list_base
{
public:
    sorted_list_base(bool sort_ascending_, sorted_list_element<T,K>* buffer, int num); // <weapon_fire_info,float> 0x00A244B0 (sorted_list.inl:16)
    void clear();
    int size() const; // <weapon_fire_info,float> 0x00BDB080 (.h:38)
    bool add(const T& data, K val); // <weapon_fire_info,float> 0x00BDB280 (sorted_list.inl:49)
    const T* get_first() const; // <weapon_fire_info,float> 0x00A21E20 (.h:41)
    const T* get_next(const T* prev) const; // <weapon_fire_info,float> 0x00BDB090 (.h:44)
    const T* get_prev(const T*) const;
    const T* get_last() const; // <weapon_fire_info,float> 0x00BDB0B0 (.h:52)
protected:
    K get_value(const T*) const;

    bool sort_ascending; // 0x00
    int num_elements; // 0x04
    sorted_list_element<T,K>* free_list; // 0x08
    sorted_list_element<T,K>* in_use_list; // 0x0C
};

// size (<weapon_fire_info,50,float>) 0x12D0
template<typename T, int N, typename K>
class sorted_list : public sorted_list_base<T,K>
{
public:
    sorted_list(bool sort_ascending_); // <weapon_fire_info,50,float> 0x00A462C0 (.h:102)

protected:
    sorted_list_element<T,K> elements[N]; // 0x10
};

// size (<human const*,float>) 0x10
template<typename T, typename K>
class sorted_list_base_with_identity : public sorted_list_base<T,K>
{
public:
    enum add_unique_replace_modes
    {
        NEVER_REPLACE = 0,
        ALWAYS_REPLACE = 1,
        REPLACE_IF_LESS_THAN = 2,
        REPLACE_IF_GREATER_THAN = 3,
        NUM_ADD_UNIQUE_REPLACE_MODES = 4,
    };

    sorted_list_base_with_identity(bool sort_ascending_, sorted_list_element<T,K>* buffer, int num); // <human const*,float> 0x00648EE0 (.h:89)
    bool contains(const T&) const;
    bool add_unique(const T& data, K val, add_unique_replace_modes replace_mode); // <human const*,float> 0x00649CA0 (sorted_list.inl:124)
private:
    sorted_list_element<T,K>* find(const T& data) const; // <human const*,float> 0x005FA790 (sorted_list.inl:98)

};

// size (<human const*,3,float>) 0x40
template<typename T, int N, typename K>
class sorted_list_with_identity : public sorted_list_base_with_identity<T,K>
{
public:
    sorted_list_with_identity(bool sort_ascending_); // <human const*,3,float> 0x00671980 (.h:112)

protected:
    sorted_list_element<T,K> elements[N]; // 0x10
};

// template instance: void __thiscall sorted_list_base_with_identity<human const*,float>::sorted_list_base_with_identity<human const*,float>(bool sort_ascending_, sorted_list_element<human const*,float>* buffer, int num); // 0x00648EE0 (.h:89)
// template instance: void __thiscall sorted_list<turret*,2,int>::sorted_list<turret*,2,int>(bool sort_ascending_); // 0x00A47460 (.h:102)
// template instance: void __thiscall sorted_list<guard_node_group_list const*,16,int>::sorted_list<guard_node_group_list const*,16,int>(bool sort_ascending_); // 0x00648590 (.h:102)
// template instance: void __thiscall sorted_list<navpoint*,10,float>::sorted_list<navpoint*,10,float>(bool sort_ascending_); // 0x00648AF0 (.h:102)
// template instance: void __thiscall sorted_list<cover_node*,10,float>::sorted_list<cover_node*,10,float>(bool sort_ascending_); // 0x006481F0 (.h:102)
// template instance: void __thiscall sorted_list<weapon_fire_info,50,float>::sorted_list<weapon_fire_info,50,float>(bool sort_ascending_); // 0x00A462C0 (.h:102)
// template instance: void __thiscall sorted_list_with_identity<human const*,3,float>::sorted_list_with_identity<human const*,3,float>(bool sort_ascending_); // 0x00671980 (.h:112)
// template instance: void __thiscall sorted_list_base<guard_node_group_list const*,int>::sorted_list_base<guard_node_group_list const*,int>(bool sort_ascending_, sorted_list_element<guard_node_group_list const*,int>* buffer, int num); // 0x005F9470 (sorted_list.inl:16)
// template instance: void __thiscall sorted_list_base<weapon_fire_info,float>::sorted_list_base<weapon_fire_info,float>(bool sort_ascending_, sorted_list_element<weapon_fire_info,float>* buffer, int num); // 0x00A244B0 (sorted_list.inl:16)
// template instance: void __thiscall sorted_list_base<turret*,int>::sorted_list_base<turret*,int>(bool sort_ascending_, sorted_list_element<turret*,int>* buffer, int num); // 0x00A261F0 (sorted_list.inl:16)
// template instance: void __thiscall sorted_list_base<navpoint*,float>::sorted_list_base<navpoint*,float>(bool sort_ascending_, sorted_list_element<navpoint*,float>* buffer, int num); // 0x005F9970 (sorted_list.inl:16)
// template instance: void __thiscall sorted_list_base<human const*,float>::sorted_list_base<human const*,float>(bool sort_ascending_, sorted_list_element<human const*,float>* buffer, int num); // 0x005F8F60 (sorted_list.inl:16)
// template instance: void __thiscall sorted_list_base<cover_node*,float>::sorted_list_base<cover_node*,float>(bool sort_ascending_, sorted_list_element<cover_node*,float>* buffer, int num); // 0x005F8FD0 (sorted_list.inl:16)

#include "volition/rfg/code/ai/sorted_list.inl"
