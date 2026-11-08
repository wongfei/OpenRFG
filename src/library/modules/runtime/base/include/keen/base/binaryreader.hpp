// Original: library/modules/runtime/base/include/keen/base/binaryreader.hpp
// Implementation: library/modules/runtime/base/sources/global/binaryreader.cpp
//
// BinaryReader, typed reads from a ReadStream with optional endian swapping (counterpart of BinaryWriter).
#pragma once

#include "keen/base/types.hpp"
#include "keen/base/endianness.hpp"
#include "keen/base/ioresult.hpp"

namespace keen
{
    class ReadStream;

    // size 0x8
    class BinaryReader
    {
        BinaryReader(const BinaryReader&);
    public:
        BinaryReader(ReadStream* pBuffer, Endianness sourceEndianness); // 0x00BFC730 (.h:19)
        BinaryReader(); // 0x00BEE870 (:8)
        ~BinaryReader(); // 0x00BF4C60 (:14)
        void open(ReadStream* pBuffer, Endianness sourceEndianness); // 0x00BF4C70 (:19)
        void close(); // 0x00BEE880 (:30)
        void setDataEndianness(Endianness sourceEndianness); // 0x00BEE890 (:36)
        bool isOpen() const;
        bool needsEndianSwapping() const;
        void readDataBlock(void* pTarget, uint32 size); // 0x00BF4C90 (:42)
        bool hasError(); // 0x00BEE860 (.h:32)
        IoError getError() const;
        bool readBool(); // 0x00BF5290 (:149)
        void readBool(bool* pTarget, uint32 count); // 0x00BFC750 (:47)
        sint8 readSint8(); // 0x00BF52F0 (:156)
        void readSint8(sint8* pTarget, uint32 count); // 0x00BF4CE0 (:59)
        uint8 readUint8(); // 0x00BF5350 (:163)
        void readUint8(uint8* pTarget, uint32 count); // 0x00BF4D30 (:64)
        sint16 readSint16(); // 0x00BF53B0 (:170)
        void readSint16(sint16* pTarget, uint32 count); // 0x00BF4D80 (:69)
        uint16 readUint16(); // 0x00BF5430 (:181)
        void readUint16(uint16* pTarget, uint32 count); // 0x00BF4E10 (:79)
        int readSint32(); // 0x00BF54B0 (:192)
        void readSint32(int* pTarget, uint32 count); // 0x00BF4EA0 (:89)
        uint32 readUint32(); // 0x00BF5540 (:203)
        void readUint32(uint32* pTarget, uint32 count); // 0x00BF4F50 (:99)
        sint64 readSint64(); // 0x00BF55D0 (:214)
        void readSint64(sint64* pTarget, uint32 count); // 0x00BF5000 (:109)
        uint64 readUint64(); // 0x00BF56E0 (:225)
        void readUint64(uint64* pTarget, uint32 count); // 0x00BF5140 (:119)
        float readFloat32(); // 0x00BF57F0 (:249)
        void readFloat32(float* pTarget, uint32 count); // 0x00BFC7E0 (:141)
        void readFixedString(char* pTarget, uint32 size); // 0x00BF5870 (:260)
        uint32 readCompactString(char* pTarget, uint32 capacity); // 0x00BF58E0 (:271)
    private:
        BinaryReader& operator=(const BinaryReader&);

        ReadStream* m_pBuffer; // 0x00
        bool m_needEndianSwapping; // 0x04
    };
}
