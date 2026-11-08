// Original: library/modules/runtime/base/include/keen/base/nativefilereadstream.hpp
// Implementation: library/modules/runtime/base/sources/global/nativefilereadstream.cpp
#pragma once

#include "keen/base/readstream.hpp"

namespace keen
{
    // ReadStream over a native OS file handle, bypassing the FileSystem.
    // size 0x828
    class NativeFileReadStream : public ReadStream
    {
    public:
        enum
        {
            DecompressionBufferSize = 2048,
        };

        NativeFileReadStream(); // 0x00BFFC20
        ~NativeFileReadStream(); // 0x00BF9810
        bool open(const char* pFileName); // 0x00C0C370
        void openFromHandle(void* fileHandle); // 0x00BFFC60
        void close(); // 0x00BF0FD0
        bool isOpen() const;

    private:
        static void refillBuffer(ReadStream* pBuffer); // 0x00BF9900
        void readBufferFromFile(); // 0x00BF9850

        uint8 m_buffer[2048]; // 0x18
        bool m_ownsFile; // 0x818
        void* m_file; // 0x81C
        uint64 m_position; // 0x820
    };
}
