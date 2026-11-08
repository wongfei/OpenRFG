// Original: library/modules/runtime/base/include/keen/base/bitstream.hpp
// Implementation: library/modules/runtime/base/sources/global/bitstream.cpp
//
// BitStream, a bit-granular stream that can be opened for reading or writing.
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    // size 0x14
    class BitStream
    {
    public:
        BitStream(const BitStream& other); // 0x00BFC9C0 (:16)
        BitStream(); // 0x00BEEA30 (:10)
        ~BitStream();
        void openWriteable(void* pData, uint32 bitCount, uint32 startBitPosition); // 0x00BF6B20 (:21)
        void openReadable(const void* pData, const BitStream& other); // 0x00BF6BA0 (:52)
        void openReadable(const void* pData, uint32 bitCount, uint32 startBitPosition); // 0x00BF6B60 (:37)
        void close(); // 0x00BF6BE0 (:57)
        bool isOpen(); // 0x00BEED10 (:224)
        void writeUint(uint32 val, uint32 numBits); // 0x00BEEB20 (:141)
        uint32 readUint(uint32 numBits); // 0x00BEEBD0 (:170)
        void writeUint32(uint32 val, uint32 numBits); // 0x00BEEB70 (:152)
        uint32 readUint32(uint32 numBits); // 0x00BEEC20 (:183)
        void writeUint8(uint8 val, uint32 numBits); // 0x00BEEBC0 (:163)
        uint8 readUint8(uint32 numBits); // 0x00BEEC70 (:196)
        void writeBit(uint8 bit); // 0x00BEEAB0 (:105)
        uint8 readBit(); // 0x00BEEAF0 (:122)
        void writeData(const void* pData, uint32 sizeInBytes); // 0x00BF6C60 (:241)
        void* writeData(uint32 sizeInBytes); // 0x00BF6C00 (:229)
        void readData(void* pData, uint32 sizeInBytes); // 0x00BF6CD0 (:247)
        void skipBits(uint32 bitCount); // 0x00BEEC80 (:203)
        uint32 getCurrentBitPosition() const; // 0x00BEED00 (:219)
        uint32 getBitCapacity() const;
        uint32 getRemainingBitCount() const;
    private:
        void flushCachedByte(); // 0x00BEEA40 (:69)
        void loadCachedByte(); // 0x00BEEA80 (:89)

        uint8* m_pFirstByte; // 0x00
        uint32 m_currentBitPosition; // 0x04
        uint32 m_bitCount; // 0x08
        uint32 m_startBitPosition; // 0x0C
        bool m_writeable; // 0x10
        uint8 m_cachedByte; // 0x11
    };

    void appendBitStream(BitStream* pDstStream, BitStream* pSrcStream, uint32 bitCount); // 0x00BEED20 (:304)
}
