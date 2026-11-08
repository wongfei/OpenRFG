// Original: library/modules/runtime/base/sources/global/inivariableparser.hpp
// Implementation: library/modules/runtime/base/sources/global/inivariableparser.cpp
//
// The ini variable file parser (tokens, scanner, parser).
#pragma once

#include "keen/base/types.hpp"
#include "keen/base/filepath.hpp"
#include "keen/base/scannerinput.hpp"

namespace keen
{
    class ReadStream;

    enum IniVariableTokenType
    {
        IniVariableTokenType_Invalid = -1,
        IniVariableTokenType_Identifier = 0,
        IniVariableTokenType_StringLiteral = 1,
        IniVariableTokenType_IntegerLiteral = 2,
        IniVariableTokenType_FloatLiteral = 3,
        IniVariableTokenType_LeftBrace = 4,
        IniVariableTokenType_RightBrace = 5,
        IniVariableTokenType_Semicolon = 6,
        IniVariableTokenType_Assignment = 7,
        IniVariableTokenType_Append = 8,
        IniVariableTokenType_Namespace = 9,
        IniVariableTokenType_Include = 10,
        IniVariableTokenType_Count = 11,
    };

    // size 0x108
    class IniVariableToken
    {
    public:
        enum
        {
            MaxLexemLength = 256,
        };

        IniVariableToken(); // 0x00BF84A0 (:11)
        void clear(); // 0x00BF0830 (:16)
        IniVariableTokenType getType() const; // 0x00BF0800 (.h:49)
        const char* getLexem() const; // 0x00BF0810 (.h:50)
        int getSourceLine() const; // 0x00BF0820 (.h:51)

    private:
        IniVariableTokenType m_type; // 0x00
        char m_lexem[256]; // 0x04
        int m_line; // 0x104
    };

    // size 0x14
    class IniVariableScanner
    {
        IniVariableScanner(const IniVariableScanner&);
    public:
        IniVariableScanner(ReadStream* pBuffer); // 0x00BF84C0 (:24)
        char getNextChar(); // 0x00BFE4B0 (:28)
        char peekNextChar(); // 0x00BFE4E0 (:43)
        void putBackChar(char ch); // 0x00BF84E0 (:57)
        int getCurrentInputLine() const; // 0x00BF8500 (:62)
        void clear();
        bool getNextToken(IniVariableToken& token); // 0x00C06100 (:67)
        bool getNextTokenUntilEndOfLine(IniVariableToken& token); // 0x00C061A0 (:102)
    private:
        bool skipWhiteSpace(); // 0x00C038F0 (:177)
        bool skipToEndOfLine(); // 0x00BFE520 (:252)
        bool readString(IniVariableToken& token, char ch); // 0x00C03AD0 (:267)
        char readEscapeCharacter(); // 0x00BFE560 (:328)
        bool isIdentifierChar(char ch, bool firstChar) const; // 0x00BF8510 (:372)
        bool readIdentifier(IniVariableToken& token, char ch); // 0x00C03C40 (:387)
        bool readOperator(IniVariableToken& token, char ch); // 0x00C03D30 (:417)
        IniVariableScanner& operator=(const IniVariableScanner&);
    public:
        ~IniVariableScanner(); // 0x00BF8560

    private:
        ScannerInput m_input; // 0x00
    };

    // size 0x770
    class IniVariableParser
    {
    public:
        enum
        {
            MaxVariableNameLength = 256,
            MaxVariableValueLength = 256,
        };

        IniVariableParser(ReadStream* pBuffer, const char* pFileName); // 0x00BFE660 (:449)
        ~IniVariableParser(); // 0x00BF8570 (:456)
        int getCurrentLine() const; // 0x00BFE4A0 (.h:110)
        bool endOfFileReached() const; // 0x00BF0850 (:460)
        bool isValid() const; // 0x00BF0860 (:465)
        bool parseNextVariableAssignment(); // 0x00C0B7A0 (:470)
        const char* getVariableName() const; // 0x00BF0880 (:561)
        const char* getVariableValue() const; // 0x00BF0890 (:566)
    protected:
        bool peekNextToken(IniVariableToken& token); // 0x00C083D0 (:578)
        IniVariableTokenType peekNextTokenType(); // 0x00C08420 (:588)
        void putBackToken(const IniVariableToken& token); // 0x00BF08A0 (:571)
        bool getNextToken(IniVariableToken& token); // 0x00C06310 (:599)
        bool getNextTokenUntilEndOfLine(IniVariableToken& token); // 0x00C06340 (:612)
        bool consumeToken(IniVariableTokenType type); // 0x00C06370 (:626)
        bool parseIdentifier(char* pBuffer, uint32 capacity); // 0x00C06410 (:637)

        IniVariableScanner m_scanner; // 0x00
        IniVariableToken m_putBackToken; // 0x14
        FilePath m_currentVariableName; // 0x11C
        const char* m_pFileName; // 0x564
        int m_currentNamespaceDepth; // 0x568
        char m_variableName[256]; // 0x56C
        char m_variableValue[256]; // 0x66C
        bool m_eofReached; // 0x76C
        bool m_inErrorState; // 0x76D
    };
}
