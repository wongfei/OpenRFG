// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/string/hkstringbuf.cpp
//
// Havok 2014.2.5 (stub_impl.md S4, serializer SA): the string buffer, from rfg.exe. Only the functions the serializer
// reaches are ported so far. The array (hkInplaceArray<char, 128>, temp allocator) always holds the terminator, so
// its size is the length + 1.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/string/hkstringbuf.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/string/hkstring.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/util/hkmemutil.h"

#include <stdarg.h>

// 0x00D145D0: the first c in [startIndex, endIndex), the end clamped to the length; -1 when absent
int hkStringBuf::indexOf(char c, int startIndex, int endIndex) const
{
    const int len = m_string.getSize() - 1;
    if (endIndex > len)
    {
        endIndex = len;
    }
    for (int i = startIndex; i < endIndex; i++)
    {
        if (m_string[i] == c)
        {
            return i;
        }
    }
    return -1;
}

// 0x00D147A0: drops the first n characters (at most the whole string)
void hkStringBuf::chompStart(int n)
{
    const int len = m_string.getSize() - 1;
    if (n > len)
    {
        n = len;
    }
    if (n > 0)
    {
        m_string.setSizeUnchecked(m_string.getSize() - n);
        char* p = m_string.begin();
        for (int i = 0; i < m_string.getSize(); i++)
        {
            p[i] = p[i + n];
        }
    }
}

// 0x00D14990: the array copied (an exact new buffer when it does not fit)
hkStringBuf& hkStringBuf::operator=(const hkStringBuf& s)
{
    static_cast<hkArray<char, hkContainerTempAllocator>&>(m_string) = s.m_string;
    return *this;
}

// 0x00D14B10: a null string makes the empty string
hkStringBuf::hkStringBuf(const char* s)
{
    if (s != nullptr)
    {
        const int len = hkString::strLen(s);
        setLength(len);
        hkString::memCpy(m_string.begin(), s, len);
    }
    else
    {
        m_string.setSizeUnchecked(1);
        m_string[0] = 0;
    }
}

// 0x00D14BA0: the first len characters of b
hkStringBuf::hkStringBuf(const char* b, int len)
{
    setLength(len);
    hkString::memCpy(m_string.begin(), b, len);
}

// 0x00D14650
int hkStringBuf::compareTo(const char* other) const
{
    return hkString::strCmp(m_string.begin(), other);
}

// 0x00D146B0
unsigned int hkStringBuf::operator==(const char* other) const
{
    return compareTo(other) == 0;
}

// 0x00D14C10
hkStringBuf::hkStringBuf(const hkStringBuf& s)
{
    static_cast<hkArray<char, hkContainerTempAllocator>&>(m_string) = s.m_string;
}

// 0x00D14C80: a null string clears
hkStringBuf& hkStringBuf::operator=(const char* s)
{
    if (s == nullptr)
    {
        clear();
        return *this;
    }
    const int len = hkString::strLen(s);
    setLength(len);
    hkString::memCpy(m_string.begin(), s, len);
    return *this;
}

// 0x00D14D30: the empty string (room for the terminator reserved when the array has none)
void hkStringBuf::clear()
{
    if (m_string.getCapacity() == 0)
    {
        m_string.reserve(1);
    }
    m_string.setSizeUnchecked(1);
    m_string[0] = 0;
}

// 0x00D14D80: formatted in place, the buffer grown until vsnprintf fits (doubled, at least 255, while it fails; the
// reported size otherwise)
void hkStringBuf::printf(const char* fmt, ...)
{
    while (true)
    {
        va_list args;
        va_start(args, fmt);
        const int size = m_string.getCapacity();
        const int nchars = hkString::vsnprintf(m_string.begin(), size, fmt, args);
        va_end(args);
        if (nchars < 0)
        {
            const int grown = size * 2;
            setLength(grown < 256 ? 255 : grown);
        }
        else if (nchars < size)
        {
            setLength(nchars);
            return;
        }
        else
        {
            setLength(nchars);
        }
    }
}

// 0x00D15080: the length characters from startOffset
void hkStringBuf::slice(int startOffset, int length)
{
    if (startOffset != 0)
    {
        hkMemUtil::memMove(m_string.begin(), m_string.begin() + startOffset, length);
    }
    setLength(length);
}

// 0x00D14EA0: inserted before the terminator; nothing for null
hkStringBuf& hkStringBuf::operator+=(const char* other)
{
    if (other != nullptr)
    {
        m_string.insertAt(m_string.getSize() - 1, other, hkString::strLen(other));
    }
    return *this;
}

// 0x00D14410: the array as it is, terminator included
hkArray<char,hkContainerTempAllocator>& hkStringBuf::getArray()
{
    return m_string;
}

// 0x00D15150: len -1 appends the whole string; nothing for null
void hkStringBuf::append(const char* s, int len)
{
    if (s == nullptr)
    {
        return;
    }
    if (len < 0)
    {
        len = hkString::strLen(s);
    }
    const int oldLen = m_string.getSize() - 1;
    setLength(oldLen + len);
    hkMemUtil::memCpy(m_string.begin() + oldLen, s, len);
}
