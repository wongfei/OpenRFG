// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/string/hkstringptr.cpp
//
// Havok 2014.2.5 (havok_impl.md HK1): a string pointer that owns a copy of its string. Bit 0 of the pointer
// (OWNED_FLAG) marks an owned copy, made with hkMemoryRouter::easyAlloc on the thread's heap and freed with easyFree;
// setPointerAligned keeps a caller's (aligned) string without copying. The memory statistics of the memory system
// name their allocators with it.
// The string helpers of hkString (S phase) the copies use are file-local here.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/string/hkstringptr.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/router/hkmemoryrouter.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/hkcontainerallocators.h"

#include <stdarg.h>
#include <stdint.h>
#include <string.h>

namespace
{
// 0x00CF0B00: a heap copy of s (hkString::strLen, easyAlloc, hkString::strCpy)
char* strDup(const char* s)
{
    const int size = (int)strlen(s) + 1;
    char* d = (char*)hkMemoryRouter::easyAlloc(hkMemoryRouter::getInstance().heap(), size);
    strcpy(d, s);
    return d;
}

// 0x00CF0B40: a heap copy of at most len characters of s, terminated
char* strNdup(const char* s, int len)
{
    int n = (int)strlen(s);
    if (n > len)
    {
        n = len;
    }
    char* d = (char*)hkMemoryRouter::easyAlloc(hkMemoryRouter::getInstance().heap(), n + 1);
    strncpy(d, s, n);
    d[n] = 0;
    return d;
}

// 0x00CF0B90 (LTCG: s in EAX)
// Frees an owned old string and stores a copy of s (len -1: the whole string); nothing happens when s is the
// current string.
void assign(const char** data, const char* s, int len)
{
    const uintptr_t old = (uintptr_t)*data;
    if ((const char*)(old & ~(uintptr_t)1) == s)
    {
        return;
    }
    if ((old & hkStringPtr::OWNED_FLAG) != 0)
    {
        hkMemoryRouter::easyFree(hkMemoryRouter::getInstance().heap(), (void*)(old - 1));
    }
    if (s == nullptr)
    {
        *data = nullptr;
        return;
    }
    char* copy = len == -1 ? strDup(s) : strNdup(s, len);
    *data = copy + hkStringPtr::OWNED_FLAG;
}
}

// 0x00CF0AC0
hkStringPtr::hkStringPtr()
    : m_stringAndFlag(nullptr)
{
}

// 0x00CF0AD0
hkStringPtr::hkStringPtr(hkFinishLoadedObjectFlag f)
{
}

// 0x00CF0C00
hkStringPtr::hkStringPtr(const char* string)
    : m_stringAndFlag(nullptr)
{
    assign(&m_stringAndFlag, string, -1);
}

// 0x00CF0C20
hkStringPtr::hkStringPtr(const char* s, int len)
    : m_stringAndFlag(nullptr)
{
    assign(&m_stringAndFlag, s, len);
}

// 0x00CF0C50
hkStringPtr::hkStringPtr(const hkStringPtr& strRef)
    : m_stringAndFlag(nullptr)
{
    assign(&m_stringAndFlag, strRef.cString(), -1);
}

// 0x00CF0C80
hkStringPtr::~hkStringPtr()
{
    assign(&m_stringAndFlag, nullptr, -1);
}

// 0x00CF0C90
hkStringPtr& hkStringPtr::operator=(const char* string)
{
    assign(&m_stringAndFlag, string, -1);
    return *this;
}

// 0x00CF0CB0: a copy of the other's string
hkStringPtr& hkStringPtr::operator=(const hkStringPtr& string)
{
    assign(&m_stringAndFlag, string.cString(), -1);
    return *this;
}

// 0x00CF0D20 (stub_impl.md S4): formatted into a growing buffer (255 characters on the stack first, doubled while
// vsnprintf fails, or the needed size when it reports one).
// Original behaviour kept: the result is assigned twice (the second assignment replaces the first copy with another).
void hkStringPtr::printf(const char* fmt, ...)
{
    hkInplaceArray<char, 255, hkContainerHeapAllocator> buf;
    while (true)
    {
        va_list args;
        va_start(args, fmt);
        const int size = buf.getCapacity();
        const int n = hkString::vsnprintf(buf.begin(), size, fmt, args);
        va_end(args);
        if (n < 0)
        {
            buf.reserve(size * 2);
        }
        else if (n < size)
        {
            break;
        }
        else
        {
            buf.reserve(n + 1);
        }
    }
    *this = buf.begin();
    *this = buf.begin();
}

// 0x00814120 (hkstringptr.inl:10)
const char* hkStringPtr::cString() const
{
    return (const char*)((uintptr_t)m_stringAndFlag & ~(uintptr_t)1);
}

// 0x00CF0AE0
int hkStringPtr::getLength() const
{
    const char* s = cString();
    return s != nullptr ? (int)strlen(s) : 0;
}

// 0x00CF0CD0
void hkStringPtr::set(const char* s, int len)
{
    assign(&m_stringAndFlag, s, len);
}

// 0x00CF0CF0
// Points at s without copying (s must be aligned, so its bit 0 is clear).
void hkStringPtr::setPointerAligned(const char* s)
{
    if (cString() != s)
    {
        assign(&m_stringAndFlag, nullptr, -1);
        m_stringAndFlag = s;
    }
}
