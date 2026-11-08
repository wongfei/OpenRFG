// Original: library/modules/runtime/base/sources/global/floattostring.cpp
//
// Exact float to decimal conversion: the integer part as a 128-bit integer (divided by 10 for each digit), the fraction
// as a 160-bit fixed point number (multiplied by 10 for each digit), then rounding half up at the target precision.
// The output is the digits only, without sign or dot: FloatOutputInfo gives the digit count, the dot position and the
// sign. The original keeps the big integers in BigIntUint32<N> (an anonymous namespace); here they are word arrays,
// most significant word first, as in the binary.

#include "library/modules/runtime/base/sources/global/floattostring.hpp"
#include "keen/base/string.hpp"
#include "keen/base/stringbuilder.hpp"

#include <string.h>

namespace keen
{
    // 0x00BFD690 (:133)
    bool printFloatToBuffer(FloatOutputInfo* pOutputInfo, char* pBuffer, uint32 capacity, float value, uint32 targetPrecision)
    {
        StringBuilder builder;
        builder.setBuffer(pBuffer, capacity);

        uint32 bits;
        memcpy(&bits, &value, sizeof(bits));
        const bool isNegative = (bits >> 31) != 0u;
        const uint32 exponentBits = (bits >> 23) & 0xFFu;
        pOutputInfo->digitCount = 0u;
        pOutputInfo->dotIndex = 0u;
        pOutputInfo->isSpecial = false;
        pOutputInfo->isNegative = isNegative;
        if (exponentBits == 0xFFu)
        {
            pOutputInfo->isSpecial = true;
            const char* pSpecial;
            if ((bits & 0x7FFFFFu) == 0u)
            {
                pSpecial = isNegative ? "-inf" : "+inf";
            }
            else
            {
                pSpecial = "nan";
            }
            builder.appendString(pSpecial);
            return !builder.isOverflown();
        }

        // The value is mantissa * 2^(exponent - 23).
        int exponent = (int)exponentBits - 127;
        uint32 mantissa = (bits & 0x7FFFFFu) + 0x800000u;
        if (exponentBits == 0u)
        {
            exponent = -126;
            mantissa = bits & 0x7FFFFFu;
        }

        // digits[0] is a leading '0' that takes a rounding carry; the digits follow from index 1. The integer digits are
        // produced least significant first at the end of the buffer and moved to the front afterwards.
        char digits[128];
        uint32 integerPart[4] = { 0u, 0u, 0u, 0u };
        int shift = exponent + 9;
        if (shift > 0)
        {
            const int wordIndex = -(int)((uint32)shift >> 5);
            const uint64 shifted = (uint64)mantissa << (shift & 31);
            if ((uint32)(wordIndex + 3) < 4u)
            {
                integerPart[wordIndex + 3] |= (uint32)(shifted >> 32);
            }
            if ((uint32)(wordIndex + 4) < 4u)
            {
                integerPart[wordIndex + 4] |= (uint32)shifted;
            }
        }
        digits[0] = '0';
        uint32 integerDigitCount = 0u;
        for (;;)
        {
            if (integerDigitCount + 1u > 0x7Fu)
            {
                builder.appendString("<overflow>");
                return false;
            }
            uint64 remainder = 0u;
            for (uint32 i = 0u; i < 4u; ++i)
            {
                const uint64 dividend = (remainder << 32) | integerPart[i];
                integerPart[i] = (uint32)(dividend / 10u);
                remainder = dividend % 10u;
            }
            digits[0x7F - integerDigitCount] = (char)('0' + remainder);
            ++integerDigitCount;
            if ((integerPart[0] | integerPart[1] | integerPart[2] | integerPart[3]) == 0u)
            {
                break;
            }
        }
        for (uint32 i = 1u; i <= integerDigitCount; ++i)
        {
            digits[i] = digits[i + 0x7F - integerDigitCount];
        }
        pOutputInfo->dotIndex = integerDigitCount;

        // The fraction is F / 2^160; the integer bits fall off the top word.
        uint32 fraction[5] = { 0u, 0u, 0u, 0u, 0u };
        shift = exponent + 0xA9;
        if (shift > 0)
        {
            const int wordIndex = -(int)((uint32)shift >> 5);
            const uint64 shifted = (uint64)mantissa << (shift & 31);
            if ((uint32)(wordIndex + 4) < 5u)
            {
                fraction[wordIndex + 4] |= (uint32)(shifted >> 32);
            }
            if ((uint32)(wordIndex + 5) < 5u)
            {
                fraction[wordIndex + 5] |= (uint32)shifted;
            }
        }
        uint32 digitCount = integerDigitCount;
        while ((fraction[0] | fraction[1] | fraction[2] | fraction[3] | fraction[4]) != 0u)
        {
            if (digitCount > 0x7Eu)
            {
                if (capacity != 0u)
                {
                    copyString(pBuffer, capacity, "<overflow>");
                }
                return false;
            }
            uint32 carry = 0u;
            for (int i = 4; i >= 0; --i)
            {
                const uint64 product = (uint64)fraction[i] * 10u + carry;
                fraction[i] = (uint32)product;
                carry = (uint32)(product >> 32);
            }
            digits[digitCount + 1u] = (char)('0' + carry);
            ++digitCount;
        }

        uint32 precision = digitCount - integerDigitCount;
        if (precision > targetPrecision)
        {
            int lastIndex = (int)(integerDigitCount + targetPrecision);
            // The original scans the digits after a '5' for a non-zero one but rounds up either way (half up).
            const bool roundUp = digits[lastIndex + 1] >= '5';
            precision = targetPrecision;
            if (roundUp)
            {
                for (; digits[lastIndex] > '8'; --lastIndex)
                {
                    digits[lastIndex] = '0';
                }
                ++digits[lastIndex];
            }
        }
        // A carry into the leading '0' makes it the first digit.
        if (digits[0] == '1')
        {
            pOutputInfo->dotIndex = integerDigitCount + 1u;
        }
        const uint32 outputCount = pOutputInfo->dotIndex + precision;
        const uint32 firstIndex = digits[0] != '1' ? 1u : 0u;
        for (uint32 i = 0u; i < outputCount; ++i)
        {
            builder.appendCharacter(digits[i + firstIndex]);
        }
        pOutputInfo->digitCount = outputCount;
        return !builder.isOverflown();
    }
}
