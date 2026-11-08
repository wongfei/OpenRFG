// Original: volition/render_lib/code/rl_lf_hash.h
// Implementation: volition/render_lib/code/rl_lf_hash.cpp
//
// rl_lf_hash<K, V, H>, a lock-free open-addressing hash (used to batch materials across threads) and
// rl_lf_static_hash<K, V, H, N> with inline storage. Written by hand; the addresses are of the <uint64,
// rl_batched_material_info, &rl_hash_uint64> instantiation.
#pragma once

#include <intrin.h>
#include <stdint.h>
#include <string.h>

unsigned int rl_hash_uint64(uint64_t val); // 0x004BA1C0 (.h:229)

// size 0xC
template<typename K, typename V, unsigned int (*H)(K)>
class rl_lf_hash
{
public:
    // size 0x10
    struct hash_entry
    {
        V* data_value; // 0x00
        K key_value; // 0x08
    };

    rl_lf_hash(); // 0x004D6BC0 (.h:39)
    ~rl_lf_hash(); // 0x004D6BD0 (.h:40)
    void clear(); // 0x004D86B0 (.h:97)
    V* find_or_insert(K key, V* value); // 0x004D86D0 (.h:145)
    V* find(K key); // 0x004D87E0 (.h:106)
protected:
    void init_internal(hash_entry* entry_array, unsigned int* hash_array, unsigned int hash_size); // 0x004D8880 (.h:85)

    volatile hash_entry* m_entry_p; // 0x00
    volatile unsigned int* m_hash_p; // 0x04
    unsigned int m_hash_size; // 0x08
};

// size 0xA010 (<..., 2048>)
template<typename K, typename V, unsigned int (*H)(K), unsigned int N>
class rl_lf_static_hash : public rl_lf_hash<K, V, H>
{
public:
    rl_lf_static_hash(); // 0x00502650 (.h:66)
    ~rl_lf_static_hash(); // 0x00502680 (.h:67)

private:
    typename rl_lf_hash<K, V, H>::hash_entry m_entry_array[N]; // 0x10
    unsigned int m_hash_array[N]; // 0x8010
};

// template instance: void __thiscall rl_lf_hash<unsigned __int64,rl_batched_material_info,&rl_hash_uint64>::rl_lf_hash<unsigned __int64,rl_batched_material_info,&rl_hash_uint64>(); // 0x004D6BC0 (.h:39)
// template instance: void __thiscall rl_lf_hash<unsigned __int64,rl_batched_material_info,&rl_hash_uint64>::~rl_lf_hash<unsigned __int64,rl_batched_material_info,&rl_hash_uint64>(); // 0x004D6BD0 (.h:40)
// template instance: void __thiscall rl_lf_static_hash<unsigned __int64,rl_batched_material_info,&rl_hash_uint64,2048>::rl_lf_static_hash<unsigned __int64,rl_batched_material_info,&rl_hash_uint64,2048>(); // 0x00502650 (.h:66)
// template instance: void __thiscall rl_lf_static_hash<unsigned __int64,rl_batched_material_info,&rl_hash_uint64,2048>::~rl_lf_static_hash<unsigned __int64,rl_batched_material_info,&rl_hash_uint64,2048>(); // 0x00502680 (.h:67)
unsigned int rl_hash_mix_32(unsigned int val); // 0x004BA180 (.h:215)
unsigned int rl_hash_uint64(uint64_t val); // 0x004BA1C0 (.h:229)

// ---- inline members (no .cpp home)

// Hash slot values besides a stored hash: free, and taken by a writer.
static const unsigned int RL_LF_HASH_EMPTY = 0xFFFFFFFF;
static const unsigned int RL_LF_HASH_LOCKED = 0xFFFFFFFE;

// 0x004D6BC0 (.h:39)
template<typename K, typename V, unsigned int (*H)(K)>
inline rl_lf_hash<K, V, H>::rl_lf_hash()
{
}

// 0x004D6BD0 (.h:40)
template<typename K, typename V, unsigned int (*H)(K)>
inline rl_lf_hash<K, V, H>::~rl_lf_hash()
{
}

// 0x004D8880 (.h:85)
template<typename K, typename V, unsigned int (*H)(K)>
inline void rl_lf_hash<K, V, H>::init_internal(hash_entry* entry_array, unsigned int* hash_array, unsigned int hash_size)
{
    m_entry_p = entry_array;
    m_hash_p = hash_array;
    m_hash_size = hash_size;
    memset((void*)hash_array, 0xFF, hash_size * sizeof(unsigned int));
}

