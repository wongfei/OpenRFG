// Original: volition/vlib/code/util/bit_util.h
//
// Bit and byte swapping helpers (packet float swap; templates listed as comments).
#pragma once

#include <string.h>

// 0x00941970 (.h:108): the float's bytes reversed (network order). rfg.exe returns the result on the x87 stack (fld),
// which turns a signalling NaN pattern into a quiet one (bit 22 set); the port does the same so that both ends see the
// bits rfg.exe would.
inline float PACKET_SWAP_FLOAT(float x)
{
    unsigned int bits;
    memcpy(&bits, &x, sizeof(bits));
    bits = (bits >> 24) | ((bits >> 8) & 0xFF00) | ((bits << 8) & 0xFF0000) | (bits << 24);
    if ((bits & 0x7F800000) == 0x7F800000 && (bits & 0x007FFFFF) != 0 && (bits & 0x00400000) == 0)
    {
        bits |= 0x00400000;
    }
    float result;
    memcpy(&result, &bits, sizeof(result));
    return result;
}
// <unsigned int> 0x0096B590 (bit_util.inl:18): the set bits, a nybble at a time from a table
template<typename T>
int bit_count(T mask)
{
    static const char NYBBLE_BIT_COUNTS[16] = {0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4};
    int count = 0;
    for (unsigned int i = 0; i < sizeof(T) * 2; i++)
    {
        count += NYBBLE_BIT_COUNTS[(mask >> (i * 4)) & 0xF];
    }
    return count;
}
