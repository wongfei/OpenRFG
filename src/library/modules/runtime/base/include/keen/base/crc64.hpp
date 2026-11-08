// Original: library/modules/runtime/base/include/keen/base/crc64.hpp
// Implementation: library/modules/runtime/base/sources/global/crc64.cpp
//
// CRC64 checksums, case-sensitive and lower-cased (Lwr) variants.
#pragma once

#include "keen/base/types.hpp"
#include <stdint.h>

namespace keen
{
    uint64_t addCrc64Value(uint64_t hashValue, const char* pString); // 0x00BEF550 (:8)
    uint64_t addCrc64Value(uint64_t hashValue, const void* pVoidBuffer, uint32 size); // 0x00BEF5A0 (:18)
    uint64_t addCrc64LwrValue(uint64_t hashValue, const char* pString); // 0x00BEF5F0 (:30)
    uint64_t getCrc64Value(const char* pString); // 0x00BEF650 (:46)
    uint64_t getCrc64Value(const void* pBuffer, uint32 size); // 0x00BEF670 (:51)
    uint64_t getCrc64LwrValue(const char* pString); // 0x00BEF690 (:56)
}
