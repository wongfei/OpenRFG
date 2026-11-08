// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/system/io/istream/hkistream.cpp
//
// Havok 2014.2.5 (stub_impl.md S4, serializer SA, SF): the formatted input stream over a stream reader, from rfg.exe.
// The text parsing reads ahead with peek and consumes with skip. Not ported: the file and memory track constructors
// (no hkFileSystem or hkMemoryTrackStreamReader in src; RFG loads from memory).

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/system/io/istream/hkistream.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/string/hkstring.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/string/hkstringbuf.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/string/hkstringptr.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/system/io/reader/memory/hkmemorystreamreader.h"

namespace
{
    bool isWhite(char c)
    {
        return c == ' ' || c == '\t' || c == '\r' || c == '\n';
    }
}

// 0x00FBFBA0
hkIstream& hkIstream::operator>>(char& c)
{
    m_streamReader->read(&c, 1);
    return *this;
}

// 0x00FBFBC0
int hkIstream::read(void* buf, int nbytes)
{
    return m_streamReader->read(buf, nbytes);
}

// 0x00FBFBD0
hkIstream::hkIstream(hkStreamReader* sb)
    : m_streamReader(sb)
{
}

// 0x00FBFC50: reads the caller's block in place
hkIstream::hkIstream(const void* mem, int memSize)
    : m_streamReader((hkStreamReader*)nullptr)
{
    m_streamReader.setAndDontIncrementRefCount(new hkMemoryStreamReader(mem, memSize, hkMemoryStreamReader::MEMORY_INPLACE));
}

// 0x00FBFD30
hkIstream::~hkIstream()
{
    m_streamReader = nullptr;
}

// 0x00FBFD60
hkBool hkIstream::isOk() const
{
    return m_streamReader.val() != nullptr && m_streamReader->isOk();
}

// 0x00FBF8F0 (:71): skips white space, 64 bytes at a time. LTCG: reader in ESI.
static void eatWhiteSpace(hkStreamReader* reader)
{
    char buf[64];
    int n = reader->peek(buf, sizeof(buf));
    while (n != 0)
    {
        for (int i = 0; i < n; i++)
        {
            if (!isWhite(buf[i]))
            {
                reader->skip(i);
                return;
            }
        }
        reader->skip(n);
        n = reader->peek(buf, sizeof(buf));
    }
}

// 0x00FBF960 (:93): an optional sign, then "0x" hexadecimal (a hex digit must follow), a leading-zero octal or
// decimal, from at most 255 bytes peeked; consumes what it read (one byte at the end of the stream). The magnitude
// is returned, the sign in negOut. Original bug kept: the digit loop takes 'B'-'F' and 'b'-'f' but not 'A' or 'a', so
// a hex number stops at its first A.
static uint64_t readInteger64(hkStreamReader* reader, hkBool& negOut)
{
    eatWhiteSpace(reader);
    negOut = false;
    uint64_t value = 0;
    char buf[256];
    int i = 0;
    const int n = reader->peek(buf, 255);
    if (n == 0)
    {
        reader->skip(1);
        return 0;
    }
    if (buf[0] == '+')
    {
        i = 1;
    }
    else if (buf[0] == '-')
    {
        negOut = true;
        i = 1;
    }
    unsigned int base;
    if (i + 3 < n && buf[i] == '0' && (buf[i + 1] == 'x' || buf[i + 1] == 'X') &&
        ((buf[i + 2] >= '0' && buf[i + 2] <= '9') || (buf[i + 2] >= 'a' && buf[i + 2] <= 'f') || (buf[i + 2] >= 'A' && buf[i + 2] <= 'F')))
    {
        base = 16;
        i += 2;
    }
    else if (i + 2 < n && buf[i] == '0' && buf[i + 1] >= '0' && buf[i + 1] <= '9')
    {
        base = 8;
        i += 1;
    }
    else
    {
        base = 10;
    }
    for (; i < n; i++)
    {
        const char c = buf[i];
        unsigned int digit = 0xFFFFFFFF;
        if (c >= '0' && c <= '9')
        {
            digit = c - '0';
        }
        else if (c > 'A' && c <= 'F')
        {
            digit = c - 'A' + 10;
        }
        else if (c > 'a' && c <= 'f')
        {
            digit = c - 'a' + 10;
        }
        if (digit >= base)
        {
            break;
        }
        value = value * base + digit;
    }
    reader->skip(i);
    return value;
}

// 0x00FBFAD0 (:181): digits, signs, exponents and points from at most 255 bytes peeked; ',' is read as '.' (except as
// the first character). Consumes what it read; -1 (and one byte consumed) when nothing reads as a number.
// LTCG: reader in EAX.
static float readFloat(hkStreamReader* reader)
{
    eatWhiteSpace(reader);
    char buf[256];
    int i = 0;
    const int n = reader->peek(buf, 255);
    if (n != 0 && ((buf[0] >= '0' && buf[0] <= '9') || buf[0] == '+' || buf[0] == '-' || buf[0] == '.' || buf[0] == ','))
    {
        for (i = 1; i < n; i++)
        {
            const char c = buf[i];
            if (c == '.' || c == ',')
            {
                buf[i] = '.';
            }
            else if (!((c >= '0' && c <= '9') || c == '+' || c == '-' || c == 'E' || c == 'e'))
            {
                break;
            }
        }
        reader->skip(i);
    }
    else
    {
        reader->skip(1);
    }
    buf[i] = 0;
    if (i <= 0)
    {
        return -1.0f;
    }
    return hkString::atof(buf);
}

