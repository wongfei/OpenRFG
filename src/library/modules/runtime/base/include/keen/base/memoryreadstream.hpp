// Original: library/modules/runtime/base/include/keen/base/memoryreadstream.hpp
// Implementation: library/modules/runtime/base/sources/global/memoryreadstream.cpp
#pragma once

#include "keen/base/readstream.hpp"

namespace keen
{
    // ReadStream over a memory range.
    // size 0x18
    class MemoryReadStream : public ReadStream
    {
    public:
        MemoryReadStream(const void* pMemory, uint32 size, const char* pDebugName);
        MemoryReadStream();
        void open(const void* pMemory, uint32 size, const char* pDebugName); // 0x00BFF440
        void close(); // 0x00BF9300
        bool setPosition(uint32 offset); // 0x00BF9320
        bool alignPosition(uint32 alignment); // 0x00BF9340

    private:
        static void refillMemoryStream(ReadStream* pBuffer); // 0x00BF9370
    };
}
