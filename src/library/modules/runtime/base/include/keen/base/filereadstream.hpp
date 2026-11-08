// Original: library/modules/runtime/base/include/keen/base/filereadstream.hpp
// Implementation: library/modules/runtime/base/sources/global/filereadstream.cpp
#pragma once

#include "keen/base/readstream.hpp"

namespace keen
{
    struct FileSystem;

    // ReadStream over a keen::file handle.
    // size 0x824
    class FileReadStream : public ReadStream
    {
    public:
        enum
        {
            BufferSize = 2048,
        };

        FileReadStream(FileSystem* pFileSystem, uint32 file); // 0x00C04DF0
        FileReadStream(); // 0x00BFD0A0
        ~FileReadStream(); // 0x00C02E00
        void open(FileSystem* pFileSystem, uint32 file); // 0x00C02E10
        bool open(FileSystem* pFileSystem, const char* pFileName); // 0x00C04E60
        void close(); // 0x00BFD0D0
        bool isOpen() const; // 0x00BEFCC0
        void setFilePosition(uint64 position); // 0x00BFD130
        uint64 getFileSize(); // 0x00BFD1B0
        FileSystem* getFileSystem();
        uint32 getFileHandle();
        const char* getFileName() const; // 0x00BEFCD0

    private:
        static void refillBuffer(ReadStream* pBuffer); // 0x00BFD2C0
        void readBufferFromFile(); // 0x00BFD260

        uint8 m_buffer[2048]; // 0x18
        FileSystem* m_pFileSystem; // 0x818
        uint32 m_fileHandle; // 0x81C
        bool m_isInternalHandle; // 0x820
    };
}
