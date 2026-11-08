// Original: library/modules/runtime/base/include/keen/base/scannerinput.hpp
// Implementation: library/modules/runtime/base/sources/global/scannerinput.cpp
//
// ScannerInput, a character source for scanners (ini/xml parsing).
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    class ReadStream;

    // size 0x14
    class ScannerInput
    {
    public:
        enum
        {
            MaxPutBackCharCount = 8,
        };

    private:
        ScannerInput(const ScannerInput&);
    public:
        ScannerInput(ReadStream* pBuffer); // 0x00BF1730 (:12)
        ~ScannerInput(); // 0x00BF1750 (:18)
        bool getNextChar(char& ch); // 0x00BFA310 (:22)
        bool peekNextChar(char& ch); // 0x00BFA3B0 (:51)
        void putBackChar(char ch); // 0x00BF1760 (:62)
        bool canRead() const; // 0x00BF1780 (:74)
        int getCurrentLine() const; // 0x00BF17A0 (:79)
    private:
        ScannerInput& operator=(const ScannerInput&);

        ReadStream* m_pBuffer; // 0x00
        int m_currentLine; // 0x04
        char m_putBackChars[8]; // 0x08
        uint32 m_putBackCharCount; // 0x10
    };
}
