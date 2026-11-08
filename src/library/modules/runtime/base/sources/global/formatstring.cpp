// Original: library/modules/runtime/base/sources/global/formatstring.cpp
//
// keen's printf-like formatter, written through a FormattedStringWriter into a WriteStream (final_push.md step 5;
// until then the buffer variant used the C runtime's vsnprintf). The buffer variants write into a fixed memory stream:
// a full buffer stops the output (the stream's error state) but the count goes on, and the text is terminated at the
// count or the last byte of the buffer.
//
// The format: '%', flags (' ' blank for a positive number, '#' alternate form: a 0x/0X/0b/0 prefix, '+' always a sign,
// ',' digit grouping, '-' left align, '0' zero fill, 'q' quote and escape strings), a decimal width, '.' and a decimal
// precision, then the conversion:
// - c, s; d, i (int); u, x, X, o, b (unsigned int); ll or l before d, i, u, x, X, b (64-bit); z before u, x, X (32-bit
//   values through the 64-bit writer);
// - f (a double argument printed as a float, fixed point), g (the same, without trailing zeros);
// - t (an int as 20.12 fixed point), y (a FourCC), p (a pointer: 0x and 8 zero-filled hex digits), n (stores the
//   count so far).
// Notes (decompiled behaviour, kept as is):
// - A single l reads 64 bits, as ll does.
// - An unknown conversion prints "[base] Invalid format string '%s'!" (the whole format) and writes '%'; the
//   conversion character is then written as text. An unknown character after l or z prints "invalid input format"
//   and is written as text too. Neither consumes an argument.
// - There is no '*' width or precision, no h, no e/E/G/a.
// formatString(WriteStream*, ...) (0x00C05F70) is not ported: nothing in rfg.exe calls it.

#include "keen/base/string.hpp"
#include "keen/base/memorywritestream.hpp"
#include "keen/base/trace.hpp"
#include "library/modules/runtime/base/sources/global/formattedstringwriter.hpp"

#include <stdarg.h>

namespace keen
{
    template<typename T>
    static T formatStringNextArgument(char*& arguments)
    {
        const T value = *(const T*)arguments;
        arguments += (sizeof(T) + 3u) & ~3u;
        return value;
    }

    static bool formatStringIsDigit(char c)
    {
        return (unsigned char)(c - '0') < 10u;
    }

