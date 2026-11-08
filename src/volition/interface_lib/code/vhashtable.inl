// Original: volition/interface_lib/code/vhashtable.inl
//
// vhashtable's methods (UI.md UI2): an open-addressing table of references (linear probing). H::NULL_REFERENCE marks a
// free slot; H::hash gives the start index and H::valid matches a stored reference against the key. m_search_limit is
// the longest probe any insert needed, and bounds the searches. The one instantiation is the bitmap sheets' image
// lookup (bitmap_sheets.cpp).
#pragma once

#include "volition/interface_lib/code/vhashtable.h"

// <checksum_stri,unsigned int,bitmap_image_hasher> 0x00467480 (vhashtable.inl:22)
template<typename K, typename V, typename H>
vhashtable<K, V, H>::vhashtable()
{
    m_table_size = 0;
    m_table = nullptr;
    m_num_in_table = 0;
    m_search_limit = 0;
}

// <checksum_stri,unsigned int,bitmap_image_hasher> 0x004672C0 (.h:70)
template<typename K, typename V, typename H>
vhashtable<K, V, H>::~vhashtable()
{
}

// <checksum_stri,unsigned int,bitmap_image_hasher> 0x004674A0 (vhashtable.inl:31)
template<typename K, typename V, typename H>
void vhashtable<K, V, H>::initialize(V* table_buffer, unsigned int table_size)
{
    m_table = table_buffer;
    m_table_size = table_size;
    clear();
}

// <checksum_stri,unsigned int,bitmap_image_hasher> 0x00467EE0 (vhashtable.inl:48)
template<typename K, typename V, typename H>
unsigned int vhashtable<K, V, H>::key_to_index(H& functor, const K& key) const
{
    return functor.hash(key) % m_table_size;
}

// <checksum_stri,unsigned int,bitmap_image_hasher> 0x00470D90 (vhashtable.inl:70)
template<typename K, typename V, typename H>
bool vhashtable<K, V, H>::insert(const K& key, const V& reference, H& functor)
{
    if (m_num_in_table == m_table_size)
    {
        return false;
    }
    unsigned int index = key_to_index(functor, key);
    unsigned int probes = 0;
    for (unsigned int i = 0; i < m_table_size; ++i)
    {
        if (m_table[index] == (V)H::NULL_REFERENCE)
        {
            break;
        }
        ++index;
        ++probes;
        if (index == m_table_size)
        {
            index = 0;
        }
    }
    if (probes >= m_search_limit)
    {
        m_search_limit = probes + 1;
    }
    m_table[index] = reference;
    ++m_num_in_table;
    return true;
}

// <checksum_stri,unsigned int,bitmap_image_hasher> 0x00470DF0 (vhashtable.inl:137)
template<typename K, typename V, typename H>
bool vhashtable<K, V, H>::find(V*& p_reference, const K& key, H& functor)
{
    unsigned int index = key_to_index(functor, key);
    for (unsigned int i = 0; i < m_search_limit; ++i)
    {
        if (m_table[index] != (V)H::NULL_REFERENCE && functor.valid(m_table[index], key))
        {
            p_reference = &m_table[index];
            return true;
        }
        ++index;
        if (index == m_table_size)
        {
            index = 0;
        }
    }
    p_reference = nullptr;
    return false;
}

// <checksum_stri,unsigned int,bitmap_image_hasher> 0x004674C0 (vhashtable.inl:198)
template<typename K, typename V, typename H>
bool vhashtable<K, V, H>::remove(const K& key, H& functor)
{
    V* reference;
    if (!find(reference, key, functor))
    {
        return false;
    }
    *reference = (V)H::NULL_REFERENCE;
    --m_num_in_table;
    return true;
}

// <checksum_stri,unsigned int,bitmap_image_hasher> 0x00467500 (vhashtable.inl:221)
template<typename K, typename V, typename H>
void vhashtable<K, V, H>::clear()
{
    m_num_in_table = 0;
    m_search_limit = 1;
    for (unsigned int i = 0; i < m_table_size; ++i)
    {
        m_table[i] = (V)H::NULL_REFERENCE;
    }
}
