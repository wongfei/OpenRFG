// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/util/hkmemutil.cpp
//
// Havok 2014.2.5 (havok_impl.md HK1): memory copies. memCpy and memSet are the CRT functions; memMove copies
// backwards (widest aligned element first) when the destination is above the source, and uses the CRT memmove
// otherwise.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/util/hkmemutil.h"

#include <stdint.h>
#include <string.h>

namespace
{
// 0x00CF9070 <8>, 0x00CF90A0 <4>, 0x00CF90D0 <2>, 0x00CF9100 <1> (:49)
// Copies nbytes / N elements of N bytes, last first (the 8-byte form moves two 32-bit words per element).
template<int N, typename T>
void memCopyBackwards(void* dst, const void* src, int nbytes)
{
    T* d = static_cast<T*>(dst);
    const T* s = static_cast<const T*>(src);
    for (int i = (int)((unsigned int)nbytes / sizeof(T)) - 1; i >= 0; i--)
    {
        d[i] = s[i];
    }
}

// The <8> instance: element pairs of 32-bit words.
struct Words8
{
    uint32_t w[2];
};
}

// 0x00CF9050 (:64)
void hkMemUtil::memCpy(void* dst, const void* src, int nbytes)
{
    memcpy(dst, src, nbytes);
}

// 0x00CF9060 (:69)
void hkMemUtil::memSet(void* dst, const int c, int n)
{
    memset(dst, c, n);
}

// 0x00CF9120 (:74)
// The widest element size that divides nbytes and both addresses copies everything.
void hkMemUtil::memCpyBackwards(void* dst, const void* src, int nbytes)
{
    const uintptr_t d = (uintptr_t)dst;
    const uintptr_t s = (uintptr_t)src;
    if ((nbytes & ~7) != 0 && (nbytes & 7) == 0 && (d & 7) == 0 && (s & 7) == 0)
    {
        memCopyBackwards<8, Words8>(dst, src, nbytes);
        return;
    }
    if ((nbytes & ~3) != 0 && (nbytes & 3) == 0 && (d & 3) == 0 && (s & 3) == 0)
    {
        memCopyBackwards<4, uint32_t>(dst, src, nbytes);
        return;
    }
    if ((nbytes & ~1) != 0 && (nbytes & 1) == 0 && (d & 1) == 0 && (s & 1) == 0)
    {
        memCopyBackwards<2, uint16_t>(dst, src, nbytes);
        return;
    }
    memCopyBackwards<1, uint8_t>(dst, src, nbytes);
}

// 0x00CF9210 (:107)
void hkMemUtil::memMove(void* dst, const void* src, int nbytes)
{
    if (src < dst)
    {
        memCpyBackwards(dst, src, nbytes);
        return;
    }
    if (dst < src)
    {
        memmove(dst, src, nbytes);
    }
}
