// Original: library/modules/runtime/base/include/keen/base/bitreader.hpp
// Implementation: library/modules/runtime/base/sources/global/bitreader.cpp
//
// BitReader, reads bit fields from a BitReadBuffer (network packets, compressed state).
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    // size 0xC
    struct BitReadBuffer
    {
        const uint8* pData; // 0x00
        uint32 bitCount; // 0x04
        uint32 bitPosition; // 0x08
    };

    // size 0x18
    class BitReader
    {
    public:
        enum
        {
            BitBufferRegisterByteSize = 1,
            BitBufferRegisterBitSize = 8,
            BitBufferRegisterBitMask = 255,
        };

        typedef uint8 RegisterType;

        BitReader(const void* pData, uint32 bitCount, uint32 bitPosition); // 0x00C81060 (bitreader.inl:42)
        BitReader(BitReadBuffer* pBitReadBuffer); // 0x00C81030 (bitreader.inl:30)
        ~BitReader(); // 0x00C83270 (bitreader.inl:55)
        void close(); // 0x00C81090 (bitreader.inl:60)
        bool flushSection();
        void clearSection();
        uint32 readBit(); // 0x00C83290 (bitreader.inl:98)
        uint8 readUint8(uint32 bitCount); // 0x00C83300 (bitreader.inl:103)
        uint16 readUint16(uint32 bitCount); // 0x00C83370 (bitreader.inl:109)
        uint32 readUint32(uint32 bitCount); // 0x00C83440 (bitreader.inl:124)
        uint64 readUint64(uint32);
        void readData(void*, uint32);
        void skipBits(uint32 bitCount); // 0x00C810B0 (bitreader.inl:154)
        uint32 getSectionBitPosition() const;
        uint32 getBitPosition() const; // 0x00C81010 (.h:49)
        uint32 getBitCapacity() const;
        bool hasCapacityExceeded() const;
        uint32 getRemainingBitCount() const; // 0x00C81020 (.h:52)
    private:
        uint8 readRegister(uint32 bitCount); // 0x00C810E0 (bitreader.inl:187)

        BitReadBuffer* m_pBitReadBuffer; // 0x00
        BitReadBuffer m_localBuffer; // 0x04
        uint32 m_initialBitPosition; // 0x10
        bool m_hasCapacityExeeded; // 0x14
    };

    void createBitReadBuffer(BitReadBuffer* pBuffer, const void* pData, uint32 sizeInBits, uint32 bitPosition); // 0x00C81160 (bitreader.inl:234)
}
