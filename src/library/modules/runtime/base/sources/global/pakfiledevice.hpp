// Original: library/modules/runtime/base/sources/global/pakfiledevice.hpp (private to keen base)
// Implementation: library/modules/runtime/base/sources/global/pakfiledevice.cpp
#pragma once

#include "keen/base/array.hpp"
#include "keen/base/endianness.hpp"
#include "keen/base/filedevice.hpp"
#include "keen/base/tlsfmemoryallocator.hpp"

namespace keen
{
    struct FileSystem;

    // Directory entry of a keen .pak file (sorted by crc for binary search).
    // size 0x10
    struct PakfileEntry
    {
        bool operator<(const PakfileEntry& other) const; // 0x00BF1010

        uint32 crc; // 0x00
        uint32 size; // 0x04
        uint64 position; // 0x08
    };

    // Placement inferred (no line info).
    // size 0x20
    struct PakFileDeviceMountData : public FileDeviceMountData
    {
        PakFileDeviceMountData(); // 0x00BF9D20
        ~PakFileDeviceMountData(); // 0x00BF3DA0

        FileSystem* pFileSystem; // 0x00
        uint32 pakFileHandle; // 0x04
        Array<PakfileEntry> fileEntries; // 0x08
        Endianness endianness; // 0x10
        bool isPakfileCompressed; // 0x14
        uint32 alignment; // 0x18
        uint32 chunkSize; // 0x1C
    };

    // Read-only device over a keen .pak file (optionally chunk-compressed). This is keen's own pak format,
    // not Volition's .vpp_pc packfiles, which go through cfile.
    // size 0x134
    class PakFileDevice : public FileDeviceInterface
    {
    public:
        enum
        {
            InvalidChunkIndex = -1,
        };

        // size 0x3050
        struct PakFileStream
        {
            PakFileStream(); // 0x00BFBF50
            ~PakFileStream(); // 0x00BFBF70

            uint8 compressedChunkData[8192]; // 0x00
            uint8 uncompressedChunkData[4096]; // 0x2000
            uint32 uncompressedChunkDataSize; // 0x3000
            uint32 uncompressedChunkDataPosition; // 0x3004
            Array<uint32> chunkSizes; // 0x3008
            uint32 chunkCount; // 0x3010
            uint32 chunkIndex; // 0x3014
            uint64 firstChunkPosition; // 0x3018
            uint64 nextChunkPosition; // 0x3020
            uint64 uncompressedDataBegin; // 0x3028
            uint64 uncompressedDataPosition; // 0x3030
            bool isFileCompressed; // 0x3038
            PakFileDeviceMountData* pMountData; // 0x303C
            uint32 fileNameCrc; // 0x3040
            uint64 fileSize; // 0x3048
        };

        PakFileDevice(); // 0x00C040B0
        virtual ~PakFileDevice(); // vtbl+0x00 0x00BF9D10
        bool create(MemoryAllocator* pAllocator, uint32 maxStreamCount, uint32 maxFileSize); // 0x00C0D9A0
        void destroy(MemoryAllocator* pAllocator); // 0x00C0C4A0
        IoResult<FileDeviceMountData*> allocateMountData(MemoryAllocator* pAllocator, FileSystem* pFileSystem, const char* pPakFileName); // 0x00C08B60
        virtual IoError freeMountData(MemoryAllocator* pAllocator, FileDeviceMountData* pMountData); // vtbl+0x0C 0x00C04120
        virtual IoResult<uint16> openFile(const FileDeviceMountData* pMountData, const char* pFileName, FileAccessMode accessMode); // vtbl+0x04 0x00C08F00
        virtual IoError closeFile(uint16 file); // vtbl+0x08 0x00C093F0
        virtual IoResult<uint32> read(uint16 file, void* pDest, uint32 size); // vtbl+0x10 0x00BFFDD0
        virtual IoResult<uint32> write(uint16 file, const void* pSrc, uint32 size); // vtbl+0x14 0x00BF9D30
        virtual IoError flushWriteBuffer(uint16 file); // vtbl+0x18 0x00BF1020
        virtual IoResult<uint64> getSize(uint16 file) const; // vtbl+0x1C 0x00C00030
        virtual IoError setPosition(uint16 file, uint64 position); // vtbl+0x20 0x00C00090
        virtual IoResult<uint64> getPosition(uint16 file) const; // vtbl+0x24 0x00C00170
        virtual IoError setFilePermissionByName(const FileDeviceMountData* pMountData, const char* pFileName, bool isWriteable); // vtbl+0x28 0x00BF1030
        virtual IoError getFileStatusByName(FileStatus* pStatus, const FileDeviceMountData* pMountData, const char* pFileName, uint32 valueMask) const; // vtbl+0x2C 0x00C06D30

    private:
        IoResult<uint32> readAndDecompressChunk(PakFileStream* pStream, uint32 chunkIndex); // 0x00BF9D50

        FileStreamAllocator<PakFileStream> m_streams; // 0x04
        TlsfMemoryAllocator m_chunkAllocator; // 0x54
    };
}
