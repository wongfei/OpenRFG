// Original: library/modules/runtime/base/sources/global/formattedstringwriter.cpp
//
// The output side of keen's formatters: characters, strings, integers (with fill, alignment, sign, prefix and ','
// grouping) and fixed point floats (through printFloatToBuffer), counted as they are written.
//
// Partial: what keen's formatStringVarArgs and the parameterized formatString call: the 32-bit and 64-bit integer
// paths, strings, fixed point floats, FourCCs and 20.12 fixed point integers (final_push.md step 5). Not ported (no
// callers in rfg.exe: inlined or unused): outputUnsignedInteger, getDigitChar, getDigitCount(uint32).
//
// Notes (decompiled behaviour, kept as is):
// - A left aligned integer gets its prefix only when it is not zero; right aligned it always gets it.
// - A missing prefix is written as "[nullptr]".
// - outputFloatingPointFixed sizes the fraction from the unclamped precision but prints with the clamped one (0-100).
// - outputFixedPointInteger prints at most 6 fraction digits, and rounds a fraction of 1..0xFFE units up by one unit
//   whatever decimalBitCount is.

#include "library/modules/runtime/base/sources/global/formattedstringwriter.hpp"
#include "library/modules/runtime/base/sources/global/floattostring.hpp"
#include "keen/base/writestream.hpp"

namespace keen
{
    // 0x00BF7E50 (.h:14)
    FormatOutputOptions::FormatOutputOptions()
    {
        reset();
    }

    // 0x00BF0080 (.h:26)
    void FormatOutputOptions::reset()
    {
        mode = Mode_Decimal;
        outputWidth = 0;
        // Magic number: printf's default precision.
        outputPrecision = 6;
        fillChar = ' ';
        alwaysDisplaySign = false;
        bigHexDigits = false;
        alternateForm = false;
        leftAlign = false;
        insertBlankSpace = false;
        showGroupingSymbol = false;
        quoteString = false;
    }

    // 0x00BF0100 (:24)
    FormattedStringWriter::FormattedStringWriter(WriteStream* pStream)
    {
        m_pStream = pStream;
        m_charCount = 0u;
    }

    // 0x00BF7FB0 (:32)
    // '\0' is not written.
    void FormattedStringWriter::outputChar(char ch)
    {
        if (ch != '\0')
        {
            m_pStream->writeDataBlock(&ch, 1u);
            ++m_charCount;
        }
    }

    // 0x00BF8020 (:42)
    // Quoting escapes tab, new line, carriage return, '"' and '\' and turns other control characters (and, char being
    // signed, bytes from 0x80) into '?'; it adds no quotes around the string.
    void FormattedStringWriter::outputString(const char* pString, bool quoteString)
    {
        if (pString == nullptr)
        {
            pString = "[nullptr]";
        }
        else if (quoteString)
        {
            for (; *pString != '\0'; ++pString)
            {
                char c = *pString;
                switch (c)
                {
                case '\t':
                    outputChar('\\');
                    c = 't';
                    break;
                case '\n':
                    outputChar('\\');
                    c = 'n';
                    break;
                case '\r':
                    outputChar('\\');
                    c = 'r';
                    break;
                case '"':
                    outputChar('\\');
                    c = '"';
                    break;
                case '\\':
                    outputChar('\\');
                    c = '\\';
                    break;
                default:
                    if (c < ' ')
                    {
                        c = '?';
                    }
                    break;
                }
                outputChar(c);
            }
            return;
        }
        for (; *pString != '\0'; ++pString)
        {
            outputChar(*pString);
        }
    }

    // 0x00C034D0 (:100)
    void FormattedStringWriter::outputSignedInteger(const FormatOutputOptions& options, int number)
    {
        char sign = '\0';
        if (number < 0)
        {
            sign = '-';
            number = -number;
        }
        else if (number > 0 && options.alwaysDisplaySign)
        {
            sign = '+';
        }
        outputInteger(options, (uint32)number, 10u, sign, "");
    }

    // 0x00C03510 (:117)
    void FormattedStringWriter::outputSignedInteger(const FormatOutputOptions& options, sint64 number)
    {
        char sign = '\0';
        if (number < 0)
        {
            sign = '-';
            number = -number;
        }
        else if (number > 0 && options.alwaysDisplaySign)
        {
            sign = '+';
        }
        outputInteger(options, (uint64)number, 10u, sign, "");
    }

