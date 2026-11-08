// Original: volition/vlib/code/util/bitarray.cpp
//
// Bit set over a byte buffer (bit 0 is the low bit of byte 0). The size is kept in bytes.

#include "volition/vlib/code/util/bitarray.h"
#include "volition/vlib/code/memory/mempool_base.h"

#include <string.h>

// 0x0058EB50 (:19)
bit_array::bit_array()
{
    mem = nullptr;
    is_allocated = false;
    size = -1;
}

// 0x0058EB70 (:28): the buffer is not freed
bit_array::~bit_array()
{
    mem = nullptr;
    is_allocated = false;
    size = -1;
}

// 0x0058EBB0 (:58): Original behaviour kept: is_allocated stays false although the buffer comes from the pool.
void bit_array::init(mempool_base* pool, int _size)
{
    size = (_size + 7) >> 3;
    mem = (unsigned char*)pool->alloc(size, 0);
    is_allocated = false;
}

// 0x0058EB90 (:48): _size is in bits
void bit_array::init(unsigned char* _mem, int _size)
{
    size = (_size + 7) >> 3;
    mem = _mem;
    is_allocated = false;
}

// 0x0058EBE0 (:85)
void bit_array::clear(bool as_ones)
{
    memset(mem, as_ones ? 0xFF : 0, size);
}

// 0x0058EC10 (:97)
int bit_array::get(int index) const
{
    return (mem[index >> 3] & (unsigned char)(1 << (index & 7))) != 0 ? 1 : 0;
}

// 0x0058EC40 (:111)
void bit_array::set(int index, int value)
{
    unsigned char* byte = mem + (index >> 3);
    const unsigned char bit = (unsigned char)(1 << (index & 7));
    if (value != 0)
    {
        *byte |= bit;
    }
    else
    {
        *byte &= (unsigned char)~bit;
    }
}

// 0x0058EC70 (:130): the partial first and last bytes by mask, the whole bytes between by memset
void bit_array::set_span(int first_index, int num_bits, int value)
{
    const unsigned int last_index = (unsigned int)(first_index - 1 + num_bits);
    int first_byte = first_index >> 3;
    int last_byte = (int)last_index >> 3;
    const unsigned int first_bit = (unsigned int)first_index & 7;
    const unsigned int last_bit = last_index & 7;
    if (first_byte == last_byte)
    {
        unsigned char* byte = mem + first_byte;
        const unsigned char below = (unsigned char)((1 << first_bit) - 1);
        const unsigned char above = (unsigned char)~((1 << last_bit) * 2 - 1);
        if (value != 0)
        {
            *byte |= (unsigned char)(above ^ (unsigned char)~below);
        }
        else
        {
            *byte &= (unsigned char)(above | below);
        }
        return;
    }
    if (first_bit != 0)
    {
        unsigned char* byte = mem + first_byte;
        const unsigned char below = (unsigned char)((1 << first_bit) - 1);
        if (value == 0)
        {
            *byte &= below;
        }
        else
        {
            *byte |= (unsigned char)~below;
        }
        first_byte++;
    }
    if (last_bit != 7)
    {
        const unsigned char upto = (unsigned char)((1 << (last_bit + 1)) - 1);
        unsigned char* byte = mem + last_byte;
        if (value == 0)
        {
            *byte &= (unsigned char)~upto;
        }
        else
        {
            *byte |= upto;
        }
        last_byte--;
    }
    if (first_byte < last_byte) // original behaviour kept: a single whole byte left between the ends is not set
    {
        memset(mem + first_byte, value != 0 ? 0xFF : 0, last_byte - first_byte + 1);
    }
}
