// Original: library/modules/runtime/base/sources/global/stringscanner.cpp
//
// Number parsing. Each reader skips leading white space (tab to carriage return and space), leaves the string pointer
// after the number on success and untouched on failure.
//
// The decimal readers GameFramework and the ini reader use, the 32-bit hexadecimal readers (vint) and, since
// stub_impl.md S2, the rest (the 64-bit readers, readBoolFromString, readDelimiter, skipTo).
//
// Notes (decompiled behaviour, kept as is):
// - The signed readers skip white space again after the sign ("- 5" reads as -5).
// - readApproximateFloatFromDecimalString accumulates in float with double intermediates (the original's x87 code) and
//   accepts white space between the integer part and the '.'; it has no exponent and never overflows.

#include "keen/base/stringscanner.hpp"
#include "keen/base/string.hpp"
#include "keen/base/trace.hpp"

namespace keen
{
    static inline bool isWhiteSpace(char c)
    {
        return (c >= '\t' && c <= '\r') || c == ' ';
    }

    static inline bool isDigit(char c)
    {
        return (uint8)(c - '0') <= 9u;
    }

    // 0x00BF27D0 (:8)
    void skipWhiteSpaces(const char** pStringPtr)
    {
        while (isWhiteSpace(**pStringPtr))
        {
            ++*pStringPtr;
        }
    }

    // 0x00BF27F0 (:16)
    StringReadResult readSignLessUint32FromDecimalString(uint32* pResult, const char** pStringPtr)
    {
        const char* pString = *pStringPtr;
        while (isWhiteSpace(*pString))
        {
            ++pString;
        }
        if (!isDigit(*pString))
        {
            return StringReadResult_NoNumber;
        }
        uint32 value = 0u;
        for (;;)
        {
            const char c = *pString;
            if (!isDigit(c))
            {
                *pStringPtr = pString;
                *pResult = value;
                return StringReadResult_Ok;
            }
            ++pString;
            // Magic number: 0x1999999A * 10 no longer fits in 32 bits.
            if (value >= 0x1999999Au)
            {
                return StringReadResult_Overflow;
            }
            const uint32 newValue = (uint32)(c - '0') + value * 10u;
            if (newValue < value)
            {
                return StringReadResult_Overflow;
            }
            value = newValue;
        }
    }

    // 0x00BF2860 (:58)
    StringReadResult readSint32FromDecimalString(int* pResult, const char** pStringPtr)
    {
        const char* pString = *pStringPtr;
        while (isWhiteSpace(*pString))
        {
            ++pString;
        }
        bool isNegative = false;
        if (*pString == '-')
        {
            isNegative = true;
            ++pString;
        }
        else if (*pString == '+')
        {
            ++pString;
        }
        uint32 value = 0u;
        const StringReadResult result = readSignLessUint32FromDecimalString(&value, &pString);
        if (result != StringReadResult_Ok)
        {
            return result;
        }
        if (isNegative)
        {
            if (value <= 0x80000000u)
            {
                *pResult = (int)(0u - value);
                *pStringPtr = pString;
                return StringReadResult_Ok;
            }
        }
        else if (value < 0x80000000u)
        {
            *pResult = (int)value;
            *pStringPtr = pString;
            return StringReadResult_Ok;
        }
        return StringReadResult_Overflow;
    }

    // 0x00BF2910 (:103)
    // A trailing 'u' (an unsigned suffix) is consumed.
    StringReadResult readSignLessUint64FromDecimalString(uint64_t* pResult, const char** pStringPtr)
    {
        const char* pString = *pStringPtr;
        while (isWhiteSpace(*pString))
        {
            ++pString;
        }
        if (!isDigit(*pString))
        {
            return StringReadResult_NoNumber;
        }
        uint64_t value = 0u;
        for (;;)
        {
            const char c = *pString;
            if (!isDigit(c))
            {
                if (*pString == 'u')
                {
                    ++pString;
                }
                *pStringPtr = pString;
                *pResult = value;
                return StringReadResult_Ok;
            }
            ++pString;
            if (value > 0x1999999999999999ull)
            {
                return StringReadResult_Overflow;
            }
            const uint64_t newValue = value * 10u + (uint64_t)(c - '0');
            if (newValue < value)
            {
                return StringReadResult_Overflow;
            }
            value = newValue;
        }
    }

