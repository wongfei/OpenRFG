// Original: library/modules/runtime/base/sources/global/aliaspathfiledevice.hpp (private to keen base)
// Implementation: library/modules/runtime/base/sources/global/aliaspathfiledevice.cpp
#pragma once

#include "keen/base/filedevice.hpp"

namespace keen
{
    struct FileSystem;

    // Placement inferred (no line info).
    // size 0x108
    struct AliasPathFileDeviceMountData : public FileDeviceMountData
    {
        FileSystem* pFileSystem; // 0x00
        char targetPath[260]; // 0x04
    };

    // Redirects a mount point to another path in the same FileSystem (file::mountAliasPath).
    // size 0x54
    class AliasPathFileDevice : public FileDeviceInterface
    {
    public:
        // size 0x8
        struct StreamEntry
        {
            AliasPathFileDeviceMountData* pMountData; // 0x00
            uint32 fileHandle; // 0x04
        };

        AliasPathFileDevice(); // 0x00BFC450
        virtual ~AliasPathFileDevice(); // vtbl+0x00 0x00BF4AD0
        bool create(MemoryAllocator* pAllocator, uint32 maxStreamCount); // 0x00C0D3A0
        void destroy(MemoryAllocator* pAllocator); // 0x00C01190
        IoResult<FileDeviceMountData*> allocateMountData(MemoryAllocator* pAllocator, FileSystem* pFileSystem, const char* pTargetPath); // 0x00BF4AE0
        virtual IoError freeMountData(MemoryAllocator* pAllocator, FileDeviceMountData* pMountData); // vtbl+0x0C 0x00BF4B40
        virtual IoResult<uint16> openFile(const FileDeviceMountData* pMountData, const char* pFileName, FileAccessMode accessMode); // vtbl+0x04 0x00C075E0
        virtual IoError closeFile(uint16 file); // vtbl+0x08 0x00C04710
        virtual IoResult<uint32> read(uint16 file, void* pDest, uint32 size); // vtbl+0x10 0x00BFC490
        virtual IoResult<uint32> write(uint16 file, const void* pSrc, uint32 size); // vtbl+0x14 0x00BFC4D0
        virtual IoError flushWriteBuffer(uint16 file); // vtbl+0x18 0x00BFC540
        virtual IoResult<uint64> getSize(uint16 file) const; // vtbl+0x1C 0x00BFC5A0
        virtual IoError setPosition(uint16 file, uint64 position); // vtbl+0x20 0x00BFC630
        virtual IoResult<uint64> getPosition(uint16 file) const; // vtbl+0x24 0x00BFC680
        virtual IoError setFilePermissionByName(const FileDeviceMountData* pMountData, const char* pFileName, bool isWriteable); // vtbl+0x28 0x00C07770
        virtual IoError getFileStatusByName(FileStatus* pStatus, const FileDeviceMountData* pMountData, const char* pFileName, uint32 valueMask) const; // vtbl+0x2C 0x00C077E0

    private:
        FileStreamAllocator<StreamEntry> m_streams; // 0x04
    };
}
