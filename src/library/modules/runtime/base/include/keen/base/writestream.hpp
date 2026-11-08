// Original: library/modules/runtime/base/include/keen/base/writestream.hpp
// Implementation: library/modules/runtime/base/sources/global/writestream.cpp
//
// WriteStream, a buffered output stream: writes go into a buffer and a flush function (set by the concrete stream,
// for example a file or memory write stream) empties it.
#pragma once

#include "keen/base/types.hpp"
#include "keen/base/ioresult.hpp"

namespace keen
{
    // size 0x18
    class WriteStream
    {
    public:
        typedef void FlushFunction(WriteStream*);

        WriteStream(); // 0x00BFC340 (.h:17)
        void writeDataBlock(const void* pSource, uint32 size); // 0x00BF4300 (.h:75)
        void writeByte(uint8);
        void setError(IoError error); // 0x00BFB3A0 (:13)
        IoError getError() const; // 0x00CE3A50 (.h:27)
        bool hasError() const; // 0x00BED640 (.h:28)
        void flush(); // 0x00BF3040 (:57)
    protected:
        bool isBufferFull() const;
        uint32 getBufferCapacity() const; // 0x00BED650 (.h:35)
        uint32 getBufferSize() const; // 0x00BED660 (.h:36)
        uint32 getRemainingBufferCapacity() const;
        void initialize(uint8* pBuffer, uint32 bufferSize, void (*pFlushFunction)(WriteStream*), const char* pDebugName); // 0x00BF42D0 (.h:40)
        void setBuffer(uint8* pBuffer, uint32 bufferSize); // 0x00BED670 (.h:48)
        bool setBufferSize(uint32 size); // 0x00BF3020 (:41)
        const uint8* getBufferStart() const;
        uint8* getBufferStart(); // 0x00BED690 (.h:57)
    private:
        static void flushToEmptyBuffer(WriteStream* pStream); // 0x00BF3000 (:7)

        uint8* m_pStart; // 0x00
        uint32 m_capacity; // 0x04
        uint32 m_size; // 0x08
        IoError m_error; // 0x0C
        void (*m_pFlushFunction)(WriteStream*); // 0x10
        const char* m_pName; // 0x14
    };
}
