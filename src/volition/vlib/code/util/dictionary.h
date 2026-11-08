// Original: volition/vlib/code/util/dictionary.h
//
// dictionary<T>, a sorted name to item table (console commands, script actions, functions and events). Written from
// the <console_command> instantiation (orig_main.md OM5: the developer console's commands).
// Notes (decompiled behaviour, kept as is): despite the name nothing is sorted; insert pushes the entry on the front
// of a singly linked list (word_list is the last inserted entry, prev the one inserted before it). The instance is a
// function static of get_instance, re-checked by every member.
#pragma once

#include "volition/vlib/code/util/string_comp.h"

// size (<console_command>) 0x4
template<typename T>
class dictionary
{
public:
    // size 0xC
    class entry
    {
    public:
        entry(const char* _word, T* _def); // <console_command> 0x00876420 (dictionary.inl:45)

    private:
        friend class dictionary<T>;
        entry* prev; // 0x00
    public:
        T* def; // 0x04
        const char* word; // 0x08
    };

private:
    dictionary(); // <console_command> 0x00596ED0 (dictionary.inl:23)
protected:
    static dictionary<T>* get_instance(); // <console_command> 0x005B4B30 (dictionary.inl:31)
public:
    static const char* find(const T*);
    static T* find(const char*);
    static void insert(entry* new_entry); // <console_command> 0x00866A00 (dictionary.inl:103)
    static entry* get_first(); // <console_command> 0x005C0DF0 (dictionary.inl:112)
    static entry* get_next(const entry* prev); // <console_command> 0x00596020 (dictionary.inl:120)

private:
    static dictionary<T>* singleton; // <console_command> 0x01D584CC
    entry* word_list; // 0x00
};

template<typename T>
dictionary<T>* dictionary<T>::singleton = nullptr;

// <console_command> 0x00596ED0 (dictionary.inl:23)
template<typename T>
dictionary<T>::dictionary()
{
    word_list = nullptr;
}

// <console_command> 0x005B4B30 (dictionary.inl:31)
template<typename T>
dictionary<T>* dictionary<T>::get_instance()
{
    static dictionary<T> the_dictionary;
    if (singleton == nullptr)
    {
        singleton = &the_dictionary;
    }
    return singleton;
}

// <console_command> 0x00876420 (dictionary.inl:45)
// The entries of rfg.exe's console commands are constant-initialized and inserted by their dynamic initializers.
template<typename T>
dictionary<T>::entry::entry(const char* _word, T* _def)
{
    word = _word;
    def = _def;
    prev = nullptr;
    dictionary<T>::get_instance();
    dictionary<T>::insert(this);
}

// <console_command> 0x00866A00 (dictionary.inl:103)
template<typename T>
void dictionary<T>::insert(entry* new_entry)
{
    new_entry->prev = get_instance()->word_list;
    get_instance()->word_list = new_entry;
}

// <console_command> 0x005C0DF0 (dictionary.inl:112)
template<typename T>
typename dictionary<T>::entry* dictionary<T>::get_first()
{
    return get_instance()->word_list;
}

// <console_command> 0x00596020 (dictionary.inl:120)
template<typename T>
typename dictionary<T>::entry* dictionary<T>::get_next(const entry* prev)
{
    if (prev == nullptr)
    {
        return nullptr;
    }
    return prev->prev;
}

// Linear, by word, ignoring case (script actions, functions and events are looked up while a script is parsed).
template<typename T>
T* dictionary<T>::find(const char* word)
{
    for (entry* cur = get_first(); cur != nullptr; cur = get_next(cur))
    {
        if (v_stricmp(cur->word, word) == 0)
        {
            return cur->def;
        }
    }
    return nullptr;
}

template<typename T>
const char* dictionary<T>::find(const T* def)
{
    for (entry* cur = get_first(); cur != nullptr; cur = get_next(cur))
    {
        if (cur->def == def)
        {
            return cur->word;
        }
    }
    return nullptr;
}
