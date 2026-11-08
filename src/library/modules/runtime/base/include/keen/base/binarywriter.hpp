// Original: library/modules/runtime/base/include/keen/base/binarywriter.hpp
// Implementation: library/modules/runtime/base/sources/global/binarywriter.cpp
//
// BinaryWriter, typed writes into a WriteStream with optional endian swapping.
#pragma once

#include "keen/base/types.hpp"
#include "keen/base/endianness.hpp"
#include "keen/base/ioresult.hpp"

namespace keen
{
    class WriteStream;

    // size 0x8
    class BinaryWriter
    {
        BinaryWriter(const BinaryWriter&);
    public:
        BinaryWriter(WriteStream*, Endianness);
        BinaryWriter(); // 0x00BEE8A0 (:8)
        ~BinaryWriter(); // 0x00BF5990 (:14)
        void open(WriteStream* pBuffer, Endianness targetEndianness); // 0x00BF59A0 (:19)
        void close(); // 0x00BEE8B0 (:31)
        bool isOpen() const;
        bool needsEndianSwapping() const;
        void writeDataBlock(const void* pSource, uint32 size); // 0x00BF59C0 (:37)
        bool hasError(); // 0x00CE3A60 (.h:28)
        IoError getError() const; // 0x00CE3A70 (.h:29)
        void writeBool(const bool* pValues, uint32 count); // 0x00BFC8C0 (:48)
        void writeBool(bool value); // 0x00BFC8A0 (:42)
        void writeSint8(const sint8* pValues, uint32 count); // 0x00BF5A60 (:62)
        void writeSint8(sint8 value); // 0x00BF5A10 (:57)
        void writeUint8(const uint8* pValues, uint32 count); // 0x00BF5B00 (:72)
        void writeUint8(uint8 value); // 0x00BF5AB0 (:67)
        void writeSint16(const sint16* pValues, uint32 count); // 0x00BF5BC0 (:86)
        void writeSint16(sint16 value); // 0x00BF5B50 (:77)
        void writeUint16(const uint16* pValues, uint32 count); // 0x00BF5D30 (:112)
        void writeUint16(uint16 value); // 0x00BF5CC0 (:103)
        void writeSint32(const int* pValues, uint32 count); // 0x00BF5EB0 (:138)
        void writeSint32(int value); // 0x00BF5E30 (:129)
        void writeUint32(const uint32* pValues, uint32 count); // 0x00BF6050 (:164)
        void writeUint32(uint32 value); // 0x00BF5FD0 (:155)
        void writeSint64(const sint64* pValues, uint32 count); // 0x00BF6270 (:190)
        void writeSint64(sint64 value); // 0x00BF6170 (:181)
        void writeUint64(const uint64* pValues, uint32 count); // 0x00BF6500 (:216)
        void writeUint64(uint64 value); // 0x00BF6400 (:207)
        void writeFloat32(const float* pValues, uint32 count); // 0x00BF6710 (:270)
        void writeFloat32(float value); // 0x00BF6690 (:261)
        void writeFixedString(const char* pSource, uint32 capacity); // 0x00BF6840 (:287)
        void writeCompactString(const char* pSource); // 0x00BF68E0 (:302)
        uint32 writeFormattedCompactString(const char*, ...); // 0x00C07860 (:309)
    private:
        BinaryWriter& operator=(const BinaryWriter&);

        WriteStream* m_pBuffer; // 0x00
        bool m_needEndianSwapping; // 0x04
    };
}
