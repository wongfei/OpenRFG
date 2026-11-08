// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/util/xml/hkxmllexanalyzer.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (stub_impl.md S4, serializer); inline members as in the 2014.1 SDK,
// checked against rfg.exe (nobuild).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/object/hkreferencedobject.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/util/xml/hkparserbuffer.h"

class hkStreamReader;

// size 0x38
class hkXmlLexAnalyzer : public hkReferencedObject
{
public:
    enum
    {
        IN_BRACKETS = 1,
        IN_QBLOCK = 2,
    };

    enum Token
    {
        TOKEN_START_BLOCK = 0,
        TOKEN_START_QBLOCK = 1,
        TOKEN_TEXT = 2,
        TOKEN_WHITESPACE = 3,
        TOKEN_COMMENT = 4,
        TOKEN_EOF = 5,
        TOKEN_END_BLOCK = 6,
        TOKEN_END_QBLOCK = 7,
        TOKEN_IDENTIFIER = 8,
        TOKEN_EQUALS = 9,
        TOKEN_QUOTED_STRING = 10,
        TOKEN_SLASH = 11,
        TOKEN_ERROR = 12,
        TOKEN_COUNT_OF = 13,
    };

    HK_DECLARE_CLASS_ALLOCATOR(); // delete(void* p, unsigned int nbytes) 0x00F7AD00
    hkXmlLexAnalyzer(hkStreamReader* reader); // 0x00F7A750
    Token advance(); // 0x00F7AC70
    // 0x00F722B0 (hkxmllexanalyzer.h:58)
    hkSubString getLexeme(int startIndex, int endIndex) const { return m_buffer.getLexeme(startIndex, endIndex); }
    // 0x00F72280 (hkxmllexanalyzer.h:51)
    hkSubString getLexeme() const { return hkSubString(m_buffer.getLexemeStart(), m_buffer.getLexemeEnd()); }
    // 0x00F71490 (hkxmllexanalyzer.h:54)
    int getLexemeStartIndex() const { return m_buffer.getLexemeStartIndex(); }
    // 0x00F722A0 (hkxmllexanalyzer.h:56)
    int getLexemeEndIndex() const { return m_buffer.getLexemeEndIndex(); }
    // 0x00F714A0 (hkxmllexanalyzer.h:61)
    void bufferCommit() { m_buffer.bufferCommit(); }
    // 0x00F7A6E0 (hkxmllexanalyzer.h:63)
    static hkBool isLetter(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
    // 0x00F714B0 (hkxmllexanalyzer.h:64)
    static hkBool isDigit(char c) { return c >= '0' && c <= '9'; }
    // 0x00F714D0 (hkxmllexanalyzer.h:65)
    static hkBool isHexDigit(char c) { return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f'); }
    static hkBool isAlphaNumeric(char c) { return isLetter(c) || isDigit(c); }
    // 0x00F7A710 (hkxmllexanalyzer.h:67)
    static hkBool isWhiteSpace(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }
public:
    Token _lexComment(); // 0x00F7A780
    Token _matchInBrackets(); // 0x00F7A9D0
    Token _lexText(); // 0x00F7A950
    Token _matchOutsideBrackets(); // 0x00F7AB70
    Token _lexQuotedString(); // 0x00F7A7F0
    Token _handleError(const char* desc); // 0x00F7A740
    Token _lexIdentifier(); // 0x00F7A860
    Token _lexWhiteSpace(); // 0x00F7A8E0
public:
    virtual ~hkXmlLexAnalyzer() {} // vtbl+0x00 0x00F53930

    static const char* const tokenNames[]; // 0x0137A810
public:
    hkParserBuffer m_buffer; // 0x08
    int m_state; // 0x34
};
