// Original: library/modules/runtime/base/sources/global/formatparameterizedstring.cpp
//
// formatString with numbered parameters, for localized texts: "$(N)" inserts parameter N, "$(N=flags width.precision)"
// formats it (flags ' ', '#', '+', ',', '-', '0', 'x'/'X' for hexadecimal with a "0x" prefix); any other '$' is copied.
// The result is truncated to the buffer; the return value is the untruncated length plus one.
//
// Partial: formatString(..., const StringParameter*, uint32). Not ported: StringParameterBuilder and the builder
// overload of formatString.
//
// Notes (decompiled behaviour, kept as is):
// - 'X' gives lower case digits like 'x' (both cases share one branch that clears bigHexDigits).
// - The hexadecimal prefix's two characters are added to the width.
// - The basis only applies to unsigned parameters; signed ones are always decimal.
// - A field without ')' prints "(field syntax error)" and the rest of the field is copied as text.

#include "keen/base/string.hpp"
#include "keen/base/memorywritestream.hpp"
#include "library/modules/runtime/base/sources/global/formatstringinput.hpp"
#include "library/modules/runtime/base/sources/global/formattedstringwriter.hpp"

namespace keen
{
    // 0x00C054D0 (:68)
    uint32 formatString(char* pTargetBuffer, uint32 targetCapacity, const char* pFormat, const StringParameter* pParameters, uint32 parameterCount)
    {
        MemoryWriteStream stream(pTargetBuffer, targetCapacity, "<memory>");
        FormattedStringWriter writer(&stream);
        FormatOutputOptions options;

        while (*pFormat != '\0')
        {
            if (*pFormat != '$')
            {
                writer.outputChar(*pFormat);
                ++pFormat;
                continue;
            }
            options.reset();
            if (pFormat[1] != '(')
            {
                writer.outputChar('$');
                ++pFormat;
                continue;
            }

            uint32 parameterIndex = 0u;
            const char* p = pFormat + 2;
            for (; *p != '\0' && (uint8)(*p - '0') <= 9u; ++p)
            {
                parameterIndex = parameterIndex * 10u + (uint32)(*p - '0');
            }
            int prefixLength = 0;
            uint32 basis = 10u;
            const char* pPrefix = "";
            pFormat = p;
            if (*p == '=')
            {
                bool parsingFlags = true;
                while (parsingFlags)
                {
                    ++p;
                    switch (*p)
                    {
                    case ' ':
                        options.insertBlankSpace = true;
                        break;
                    case '#':
                        options.alternateForm = true;
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
                    case 'X':
                    case 'x':
                        basis = 16u;
                        pPrefix = "0x";
                        prefixLength = 2;
                        options.bigHexDigits = false;
                        break;
                    default:
                        if (*p != '\0' && (uint8)(*p - '0') <= 9u)
                        {
                            options.outputWidth = 0;
                            for (; *p != '\0' && (uint8)(*p - '0') <= 9u; ++p)
                            {
                                options.outputWidth = options.outputWidth * 10 + (*p - '0');
                            }
                            options.outputWidth += prefixLength;
                        }
                        pFormat = p;
                        if (*p == '.')
                        {
                            pFormat = p + 1;
                            FormatStringInput input(pFormat);
                            if (input.isNextCharDigit())
                            {
                                options.outputPrecision = (int)input.readUnsignedInt();
                                // The original continues at the input's position, after the digits.
                                while ((uint8)(*pFormat - '0') <= 9u)
                                {
                                    ++pFormat;
                                }
                            }
                        }
                        parsingFlags = false;
                        break;
                    }
                }
            }

            if (*pFormat != ')')
            {
                writer.outputString("(field syntax error)", false);
                continue;
            }
            ++pFormat;
            if (parameterIndex >= parameterCount)
            {
                writer.outputString("(parameter not found)", false);
                continue;
            }
            const StringParameter& parameter = pParameters[parameterIndex];
            switch (parameter.type)
            {
            case StringParameterType_IntegerUnsigned:
                writer.outputInteger(options, parameter.integerValue, basis, '\0', pPrefix);
                break;
            case StringParameterType_IntegerSigned:
                writer.outputSignedInteger(options, (int)parameter.integerValue);
                break;
            case StringParameterType_Float:
                writer.outputFloatingPointFixed(options, parameter.floatValue);
                break;
            case StringParameterType_String:
                writer.outputString(parameter.pStringValue, false);
                break;
            default:
                writer.outputString("(Invalid parameter type)", false);
                break;
            }
        }

        const uint32 charCount = writer.getCharCount();
        const uint32 terminatorIndex = charCount < targetCapacity - 1u ? charCount : targetCapacity - 1u;
        pTargetBuffer[terminatorIndex] = '\0';
        return charCount + 1u;
    }
}