    // 0x00BF8160 (:146)
    // The four characters, lowest byte first (a zero byte writes nothing).
    void FormattedStringWriter::outputFourCC(const FormatOutputOptions& options, uint32 fourCC)
    {
        outputChar((char)fourCC);
        outputChar((char)(fourCC >> 8));
        outputChar((char)(fourCC >> 16));
        outputChar((char)(fourCC >> 24));
    }

    // 0x00C035C0 (:336)
    // A fixed point number with decimalBitCount fraction bits (formatStringVarArgs' %t: 20.12): the integer part, '.',
    // the fraction's leading zeros, then its other digits (at most 6 in all). The width counts the whole number: right
    // aligned, the integer part takes the padding; left aligned, the fraction is padded on its right.
    void FormattedStringWriter::outputFixedPointInteger(const FormatOutputOptions& options, int number, int decimalBitCount)
    {
        char sign = '\0';
        if (number < 0)
        {
            sign = '-';
            number = -number;
        }
        else if (number > 0 && options.alwaysDisplaySign)
        {
            sign = '+';
        }
        const uint32 integerPart = (uint32)number >> decimalBitCount;
        uint32 fraction = ((1u << decimalBitCount) - 1u) & (uint32)number;
        if (options.outputPrecision <= 0)
        {
            outputInteger(options, integerPart, 10u, sign, "");
            return;
        }

        FormatOutputOptions partOptions = options;
        if (partOptions.insertBlankSpace && sign == '\0')
        {
            outputChar(' ');
            --partOptions.outputWidth;
        }
        // Magic number: 0xFFE, a fraction of 1..0xFFE units is rounded up by one unit (a 12-bit fraction's range less
        // its ends, whatever decimalBitCount is).
        if (fraction - 1u < 0xFFEu)
        {
            ++fraction;
        }
        // Magic number: 6, the most fraction digits printed.
        int leadingZeroCount = 0;
        {
            const int zeroDigitLimit = partOptions.outputPrecision - 1 < 0 ? 0 : (partOptions.outputPrecision - 1 < 7 ? partOptions.outputPrecision - 1 : 6);
            uint32 scale = 1u;
            for (int i = 0; i < zeroDigitLimit; ++i)
            {
                scale *= 10u;
                if ((scale * fraction) >> decimalBitCount == 0u)
                {
                    ++leadingZeroCount;
                }
            }
        }
        uint32 fractionDigits = 0u;
        {
            const int digitLimit = partOptions.outputPrecision < 0 ? 0 : (partOptions.outputPrecision > 6 ? 6 : partOptions.outputPrecision);
            uint32 scale = 1u;
            for (int i = 0; i < digitLimit; ++i)
            {
                scale *= 10u;
                fractionDigits = ((scale * fraction) >> decimalBitCount) % 10u + fractionDigits * 10u;
            }
        }

        int rightPadding = 0;
        int integerWidth = 0;
        if (!partOptions.leftAlign)
        {
            int fractionDigitCount = 0;
            if (fractionDigits == 0u)
            {
                fractionDigitCount = 1;
            }
            else
            {
                for (uint32 rest = fractionDigits; rest != 0u; rest /= 10u)
                {
                    ++fractionDigitCount;
                }
            }
            integerWidth = partOptions.outputWidth - fractionDigitCount - leadingZeroCount - 1;
            if (integerWidth < 1)
            {
                integerWidth = 0;
            }
        }
        else
        {
            int integerDigitCount = 0;
            if (integerPart == 0u)
            {
                integerDigitCount = 1;
            }
            else
            {
                for (uint32 rest = integerPart; rest != 0u; rest /= 10u)
                {
                    ++integerDigitCount;
                }
            }
            rightPadding = partOptions.outputWidth - leadingZeroCount - integerDigitCount - 1;
            if (rightPadding < 1)
            {
                rightPadding = 0;
            }
            integerWidth = integerDigitCount;
        }
        partOptions.outputWidth = integerWidth;
        outputInteger(partOptions, integerPart, 10u, sign, "");
        outputChar('.');
        for (; leadingZeroCount > 0; --leadingZeroCount)
        {
            outputChar('0');
        }

        // The fraction's digits: plain decimal, padded on the right when left aligned.
        partOptions.mode = FormatOutputOptions::Mode_Decimal;
        partOptions.fillChar = ' ';
        partOptions.quoteString = false;
        partOptions.outputPrecision = 6;
        partOptions.outputWidth = partOptions.leftAlign ? rightPadding : 0;
        partOptions.insertBlankSpace = false;
        partOptions.showGroupingSymbol = false;
        partOptions.alternateForm = false;
        partOptions.alwaysDisplaySign = false;
        partOptions.bigHexDigits = false;
        outputInteger(partOptions, fractionDigits, 10u, '\0', "");
    }

