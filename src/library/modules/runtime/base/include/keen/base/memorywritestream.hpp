// Original: library/modules/runtime/base/include/keen/base/memorywritestream.hpp
// Implementation: library/modules/runtime/base/sources/global/memorywritestream.cpp
//
// MemoryWriteStream, a WriteStream into a memory block.
#pragma once

#include "keen/base/types.hpp"
#include "keen/base/writestream.hpp"

namespace keen
{
    class MemoryAllocator;

    // size 0x1C
    class MemoryWriteStream : public WriteStream
    {
    public:
        MemoryWriteStream(MemoryAllocator*, const char*);
        MemoryWriteStream(void* pMemory, uint32 size, const char* pDebugName); // 0x00C054A0 (.h:14)
        MemoryWriteStream(); // 0x00BFF470 (:7)
        ~MemoryWriteStream(); // 0x00BFF490 (:12)
        void open(MemoryAllocator* pAllocator, const char* pDebugName); // 0x00C03FB0 (:26)
        void open(void* pMemory, uint32 size, const char* pDebugName); // 0x00C03F80 (:17)
        void close(); // 0x00BF93A0 (:35)
        bool setPosition(uint32 position); // 0x00BF93E0 (:47)
        void alignPosition(uint32 alignment); // 0x00BF9400 (:52)
        const uint8* getMemoryBufferStart() const;
        uint32 getMemoryBufferSize() const; // 0x00BF00A0 (.h:27)
        uint32 getMemoryBufferCapacity() const;
    private:
        static void flushMemoryStream(WriteStream* pStream); // 0x00BFF4D0 (:76)

        MemoryAllocator* m_pAllocator; // 0x18
    };
}
