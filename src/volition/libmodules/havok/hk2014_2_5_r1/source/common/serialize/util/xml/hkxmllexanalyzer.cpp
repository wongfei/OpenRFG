// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/util/xml/hkxmllexanalyzer.cpp
//
// Havok 2014.2.5 (stub_impl.md S4, serializer SF): the XML tokenizer under hkXmlStreamParser, from rfg.exe.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/util/xml/hkxmllexanalyzer.h"

// 0x0137A810: indexed by Token
const char* const hkXmlLexAnalyzer::tokenNames[] =
{
    "START_BLOCK",
    "START_QBLOCK",
    "TEXT",
    "WHITESPACE",
    "COMMENT",
    "EOF",
    "END_BLOCK",
    "END_QBLOCK",
    "IDENTIFIER",
    "EQUALS",
    "QUOTED_STRING",
    "SLASH",
    "ERROR",
};

// 0x00F7A750 (:33)
hkXmlLexAnalyzer::hkXmlLexAnalyzer(hkStreamReader* reader)
    : m_buffer(reader)
    , m_state(0)
{
}

// 0x00F7A780 (:37): after "<!--", up to "-->". Original bug kept: a mismatch restarts the match without looking at
// the mismatching character again, so "--->" does not end the comment.
hkXmlLexAnalyzer::Token hkXmlLexAnalyzer::_lexComment()
{
    const char end[] = "-->";
    int matched = 0;
    for (;;)
    {
        const char c = m_buffer.nextChar();
        if (c == 0)
        {
            return _handleError("Badly formed comment");
        }
        if (end[matched] != c)
        {
            matched = 0;
            continue;
        }
        if (++matched == 3)
        {
            return TOKEN_COMMENT;
        }
    }
}

// 0x00F7A7F0 (:64): after the opening quote; a backslash escapes the next character
hkXmlLexAnalyzer::Token hkXmlLexAnalyzer::_lexQuotedString()
{
    bool escaped = false;
    for (;;)
    {
        const char c = m_buffer.nextChar();
        if (c == 0)
        {
            return _handleError("Didn't hit terminating \"");
        }
        if (escaped)
        {
            escaped = false;
        }
        else if (c == '\\')
        {
            escaped = true;
        }
        else if (c == '"')
        {
            return TOKEN_QUOTED_STRING;
        }
    }
}

// 0x00F7A860 (:96): letters, digits, '_' and ':'
hkXmlLexAnalyzer::Token hkXmlLexAnalyzer::_lexIdentifier()
{
    for (;;)
    {
        const char c = m_buffer.getChar();
        if (!(isLetter(c) || isDigit(c) || c == '_' || c == ':'))
        {
            return TOKEN_IDENTIFIER;
        }
        m_buffer.nextChar();
    }
}

// 0x00F7A8E0 (:112)
hkXmlLexAnalyzer::Token hkXmlLexAnalyzer::_lexWhiteSpace()
{
    for (;;)
    {
        const char c = m_buffer.getChar();
        if (!isWhiteSpace(c))
        {
            return TOKEN_WHITESPACE;
        }
        m_buffer.nextChar();
    }
}

// 0x00F7A950 (:128): from the character already read, up to white space or '<' (not included)
hkXmlLexAnalyzer::Token hkXmlLexAnalyzer::_lexText()
{
    m_buffer.rewindChar();
    for (;;)
    {
        const char c = m_buffer.nextChar();
        if (c == 0)
        {
            return TOKEN_TEXT;
        }
        if (c == '<' || isWhiteSpace(c))
        {
            m_buffer.rewindChar();
            return TOKEN_TEXT;
        }
    }
}

// 0x00F7A9D0 (:159): inside "<...>" or "<?...?>"; white space is skipped. A '?' outside a <? block is skipped
// too, and so is one in it not followed by '>'.
hkXmlLexAnalyzer::Token hkXmlLexAnalyzer::_matchInBrackets()
{
    for (;;)
    {
        const char c = m_buffer.nextChar();
        switch (c)
        {
            case 0:
            {
                return TOKEN_EOF;
            }
            case ' ':
            case '\t':
            case '\r':
            case '\n':
            {
                _lexWhiteSpace();
                m_buffer.lexemeCommit();
                break;
            }
            case '"':
            {
                return _lexQuotedString();
            }
            case '/':
            {
                return TOKEN_SLASH;
            }
            case '=':
            {
                return TOKEN_EQUALS;
            }
            case '>':
            {
                if (m_state & IN_QBLOCK)
                {
                    return _handleError("Expecting ?> to close <? section");
                }
                m_state &= ~IN_BRACKETS;
                return TOKEN_END_BLOCK;
            }
            case '?':
            {
                if (m_state & IN_QBLOCK)
                {
                    if (m_buffer.nextChar() == '>')
                    {
                        m_state &= ~(IN_BRACKETS | IN_QBLOCK);
                        return TOKEN_END_QBLOCK;
                    }
                    m_buffer.rewindChar();
                }
                break;
            }
            default:
            {
                if (isLetter(c) || c == '_' || c == ':')
                {
                    return _lexIdentifier();
                }
                return _handleError("Unexpected token");
            }
        }
    }
}

// 0x00F7AB70 (:231): text, white space, a comment, or the start of a block
hkXmlLexAnalyzer::Token hkXmlLexAnalyzer::_matchOutsideBrackets()
{
    const char c = m_buffer.nextChar();
    if (c == 0)
    {
        return TOKEN_EOF;
    }
    if (c == '<')
    {
        if (m_buffer.getChar() == '?')
        {
            m_buffer.nextChar();
            m_state |= IN_BRACKETS | IN_QBLOCK;
            return TOKEN_START_QBLOCK;
        }
        if (m_buffer.matchAndConsume("!--"))
        {
            return _lexComment();
        }
        m_state |= IN_BRACKETS;
        return TOKEN_START_BLOCK;
    }
    if (isWhiteSpace(c))
    {
        return _lexWhiteSpace();
    }
    return _lexText();
}

// 0x00F7A740 (:272): the description is not kept
hkXmlLexAnalyzer::Token hkXmlLexAnalyzer::_handleError(const char* desc)
{
    (void)desc;
    return TOKEN_ERROR;
}

// 0x00F7AC70 (:278): each token starts a new lexeme
hkXmlLexAnalyzer::Token hkXmlLexAnalyzer::advance()
{
    m_buffer.lexemeCommit();
    if (m_state & IN_BRACKETS)
    {
        return _matchInBrackets();
    }
    return _matchOutsideBrackets();
}
