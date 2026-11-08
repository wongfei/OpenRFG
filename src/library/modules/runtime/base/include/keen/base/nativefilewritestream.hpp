// Original: library/modules/runtime/base/include/keen/base/nativefilewritestream.hpp
// Implementation: library/modules/runtime/base/sources/global/nativefilewritestream.cpp
//
// NativeFileWriteStream, a WriteStream into a native (OS) file.
#pragma once

#include "keen/base/types.hpp"
#include "keen/base/writestream.hpp"

namespace keen
{
    // size 0x820
    class NativeFileWriteStream : public WriteStream
    {
    public:
        enum
        {
            BufferSize = 2048,
        };

        NativeFileWriteStream(); // 0x00BFFCD0 (:8)
        ~NativeFileWriteStream(); // 0x00C03FE0 (:14)
        bool open(const char* pFileName); // 0x00C0C400 (:19)
        void openFromHandle(void* fileHandle); // 0x00C04030 (:50)
        void close(); // 0x00BFFCF0 (:37)
        bool isOpen() const;
        void flushWriteBuffer(); // 0x00BF9910 (:59)
    private:
        static void flushBuffer(WriteStream* pStream); // 0x00BFFDC0 (:84)
        void flushToFile(); // 0x00BFFD40 (:65)

        void* m_file; // 0x18
        bool m_ownsFile; // 0x1C
        uint8 m_buffer[2048]; // 0x1D
    };
}
