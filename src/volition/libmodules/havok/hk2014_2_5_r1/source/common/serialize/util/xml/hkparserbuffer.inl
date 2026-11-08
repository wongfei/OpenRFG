// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/util/xml/hkparserbuffer.inl
//
// Havok 2014.2.5 (stub_impl.md S4, serializer SF): the 2014.1 SDK's inline code, checked against rfg.exe (the
// asserts are compiled out).

// 0x00F7A6C0 (:11): reads more at the terminating zero
inline char hkParserBuffer::getChar()
{
    char c = *m_pos;
    if (c == 0)
    {
        read(MIN_READ_SIZE);
        c = *m_pos;
    }
    return c;
}

// 0x00F713D0 (:23)
inline int hkParserBuffer::requirePostSize(int size)
{
    const int postSize = getPostSize();
    if (size <= postSize)
    {
        return postSize;
    }
    read(size - postSize);
    return getPostSize();
}

// 0x00F70FD0 (:38)
inline void hkParserBuffer::advance(int numChars)
{
    if (numChars <= getPostSize())
    {
        m_pos += numChars;
    }
}

// 0x00F7ACA0 (:50): never before the lexeme start
inline void hkParserBuffer::rewindChar()
{
    if (m_pos > m_buffer.begin() + m_lexemeStart)
    {
        m_pos--;
    }
}

// 0x00F7ACC0 (:60): 0 at the end of the stream
inline char hkParserBuffer::nextChar()
{
    if (m_pos >= m_buffer.end())
    {
        read(MIN_READ_SIZE);
        if (m_pos >= m_buffer.end())
        {
            return 0;
        }
    }
    return *m_pos++;
}

inline char hkParserBuffer::getCharAt(int offset) const
{
    const char* pos = m_pos + offset;
    if (pos >= m_buffer.begin() + m_lexemeStart && pos < m_buffer.end())
    {
        return *pos;
    }
    return 0;
}

// 0x00F72260 (:87): indices relative to m_bufferStart
inline hkSubString hkParserBuffer::getLexeme(int startIndex, int endIndex) const
{
    const char* base = m_buffer.begin() + m_bufferStart;
    return hkSubString(base + startIndex, base + endIndex);
}
