// Original: library/modules/runtime/base/include/keen/base/networkreader.hpp
// Implementation: library/modules/runtime/base/sources/global/networkreader.cpp
//
// NetworkReader, reads network-endian values from a buffer.
#pragma once

#include "keen/base/types.hpp"
#include "keen/base/ioresult.hpp"

namespace keen
{
    class ReadStream;

    // size 0x4
    class NetworkReader
    {
        NetworkReader(const NetworkReader&);
    public:
        NetworkReader(ReadStream*);
        void open(ReadStream*);
        void close();
        bool isOpen() const;
        void readDataBlock(void*, uint32);
        bool hasError();
        IoError getError() const;
        bool readBool();
        void readBool(bool*, uint32);
        sint8 readSint8();
        void readSint8(sint8*, uint32);
        uint8 readUint8(); // 0x00BF7210 (.h:44)
        void readUint8(uint8*, uint32);
        sint16 readSint16();
        void readSint16(sint16*, uint32);
        uint16 readUint16();
        void readUint16(uint16*, uint32);
        int readSint32();
        void readSint32(int*, uint32);
        uint32 readUint32(); // 0x00BF7270 (.h:48)
        void readUint32(uint32*, uint32);
        sint64 readSint64();
        void readSint64(sint64*, uint32);
        uint64 readUint64();
        void readUint64(uint64*, uint32);
        float readFloat32();
        void readFloat32(float*, uint32);
        void readFixedString(char* pTarget, uint32 size); // 0x00BF9940 (:7)
        uint32 readCompactString(char* pTarget, uint32 capacity); // 0x00BF99B0 (:18)
    private:
        NetworkReader& operator=(const NetworkReader&);

        ReadStream* m_pStream; // 0x00
    };
}
