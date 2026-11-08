// Original: library/modules/runtime/base/include/keen/base/stringscanner.hpp
// Implementation: library/modules/runtime/base/sources/global/stringscanner.cpp
//
// Parsing numbers and booleans from strings (StringReadResult).
#pragma once

#include "keen/base/types.hpp"
#include <stdint.h>

namespace keen
{
    enum StringReadResult
    {
        StringReadResult_Ok = 0,
        StringReadResult_Overflow = 1,
        StringReadResult_NoNumber = 2,
    };

    void skipWhiteSpaces(const char** pStringPtr); // 0x00BF27D0 (:8)
    StringReadResult readSignLessUint32FromDecimalString(uint32* pResult, const char** pStringPtr); // 0x00BF27F0 (:16)
    StringReadResult readSint32FromDecimalString(int* pResult, const char** pStringPtr); // 0x00BF2860 (:58)
    StringReadResult readSignLessUint64FromDecimalString(uint64_t* pResult, const char** pStringPtr); // 0x00BF2910 (:103)
    StringReadResult readSint64FromDecimalString(int64_t* pResult, const char** pStringPtr); // 0x00BF29E0 (:150)
    StringReadResult readSignLessUint32FromHexadecimalString(uint32* pResult, const char** pStringPtr); // 0x00BF2AB0 (:193)
    StringReadResult readSignLessUint64FromHexadecimalString(uint64_t* pResult, const char** pStringPtr); // 0x00BF2B50 (:244)
    StringReadResult readUint32FromDecimalString(uint32* pResult, const char** pStringPtr); // 0x00BF2C20 (:295)
    StringReadResult readUint32FromHexadecimalString(uint32* pResult, const char** pStringPtr); // 0x00BF2C70 (:314)
    StringReadResult readUint64FromHexadecimalString(uint64_t* pResult, const char** pStringPtr); // 0x00BF2CE0 (:340)
    StringReadResult readApproximateFloatFromDecimalString(float* pResult, const char** pStringPtr); // 0x00BFAFA0 (:366)
    bool readBoolFromString(bool* pResult, const char** ppString); // 0x00C09D50 (:424)
    bool readDelimiter(const char** ppString, char delimiter); // 0x00BF2D50 (:449)
    const char* skipTo(const char* pString, char delimiter, char oppositeDelimiter); // 0x00BF2D80 (:465)
}
