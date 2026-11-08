// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/util/xml/hkxmlstreamparser.cpp
//
// Havok 2014.2.5 (stub_impl.md S4, serializer SF): the XML event parser of the XML tagfile reader, from rfg.exe.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/util/xml/hkxmlstreamparser.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/string/hkstring.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/string/hkstringbuf.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/system/io/ostream/hkostream.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/util/xml/hkfloatparseutil.h"

#include <string.h>

// 0x00F71DA0 (:15)
hkXmlStreamParser::hkXmlStreamParser(hkStreamReader* reader)
    : m_lex(reader)
    , m_token(TOKEN_ERROR)
{
}

// 0x00F71F10 (:20): after '<': "</name>", "<name a="v" ...>" or "<name a="v" .../>"
hkXmlStreamParser::Token hkXmlStreamParser::_parseBlock()
{
    hkXmlLexAnalyzer::Token tok = m_lex.advance();
    if (tok == hkXmlLexAnalyzer::TOKEN_SLASH)
    {
        if (m_lex.advance() == hkXmlLexAnalyzer::TOKEN_IDENTIFIER)
        {
            _pushLexeme();
            return m_lex.advance() == hkXmlLexAnalyzer::TOKEN_END_BLOCK ? TOKEN_BLOCK_END : TOKEN_ERROR;
        }
        return TOKEN_ERROR;
    }
    if (tok != hkXmlLexAnalyzer::TOKEN_IDENTIFIER)
    {
        return TOKEN_ERROR;
    }
    for (;;)
    {
        _pushLexeme(); // the name, then each value
        tok = m_lex.advance();
        if (tok == hkXmlLexAnalyzer::TOKEN_END_BLOCK)
        {
            _extractAttributes();
            return TOKEN_BLOCK_START;
        }
        if (tok != hkXmlLexAnalyzer::TOKEN_IDENTIFIER)
        {
            if (tok != hkXmlLexAnalyzer::TOKEN_SLASH)
            {
                return TOKEN_ERROR;
            }
            tok = m_lex.advance();
            _extractAttributes();
            return tok == hkXmlLexAnalyzer::TOKEN_END_BLOCK ? TOKEN_BLOCK_START_END : TOKEN_ERROR;
        }
        _pushLexeme(); // the key
        if (m_lex.advance() != hkXmlLexAnalyzer::TOKEN_EQUALS)
        {
            return TOKEN_ERROR;
        }
        if (m_lex.advance() != hkXmlLexAnalyzer::TOKEN_QUOTED_STRING)
        {
            return TOKEN_ERROR;
        }
    }
}

// 0x00F71FD0 (:96): after "<?": "name a="v" ...?>"
hkXmlStreamParser::Token hkXmlStreamParser::_parseQBlock()
{
    if (m_lex.advance() != hkXmlLexAnalyzer::TOKEN_IDENTIFIER)
    {
        return TOKEN_ERROR;
    }
    for (;;)
    {
        _pushLexeme();
        const hkXmlLexAnalyzer::Token tok = m_lex.advance();
        if (tok == hkXmlLexAnalyzer::TOKEN_END_QBLOCK)
        {
            _extractAttributes();
            return TOKEN_QBLOCK;
        }
        if (tok != hkXmlLexAnalyzer::TOKEN_IDENTIFIER)
        {
            return TOKEN_ERROR;
        }
        _pushLexeme();
        if (m_lex.advance() != hkXmlLexAnalyzer::TOKEN_EQUALS)
        {
            return TOKEN_ERROR;
        }
        if (m_lex.advance() != hkXmlLexAnalyzer::TOKEN_QUOTED_STRING)
        {
            return TOKEN_ERROR;
        }
    }
}

// 0x00F71D50 (:144): the current lexeme, by index
void hkXmlStreamParser::_pushLexeme()
{
    SubString& s = m_lexemes.expandOne();
    s.m_start = m_lex.getLexemeStartIndex();
    s.m_end = m_lex.getLexemeEndIndex();
}

