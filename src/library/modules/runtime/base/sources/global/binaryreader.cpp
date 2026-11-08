// Original: library/modules/runtime/base/sources/global/binaryreader.cpp
//
// BinaryReader, typed reads from a ReadStream, byte-swapped when the source endianness is not little endian.
//
// Partial: the uint32/uint64 readers (what PakFileDevice uses). The stream copy loop is inlined in every reader in the
// original; here they share readDataBlock.

#include "keen/base/binaryreader.hpp"
#include "keen/base/readstream.hpp"

namespace keen
{
    static inline uint32 swapUint32(uint32 value)
    {
        return (value >> 24) | ((value >> 8) & 0xff00u) | ((value << 8) & 0xff0000u) | (value << 24);
    }

    static inline uint64 swapUint64(uint64 value)
    {
        return ((uint64)swapUint32((uint32)value) << 32) | swapUint32((uint32)(value >> 32));
    }

    // 0x00BFC730 (.h:19)
    BinaryReader::BinaryReader(ReadStream* pBuffer, Endianness sourceEndianness)
    {
        m_pBuffer = pBuffer;
        m_needEndianSwapping = sourceEndianness != Endianness_Little;
    }

    // 0x00BEE870 (:8)
    BinaryReader::BinaryReader()
    {
        m_pBuffer = nullptr;
        m_needEndianSwapping = false;
    }

    // 0x00BF4C60 (:14)
    BinaryReader::~BinaryReader()
    {
        m_pBuffer = nullptr;
        m_needEndianSwapping = false;
    }

    // 0x00BF4C70 (:19)
    void BinaryReader::open(ReadStream* pBuffer, Endianness sourceEndianness)
    {
        m_pBuffer = pBuffer;
        m_needEndianSwapping = sourceEndianness != Endianness_Little;
    }

    // 0x00BEE880 (:30)
    void BinaryReader::close()
    {
        m_pBuffer = nullptr;
        m_needEndianSwapping = false;
    }

    // 0x00BEE890 (:36)
    void BinaryReader::setDataEndianness(Endianness sourceEndianness)
    {
        m_needEndianSwapping = sourceEndianness != Endianness_Little;
    }

    // 0x00BF4C90 (:42)
    void BinaryReader::readDataBlock(void* pTarget, uint32 size)
    {
        m_pBuffer->readDataBlock(pTarget, size);
    }

    // 0x00BEE860 (.h:32)
    bool BinaryReader::hasError()
    {
        return m_pBuffer->hasError();
    }

    // 0x00BF4F50 (:99)
    void BinaryReader::readUint32(uint32* pTarget, uint32 count)
    {
        m_pBuffer->readDataBlock(pTarget, count * 4);
        if (m_needEndianSwapping)
        {
            for (uint32 i = 0; i < count; ++i)
            {
                pTarget[i] = swapUint32(pTarget[i]);
            }
        }
    }

    // 0x00BF5140 (:119)
    void BinaryReader::readUint64(uint64* pTarget, uint32 count)
    {
        m_pBuffer->readDataBlock(pTarget, count * 8);
        if (m_needEndianSwapping)
        {
            for (uint32 i = 0; i < count; ++i)
            {
                pTarget[i] = swapUint64(pTarget[i]);
            }
        }
    }

    // 0x00BF5540 (:203)
    uint32 BinaryReader::readUint32()
    {
        uint32 value;
        m_pBuffer->readDataBlock(&value, sizeof(value));
        return m_needEndianSwapping ? swapUint32(value) : value;
    }

    // 0x00BF56E0 (:225)
    uint64 BinaryReader::readUint64()
    {
        uint64 value;
        m_pBuffer->readDataBlock(&value, sizeof(value));
        return m_needEndianSwapping ? swapUint64(value) : value;
    }
}