    // 0x00C05880 (:31)
    uint32 formatStringVarArgs(WriteStream* pStream, const char* pFormat, char* arguments)
    {
        const char* const pFullFormat = pFormat;
        FormattedStringWriter writer(pStream);
        FormatOutputOptions options;
        while (*pFormat != '\0')
        {
            if (*pFormat != '%')
            {
                writer.outputChar(*pFormat++);
                continue;
            }
            ++pFormat;
            options.reset();
            if (*pFormat == '%')
            {
                ++pFormat;
                writer.outputChar('%');
                continue;
            }

            bool alternateForm = false;
            bool flagsDone = false;
            while (!flagsDone)
            {
                switch (*pFormat)
                {
                case ' ':
                    options.insertBlankSpace = true;
                    break;
                case '#':
                    alternateForm = true;
                    break;
                case '+':
                    options.alwaysDisplaySign = true;
                    break;
                case ',':
                    options.showGroupingSymbol = true;
                    break;
                case '-':
                    options.leftAlign = true;
                    break;
                case '0':
                    options.fillChar = '0';
                    break;
                case 'q':
                    options.quoteString = true;
                    break;
                default:
                    flagsDone = true;
                    break;
                }
                if (!flagsDone)
                {
                    ++pFormat;
                }
            }
            options.alternateForm = alternateForm;
            if (formatStringIsDigit(*pFormat))
            {
                int width = 0;
                for (; formatStringIsDigit(*pFormat); ++pFormat)
                {
                    width = (*pFormat - '0') + width * 10;
                }
                options.outputWidth = width;
            }
            if (*pFormat == '.')
            {
                ++pFormat;
                if (formatStringIsDigit(*pFormat))
                {
                    int precision = 0;
                    for (; formatStringIsDigit(*pFormat); ++pFormat)
                    {
                        precision = (*pFormat - '0') + precision * 10;
                    }
                    options.outputPrecision = precision;
                }
            }

            // Each case leaves pFormat on the conversion character, which the end of the loop steps over, or (l, z)
            // steps over its characters itself and continues.
            switch (*pFormat)
            {
            case 'X':
                options.bigHexDigits = true;
                writer.outputInteger(options, formatStringNextArgument<uint32>(arguments), 16u, '\0', alternateForm ? "0X" : "");
                break;
            case 'b':
                writer.outputInteger(options, formatStringNextArgument<uint32>(arguments), 2u, '\0', alternateForm ? "0b" : "");
                break;
            case 'c':
                writer.outputChar((char)formatStringNextArgument<uint32>(arguments));
                break;
            case 'd':
            case 'i':
                writer.outputSignedInteger(options, formatStringNextArgument<int>(arguments));
                break;
            case 'g':
                options.mode = FormatOutputOptions::Mode_Short;
                writer.outputFloatingPointFixed(options, (float)formatStringNextArgument<double>(arguments));
                break;
            case 'f':
                writer.outputFloatingPointFixed(options, (float)formatStringNextArgument<double>(arguments));
                break;
            case 'l':
                ++pFormat;
                if (*pFormat == 'l')
                {
                    ++pFormat;
                }
                switch (*pFormat)
                {
                case 'd':
                case 'i':
                    writer.outputSignedInteger(options, formatStringNextArgument<sint64>(arguments));
                    break;
                case 'u':
                    writer.outputInteger(options, formatStringNextArgument<uint64>(arguments), 10u, '\0', "");
                    break;
                case 'b':
                    writer.outputInteger(options, formatStringNextArgument<uint64>(arguments), 2u, '\0', "");
                    break;
                case 'x':
                    writer.outputInteger(options, formatStringNextArgument<uint64>(arguments), 16u, '\0', alternateForm ? "0x" : "");
                    break;
                case 'X':
                    options.bigHexDigits = true;
                    writer.outputInteger(options, formatStringNextArgument<uint64>(arguments), 16u, '\0', alternateForm ? "0X" : "");
                    break;
                default:
                    debug::nativePrintArgs("invalid input format\n");
                    continue;
                }
                break;
            case 'n':
            {
                uint32* pCount = formatStringNextArgument<uint32*>(arguments);
                if (pCount != nullptr)
                {
                    *pCount = writer.getCharCount();
                }
                break;
            }
            case 'o':
                writer.outputInteger(options, formatStringNextArgument<uint32>(arguments), 8u, '\0', alternateForm ? "0" : "");
                break;
            case 'p':
                // Magic numbers: a pointer is 10 characters, zero filled ("0x" and 8 digits).
                options.outputWidth = 10;
                options.fillChar = '0';
                writer.outputInteger(options, formatStringNextArgument<uint32>(arguments), 16u, '\0', "0x");
                break;
            case 's':
                writer.outputString(formatStringNextArgument<const char*>(arguments), options.quoteString);
                break;
            case 't':
                // Magic number: 12 fraction bits.
                writer.outputFixedPointInteger(options, formatStringNextArgument<int>(arguments), 12);
                break;
            case 'u':
                writer.outputInteger(options, formatStringNextArgument<uint32>(arguments), 10u, '\0', "");
                break;
            case 'x':
                writer.outputInteger(options, formatStringNextArgument<uint32>(arguments), 16u, '\0', alternateForm ? "0x" : "");
                break;
            case 'y':
                writer.outputFourCC(options, formatStringNextArgument<uint32>(arguments));
                break;
            case 'z':
                ++pFormat;
                switch (*pFormat)
                {
                case 'u':
                    ++pFormat;
                    writer.outputInteger(options, (uint64)formatStringNextArgument<uint32>(arguments), 10u, '\0', "");
                    break;
                case 'x':
                    ++pFormat;
                    writer.outputInteger(options, (uint64)formatStringNextArgument<uint32>(arguments), 16u, '\0', alternateForm ? "0x" : "");
                    break;
                case 'X':
                    ++pFormat;
                    options.bigHexDigits = true;
                    writer.outputInteger(options, (uint64)formatStringNextArgument<uint32>(arguments), 16u, '\0', alternateForm ? "0X" : "");
                    break;
                default:
                    debug::nativePrintArgs("invalid input format\n");
                    break;
                }
                continue;
            default:
                debug::nativePrintArgs("[base] Invalid format string '%s'!\n", pFullFormat);
                writer.outputChar('%');
                continue;
            }
            if (*pFormat != '\0')
            {
                ++pFormat;
            }
        }
        return writer.getCharCount();
    }

    // 0x00C05EC0 (:363)
    // The untruncated length plus one; the text is cut at the buffer's last byte.
    uint32 formatStringVarArgs(char* pTargetBuffer, uint32 targetCapacity, const char* pFormat, char* arguments)
    {
        MemoryWriteStream stream(pTargetBuffer, targetCapacity, "<memory>");
        const uint32 length = formatStringVarArgs(&stream, pFormat, arguments);
        const uint32 terminatorIndex = length < targetCapacity - 1u ? length : targetCapacity - 1u;
        pTargetBuffer[terminatorIndex] = '\0';
        return length + 1u;
    }

    // 0x00C05F50 (:392)
    uint32 formatString(char* pTargetBuffer, uint32 targetCapacity, const char* pFormat, ...)
    {
        va_list arguments;
        va_start(arguments, pFormat);
        const uint32 result = formatStringVarArgs(pTargetBuffer, targetCapacity, pFormat, arguments);
        va_end(arguments);
        return result;
    }
}
