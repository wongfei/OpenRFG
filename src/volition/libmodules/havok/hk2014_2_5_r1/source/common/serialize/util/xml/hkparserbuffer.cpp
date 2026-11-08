// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/util/xml/hkparserbuffer.cpp
//
// Havok 2014.2.5 (stub_impl.md S4, serializer SF): the XML lexer's read-ahead buffer, from rfg.exe.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/util/xml/hkparserbuffer.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/string/hkstring.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/system/io/reader/hkstreamreader.h"

// 0x00F710A0 (:13): holds a reference to the reader; the buffer starts as a lone terminating zero
hkParserBuffer::hkParserBuffer(hkStreamReader* reader)
    : m_reader(reader)
{
    reader->addReference();
    m_buffer.setSize(1);
    m_buffer[0] = 0;
    char* start = m_buffer.begin();
    m_buffer.setSizeUnchecked(0);
    m_lexemeStart = 0;
    m_bufferStart = 0;
    m_row = 0;
    m_col = 0;
    m_pos = start;
}

// 0x00F71150 (:35)
hkParserBuffer::~hkParserBuffer()
{
    m_reader->removeReference();
}

// 0x00F711A0 (:40): appends at least MIN_READ_SIZE bytes of room, reads into it and keeps the zero after the data;
// m_pos follows a reallocation. Returns the number of bytes read.
int hkParserBuffer::read(int size)
{
    if (size < MIN_READ_SIZE)
    {
        size = MIN_READ_SIZE;
    }
    const char* oldStart = m_buffer.begin();
    const int oldSize = m_buffer.getSize();
    m_buffer.expandBy(size + 1);
    const int numRead = m_reader->read(m_buffer.begin() + oldSize, size);
    m_buffer[oldSize + numRead] = 0;
    m_buffer.setSizeUnchecked(oldSize + numRead);
    if (m_buffer.begin() != oldStart)
    {
        m_pos += m_buffer.begin() - oldStart;
    }
    return numRead;
}

// 0x00F70FF0 (:75): the lexeme is consumed; every CR and LF in it counts a row (so CRLF counts two). m_col is
// only ever reset.
void hkParserBuffer::lexemeCommit()
{
    for (const char* p = m_buffer.begin() + m_lexemeStart; p < m_pos; p++)
    {
        if (*p == '\r' || *p == '\n')
        {
            m_row++;
            m_col = 0;
        }
    }
    m_lexemeStart = int(m_pos - m_buffer.begin());
}

// 0x00F71050 (:108): drops what is before the lexeme once it is over 1 KB
void hkParserBuffer::bufferCommit()
{
    m_bufferStart = m_lexemeStart;
    if (m_bufferStart > 1024)
    {
        hkString::memMove(m_buffer.begin(), m_buffer.begin() + m_bufferStart, m_buffer.getSize() - m_bufferStart);
        const int newSize = m_buffer.getSize() - m_bufferStart;
        m_buffer.setSizeUnchecked(newSize);
        m_buffer[newSize] = 0;
        m_pos -= m_bufferStart;
        m_lexemeStart -= m_bufferStart;
        m_bufferStart = 0;
    }
}

// 0x00F71040 (:138): pos is relative to the lexeme start
void hkParserBuffer::setLexemePosition(int pos)
{
    m_pos = m_buffer.begin() + m_lexemeStart + pos;
}

// 0x00F71250 (:144): text matches at m_pos (reading on as needed)
hkBool hkParserBuffer::match(const char* text, int len)
{
    if (len > getPostSize())
    {
        read(len - getPostSize());
        if (len > getPostSize())
        {
            return false;
        }
    }
    return hkString::strNcmp(m_pos, text, len) == 0;
}

// 0x00F712B0 (:155)
hkBool hkParserBuffer::match(const char* text)
{
    return match(text, hkString::strLen(text));
}

// 0x00F712E0 (:160)
hkBool hkParserBuffer::matchAndConsume(const char* text)
{
    const int len = hkString::strLen(text);
    if (match(text, len))
    {
        advance(len);
        return true;
    }
    return false;
}
