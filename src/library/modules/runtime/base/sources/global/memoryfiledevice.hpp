// Original: library/modules/runtime/base/sources/global/memoryfiledevice.hpp (private to keen base)
// Implementation: library/modules/runtime/base/sources/global/memoryfiledevice.cpp
#pragma once

#include "keen/base/array.hpp"
#include "keen/base/filedevice.hpp"
#include "keen/base/indexarray.hpp"
#include "keen/base/memory.hpp"

namespace keen
{
    // Placement inferred (no line info).
    // size 0x8
    struct MemoryFileDeviceMountData : public FileDeviceMountData
    {
        MemoryAllocator* pAllocator; // 0x00
        uint32 fileHandle; // 0x04
    };

    // Device whose files live in memory blocks (read-only blobs, or writable files backed by an allocator).
    // size 0x68
    class MemoryFileDevice : public FileDeviceInterface
    {
    public:
        // size 0x1C
        struct FileEntry
        {
            FileEntry(); // 0x00BF7950
            bool isFree() const; // 0x00BEFCE0
            void setFree(); // 0x00BEFCF0
            bool isOpen() const; // 0x00BEFD00

            MemoryFileDeviceMountData* pMountData; // 0x00
            MemoryBlock memoryBlock; // 0x04
            uint32 fileSize; // 0x0C
            uint32 openCount; // 0x10
            uint32 fileNameCrc; // 0x14
            bool isWritten; // 0x18
            bool isWriteable; // 0x19
        };

        // size 0xC
        struct StreamEntry
        {
            FileEntry* pFile; // 0x00
            uint32 position; // 0x04
            FileAccessMode accessMode; // 0x08
        };

        MemoryFileDevice(); // 0x00BFEFB0
        virtual ~MemoryFileDevice(); // vtbl+0x00 0x00BF8E30
        bool create(MemoryAllocator* pAllocator, uint32 maxFileCount, uint32 maxStreamCount); // 0x00C0D480
        void destroy(MemoryAllocator* pAllocator); // 0x00C03F00
        virtual IoResult<uint16> openFile(const FileDeviceMountData* pMountData, const char* pFileName, FileAccessMode accessMode); // vtbl+0x04 0x00C066F0
        virtual IoError closeFile(uint16 file); // vtbl+0x08 0x00C069D0
        IoResult<FileDeviceMountData*> allocateDeviceMountData(MemoryAllocator* pAllocator, MemoryAllocator* pDeviceAllocator); // 0x00BF8ED0
        IoResult<FileDeviceMountData*> allocateFileMountData(MemoryAllocator* pAllocator, MemoryBlock memoryBlock, const char* pFilename); // 0x00BF8F20
        virtual IoError freeMountData(MemoryAllocator* pAllocator, FileDeviceMountData* pMountData); // vtbl+0x0C 0x00BFF2A0
        virtual IoResult<uint32> read(uint16 file, void* pTarget, uint32 size); // vtbl+0x10 0x00BFEFF0
        virtual IoResult<uint32> write(uint16 file, const void* pSource, uint32 size); // vtbl+0x14 0x00BFF080
        virtual IoError flushWriteBuffer(uint16 file); // vtbl+0x18 0x00BF0EE0
        virtual IoResult<uint64> getSize(uint16 file) const; // vtbl+0x1C 0x00BFF1C0
        virtual IoError setPosition(uint16 file, uint64 position); // vtbl+0x20 0x00BFF210
        virtual IoResult<uint64> getPosition(uint16 file) const; // vtbl+0x24 0x00BFF250
        virtual IoError setFilePermissionByName(const FileDeviceMountData* pMountData, const char* pFileName, bool isWriteable); // vtbl+0x28 0x00BFF300
        virtual IoError getFileStatusByName(FileStatus* pStatus, const FileDeviceMountData* pMountData, const char* pFileName, uint32 valueMask) const; // vtbl+0x2C 0x00BFF380

    private:
        uint32 addFile(MemoryFileDeviceMountData* pMountData, MemoryBlock memoryBlock, const char* pFilename); // 0x00BF8E40
        void removeFile(uint32 handle); // 0x00BF8EA0
        void removeFiles(const MemoryFileDeviceMountData* pMountData); // 0x00BF8FA0
        const FileEntry* findFileEntryByNameCrc(const FileDeviceMountData* pMountData, uint32 fileNameCrc) const; // 0x00BF9030
        FileEntry* findFileEntryByNameCrc(const FileDeviceMountData* pMountData, uint32 fileNameCrc); // 0x00BF9000

        FileStreamAllocator<StreamEntry> m_streams; // 0x04
        Array<FileEntry> m_files; // 0x54
        IndexArray m_fileIndices; // 0x5C
    };
}
