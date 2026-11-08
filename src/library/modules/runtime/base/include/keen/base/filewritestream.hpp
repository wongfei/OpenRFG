// Original: library/modules/runtime/base/include/keen/base/filewritestream.hpp
// Implementation: library/modules/runtime/base/sources/global/filewritestream.cpp
//
// FileWriteStream, a WriteStream that flushes into a File.
#pragma once

#include "keen/base/types.hpp"
#include "keen/base/writestream.hpp"

namespace keen
{
    struct FileSystem;

    // size 0x824
    class FileWriteStream : public WriteStream
    {
    public:
        enum
        {
            BufferSize = 2048,
        };

        FileWriteStream(); // 0x00BFD500 (:7)
        ~FileWriteStream(); // 0x00C053C0 (:13)
        bool open(FileSystem* pFileSystem, const char* pFileName, bool append); // 0x00C053D0 (:18)
        bool open(FileSystem* pFileSystem, uint32 file); // 0x00C05450 (:39)
        void close(); // 0x00C03460 (:54)
        bool isOpen() const;
        bool writeBlockToFile(const void* pSource, uint32 size); // 0x00BFD530 (:71)
        FileSystem* getFileSystem();
        uint32 getFileHandle();
    private:
        static void flushBuffer(WriteStream* pStream); // 0x00BFD680 (:122)
        void flushToFile(); // 0x00BFD5F0 (:103)

        uint8 m_buffer[2048]; // 0x18
        FileSystem* m_pFileSystem; // 0x818
        uint32 m_fileHandle; // 0x81C
        bool m_isInternalHandle; // 0x820
    };
}
