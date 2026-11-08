// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/util/xml/hkxmlstreamparser.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (stub_impl.md S4, serializer); inline members and the
// getIntListAttribute template (hkXmlStreamParser.inl) as in the 2014.1 SDK, checked against rfg.exe (nobuild).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include <stdint.h>
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/hkcontainerallocators.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/stringmap/hkstringmap.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/object/hkreferencedobject.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/util/xml/hkxmllexanalyzer.h"

class hkOstream;
class hkStreamReader;
class hkStringBuf;

// size 0x78: an event parser over hkXmlLexAnalyzer. After a block token, lexeme 0 is the block name and the
// attributes follow as (key, value) lexeme pairs; m_attribMap maps a key (in m_keyStorage) to its value's lexeme.
class hkXmlStreamParser : public hkReferencedObject
{
public:
    enum Token
    {
        TOKEN_QBLOCK = 0,
        TOKEN_BLOCK_START = 1,
        TOKEN_BLOCK_START_END = 2,
        TOKEN_BLOCK_END = 3,
        TOKEN_TEXT = 4,
        TOKEN_WHITESPACE = 5,
        TOKEN_ERROR = 6,
        TOKEN_EOF = 7,
    };

    // size 0x8: indices relative to the lexer's buffer start
    struct SubString
    {
        int m_start; // 0x00
        int m_end; // 0x04
    };

    HK_DECLARE_CLASS_ALLOCATOR(); // delete(void* p, unsigned int nbytes) 0x00F72350
    Token advance(); // 0x00F720C0
    hkXmlStreamParser(hkStreamReader* reader); // 0x00F71DA0
    hkSubString getBlockName() const; // 0x00F71B20
    // 0x00F53910 (hkxmlstreamparser.h:45)
    Token getToken() const { return m_token; }
    // 0x00F71500 (hkxmlstreamparser.h:48)
    const hkArray<char const*,hkContainerHeapAllocator>& getKeys() const { return m_keys; }
    hkResult getValue(const char* key, hkSubString& valueOut) const; // 0x00F71AD0
    hkResult getIntAttribute(const char* key, int& value) const; // 0x00F71C10
    hkResult getRealAttribute(const char* key, float& value) const; // 0x00F71CD0
    template<typename IntegerType>
    hkResult getIntListAttribute(const char* key, hkArray<IntegerType,hkContainerHeapAllocator>& out) const; // <unsigned short> 0x00F56900
    hkBool hasAttribute(const char* key) const; // 0x00F71CA0
    hkSubString getLexeme() const; // 0x00F71B60
    void dumpParse(hkOstream& out); // 0x00F720D0
    static hkBool needsDecode(const hkSubString& subString); // 0x00F718A0
    static hkResult decodeString(const hkSubString& subString, hkStringBuf& buf); // 0x00F718D0
    static hkResult parseInt(const hkSubString& subString, int64_t& valueOut); // 0x00F71510
    static hkResult parseInt(const hkSubString& subString, int& signOut, uint64_t& magOut); // 0x00F715A0
    static hkResult parseReal(const hkSubString& subString, float& valueOut); // 0x00F716A0
public:
    void _pushLexeme(); // 0x00F71D50
    Token _parseBlock(); // 0x00F71F10
    Token _parseQBlock(); // 0x00F71FD0
    void _extractAttributes(); // 0x00F71E10
    Token _advance(); // 0x00F72030
public:
    virtual ~hkXmlStreamParser() {} // vtbl+0x00 0x00F56E30

public:
    hkStringMap<int,hkContainerHeapAllocator> m_attribMap; // 0x08
    hkArray<char const*,hkContainerHeapAllocator> m_keys; // 0x18
    hkArray<char,hkContainerHeapAllocator> m_keyStorage; // 0x24
    hkXmlLexAnalyzer m_lex; // 0x30
    hkArray<SubString,hkContainerHeapAllocator> m_lexemes; // 0x68
    Token m_token; // 0x74
};

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/util/xml/hkxmlstreamparser.inl"