    // 0x00BFDB50 (:167)
    void FormattedStringWriter::outputFloatingPointFixed(const FormatOutputOptions& options, float value)
    {
        int precision = options.outputPrecision;
        // Magic numbers: the precision is clamped to 0-100 digits.
        if (precision < 0)
        {
            precision = 0;
        }
        else if (precision > 100)
        {
            precision = 100;
        }
        FloatOutputInfo info;
        char digits[68];
        // Magic number: 0x41 is the digit buffer's capacity (64 digits and the terminator).
        if (!printFloatToBuffer(&info, digits, 0x41u, value, (uint32)precision))
        {
            outputString("<invalid>", false);
            return;
        }
        if (info.isSpecial)
        {
            for (const char* p = digits; *p != '\0'; ++p)
            {
                outputChar(*p);
            }
            return;
        }

        const int lastIntegerDigitIndex = (int)info.dotIndex - 1;
        const bool hasSign = info.isNegative || options.alwaysDisplaySign;
        const int integerDigitCount = (int)info.dotIndex < 2 ? 1 : (int)info.dotIndex;
        const int integerLength = hasSign ? integerDigitCount + 1 : integerDigitCount;
        const int fractionLength = options.outputPrecision == 0 ? 0 : options.outputPrecision + 1;
        const int groupCharCount = options.showGroupingSymbol ? integerDigitCount / 3 : 0;
        const int padding = options.outputWidth - (groupCharCount + fractionLength + integerLength);
        int leftPadding = 0;
        int rightPadding = 0;
        if (!options.leftAlign)
        {
            leftPadding = padding > 0 ? padding : 0;
        }
        else
        {
            rightPadding = padding > 0 ? padding : 0;
        }
        const int groupedLength = leftPadding + integerLength;

        const char signChar = info.isNegative ? '-' : '+';
        if (!hasSign)
        {
            if (options.insertBlankSpace)
            {
                outputChar(' ');
            }
        }
        else if (options.fillChar == '0')
        {
            outputChar(signChar);
        }

        // Zero fill takes part in the ',' grouping.
        int groupedCount = 0;
        int remainingGrouped = groupedLength;
        for (int i = leftPadding; i > 0; --i)
        {
            if (options.showGroupingSymbol && options.fillChar == '0' && groupedCount < groupedLength)
            {
                if (remainingGrouped % 3 == 0)
                {
                    outputChar(',');
                }
                ++groupedCount;
                --remainingGrouped;
            }
            outputChar(options.fillChar);
        }
        if (hasSign && options.fillChar != '0')
        {
            outputChar(signChar);
        }

        if (lastIntegerDigitIndex < 0)
        {
            outputString("0.", false);
            for (int i = -lastIntegerDigitIndex - 1; i > 0; --i)
            {
                outputChar('0');
            }
        }
        int fractionDigitCount = (int)info.digitCount - lastIntegerDigitIndex - 1;
        if (fractionDigitCount < 0)
        {
            fractionDigitCount = 0;
        }
        int trailingZeroCount = 0;
        if (options.mode == FormatOutputOptions::Mode_Decimal)
        {
            trailingZeroCount = options.outputPrecision - fractionDigitCount;
            if (trailingZeroCount < 0)
            {
                trailingZeroCount = 0;
            }
        }
        remainingGrouped = groupedLength - groupedCount;
        for (int i = 0; i < (int)info.digitCount; ++i)
        {
            if (options.showGroupingSymbol && groupedCount < groupedLength)
            {
                if (remainingGrouped % 3 == 0 && remainingGrouped < groupedLength)
                {
                    outputChar(',');
                }
                ++groupedCount;
                --remainingGrouped;
            }
            outputChar(digits[i]);
            if (i == lastIntegerDigitIndex && fractionDigitCount + trailingZeroCount > 0)
            {
                outputChar('.');
            }
        }
        for (; trailingZeroCount > 0; --trailingZeroCount)
        {
            outputChar('0');
        }
        for (; rightPadding > 0; --rightPadding)
        {
            outputChar(' ');
        }
    }

