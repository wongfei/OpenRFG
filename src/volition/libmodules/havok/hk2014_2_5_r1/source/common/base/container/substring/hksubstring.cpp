// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/substring/hksubstring.cpp
//
// Havok 2014.2.5 (stub_impl.md S4, serializer SA, SF): a [start, end) view into a string, from rfg.exe.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/substring/hksubstring.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/string/hkstring.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/system/io/ostream/hkostream.h"

// 0x00FC2840 (:15): the same start compares the ends only
hkBool hkSubString::operator==(const hkSubString& rhs) const
{
    if (m_start == rhs.m_start)
    {
        return m_end == rhs.m_end;
    }
    const int len = length();
    if (len != rhs.length())
    {
        return false;
    }
    for (int i = 0; i < len; i++)
    {
        if (m_start[i] != rhs.m_start[i])
        {
            return false;
        }
    }
    return true;
}

// 0x00FC28B0: equal when both end together
hkBool hkSubString::operator==(const char* rhs) const
{
    const char* p = m_start;
    for (; p < m_end && *rhs != 0; rhs++, p++)
    {
        if (*p != *rhs)
        {
            return false;
        }
    }
    return p == m_end && *rhs == 0;
}

// 0x00FC2770: the decimal value of a view of at most 20 characters (0 for a longer one)
int hkSubString::getInt() const
{
    char buf[24];
    const int len = (int)(m_end - m_start);
    if (len > 20)
    {
        return 0;
    }
    hkString::strNcpy(buf, m_start, len);
    buf[len] = 0;
    return hkString::atoi(buf, 10);
}

// 0x00FC27C0 (:71)
hkOstream& operator<<(hkOstream& stream, const hkSubString& text)
{
    stream.write(text.m_start, text.length());
    return stream;
}

// 0x00FC27E0 (:77): signed characters, h = h * 31 + c
unsigned int hkSubString::calcHash() const
{
    unsigned int h = 0;
    for (const char* p = m_start; p < m_end; p++)
    {
        h = h * 31 + (unsigned int)(int)*p;
    }
    return h;
}

// 0x00FC2810 (:90)
void hkSubString::operator=(const char* rhs)
{
    m_start = rhs;
    m_end = m_start + hkString::strLen(rhs);
}

// 0x00FC2900 (:96): -1, 0 or 1; signed characters; a prefix sorts first
int hkSubString::compareTo(const hkSubString& rhs) const
{
    const int len = length();
    const int rhsLen = rhs.length();
    const int minLen = len < rhsLen ? len : rhsLen;
    const char* a = m_start;
    const char* b = rhs.m_start;
    const char* end = a + minLen;
    for (; a < end; a++, b++)
    {
        if (*a != *b)
        {
            return *a < *b ? -1 : 1;
        }
    }
    if (len == rhsLen)
    {
        return 0;
    }
    return len < rhsLen ? -1 : 1;
}
