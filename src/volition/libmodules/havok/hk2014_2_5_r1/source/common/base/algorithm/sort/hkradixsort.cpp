// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/algorithm/sort/hkradixsort.cpp
//
// Havok 2014.2.5 (havok_impl.md HK1): least-significant-byte-first radix sort of key/user data pairs. sort16 sorts
// by the low key byte into the buffer, then by the high byte back into data; the order of equal keys is kept.
// Elements are processed in blocks of 4: the arrays must hold numObjects rounded up to a multiple of 4 (callers pad
// with the largest key).
// Not reimplemented: sort32 and sort64 (not in the subset).

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/algorithm/sort/hkradixsort.h"

// 0x00FB98C0 (:30)
void hkRadixSort::sort16(SortData16* data, int numObjects, SortData16* buffer)
{
    int counts0[256] = {}; // low key byte
    int counts1[256] = {}; // high key byte
    const int numPadded = numObjects > 0 ? (((numObjects - 1) >> 2) + 1) * 4 : 0;
    for (int i = 0; i < numPadded; i++)
    {
        counts0[data[i].m_keys[0]]++;
        counts1[data[i].m_keys[1]]++;
    }
    SortData16* dst0[256];
    SortData16* dst1[256];
    dst0[0] = buffer;
    dst1[0] = data;
    for (int i = 1; i < 256; i++)
    {
        dst0[i] = dst0[i - 1] + counts0[i - 1];
        dst1[i] = dst1[i - 1] + counts1[i - 1];
    }
    for (int i = 0; i < numPadded; i++)
    {
        *dst0[data[i].m_keys[0]]++ = data[i];
    }
    for (int i = 0; i < numPadded; i++)
    {
        *dst1[buffer[i].m_keys[1]]++ = buffer[i];
    }
}
