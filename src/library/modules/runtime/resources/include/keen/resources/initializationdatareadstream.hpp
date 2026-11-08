// Original: library/modules/runtime/resources/include/keen/resources/initializationdatareadstream.hpp
// Implementation: library/modules/runtime/resources/sources/initializationdatareadstream.cpp
//
// InitializationDataReadStream, a ReadStream over a resource file section used to initialize a resource.
#pragma once

#include "keen/base/types.hpp"
#include "keen/base/readstream.hpp"

namespace keen
{
    struct FileSystem;

    // size 0x38
    class InitializationDataReadStream : public ReadStream
    {
    public:
        InitializationDataReadStream(); // 0x00CDEAB0 (:8)
        void open(uint8* pBuffer, uint32 bufferSize, FileSystem* pFileSystem, uint32 fileHandle, uint64 fileOffset, uint32 dataSize); // 0x00CDF2A0 (:14)
        void close(); // 0x00CDC480 (:31)
    private:
        static void refillBuffer(ReadStream* pBuffer); // 0x00CDEAD0 (:41)
        void readBufferFromFile(); // 0x00CDD7D0 (:47)

        uint8* m_pBuffer; // 0x18
        uint32 m_bufferSize; // 0x1C
        FileSystem* m_pFileSystem; // 0x20
        uint32 m_fileHandle; // 0x24
        uint64 m_fileStartOffset; // 0x28
        uint32 m_bytesRead; // 0x30
        uint32 m_dataSize; // 0x34
    };
}