    // 0x00BF0120 (:439)
    uint32 FormattedStringWriter::getCharCount()
    {
        return m_charCount;
    }

    // 0x00BF01A0 (:486)
    int FormattedStringWriter::getDigitCount(uint64 number, uint32 basis) const
    {
        if (number == 0u)
        {
            return 1;
        }
        int digitCount = 0;
        for (; number != 0u; number /= basis)
        {
            ++digitCount;
        }
        return digitCount;
    }

    // 0x00BFE1F0 (:567)
    // The 64-bit variant of the one below, the same layout.
    void FormattedStringWriter::outputInteger(const FormatOutputOptions& options, uint64 number, uint32 basis, char sign, const char* pPrefix)
    {
        const uint32 digitCount = (uint32)getDigitCount(number, basis);
        int prefixLength = 0;
        if (pPrefix != nullptr)
        {
            for (const char* p = pPrefix; *p != '\0'; ++p)
            {
                ++prefixLength;
            }
        }
        const uint32 signLength = sign != '\0' ? 1u : 0u;
        const uint32 groupCharCount = options.showGroupingSymbol ? (uint32)((int)(digitCount - 1u) / 3) : 0u;
        int length = prefixLength + (int)(groupCharCount + signLength + digitCount);

        if (options.leftAlign)
        {
            outputChar(sign);
            if (number != 0u)
            {
                outputString(pPrefix, false);
            }
            outputNumber(options, number, digitCount, groupCharCount, basis);
            for (; length < options.outputWidth; ++length)
            {
                outputChar(' ');
            }
            return;
        }
        if (options.fillChar != ' ')
        {
            // Zero fill goes between the sign and prefix and the digits.
            if (sign != '\0')
            {
                outputChar(sign);
            }
            outputString(pPrefix, false);
            for (; length < options.outputWidth; ++length)
            {
                outputChar(options.fillChar);
            }
        }
        else
        {
            for (; length < options.outputWidth; ++length)
            {
                outputChar(options.fillChar);
            }
            if (sign != '\0')
            {
                outputChar(sign);
            }
            outputString(pPrefix, false);
        }
        char buffer[0x44];
        outputNumberIntoBuffer(buffer, 0x41u, options, number, digitCount, groupCharCount, basis);
        for (const char* p = buffer; *p != '\0'; ++p)
        {
            outputChar(*p);
        }
    }

    // 0x00BFDF60 (:504)
    void FormattedStringWriter::outputInteger(const FormatOutputOptions& options, uint32 number, uint32 basis, char sign, const char* pPrefix)
    {
        uint32 digitCount = 0u;
        if (number == 0u)
        {
            digitCount = 1u;
        }
        else
        {
            for (uint32 rest = number; rest != 0u; rest /= basis)
            {
                ++digitCount;
            }
        }
        int prefixLength = 0;
        if (pPrefix != nullptr)
        {
            for (const char* p = pPrefix; *p != '\0'; ++p)
            {
                ++prefixLength;
            }
        }
        const uint32 signLength = sign != '\0' ? 1u : 0u;
        const uint32 groupCharCount = options.showGroupingSymbol ? (uint32)((int)(digitCount - 1u) / 3) : 0u;
        int length = prefixLength + (int)(groupCharCount + signLength + digitCount);

        if (options.leftAlign)
        {
            outputChar(sign);
            if (number != 0u)
            {
                outputString(pPrefix, false);
            }
            outputNumber(options, number, digitCount, groupCharCount, basis);
            for (; length < options.outputWidth; ++length)
            {
                outputChar(' ');
            }
            return;
        }
        if (options.fillChar != ' ')
        {
            // Zero fill goes between the sign and prefix and the digits.
            if (sign != '\0')
            {
                outputChar(sign);
            }
            outputString(pPrefix, false);
            for (; length < options.outputWidth; ++length)
            {
                outputChar(options.fillChar);
            }
        }
        else
        {
            for (; length < options.outputWidth; ++length)
            {
                outputChar(options.fillChar);
            }
            if (sign != '\0')
            {
                outputChar(sign);
            }
            outputString(pPrefix, false);
        }
        char buffer[0x44];
        outputNumberIntoBuffer(buffer, 0x41u, options, number, digitCount, groupCharCount, basis);
        for (const char* p = buffer; *p != '\0'; ++p)
        {
            outputChar(*p);
        }
    }