    static inline bool isHexDigit(char c)
    {
        return isDigit(c) || (c >= 'a' && c <= 'f') || (uint8)(c - 'A') < 6u;
    }

    // 0x00BF29E0 (:150)
    StringReadResult readSint64FromDecimalString(int64_t* pResult, const char** pStringPtr)
    {
        const char* pString = *pStringPtr;
        while (isWhiteSpace(*pString))
        {
            ++pString;
        }
        bool isNegative = false;
        if (*pString == '-')
        {
            isNegative = true;
            ++pString;
        }
        else if (*pString == '+')
        {
            ++pString;
        }
        uint64_t value = 0u;
        const StringReadResult result = readSignLessUint64FromDecimalString(&value, &pString);
        if (result != StringReadResult_Ok)
        {
            return result;
        }
        if (isNegative)
        {
            if (value <= 0x8000000000000000ull)
            {
                *pResult = (int64_t)(0u - value);
                *pStringPtr = pString;
                return StringReadResult_Ok;
            }
        }
        else if (value < 0x8000000000000000ull)
        {
            *pResult = (int64_t)value;
            *pStringPtr = pString;
            return StringReadResult_Ok;
        }
        return StringReadResult_Overflow;
    }
    // 0x00BF2AB0 (:193)
    // At most 8 digits; a ninth is an overflow.
    StringReadResult readSignLessUint32FromHexadecimalString(uint32* pResult, const char** pStringPtr)
    {
        const char* pString = *pStringPtr;
        while (isWhiteSpace(*pString))
        {
            ++pString;
        }
        if (!isHexDigit(*pString))
        {
            return StringReadResult_NoNumber;
        }
        uint32 value = 0u;
        uint32 digits = 0u;
        for (; isHexDigit(*pString); ++pString)
        {
            if (digits > 7u)
            {
                return StringReadResult_Overflow;
            }
            ++digits;
            const char c = *pString;
            // '0', else 'A' - 10 or 'a' - 10
            const char base = c > '9' ? (c < '[' ? 0x37 : 0x57) : '0';
            value = value * 16u + (uint32)(c - base);
        }
        *pStringPtr = pString;
        *pResult = value;
        return StringReadResult_Ok;
    }

    // 0x00BF2B50 (:244)
    // At most 16 digits; a seventeenth is an overflow.
    StringReadResult readSignLessUint64FromHexadecimalString(uint64_t* pResult, const char** pStringPtr)
    {
        const char* pString = *pStringPtr;
        while (isWhiteSpace(*pString))
        {
            ++pString;
        }
        if (!isHexDigit(*pString))
        {
            return StringReadResult_NoNumber;
        }
        uint64_t value = 0u;
        uint32 digits = 0u;
        for (; isHexDigit(*pString); ++pString)
        {
            if (digits > 15u)
            {
                return StringReadResult_Overflow;
            }
            ++digits;
            const char c = *pString;
            // '0', else 'A' - 10 or 'a' - 10
            const char base = c > '9' ? (c < '[' ? 0x37 : 0x57) : '0';
            value = value * 16u + (uint64_t)(int64_t)(int)(c - base);
        }
        *pStringPtr = pString;
        *pResult = value;
        return StringReadResult_Ok;
    }

    // 0x00BF2C70 (:314)
    // An optional '+' or 'x', then an optional "0x".
    StringReadResult readUint32FromHexadecimalString(uint32* pResult, const char** pStringPtr)
    {
        const char* pString = *pStringPtr;
        while (isWhiteSpace(*pString))
        {
            ++pString;
        }
        if (*pString == '+' || *pString == 'x')
        {
            ++pString;
        }
        if (pString[0] == '0' && pString[1] == 'x')
        {
            pString += 2;
        }
        const StringReadResult result = readSignLessUint32FromHexadecimalString(pResult, &pString);
        if (result == StringReadResult_Ok)
        {
            *pStringPtr = pString;
        }
        return result;
    }

