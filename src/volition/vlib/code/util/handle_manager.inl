// Original: volition/vlib/code/util/handle_manager.inl (template code, included by handle_manager.h)
//
// Partial: everything but save/load (never emitted). The round-robin search: from the last slot of the last bin, the
// next free bit; a full bin (or the end of one) moves on to the next bin, wrapping to bin 0, and coming back to the
// starting bin means every handle is taken.
//
// Notes (decompiled behaviour, kept as is):
// - The wrap to bin 0 skips the "back at the start" check, so a full manager whose search started in bin 0 never
//   returns.
#pragma once

#include <string.h>

// <7280> 0x00925F00 (:39)
template<int N>
handle_manager<N>::handle_manager()
{
    memset(status, 0, sizeof(status));
    last_bin = 0;
    last_slot = 0;
    upper_bits = 0xFFFF0000;
}

// <7280> 0x0091F280 (:53)
template<int N>
void handle_manager<N>::clear()
{
    memset(status, 0, sizeof(status));
    last_bin = 0;
    last_slot = 0;
    upper_bits = 0xFFFF0000;
}

// <7280> 0x0091F2C0 (:69)
template<int N>
void handle_manager<N>::set_upper_bits(unsigned short bits)
{
    upper_bits = (unsigned int)bits << 16;
}

// <7280> 0x0091F2E0 (:78)
template<int N>
unsigned int handle_manager<N>::allocate_handle()
{
    const unsigned int first_bin = last_bin;
    unsigned int bin = first_bin;
    unsigned int slot;
    do
    {
        if (status[bin] == 0xFF || last_slot > 7)
        {
            bin++;
            last_bin = bin;
            last_slot = 0;
            if (bin < sizeof(status))
            {
                if (bin == first_bin)
                {
                    return 0xFFFFFFFF;
                }
            }
            else
            {
                last_bin = 0;
            }
        }
        bin = last_bin;
        // Bits 8 and up of the shifted mask fall out of the byte, so last_slot 8 ends the search in this bin.
        while ((status[bin] & (unsigned char)(1 << (last_slot & 0x1F))) != 0 && last_slot < 8)
        {
            last_slot++;
        }
        slot = last_slot;
    } while (slot > 7 || (status[bin] & (unsigned char)(1 << (slot & 0x1F))) != 0);
    status[bin] |= (unsigned char)(1 << (slot & 0x1F));
    last_slot++;
    return upper_bits | (slot + bin * 8);
}

// <7280> 0x0091F3B0 (:127)
template<int N>
void handle_manager<N>::reserve_handle(unsigned int handle)
{
    status[(handle >> 3) & 0x1FFF] |= (unsigned char)(1 << (handle & 7));
}

// <7280> 0x0091F3D0 (:142)
template<int N>
void handle_manager<N>::release_handle(unsigned int handle)
{
    status[(handle >> 3) & 0x1FFF] &= (unsigned char)~(1 << (handle & 7));
}

// <7280> 0x0091F3F0 (:157)
template<int N>
bool handle_manager<N>::is_handle_allocated(unsigned int handle)
{
    return (status[(handle >> 3) & 0x1FFF] & (unsigned char)(1 << (handle & 7))) != 0;
}