// 0x00F71E10 (:151): copies the keys (lexemes 1, 3, ...) zero terminated into m_keyStorage and maps each to the
// index of its value lexeme
void hkXmlStreamParser::_extractAttributes()
{
    m_attribMap.clear();
    m_keys.clear();
    int total = 0;
    for (int i = 1; i < m_lexemes.getSize(); i += 2)
    {
        total += m_lexemes[i].m_end - m_lexemes[i].m_start + 1;
    }
    m_keyStorage.setSize(total);
    char* dst = m_keyStorage.begin();
    for (int i = 1; i < m_lexemes.getSize(); i += 2)
    {
        const SubString& s = m_lexemes[i];
        const int len = s.m_end - s.m_start;
        hkString::strNcpy(dst, m_lex.m_buffer.m_buffer.begin() + m_lex.m_buffer.m_bufferStart + s.m_start, len);
        dst[len] = 0;
        m_keys.pushBack(dst);
        m_attribMap.insert(dst, i + 1);
        dst += len + 1;
    }
}

// 0x00F72030 (:185): the next event; comments are skipped. Only now may the lexer drop what came before.
hkXmlStreamParser::Token hkXmlStreamParser::_advance()
{
    m_keys.clear();
    m_attribMap.clear();
    m_lex.bufferCommit();
    m_lexemes.clear();
    for (hkXmlLexAnalyzer::Token tok = m_lex.advance();; tok = m_lex.advance())
    {
        switch (tok)
        {
            case hkXmlLexAnalyzer::TOKEN_START_BLOCK:
            {
                return _parseBlock();
            }
            case hkXmlLexAnalyzer::TOKEN_START_QBLOCK:
            {
                return _parseQBlock();
            }
            case hkXmlLexAnalyzer::TOKEN_TEXT:
            {
                return TOKEN_TEXT;
            }
            case hkXmlLexAnalyzer::TOKEN_WHITESPACE:
            {
                return TOKEN_WHITESPACE;
            }
            case hkXmlLexAnalyzer::TOKEN_COMMENT:
            {
                break;
            }
            case hkXmlLexAnalyzer::TOKEN_EOF:
            {
                return TOKEN_EOF;
            }
            default:
            {
                return TOKEN_ERROR;
            }
        }
    }
}

// 0x00F720C0 (:231)
hkXmlStreamParser::Token hkXmlStreamParser::advance()
{
    m_token = _advance();
    return m_token;
}

// 0x00F71AD0 (:236): the value as written, quotes included
hkResult hkXmlStreamParser::getValue(const char* key, hkSubString& valueOut) const
{
    const int index = m_attribMap.getWithDefault(key, -1);
    if (index < 0)
    {
        return HK_FAILURE;
    }
    const SubString& s = m_lexemes[index];
    valueOut = m_lex.getLexeme(s.m_start, s.m_end);
    return HK_SUCCESS;
}

// 0x00F71B20 (:250): empty without lexemes
hkSubString hkXmlStreamParser::getBlockName() const
{
    if (m_lexemes.getSize() > 0)
    {
        const SubString& s = m_lexemes[0];
        return m_lex.getLexeme(s.m_start, s.m_end);
    }
    return hkSubString();
}

// 0x00F71B60 (:263)
hkSubString hkXmlStreamParser::getLexeme() const
{
    return m_lex.getLexeme();
}

// 0x00F71B80 (:269)
static void _dumpAttributes(hkXmlStreamParser& parser, hkOstream& stream)
{
    stream << parser.getBlockName();
    const hkArray<const char*,hkContainerHeapAllocator>& keys = parser.getKeys();
    for (int i = 0; i < keys.getSize(); i++)
    {
        const char* key = keys[i];
        stream << " " << key << "=";
        hkSubString value;
        parser.getValue(key, value);
        stream << value;
    }
}

// 0x00F720D0 (:286): echoes the events until the end or an error
void hkXmlStreamParser::dumpParse(hkOstream& out)
{
    for (;;)
    {
        const Token tok = advance();
        switch (tok)
        {
            case TOKEN_QBLOCK:
            {
                out << "<?";
                _dumpAttributes(*this, out);
                out << "?>\n";
                break;
            }
            case TOKEN_BLOCK_START:
            case TOKEN_BLOCK_START_END:
            {
                out << "<";
                _dumpAttributes(*this, out);
                if (tok == TOKEN_BLOCK_START_END)
                {
                    out << "/";
                }
                out << ">\n";
                break;
            }
            case TOKEN_BLOCK_END:
            {
                out << "</" << getBlockName() << ">\n";
                break;
            }
            case TOKEN_TEXT:
            case TOKEN_WHITESPACE:
            {
                out << getLexeme();
                break;
            }
            case TOKEN_ERROR:
            case TOKEN_EOF:
            {
                return;
            }
        }
    }
}