// 0x004D86B0 (.h:97)
template<typename K, typename V, unsigned int (*H)(K)>
inline void rl_lf_hash<K, V, H>::clear()
{
    memset((void*)m_hash_p, 0xFF, m_hash_size * sizeof(unsigned int));
}

// 0x004D87E0 (.h:106)
// Linear probing from the key's slot; waits on slots a writer holds. Null when the key is not stored.
template<typename K, typename V, unsigned int (*H)(K)>
inline V* rl_lf_hash<K, V, H>::find(K key)
{
    const unsigned int hash = H(key);
    unsigned int slot = hash % m_hash_size;
    // The slot before the start, where a full table ends the probe.
    const unsigned int last = (slot == 0 ? m_hash_size : slot) - 1;
    for (;;)
    {
        unsigned int value;
        do
        {
            value = m_hash_p[slot];
        } while (value == RL_LF_HASH_LOCKED);
        if (value == RL_LF_HASH_EMPTY)
        {
            return nullptr;
        }
        if (value == hash && m_entry_p[slot].key_value == key)
        {
            return m_entry_p[slot].data_value;
        }
        if (slot == last)
        {
            return nullptr;
        }
        slot = (slot + 1) % m_hash_size;
    }
}

// 0x004D86D0 (.h:145)
// find, taking each probed slot while it is looked at; an empty slot gets the key and value (when value is not null).
// Returns the stored value: the existing one, value, or null (full table, or value null and the key not stored).
template<typename K, typename V, unsigned int (*H)(K)>
inline V* rl_lf_hash<K, V, H>::find_or_insert(K key, V* value)
{
    const unsigned int hash = H(key);
    unsigned int slot = hash % m_hash_size;
    const unsigned int last = (slot == 0 ? m_hash_size : slot) - 1;
    for (;;)
    {
        unsigned int current;
        do
        {
            current = m_hash_p[slot];
        } while (current == RL_LF_HASH_LOCKED || (unsigned int)_InterlockedCompareExchange((volatile long*)&m_hash_p[slot], (long)RL_LF_HASH_LOCKED, (long)current) != current);
        if (current == RL_LF_HASH_EMPTY)
        {
            break;
        }
        m_hash_p[slot] = current;
        if (current == hash && m_entry_p[slot].key_value == key)
        {
            return m_entry_p[slot].data_value;
        }
        if (slot == last)
        {
            return nullptr;
        }
        slot = (slot + 1) % m_hash_size;
    }
    if (value != nullptr)
    {
        m_entry_p[slot].data_value = value;
        m_entry_p[slot].key_value = key;
        m_hash_p[slot] = hash;
        return value;
    }
    m_hash_p[slot] = RL_LF_HASH_EMPTY;
    return value;
}

// 0x00502650 (.h:66)
template<typename K, typename V, unsigned int (*H)(K), unsigned int N>
inline rl_lf_static_hash<K, V, H, N>::rl_lf_static_hash()
{
    this->init_internal(m_entry_array, m_hash_array, N);
}

// 0x00502680 (.h:67)
template<typename K, typename V, unsigned int (*H)(K), unsigned int N>
inline rl_lf_static_hash<K, V, H, N>::~rl_lf_static_hash()
{
}

// Not in the binary: the first steps of rl_hash_mix_32 (without the final shift), shared by both hashes.
inline unsigned int rl_hash_mix_32_partial(unsigned int val)
{
    unsigned int mixed = ((val * 0x1001) >> 22 ^ val * 0x1001) * 0x11;
    mixed = (mixed >> 9 ^ mixed) * 0x401;
    return (mixed >> 2 ^ mixed) * 0x81;
}

// 0x004BA180 (.h:215)
// Magic numbers: integer hash mixing multipliers and shifts.
inline unsigned int rl_hash_mix_32(unsigned int val)
{
    const unsigned int mixed = rl_hash_mix_32_partial(val);
    return mixed >> 12 ^ mixed;
}

// 0x004BA1C0 (.h:229)
// Both halves mixed, combined, then the final shift of rl_hash_mix_32.
inline unsigned int rl_hash_uint64(uint64_t val)
{
    const unsigned int mixed = rl_hash_mix_32_partial((unsigned int)val) ^ rl_hash_mix_32_partial((unsigned int)(val >> 32));
    return mixed >> 12 ^ mixed;
}