    // 0x00BF8210 (:639)
    void FormattedStringWriter::outputNumber(const FormatOutputOptions& options, uint64 number, uint32 digitCount, uint32 groupCharCount, uint32 basis)
    {
        char buffer[0x44];
        outputNumberIntoBuffer(buffer, 0x41u, options, number, digitCount, groupCharCount, basis);
        for (const char* p = buffer; *p != '\0'; ++p)
        {
            outputChar(*p);
        }
    }

    // 0x00BF81A0 (:630)
    void FormattedStringWriter::outputNumber(const FormatOutputOptions& options, uint32 number, uint32 digitCount, uint32 groupCharCount, uint32 basis)
    {
        char buffer[0x44];
        outputNumberIntoBuffer(buffer, 0x41u, options, number, digitCount, groupCharCount, basis);
        for (const char* p = buffer; *p != '\0'; ++p)
        {
            outputChar(*p);
        }
    }

    // 0x00BF02A0 (:676)
    // The 64-bit variant of the one below.
    void FormattedStringWriter::outputNumberIntoBuffer(char* pBuffer, uint32 bufferCapacity, const FormatOutputOptions& options, uint64 number, uint32 digitCount, uint32 groupCharCount, uint32 basis)
    {
        if (bufferCapacity <= digitCount)
        {
            digitCount = bufferCapacity - 1u;
        }
        const uint32 length = digitCount + groupCharCount;
        char* pTarget = pBuffer + length - 1;
        for (uint32 i = 0u; i < digitCount; ++i)
        {
            if (groupCharCount != 0u && i != 0u && i % 3u == 0u)
            {
                *pTarget-- = ',';
                --groupCharCount;
            }
            const uint32 digit = (uint32)(number % basis);
            number /= basis;
            char c;
            if (digit < 10u)
            {
                c = (char)('0' + digit);
            }
            else if (digit - 10u < 6u)
            {
                c = (char)((options.bigHexDigits ? 'A' : 'a') + (digit - 10u));
            }
            else
            {
                c = ' ';
            }
            *pTarget-- = c;
        }
        pBuffer[length] = '\0';
    }

    // 0x00BF01F0 (:648)
    // Digits beyond 15 (bases above 16) print as ' '.
    void FormattedStringWriter::outputNumberIntoBuffer(char* pBuffer, uint32 bufferCapacity, const FormatOutputOptions& options, uint32 number, uint32 digitCount, uint32 groupCharCount, uint32 basis)
    {
        if (bufferCapacity <= digitCount)
        {
            digitCount = bufferCapacity - 1u;
        }
        const uint32 length = digitCount + groupCharCount;
        char* pTarget = pBuffer + length - 1;
        for (uint32 i = 0u; i < digitCount; ++i)
        {
            if (groupCharCount != 0u && i != 0u && i % 3u == 0u)
            {
                *pTarget-- = ',';
                --groupCharCount;
            }
            const uint32 digit = number % basis;
            number /= basis;
            char c;
            if (digit < 10u)
            {
                c = (char)('0' + digit);
            }
            else if (digit - 10u < 6u)
            {
                c = (char)((options.bigHexDigits ? 'A' : 'a') + (digit - 10u));
            }
            else
            {
                c = ' ';
            }
            *pTarget-- = c;
        }
        pBuffer[length] = '\0';
    }
}
