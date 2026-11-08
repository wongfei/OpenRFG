// Original: library/modules/runtime/base/include/keen/base/endianness.hpp
// Implementation: library/modules/runtime/base/sources/global/endianness.cpp
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    enum Endianness
    {
        Endianness_Little = 0,
        Endianness_Big = 1,
        Endianness_Count = 2,
    };

    Endianness getHostEndianness(); // 0x00BEE1E0

    uint16 changeEndianness(uint16 value); // 0x009418B0
    uint32 changeEndianness(uint32 value); // 0x00BEE1F0
    uint64 changeEndianness(uint64 value); // 0x00BEE220
    void changeEndianness(uint16* pData); // 0x009418D0
    void changeEndianness(uint32* pData); // 0x00BEE2C0
    void changeEndianness(uint64* pData); // 0x00BEE2F0
    void changeEndianness(sint16* pData); // 0x00BEE3B0
    void changeEndianness(int* pData); // 0x00BEE3C0
    void changeEndianness(float* pData); // 0x00BEE3F0
    void changeEndianness(sint64* pData); // 0x00BEE420
    void changeEndianness(uint16* pData, uint32 count); // 0x009418E0
    void changeEndianness(uint32* pData, uint32 count); // 0x00BEE4E0
    void changeEndianness(uint64* pData, uint32 count); // 0x00BEE530
    void changeEndianness(sint16* pData, uint32 count); // 0x00BEE610
    void changeEndianness(int* pData, uint32 count); // 0x00BEE640
    void changeEndianness(sint64* pData, uint32 count); // 0x00BEE690
    void changeEndianness(void* pData, uint32 elementSize); // 0x00BF4B60
    void changeEndianness(void* pData, uint32 elementSize, uint32 count); // 0x00BF4B90

    void networkToHost(uint16* pData, uint32 count); // 0x00941910
    void networkToHost(uint32* pData, uint32 count); // 0x00BEE770
    void hostToNetwork(uint16* pData, uint32 count); // 0x00941940
    void hostToNetwork(uint32* pData, uint32 count); // 0x00BEE7C0
}
