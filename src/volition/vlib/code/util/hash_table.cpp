// Original: volition/vlib/code/util/hash_table.cpp
//
// Hash functions of the vlib hash tables. uint_hash/uint64_hash use Thomas Wang's integer mix (with arithmetic
// shifts); string_hash is h = h * 33 ^ tolower(c). The hash_mix_32 family (inline in hash_table.h) is defined here
// too.

#include "volition/vlib/code/util/hash_table.h"

#include <ctype.h>

// 0x0058FDB0 (:5)
static int int_mix(int key)
{
    unsigned int value = (unsigned int)key + ~((unsigned int)key << 15);
    value = ((unsigned int)((int)value >> 10) ^ value) * 9;
    value ^= (unsigned int)((int)value >> 6);
    value += ~(value << 11);
    return (int)((unsigned int)((int)value >> 16) ^ value);
}

// 0x0058FDE0 (:18)
unsigned int uint_hash(unsigned int _key, unsigned int hash_size)
{
    return (unsigned int)int_mix((int)_key) % hash_size;
}

// 0x0058FE20 (:25)
unsigned int uint64_hash(uint64_t _key, unsigned int hash_size)
{
    // Both halves are mixed separately, then combined like the last mix step.
    unsigned int low = (unsigned int)_key + ~((unsigned int)_key << 15);
    low = ((unsigned int)((int)low >> 10) ^ low) * 9;
    low ^= (unsigned int)((int)low >> 6);
    low += ~(low << 11);
    unsigned int high = (unsigned int)(_key >> 32) + ~((unsigned int)(_key >> 32) << 15);
    high = ((unsigned int)((int)high >> 10) ^ high) * 9;
    high ^= (unsigned int)((int)high >> 6);
    high += ~(high << 11);
    const unsigned int combined = high ^ low;
    return ((unsigned int)((int)combined >> 16) ^ combined) % hash_size;
}

// 0x0058FE90 (:38)
unsigned int string_hash(const char* key, unsigned int hash_size)
{
    unsigned int hash = 0;
    for (; *key != '\0'; ++key)
    {
        hash = (tolower(*key) & 0xFF) ^ (hash * 33);
    }
    return hash % hash_size;
}

// 0x00482E50 (.h:447)
unsigned int hash_mix_32(unsigned int val)
{
    unsigned int value = val * 0x1001;
    value = ((value >> 22) ^ value) * 0x11;
    value = ((value >> 9) ^ value) * 0x401;
    value = ((value >> 2) ^ value) * 0x81;
    return (value >> 12) ^ value;
}

// 0x006E1100 (.h:461)
unsigned int hash_uint64(uint64_t val, unsigned int size)
{
    // As hash_mix_32 on both halves, combined before the last step.
    unsigned int high = (unsigned int)(val >> 32) * 0x1001;
    high = ((high >> 22) ^ high) * 0x11;
    high = ((high >> 9) ^ high) * 0x401;
    high = ((high >> 2) ^ high) * 0x81;
    unsigned int low = (unsigned int)val * 0x1001;
    low = ((low >> 22) ^ low) * 0x11;
    low = ((low >> 9) ^ low) * 0x401;
    low = ((low >> 2) ^ low) * 0x81;
    const unsigned int combined = low ^ high;
    return ((combined >> 12) ^ combined) % size;
}

// 0x00482E90 (.h:474)
unsigned int hash_uint(unsigned int val, unsigned int size)
{
    return hash_mix_32(val) % size;
}