// 0x00F71510 (:333): an optional '-' and decimal digits. Original bug kept (both overloads): the length is not
// checked, so text over 31 characters overruns the 32-byte buffer.
hkResult hkXmlStreamParser::parseInt(const hkSubString& subString, int64_t& valueOut)
{
    const char* start = subString.m_start;
    const char* end = subString.m_end;
    const int len = int(end - start);
    if (len <= 0)
    {
        return HK_FAILURE;
    }
    const char* p = start;
    if (*p == '-')
    {
        p++;
    }
    if (p >= end)
    {
        return HK_FAILURE;
    }
    for (; p < end; p++)
    {
        if (!hkXmlLexAnalyzer::isDigit(*p))
        {
            return HK_FAILURE;
        }
    }
    char buf[32];
    hkString::strNcpy(buf, start, len);
    buf[len] = 0;
    valueOut = hkString::atoll(buf, 0);
    return HK_SUCCESS;
}

// 0x00F715A0 (:378): sign and magnitude; "0x"/"0X" hexadecimal or decimal
hkResult hkXmlStreamParser::parseInt(const hkSubString& subString, int& signOut, uint64_t& magOut)
{
    signOut = 1;
    const char* start = subString.m_start;
    const char* end = subString.m_end;
    if (int(end - start) <= 0)
    {
        return HK_FAILURE;
    }
    if (*start == '-')
    {
        start++;
        signOut = -1;
    }
    if (start >= end)
    {
        return HK_FAILURE;
    }
    const int len = int(end - start);
    if (len >= 3 && start[0] == '0' && (start[1] == 'x' || start[1] == 'X'))
    {
        for (const char* p = start + 2; p < end; p++)
        {
            if (!hkXmlLexAnalyzer::isHexDigit(*p))
            {
                return HK_FAILURE;
            }
        }
    }
    else
    {
        for (const char* p = start; p < end; p++)
        {
            if (!hkXmlLexAnalyzer::isDigit(*p))
            {
                return HK_FAILURE;
            }
        }
    }
    char buf[32];
    hkString::strNcpy(buf, start, len);
    buf[len] = 0;
    magOut = hkString::atoull(buf, 0);
    return HK_SUCCESS;
}

// 0x00F716A0 (:465): text, or 'x' and the hexadecimal bits of a float (8 digits) or double (16). A double is
// narrowed, its Havok limits to the float ones (HK_DOUBLE_MAX/MIN/EPSILON/HIGH: bit patterns from rfg.exe).
hkResult hkXmlStreamParser::parseReal(const hkSubString& subString, float& valueOut)
{
    const char* s = subString.m_start;
    const int len = subString.length();
    if (len < 1 || len > 32)
    {
        return HK_FAILURE;
    }
    if (*s != 'x')
    {
        return hkFloatParseUtil::parseFloat(subString, valueOut);
    }
    if (len != 9 && len != 17)
    {
        return HK_FAILURE;
    }
    uint64_t bits = 0;
    for (int i = 0; i < len - 1; i++)
    {
        const char c = s[i + 1];
        int digit;
        if (c >= '0' && c <= '9')
        {
            digit = c - '0';
        }
        else if (c >= 'A' && c <= 'F')
        {
            digit = c - 'A' + 10;
        }
        else if (c >= 'a' && c <= 'f')
        {
            digit = c - 'a' + 10;
        }
        else
        {
            return HK_FAILURE;
        }
        bits = (bits << 4) | (uint64_t)(int64_t)digit;
    }
    if (len == 9)
    {
        const unsigned int fbits = (unsigned int)bits;
        memcpy(&valueOut, &fbits, sizeof(valueOut));
        return HK_SUCCESS;
    }
    double d;
    memcpy(&d, &bits, sizeof(d));
    static const uint64_t doubleLimits[4] = {
        0x7FEFFCD7672F2096ull, // HK_DOUBLE_MAX 1.7970e+308
        0x0010000000000000ull, // HK_DOUBLE_MIN (DBL_MIN)
        0x3CB0000000000000ull, // HK_DOUBLE_EPSILON (DBL_EPSILON)
        0x5F22087C1AEB65DCull, // HK_DOUBLE_HIGH 1.8446726e+150
    };
    static const unsigned int floatLimits[4] = {
        0x7F7FFFEE, // HK_FLOAT_MAX 3.40282e+38f
        0x00800000, // HK_FLOAT_MIN (FLT_MIN)
        0x34000000, // HK_FLOAT_EPSILON (FLT_EPSILON)
        0x5F7FFFF0, // HK_FLOAT_HIGH 1.8446726e+019f
    };
    for (int i = 0; i < 4; i++)
    {
        double limit;
        memcpy(&limit, &doubleLimits[i], sizeof(limit));
        if (d == limit)
        {
            memcpy(&valueOut, &floatLimits[i], sizeof(valueOut));
            return HK_SUCCESS;
        }
    }
    valueOut = (float)d;
    return HK_SUCCESS;
}

