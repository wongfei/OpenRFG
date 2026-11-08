// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/string/hkstring.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (stub_impl.md S4, serializer).
#pragma once

#include <stdint.h>
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"

class hkBool;
struct hkContainerHeapAllocator;
class hkMemoryAllocator;

namespace hkString
{
    enum ReplaceType
    {
        REPLACE_ONE = 0,
        REPLACE_ALL = 1,
    };
}

namespace hkString
{
    void memCpy16(void* dst, const void* src, int numQuads); // 0x00E72A60
    void memCpy16NonEmpty(void* dst, const void* src, int numQuads); // 0x00DB92E0
    // template instance: void memCpy16<32>(void* dst, const void* src); // 0x00DB93B0
    void memCpy256(void* dst, const void* src); // 0x00E72A80
    // template instance: void memSet16<16>(void* dst, const void* src); // 0x00E83EF0
    void memSet16(void* dst, const void* value, int numQuads); // 0x00E2A950
    char toUpper(char c); // 0x00CFEDD0
    char toLower(char c); // 0x00CFEDE0
    int vsnprintf(char* buf, int len, const char* fmt, void* hkargs); // 0x00CFEDF0
    int snprintf(char*, int, const char*, ...); // 0x00CFEE20
    int sprintf(char*, const char*, ...); // 0x00CFEE50
    int strCmp(const char* a, const char* b); // 0x00CFEE70
    int strNcmp(const char* a, const char* b, int n); // 0x00CFEEA0
    int strCasecmp(const char* a, const char* b); // 0x00CFEEB0
    int strNcasecmp(const char* a, const char* b, int n); // 0x00CFEF00
    void strCpy(char* dst, const char* src); // 0x00CFEF60
    void strNcpy(char* dst, const char* src, int n); // 0x00CFEF80
    int strLen(const char* src); // 0x00CFEFA0
    void strCat(char* dst, const char* src); // 0x00CFEFC0
    void strNcat(char* dst, const char* src, int n); // 0x00CFF000
    int atoi(const char* in, int base); // 0x00CFF020
    int64_t atoll(const char* in, int base); // 0x00CFF040
    uint64_t atoull(const char* in, int base); // 0x00CFF060
    float atof(const char* in); // 0x00CFF080
    const char* strStr(const char* haystack, const char* needle); // 0x00CFF0A0
    const char* strChr(const char* haystack, int needle); // 0x00CFF0B0
    const char* strRchr(const char* haystack, int needle); // 0x00CFF0C0
    char* strDup(const char* src, hkMemoryAllocator& alloc); // 0x00CFF0D0
    void strFree(char* s, hkMemoryAllocator& alloc); // 0x00CFF100
    void strFree(char* s); // 0x00CFF460
    char* strNdup(const char* src, int maxlen, hkMemoryAllocator& alloc); // 0x00CFF120
    char* strDup(const char* src); // 0x00CFF480
    char* strNdup(const char* src, int maxlen); // 0x00CFF4A0
    char* strLwr(char* s); // 0x00CFF160
    char* strUpr(char* s); // 0x00CFF190
    void memCpy(void* dst, const void* src, int n); // 0x00CFF1C0
    void memMove(void* dst, const void* src, int n); // 0x00CFF1D0
    void memSet(void* dst, const int c, int n); // 0x00CFF1E0
    int memCmp(const void* buf1, const void* buf2, int n); // 0x00CFF1F0
    hkBool findAllOccurrences(const char* haystack, const char* needle, hkArray<int,hkContainerHeapAllocator>& indices, hkString::ReplaceType rtype); // 0x00CFF500
    hkBool beginsWith(const char* a, const char* b); // 0x00CFF270
    hkBool beginsWithCase(const char* a, const char* b); // 0x00CFF2A0
    hkBool endsWith(const char* a, const char* b); // 0x00CFF300
    hkBool endsWithCase(const char* a, const char* b); // 0x00CFF350
    int lastIndexOf(const char* str, char c); // 0x00CFF3E0
    int indexOf(const char* str, char c, int startIndex, int endIndex); // 0x00CFF410
    void memClear128(void* dst, int numBytes); // 0x00CFF4D0
    void memCpy4(void* dst, const void* src, int numWords); // 0x00D0AB00
    void memSet4(void* dst, const int value, int numWords); // 0x00D4EE90
    void memClear16(void* dst, int numQuads); // 0x00CFF5B0
}
