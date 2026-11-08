// Original: library/modules/runtime/base/include/keen/base/networkwriter.hpp
// Implementation: library/modules/runtime/base/sources/global/networkwriter.cpp
//
// NetworkWriter, writes network-endian values into a buffer.
#pragma once

#include "keen/base/types.hpp"
#include "keen/base/ioresult.hpp"

namespace keen
{
    class WriteStream;

    // size 0x4
    class NetworkWriter
    {
        NetworkWriter(const NetworkWriter&);
    public:
        NetworkWriter(WriteStream*);
        void open(WriteStream*);
        void close();
        WriteStream* getStream();
        bool isOpen() const;
        void writeDataBlock(const void*, uint32);
        bool hasError();
        IoError getError() const;
        void writeBool(const bool*, uint32);
        void writeBool(bool);
        void writeSint8(const sint8*, uint32);
        void writeSint8(sint8);
        void writeUint8(const uint8*, uint32);
        void writeUint8(uint8 value); // 0x00BF72F0 (.h:37)
        void writeSint16(const sint16*, uint32);
        void writeSint16(sint16);
        void writeUint16(const uint16*, uint32);
        void writeUint16(uint16);
        void writeSint32(const int*, uint32);
        void writeSint32(int);
        void writeUint32(const uint32*, uint32);
        void writeUint32(uint32 value); // 0x00BF7340 (.h:41)
        void writeSint64(const sint64*, uint32);
        void writeSint64(sint64);
        void writeUint64(const uint64*, uint32);
        void writeUint64(uint64);
        void writeFloat32(const float*, uint32);
        void writeFloat32(float);
        void writeFixedString(const char* pSource, uint32 capacity); // 0x00BF9B00 (:8)
        void writeCompactString(const char* pSource); // 0x00BF9BF0 (:23)
        uint32 writeFormattedCompactString(const char*, ...); // 0x00C06CF0 (:30)
    private:
        NetworkWriter& operator=(const NetworkWriter&);

        WriteStream* m_pStream; // 0x00
    };
}
