// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/string/hkstring.cpp
//
// Havok 2014.2.5 (stub_impl.md S4, serializer SA): the C string and memory helpers of hkString, from rfg.exe. Only
// the functions the serializer reaches are ported so far; the printf and number parsing wrappers use a "C" locale
// of their own (cLocale), so the game's locale does not change the decimal point.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/string/hkstring.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/router/hkmemoryrouter.h"

#include <locale.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 0x01092B00 (dynamic initializer)
static _locale_t cLocale = _create_locale(LC_ALL, "C");

// 0x00CFEDE0: ASCII only
char hkString::toLower(char c)
{
    if ((unsigned char)(c - 'A') < 26)
    {
        c = c + ('a' - 'A');
    }
    return c;
}

// 0x00CFEDF0
int hkString::vsnprintf(char* buf, int len, const char* fmt, void* hkargs)
{
    return _vsnprintf_l(buf, len, fmt, cLocale, (va_list)hkargs);
}

// 0x00CFEE20
int hkString::snprintf(char* buf, int len, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    const int n = _vsnprintf_l(buf, len, fmt, cLocale, args);
    va_end(args);
    return n;
}

// 0x00CFEE70: -1, 0 or 1, the characters compared unsigned
int hkString::strCmp(const char* a, const char* b)
{
    while (true)
    {
        const unsigned char ca = (unsigned char)*a;
        const unsigned char cb = (unsigned char)*b;
        if (ca != cb)
        {
            return ca < cb ? -1 : 1;
        }
        if (ca == 0)
        {
            return 0;
        }
        a++;
        b++;
    }
}

// 0x00CFEEA0: a jump to the CRT strncmp
int hkString::strNcmp(const char* a, const char* b, int n)
{
    return strncmp(a, b, n);
}

// 0x00CFEEB0: ASCII case folding; a shorter string compares as its terminating 0
int hkString::strCasecmp(const char* a, const char* b)
{
    for (int i = 0;; i++)
    {
        if (a[i] == 0 && b[i] == 0)
        {
            return 0;
        }
        const char la = toLower(a[i]);
        const char lb = toLower(b[i]);
        if (la < lb)
        {
            return -1;
        }
        if (lb < la)
        {
            return 1;
        }
    }
}

// 0x00CFEF60
void hkString::strCpy(char* dst, const char* src)
{
    char c;
    do
    {
        c = *src++;
        *dst++ = c;
    } while (c != 0);
}

// 0x00CFEF80: strncpy (no terminator past n), nothing for n 0
void hkString::strNcpy(char* dst, const char* src, int n)
{
    if (n != 0)
    {
        strncpy(dst, src, n);
    }
}

// 0x00CFEFA0
int hkString::strLen(const char* src)
{
    const char* p = src;
    while (*p != 0)
    {
        p++;
    }
    return (int)(p - src);
}

// 0x00CFF020: strtoul, so a negative number wraps (the locale is not used)
int hkString::atoi(const char* in, int base)
{
    return (int)strtoul(in, nullptr, base);
}

// 0x00CFF040
int64_t hkString::atoll(const char* in, int base)
{
    return _strtoi64_l(in, nullptr, base, cLocale);
}

// 0x00CFF060
uint64_t hkString::atoull(const char* in, int base)
{
    return _strtoui64_l(in, nullptr, base, cLocale);
}

// 0x00CFF080: strtod rounded to float
float hkString::atof(const char* in)
{
    return (float)_strtod_l(in, nullptr, cLocale);
}

// 0x00CFF0B0: a jump to the CRT strchr
const char* hkString::strChr(const char* haystack, int needle)
{
    return strchr(haystack, needle);
}

// 0x00CFF0C0: a jump to the CRT strrchr
const char* hkString::strRchr(const char* haystack, int needle)
{
    return strrchr(haystack, needle);
}

// 0x00CFF0D0: a copy from the given allocator (easyAlloc: the size is kept with the block)
char* hkString::strDup(const char* src, hkMemoryAllocator& alloc)
{
    const int len = strLen(src);
    char* dst = (char*)hkMemoryRouter::easyAlloc(alloc, len + 1);
    strCpy(dst, src);
    return dst;
}

// 0x00CFF100
void hkString::strFree(char* s, hkMemoryAllocator& alloc)
{
    hkMemoryRouter::easyFree(alloc, s);
}

// 0x00CFF1C0: a jump to the CRT memcpy
void hkString::memCpy(void* dst, const void* src, int n)
{
    memcpy(dst, src, (size_t)n);
}

// 0x00CFF1D0: a jump to the CRT memmove
void hkString::memMove(void* dst, const void* src, int n)
{
    memmove(dst, src, (size_t)n);
}

// 0x00CFF1E0: a jump to the CRT memset
void hkString::memSet(void* dst, const int c, int n)
{
    memset(dst, c, (size_t)n);
}

// 0x00CFF1F0
// Original behaviour kept: whole words are compared first, then the bytes of the first differing word (or the
// remaining 1 to 3 bytes); the result is -1 or 1, never the difference.
int hkString::memCmp(const void* buf1, const void* buf2, int n)
{
    const unsigned char* p = (const unsigned char*)buf1;
    const unsigned char* q = (const unsigned char*)buf2;
    for (; (unsigned int)n > 3; n -= 4)
    {
        if (*(const int*)p != *(const int*)q)
        {
            goto bytes;
        }
        p += 4;
        q += 4;
    }
    if (n == 0)
    {
        return 0;
    }
bytes:
    int d = (int)p[0] - (int)q[0];
    if (d == 0)
    {
        if ((unsigned int)n < 2)
        {
            return 0;
        }
        d = (int)p[1] - (int)q[1];
        if (d == 0)
        {
            if ((unsigned int)n < 3)
            {
                return 0;
            }
            d = (int)p[2] - (int)q[2];
            if (d == 0)
            {
                if ((unsigned int)n < 4)
                {
                    return 0;
                }
                d = (int)p[3] - (int)q[3];
            }
        }
    }
    return (d >> 31) | 1;
}

// 0x00CFF460: from the thread's heap
void hkString::strFree(char* s)
{
    strFree(s, hkMemoryRouter::getInstance().heap());
}

// 0x00CFF480: on the thread's heap
char* hkString::strDup(const char* src)
{
    return strDup(src, hkMemoryRouter::getInstance().heap());
}
