// Original: library/modules/runtime/base/include/keen/base/crc32.hpp
// Implementation: library/modules/runtime/base/sources/global/crc32.cpp
//
// CRC32 checksums, case-sensitive and lower-cased (Lwr) variants.
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    uint32 addCrc32Value(uint32 hashValue, const char* pString); // 0x00BEF3D0 (:9)
    uint32 addCrc32Value(uint32 hashValue, const void* pVoidBuffer, uint32 len); // 0x00BEF400 (:19)
    uint32 addCrc32LwrValue(uint32 hashValue, const char* pString); // 0x00BEF430 (:30)
    uint32 addCrc32LwrValue(uint32 hashValue, const void* pVoidBuffer, uint32 len); // 0x00BEF470 (:45)
    uint32 getCrc32Value(const char* pString); // 0x00BEF4C0 (:61)
    uint32 getCrc32Value(const void* pBuffer, uint32 len); // 0x00BEF4F0 (:66)
    uint32 getCrc32LwrValue(const char* pString); // 0x00BEF520 (:71)
    uint32 getCrc32LwrValue(const void* pBuffer, uint32 len); // 0x00BEF530 (:76)
}
