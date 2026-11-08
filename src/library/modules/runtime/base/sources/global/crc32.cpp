// Original: library/modules/runtime/base/sources/global/crc32.cpp
//
// Standard CRC-32 (reflected polynomial 0xEDB88320, initial value and final xor 0xFFFFFFFF). The add* functions
// continue a previous value. Characters are sign-extended before the table lookup, which does not change the result.
// The Lwr variants fold 'A'..'Z' to lower case.

#include "keen/base/crc32.hpp"

namespace
{
    struct Crc32Table
    {
        uint32 values[256];

        constexpr Crc32Table() : values()
        {
            for (uint32 i = 0; i < 256; ++i)
            {
                uint32 value = i;
                for (int bit = 0; bit < 8; ++bit)
                {
                    value = (value & 1) ? (value >> 1) ^ 0xEDB88320u : value >> 1;
                }
                values[i] = value;
            }
        }
    };

    constexpr Crc32Table s_crc32Table;
}

// 0x0133C6A8 (the same values as the table literal in the binary)
static const uint32* const crc32_table = s_crc32Table.values;

namespace keen
{
    static inline uint32 updateCrc32(uint32 crc, char ch)
    {
        return (crc >> 8) ^ crc32_table[((uint32)(sint32)ch ^ crc) & 0xFF];
    }

    static inline char toLowerCrcChar(char ch)
    {
        return (uint8)(ch - 'A') < 26 ? (char)(ch + ' ') : ch;
    }

    // 0x00BEF3D0 (:9)
    uint32 addCrc32Value(uint32 hashValue, const char* pString)
    {
        uint32 crc = ~hashValue;
        for (; *pString != '\0'; ++pString)
        {
            crc = updateCrc32(crc, *pString);
        }
        return ~crc;
    }

    // 0x00BEF400 (:19)
    uint32 addCrc32Value(uint32 hashValue, const void* pVoidBuffer, uint32 len)
    {
        const char* pBuffer = (const char*)pVoidBuffer;
        uint32 crc = ~hashValue;
        for (; len != 0; --len)
        {
            crc = updateCrc32(crc, *pBuffer++);
        }
        return ~crc;
    }

    // 0x00BEF430 (:30)
    uint32 addCrc32LwrValue(uint32 hashValue, const char* pString)
    {
        uint32 crc = ~hashValue;
        for (; *pString != '\0'; ++pString)
        {
            crc = updateCrc32(crc, toLowerCrcChar(*pString));
        }
        return ~crc;
    }

    // 0x00BEF470 (:45)
    uint32 addCrc32LwrValue(uint32 hashValue, const void* pVoidBuffer, uint32 len)
    {
        const char* pBuffer = (const char*)pVoidBuffer;
        uint32 crc = ~hashValue;
        for (; len != 0; --len)
        {
            crc = updateCrc32(crc, toLowerCrcChar(*pBuffer++));
        }
        return ~crc;
    }

    // 0x00BEF4C0 (:61)
    uint32 getCrc32Value(const char* pString)
    {
        return addCrc32Value(0, pString);
    }

    // 0x00BEF4F0 (:66)
    uint32 getCrc32Value(const void* pBuffer, uint32 len)
    {
        return addCrc32Value(0, pBuffer, len);
    }

    // 0x00BEF520 (:71)
    uint32 getCrc32LwrValue(const char* pString)
    {
        return addCrc32LwrValue(0, pString);
    }

    // 0x00BEF530 (:76)
    uint32 getCrc32LwrValue(const void* pBuffer, uint32 len)
    {
        return addCrc32LwrValue(0, pBuffer, len);
    }
}
