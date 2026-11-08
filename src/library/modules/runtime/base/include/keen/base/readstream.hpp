// Original: library/modules/runtime/base/include/keen/base/readstream.hpp
// Implementation: library/modules/runtime/base/sources/global/readstream.cpp
#pragma once

#include "keen/base/ioresult.hpp"

namespace keen
{
    // Buffered input stream: derived classes supply a buffer and a refill callback (no virtuals).
    // size 0x18
    class ReadStream
    {
    public:
        typedef void RefillFunction(ReadStream*);

        ReadStream(); // 0x00BFC710
        uint32 getBufferSize() const; // 0x00BEE810
        uint32 getBufferPosition() const;
        uint32 getRemainingBufferSize() const;
        const uint8* getBufferStart() const;
        bool isDrained() const;
        uint32 copyDataFromBuffer(void*, uint32);
        void readDataBlock(void* pTarget, uint32 size); // 0x00BF4C10
        bool setBufferPosition(uint32 position); // 0x00BF14B0
        void refill() const; // 0x00BEE820
        void setError(IoError error) const; // 0x00BF14D0
        IoError getError() const;
        bool hasError() const; // 0x00BEE830

    protected:
        void initialize(const uint8* pBuffer, uint32 bufferSize, RefillFunction* pRefillFunction, const char* pDebugName); // 0x00BF4BE0
        void setBuffer(const uint8* pBuffer, uint32 bufferSize); // 0x00BEE840

    private:
        static void refillZeros(ReadStream* pBuffer); // 0x00BF1490

        const uint8* m_pStart; // 0x00
        uint32 m_size; // 0x04
        uint32 m_position; // 0x08
        IoError m_error; // 0x0C
        RefillFunction* m_pRefillFunction; // 0x10
        const char* m_pName; // 0x14
    };
}
