// Original: volition/vlib/code/util/string_comp.cpp
//
// Case-insensitive comparisons. v_stricmp/v_strnicmp (char) fold only ASCII A-Z, compare signed chars, return -1/0/1 and
// order a null string before any other; the _v_ variants use toupper and return the difference. The wchar_t versions
// (v_ inline in the header, _v_ here) use towupper.

#include "volition/vlib/code/util/string_comp.h"

#include <ctype.h>
#include <wctype.h>

static inline char string_comp_fold_ascii(char c)
{
    return (unsigned char)(c - 'A') < 26 ? (char)(c + ' ') : c;
}

// 0x0045AFD0 (.h:60)
int v_stricmp(const wchar_t* str1, const wchar_t* str2)
{
    int difference = (int)towupper(*str1) - (int)towupper(*str2);
    while (difference == 0)
    {
        if (*str1 == L'\0')
        {
            return 0;
        }
        ++str1;
        ++str2;
        difference = (int)towupper(*str1) - (int)towupper(*str2);
    }
    return difference;
}

// 0x0045AFE0 (.h:61)
int v_strnicmp(const wchar_t* str1, const wchar_t* str2, unsigned int n)
{
    for (; n != 0; --n, ++str1, ++str2)
    {
        const int difference = (int)towupper(*str1) - (int)towupper(*str2);
        if (difference != 0)
        {
            return difference;
        }
        if (*str1 == L'\0')
        {
            return 0;
        }
    }
    return 0;
}

// 0x005926C0 (:70)
int _v_stricmp(const char* str1, const char* str2)
{
    int difference = toupper(*str1) - toupper(*str2);
    while (difference == 0)
    {
        if (*str1 == '\0')
        {
            return 0;
        }
        ++str1;
        ++str2;
        difference = toupper(*str1) - toupper(*str2);
    }
    return difference;
}

// 0x00592720 (:90)
int _v_strnicmp(const char* str1, const char* str2, unsigned int n)
{
    for (; n != 0; --n, ++str1, ++str2)
    {
        const int difference = toupper(*str1) - toupper(*str2);
        if (difference != 0)
        {
            return difference;
        }
        if (*str1 == '\0')
        {
            return 0;
        }
    }
    return 0;
}

// 0x00592770 (:112)
int _v_stricmp(const wchar_t* str1, const wchar_t* str2)
{
    return v_stricmp(str1, str2);
}

// 0x005927D0 (:132)
int _v_strnicmp(const wchar_t* str1, const wchar_t* str2, unsigned int n)
{
    return v_strnicmp(str1, str2, n);
}

// 0x00592830 (:162)
int v_stricmp(const char* str1, const char* str2)
{
    if (str1 == nullptr)
    {
        return str2 == nullptr ? 0 : -1;
    }
    if (str2 == nullptr)
    {
        return 1;
    }
    for (;;)
    {
        const char c1 = string_comp_fold_ascii(*str1++);
        const char c2 = string_comp_fold_ascii(*str2++);
        if (c1 != c2)
        {
            return c2 < c1 ? 1 : -1;
        }
        if (c1 == '\0')
        {
            return 0;
        }
    }
}

// 0x00592840 (:176)
int v_strnicmp(const char* str1, const char* str2, unsigned int n)
{
    if (str1 == nullptr)
    {
        return str2 == nullptr ? 0 : -1;
    }
    if (str2 == nullptr)
    {
        return 1;
    }
    for (unsigned int i = 0; i < n; ++i)
    {
        const char c1 = string_comp_fold_ascii(*str1++);
        const char c2 = string_comp_fold_ascii(*str2++);
        if (c1 != c2)
        {
            return c2 < c1 ? 1 : -1;
        }
        if (c1 == '\0')
        {
            return 0;
        }
    }
    return 0;
}