// 0x00F71C10 (:438): the quoted value as a decimal; the value is set (0 on a parse failure) whenever quoted.
// The closing quote is checked at m_end - 1 before the opening one is skipped, so a lone '"' passes.
hkResult hkXmlStreamParser::getIntAttribute(const char* key, int& value) const
{
    hkSubString s;
    if (getValue(key, s) == HK_SUCCESS && s.m_start[0] == '"')
    {
        s.m_end--;
        if (*s.m_end == '"')
        {
            s.m_start++;
            int64_t v = 0;
            const hkResult res = parseInt(s, v);
            value = (int)v;
            return res;
        }
    }
    return HK_FAILURE;
}

// 0x00F71CA0 (:459)
hkBool hkXmlStreamParser::hasAttribute(const char* key) const
{
    hkSubString s;
    return getValue(key, s) == HK_SUCCESS;
}

// 0x00F71CD0 (:547)
hkResult hkXmlStreamParser::getRealAttribute(const char* key, float& value) const
{
    hkSubString s;
    if (getValue(key, s) == HK_SUCCESS && s.m_start[0] == '"')
    {
        s.m_end--;
        if (*s.m_end == '"')
        {
            s.m_start++;
            return parseReal(s, value);
        }
    }
    return HK_FAILURE;
}

// 0x00F718A0 (:565): any entity
hkBool hkXmlStreamParser::needsDecode(const hkSubString& subString)
{
    for (const char* p = subString.m_start; p < subString.m_end; p++)
    {
        if (*p == '&')
        {
            return true;
        }
    }
    return false;
}

// 0x00F718D0 (:580): &amp; &lt; &gt; &quot; &apos; and decimal &#N; (up to 255); any other '&' fails. The named
// entities are matched only with more characters left than their length (so one ending the text fails too).
hkResult hkXmlStreamParser::decodeString(const hkSubString& subString, hkStringBuf& buf)
{
    buf.clear();
    const char* end = subString.m_end;
    const char* s = subString.m_start;
    while (s < end)
    {
        if (*s != '&')
        {
            const char* p = s + 1;
            while (p < end && *p != '&')
            {
                p++;
            }
            buf.append(s, int(p - s));
            s = p;
            continue;
        }
        const int left = int(end - s);
        if (left > 4 && hkString::strNcmp(s + 1, "amp;", 4) == 0)
        {
            buf.append("&", 1);
            s += 5;
            continue;
        }
        if (left > 3)
        {
            if (hkString::strNcmp(s + 1, "lt;", 3) == 0)
            {
                buf.append("<", 1);
                s += 4;
                continue;
            }
            if (hkString::strNcmp(s + 1, "gt;", 3) == 0)
            {
                buf.append(">", 1);
                s += 4;
                continue;
            }
        }
        if (left > 5)
        {
            if (hkString::strNcmp(s + 1, "quot;", 5) == 0)
            {
                buf.append("\"", 1);
                s += 6;
                continue;
            }
            if (hkString::strNcmp(s + 1, "apos;", 5) == 0)
            {
                buf.append("'", 1);
                s += 6;
                continue;
            }
        }
        if (s[1] != '#' || !hkXmlLexAnalyzer::isDigit(s[2]) || left < 4)
        {
            return HK_FAILURE;
        }
        const char* p = s + 2;
        for (;;)
        {
            if (p >= end)
            {
                return HK_FAILURE;
            }
            if (!hkXmlLexAnalyzer::isDigit(*p))
            {
                break;
            }
            p++;
        }
        const int n = int(p - s);
        if (*p != ';' || n > 16)
        {
            return HK_FAILURE;
        }
        char digits[16];
        hkString::strNcpy(digits, s + 2, n - 2);
        digits[n - 2] = 0;
        const unsigned int code = (unsigned int)hkString::atoi(digits, 0);
        if (code > 255)
        {
            return HK_FAILURE;
        }
        char c[2] = { (char)code, 0 };
        buf.append(c, -1);
        s = p + 1;
    }
    return HK_SUCCESS;
}
