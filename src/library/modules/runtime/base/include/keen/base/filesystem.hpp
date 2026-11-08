// Original: library/modules/runtime/base/include/keen/base/filesystem.hpp
// Implementation: library/modules/runtime/base/sources/global/filesystem.cpp
#pragma once

#include "keen/base/fileaccessmode.hpp"
#include "keen/base/filesystemtypes.hpp"
#include "keen/base/ioresult.hpp"

namespace keen
{
    class MemoryAllocator;
    class FileDeviceInterface;
    struct FileDeviceMountData;
    struct FileSystem; // defined in filesystem.cpp

    // Virtual file system: a tree of mount points, each with a list of devices.
    // Mount ids and file handles are opaque uint32 values (file handle = device index << 16 | device file handle).
    namespace file
    {
        FileSystem* createFileSystem(MemoryAllocator* pAllocator, const FileSystemCreationParameters& parameters); // 0x00C0DB30
        void destroyFileSystem(FileSystem* pFileSystem); // 0x00C0CA10

        bool mountDefaultNativePath(FileSystem* pFileSystem, bool enableMount); // 0x00C0B2B0
        void unmountDefaultNativePath(FileSystem* pFileSystem); // 0x00C04EF0
        IoResult<uint32> mountAliasPath(FileSystem* pFileSystem, const char* pTargetPath, const char* pSourcePath); // 0x00C08180
        IoResult<uint32> mountNativeHostPath(FileSystem* pFileSystem, const char* pTargetPath, const char* pHostPath, bool createIfNotExist); // 0x00C0CB80
        IoResult<uint32> mountDevice(FileSystem* pFileSystem, const char* pTargetPath, FileDeviceInterface* pDevice, FileDeviceMountData* pMountData); // 0x00C052A0
        IoResult<uint32> mountNativePath(FileSystem* pFileSystem, const char* pTargetPath, const char* pNativePath, bool createIfNotExist); // 0x00C0B340
        IoResult<uint32> mountReadOnlyMemoryFile(FileSystem* pFileSystem, const char* pTargetPath, const void* pData, uint32 dataSize); // 0x00C08230
        IoResult<uint32> mountMemory(FileSystem* pFileSystem, const char* pTargetPath, MemoryAllocator* pAllocator); // 0x00C052F0
        IoResult<uint32> mountPakFile(FileSystem* pFileSystem, const char* pTargetPath, const char* pPakFilePath); // 0x00C0B3D0
        IoError unmount(FileSystem* pFileSystem, uint32 mountId); // 0x00C02FB0

        bool getNativeFilePath(char* pBuffer, uint32 bufferCapacity, FileSystem* pFileSystem, const char* pFileName); // 0x00C02FF0

        IoResult<uint32> openFile(FileSystem* pFileSystem, const char* pFileName, FileAccessMode accessMode); // 0x00C030F0
        IoError closeFile(FileSystem* pFileSystem, uint32 file); // 0x00BF7B00
        IoResult<uint32> read(FileSystem* pFileSystem, uint32 file, void* pTarget, uint32 size); // 0x00BF7B40
        void setIoControl(FileSystem* pFileSystem, bool useIo); // 0x00BEFE10
        IoResult<uint32> write(FileSystem* pFileSystem, uint32 file, const void* pSource, uint32 size); // 0x00BF7BD0
        IoError flushWriteBuffer(FileSystem* pFileSystem, uint32 file); // 0x00BF7C20
        IoResult<uint64> getFileSize(const FileSystem* pFileSystem, uint32 file); // 0x00BF7C60
        IoError setFilePosition(FileSystem* pFileSystem, uint32 file, uint64 position); // 0x00BF7CD0
        IoResult<uint64> getFilePosition(const FileSystem* pFileSystem, uint32 file); // 0x00BF7D10
        IoError moveFilePosition(FileSystem* pFileSystem, uint32 file, sint64 offset); // 0x00BF7D80
        bool doesFileExists(FileSystem* pFileSystem, const char* pFileName); // 0x00C05370
        IoError getFileStatusByName(FileStatus* pStatus, FileSystem* pFileSystem, const char* pFileName, uint32 valueMask); // 0x00C03230
        IoError setFilePermissionByName(FileSystem* pFileSystem, const char* pFileName, bool isWriteable); // 0x00C03350
        bool setIgnoreFirstChild(FileSystem* pFileSystem, bool value); // 0x00BEFE30

        const char* getFileName(FileSystem* pFileSystem, uint32 file); // 0x00BED750
    }
}