    // 0x00BF2C20 (:295)
    StringReadResult readUint32FromDecimalString(uint32* pResult, const char** pStringPtr)
    {
        const char* pString = *pStringPtr;
        while (isWhiteSpace(*pString))
        {
            ++pString;
        }
        if (*pString == '+')
        {
            ++pString;
        }
        const StringReadResult result = readSignLessUint32FromDecimalString(pResult, &pString);
        if (result == StringReadResult_Ok)
        {
            *pStringPtr = pString;
        }
        return result;
    }

    // 0x00BF2CE0 (:340)
    // An optional '+' or 'x', then an optional "0x".
    StringReadResult readUint64FromHexadecimalString(uint64_t* pResult, const char** pStringPtr)
    {
        const char* pString = *pStringPtr;
        while (isWhiteSpace(*pString))
        {
            ++pString;
        }
        if (*pString == '+' || *pString == 'x')
        {
            ++pString;
        }
        if (pString[0] == '0' && pString[1] == 'x')
        {
            pString += 2;
        }
        const StringReadResult result = readSignLessUint64FromHexadecimalString(pResult, &pString);
        if (result == StringReadResult_Ok)
        {
            *pStringPtr = pString;
        }
        return result;
    }

    // 0x00BFAFA0 (:366)
    StringReadResult readApproximateFloatFromDecimalString(float* pResult, const char** pStringPtr)
    {
        const char* pString = *pStringPtr;
        while (isWhiteSpace(*pString))
        {
            ++pString;
        }
        bool isNegative = false;
        if (*pString == '-')
        {
            isNegative = true;
            ++pString;
        }
        else if (*pString == '+')
        {
            ++pString;
        }
        if (!isDigit(*pString) && *pString != '.')
        {
            return StringReadResult_NoNumber;
        }
        float value = 0.0f;
        while (isDigit(*pString))
        {
            const uint32 digit = (uint32)(*pString - '0');
            ++pString;
            value = (float)((double)value * 10.0 + (double)digit);
        }
        while (isWhiteSpace(*pString))
        {
            ++pString;
        }
        if (*pString == '.')
        {
            float scale = 0.1f;
            for (;;)
            {
                ++pString;
                if (!isDigit(*pString))
                {
                    break;
                }
                const uint32 digit = (uint32)(*pString - '0');
                value = (float)((double)digit * (double)scale + (double)value);
                // The binary multiplies by 0.1f widened to double (0x3FB99999A0000000), not by 0.1.
                scale = (float)((double)scale * (double)0.1f);
            }
        }
        *pStringPtr = pString;
        *pResult = isNegative ? -value : value;
        return StringReadResult_Ok;
    }

    // 0x00C09D50 (:424): "true" or "false" in any case (prefixes: "trueness" reads as true). The leading white space is
    // skipped in the caller's pointer even when nothing is read.
    bool readBoolFromString(bool* pResult, const char** ppString)
    {
        while (isWhiteSpace(**ppString))
        {
            ++*ppString;
        }
        const char* pString = *ppString;
        if (compareStringNoCase(pString, "true", getStringLength("true")) == 0)
        {
            *ppString = pString + getStringLength("true");
            *pResult = true;
            return true;
        }
        if (compareStringNoCase(pString, "false", getStringLength("false")) == 0)
        {
            *ppString = pString + getStringLength("false");
            *pResult = false;
            return true;
        }
        debug::nativePrintArgs("Invalid bool value '%s'!\n", pString);
        return false;
    }

    // 0x00BF2D50 (:449): the leading white space is skipped in the caller's pointer even without the delimiter
    bool readDelimiter(const char** ppString, char delimiter)
    {
        while (isWhiteSpace(**ppString))
        {
            ++*ppString;
        }
        if (**ppString != delimiter)
        {
            return false;
        }
        ++*ppString;
        return true;
    }

    // 0x00BF2D80 (:465): the matching delimiter (nested opposite/delimiter pairs are skipped), or the terminator
    const char* skipTo(const char* pString, char delimiter, char oppositeDelimiter)
    {
        uint32 depth = 0;
        for (; *pString != '\0'; ++pString)
        {
            if (*pString == delimiter)
            {
                if (depth == 0)
                {
                    return pString;
                }
                --depth;
            }
            else if (*pString == oppositeDelimiter)
            {
                ++depth;
            }
        }
        return pString;
    }
}