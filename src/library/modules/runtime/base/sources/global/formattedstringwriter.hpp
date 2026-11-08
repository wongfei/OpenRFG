// Original: library/modules/runtime/base/sources/global/formattedstringwriter.hpp
// Implementation: library/modules/runtime/base/sources/global/formattedstringwriter.cpp
//
// FormattedStringWriter and FormatOutputOptions, the output side of formatString.
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    struct FormatOutputOptions;
    class StringParameter;
    class WriteStream;

    // size 0x8
    class FormattedStringWriter
    {
    public:
        FormattedStringWriter(WriteStream* pStream); // 0x00BF0100 (:24)
        void outputChar(char ch); // 0x00BF7FB0 (:32)
        void outputString(const char* pString, bool quoteString); // 0x00BF8020 (:42)
        void outputSignedInteger(const FormatOutputOptions& options, int number); // 0x00C034D0 (:100)
        void outputSignedInteger(const FormatOutputOptions& options, sint64 number); // 0x00C03510 (:117)
        void outputUnsignedInteger(const FormatOutputOptions& options, uint64 number, uint32 basis, const char* pPrefix); // 0x00C03590 (:140)
        void outputUnsignedInteger(const FormatOutputOptions& options, uint32 number, uint32 basis, const char* pPrefix); // 0x00C03570 (:134)
        void outputFourCC(const FormatOutputOptions& options, uint32 fourCC); // 0x00BF8160 (:146)
        void outputFloatingPointFixed(const FormatOutputOptions& options, float value); // 0x00BFDB50 (:167)
        void outputFixedPointInteger(const FormatOutputOptions& options, int number, int decimalBitCount); // 0x00C035C0 (:336)
        uint32 getCharCount(); // 0x00BF0120 (:439)
    private:
        // The parameterized formatString and formatStringVarArgs write unsigned numbers with outputInteger.
        friend uint32 formatString(char* pTargetBuffer, uint32 targetCapacity, const char* pFormat, const StringParameter* pParameters, uint32 parameterCount);
        friend uint32 formatStringVarArgs(WriteStream* pStream, const char* pFormat, char* arguments);

        char getDigitChar(const FormatOutputOptions& options, int digitValue) const; // 0x00BF0130 (:444)
        int getDigitCount(uint64 number, uint32 basis) const; // 0x00BF01A0 (:486)
        int getDigitCount(uint32 number, uint32 basis) const; // 0x00BF0170 (:468)
        void outputInteger(const FormatOutputOptions& options, uint64 number, uint32 basis, char sign, const char* pPrefix); // 0x00BFE1F0 (:567)
        void outputInteger(const FormatOutputOptions& options, uint32 number, uint32 basis, char sign, const char* pPrefix); // 0x00BFDF60 (:504)
        void outputNumber(const FormatOutputOptions& options, uint64 number, uint32 digitCount, uint32 groupCharCount, uint32 basis); // 0x00BF8210 (:639)
        void outputNumber(const FormatOutputOptions& options, uint32 number, uint32 digitCount, uint32 groupCharCount, uint32 basis); // 0x00BF81A0 (:630)
        void outputNumberIntoBuffer(char* pBuffer, uint32 bufferCapacity, const FormatOutputOptions& options, uint64 number, uint32 digitCount, uint32 groupCharCount, uint32 basis); // 0x00BF02A0 (:676)
        void outputNumberIntoBuffer(char* pBuffer, uint32 bufferCapacity, const FormatOutputOptions& options, uint32 number, uint32 digitCount, uint32 groupCharCount, uint32 basis); // 0x00BF01F0 (:648)

        WriteStream* m_pStream; // 0x00
        uint32 m_charCount; // 0x04
    };

    // size 0x14
    struct FormatOutputOptions
    {
        enum Mode
        {
            Mode_Decimal = 0,
            Mode_Short = 1,
        };

        FormatOutputOptions(); // 0x00BF7E50 (.h:14)
        void reset(); // 0x00BF0080 (.h:26)

        int outputWidth; // 0x00
        int outputPrecision; // 0x04
        int mode; // 0x08
        char fillChar; // 0x0C
        bool alwaysDisplaySign; // 0x0D
        bool bigHexDigits; // 0x0E
        bool alternateForm; // 0x0F
        bool leftAlign; // 0x10
        bool insertBlankSpace; // 0x11
        bool showGroupingSymbol; // 0x12
        bool quoteString; // 0x13
    };
}
