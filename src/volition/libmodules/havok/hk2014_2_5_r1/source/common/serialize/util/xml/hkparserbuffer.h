// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/util/xml/hkparserbuffer.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (stub_impl.md S4, serializer); inline members from the 2014.1 SDK
// (hkParserBuffer.h/.inl), checked against rfg.exe (nobuild).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/hkcontainerallocators.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/substring/hksubstring.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/object/hkreferencedobject.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"

class hkStreamReader;

// size 0x2C: a growing window over a stream reader. The buffer always ends in a zero; the lexeme is
// [m_lexemeStart, m_pos), and m_bufferStart is the start of the part the current token may still refer to.
class hkParserBuffer : public hkReferencedObject
{
public:
    enum
    {
        MIN_READ_SIZE = 256,
    };

    HK_DECLARE_CLASS_ALLOCATOR(); // delete(void* p, unsigned int nbytes) 0x00F71390
    hkParserBuffer(hkStreamReader* reader); // 0x00F710A0
    virtual ~hkParserBuffer(); // vtbl+0x00 0x00F71150
    hkBool match(const char* text, int len); // 0x00F71250
    hkBool match(const char* text); // 0x00F712B0
    hkBool matchAndConsume(const char* text); // 0x00F712E0
    int getRow() const { return m_row; }
    int getColumn() const { return m_col; }
    inline char getChar(); // 0x00F7A6C0
    inline char getCharAt(int offset) const;
    // 0x00F72240 (hkparserbuffer.h:46)
    const char* getLexemeStart() const { return m_buffer.begin() + m_lexemeStart; }
    // 0x00F71470 (hkparserbuffer.h:48)
    const char* getLexemeEnd() const { return m_pos; }
    // 0x00F70FA0 (hkparserbuffer.h:51)
    const char* getCurrentPosition() const { return m_pos; }
    // 0x00F71480 (hkparserbuffer.h:56)
    int getLexemeStartIndex() const { return m_lexemeStart - m_bufferStart; }
    // 0x00F72250 (hkparserbuffer.h:58)
    int getLexemeEndIndex() const { return int(m_pos - m_buffer.begin()) - m_bufferStart; }
    inline hkSubString getLexeme(int startIndex, int endIndex) const; // 0x00F72260
    inline char nextChar(); // 0x00F7ACC0
    inline void advance(int numChars); // 0x00F70FD0
    inline void rewindChar(); // 0x00F7ACA0
    void setLexemePosition(int pos); // 0x00F71040
    int getLexemePosition() const { return int(m_pos - m_buffer.begin()) - m_lexemeStart; }
    int getBufferSize() const { return m_buffer.getSize() - m_bufferStart; }
    // 0x00F70FC0 (hkparserbuffer.h:81)
    int getPostSize() const { return int(m_buffer.end() - m_pos); }
    inline int requirePostSize(int size); // 0x00F713D0
    int getPreSize() const { return int(m_pos - m_lexemeStart - m_buffer.begin()); }
    int ensureNumChars(int);
    int read(int size); // 0x00F711A0
    void lexemeCommit(); // 0x00F70FF0
    void bufferCommit(); // 0x00F71050

public:
    int m_bufferStart; // 0x08
    int m_lexemeStart; // 0x0C
    char* m_pos; // 0x10
    hkArray<char,hkContainerHeapAllocator> m_buffer; // 0x14
    hkStreamReader* m_reader; // 0x20
    int m_row; // 0x24
    int m_col; // 0x28
};

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/util/xml/hkparserbuffer.inl"
