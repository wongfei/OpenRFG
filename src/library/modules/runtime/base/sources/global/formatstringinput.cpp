// Original: library/modules/runtime/base/sources/global/formatstringinput.cpp
//
// Partial: the constructor, isNextCharDigit and readUnsignedInt (the parameterized formatString's precision). Not
// ported: the other cursor helpers.

#include "library/modules/runtime/base/sources/global/formatstringinput.hpp"

namespace keen
{
    // 0x00BF00C0 (:10)
    FormatStringInput::FormatStringInput(const char* pInput)
    {
        pInputStart = pInput;
        pCurrentPos = pInput;
    }

    // 0x00BF7F60 (:59)
    bool FormatStringInput::isNextCharDigit() const
    {
        if (*pCurrentPos == '\0')
        {
            return false;
        }
        return (uint8)(*pCurrentPos - '0') <= 9u;
    }

    // 0x00BF7F80 (:68)
    // No overflow check.
    uint32 FormatStringInput::readUnsignedInt()
    {
        uint32 value = 0u;
        while (isNextCharDigit())
        {
            value = value * 10u + (uint32)(*pCurrentPos - '0');
            ++pCurrentPos;
        }
        return value;
    }
}