// 0x00FBFD90 (:225): "true" or "false" followed by white space or the end of what was peeked. Original bug kept:
// "false" is compared by its first 4 characters only. Anything else reads as false (one byte consumed at the end of
// the stream, nothing otherwise).
hkIstream& hkIstream::operator>>(hkBool& b)
{
    eatWhiteSpace(m_streamReader);
    char buf[8];
    const int n = m_streamReader->peek(buf, 6);
    if (n >= 4 && hkString::strNcmp(buf, "true", 4) == 0 && (n == 4 || isWhite(buf[4])))
    {
        b = true;
        m_streamReader->skip(4);
        return *this;
    }
    if (n >= 5 && hkString::strNcmp(buf, "false", 4) == 0 && (n == 5 || isWhite(buf[5])))
    {
        b = false;
        m_streamReader->skip(5);
        return *this;
    }
    if (n == 0)
    {
        m_streamReader->skip(1);
    }
    b = false;
    return *this;
}

// <short> 0x00FC02E0, <unsigned short> 0x00FC0320, <int> 0x00FC0360, <unsigned int> 0x00FC0390, <__int64> 0x00FC03C0,
// <unsigned __int64> 0x00FC03F0 (:270): the magnitude negated for a '-', in T's width
template<typename T>
static void readInteger(hkStreamReader* reader, T& tOut)
{
    hkBool neg;
    const uint64_t mag = readInteger64(reader, neg);
    tOut = neg ? (T)(0 - mag) : (T)mag;
}

// 0x00FBFE80 (:281)
hkIstream& hkIstream::operator>>(short& s)
{
    readInteger(m_streamReader, s);
    return *this;
}

// 0x00FBFEC0 (:287)
hkIstream& hkIstream::operator>>(unsigned short& s)
{
    readInteger(m_streamReader, s);
    return *this;
}

// 0x00FBFF00 (:294)
hkIstream& hkIstream::operator>>(int& i)
{
    readInteger(m_streamReader, i);
    return *this;
}

// 0x00FBFF40 (:300)
hkIstream& hkIstream::operator>>(unsigned int& i)
{
    readInteger(m_streamReader, i);
    return *this;
}

// 0x00FBFF80 (:306)
hkIstream& hkIstream::operator>>(float& f)
{
    f = readFloat(m_streamReader);
    return *this;
}

// 0x00FBFFA0 (:312): read as a float
hkIstream& hkIstream::operator>>(double& d)
{
    d = readFloat(m_streamReader);
    return *this;
}

// 0x00FBFFC0 (:320)
hkIstream& hkIstream::operator>>(int64_t& i)
{
    readInteger(m_streamReader, i);
    return *this;
}

// 0x00FC0000 (:326)
hkIstream& hkIstream::operator>>(uint64_t& i)
{
    readInteger(m_streamReader, i);
    return *this;
}

// 0x00FC0120 (:332): the next white space delimited word, 64 bytes at a time (one byte consumed at the end of the
// stream when nothing was read). Original bug kept: getArray() keeps the terminator clear() wrote, so the word is
// appended after it and cString() reads empty.
hkIstream& hkIstream::operator>>(hkStringBuf& str)
{
    str.clear();
    hkArray<char,hkContainerTempAllocator>& arr = str.getArray();
    eatWhiteSpace(m_streamReader);
    char buf[64];
    int n = m_streamReader->peek(buf, sizeof(buf));
    bool ended = false;
    while (n != 0 && !ended)
    {
        int i = 0;
        for (; i < n; i++)
        {
            if (isWhite(buf[i]))
            {
                break;
            }
        }
        if (i < n)
        {
            m_streamReader->skip(i);
            arr.append(buf, i);
            ended = true;
        }
        else
        {
            m_streamReader->skip(n);
            arr.append(buf, n);
            n = m_streamReader->peek(buf, sizeof(buf));
        }
    }
    if (!ended && arr.getSize() == 0)
    {
        m_streamReader->skip(1);
    }
    arr.pushBack(0);
    return *this;
}

// 0x00FC0240 (:370)
hkIstream& hkIstream::operator>>(hkStringPtr& str)
{
    hkStringBuf buf;
    *this >> buf;
    str = buf.cString();
    return *this;
}

// 0x00FC0040 (:378): up to delim (consumed, not stored) within maxlen bytes peeked; the length, or -1 when no delim
// was found in a full buffer (str emptied, the bytes still consumed)
int hkIstream::getline(char* str, int maxlen, char delim)
{
    eatWhiteSpace(m_streamReader);
    const int n = m_streamReader->peek(str, maxlen);
    for (int i = 0; i < n; i++)
    {
        if (str[i] == delim)
        {
            str[i] = 0;
            m_streamReader->skip(i + 1);
            return i;
        }
    }
    if (n > 0 && n < maxlen)
    {
        m_streamReader->skip(n);
        str[n] = 0;
        return n;
    }
    if (n == 0 && maxlen > 0)
    {
        m_streamReader->skip(1);
    }
    m_streamReader->skip(n);
    if (maxlen != 0)
    {
        str[0] = 0;
    }
    return -1;
}

// 0x00FC00F0 (:418)
void hkIstream::setStreamReader(hkStreamReader* newReader)
{
    m_streamReader = newReader;
}
