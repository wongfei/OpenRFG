// Original: library/modules/runtime/base/sources/global/formatstringinput.hpp
// Implementation: library/modules/runtime/base/sources/global/formatstringinput.cpp
//
// FormatStringInput, a cursor over a format string.
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    // size 0x8
    class FormatStringInput
    {
    public:
        FormatStringInput(const char* pInput); // 0x00BF00C0 (:10)
        bool isEmpty() const; // 0x00BF00D0 (:18)
        bool matchNextChar(char ch); // 0x00BF7F30 (:23)
        char peekNextChar() const; // 0x00BF00E0 (:37)
        char getNextChar(); // 0x00BF7F50 (:42)
        void consumeChar(); // 0x00BF00F0 (:51)
        bool isNextCharDigit() const; // 0x00BF7F60 (:59)
        uint32 readUnsignedInt(); // 0x00BF7F80 (:68)

    private:
        const char* pInputStart; // 0x00
        const char* pCurrentPos; // 0x04
    };
}
