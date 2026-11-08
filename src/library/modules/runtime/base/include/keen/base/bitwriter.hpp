// Original: library/modules/runtime/base/include/keen/base/bitwriter.hpp
// Implementation: library/modules/runtime/base/sources/global/bitwriter.cpp
//
// BitWriter, writes bit fields into a BitWriteBuffer.
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    struct BitReadBuffer;
    class BitReader;

    // size 0xC
    struct BitWriteBuffer
    {
        uint8* pData; // 0x00
        uint32 bitCount; // 0x04
        uint32 bitPosition; // 0x08
    };

    // size 0x18
    class BitWriter
    {
    public:
        enum
        {
            BitBufferRegisterByteSize = 1,
            BitBufferRegisterBitSize = 8,
            BitBufferRegisterBitMask = 255,
        };

        typedef uint8 RegisterType;

        BitWriter(void* pData, uint32 bitCount, uint32 bitPosition); // 0x00C811A0 (bitwriter.inl:43)
        BitWriter(BitWriteBuffer*);
        ~BitWriter(); // 0x00C83500 (bitwriter.inl:56)
        void close(); // 0x00C811D0 (bitwriter.inl:61)
        bool flushSection();
        void clearSection();
        void writeBit(uint32 bit); // 0x00C83520 (bitwriter.inl:97)
        void writeUint8(uint8 value, uint32 bitCount); // 0x00C835C0 (bitwriter.inl:102)
        void writeUint16(uint16, uint32);
        void writeUint32(uint32 value, uint32 bitCount); // 0x00C83680 (bitwriter.inl:132)
        void writeUint64(uint64, uint32);
        void writeData(const void*, uint32);
        void writeBitReader(BitReader*, uint32);
        void writeBitReadBuffer(BitReadBuffer*, uint32);
        void skipBits(uint32 bitCount); // 0x00C811F0 (bitwriter.inl:180)
        uint32 getSectionBitPosition() const;
        uint32 getBitPosition() const; // 0x00C81180 (.h:47)
        uint32 getBitCapacity() const;
        bool hasCapacityExceeded() const;
        uint32 getRemainingBitCount() const; // 0x00C81190 (.h:50)
    private:
        void writeRegister(uint8 value, uint32 bitCount); // 0x00C81220 (bitwriter.inl:256)

        BitWriteBuffer* m_pBitWriteBuffer; // 0x00
        BitWriteBuffer m_localBuffer; // 0x04
        uint32 m_initialBitPosition; // 0x10
        bool m_hasCapacityExeeded; // 0x14
    };

    void createBitWriteBuffer(BitWriteBuffer* pBuffer, void* pData, uint32 sizeInBits, uint32 bitPosition); // 0x00C812E0 (bitwriter.inl:315)
}
