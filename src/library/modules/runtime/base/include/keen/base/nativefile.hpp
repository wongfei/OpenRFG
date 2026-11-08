// Original: library/modules/runtime/base/include/keen/base/nativefile.hpp
// Implementation: library/modules/runtime/base/sources/win32/nativefile_win32.cpp
#pragma once

#include "keen/base/fileaccessmode.hpp"
#include "keen/base/filesystemtypes.hpp"
#include "keen/base/ioresult.hpp"

namespace keen
{
    class FilePath;

    enum KnownWindowsFolderId
    {
        KnownWindowsFolderId_Videos = 0,
        KnownWindowsFolderId_Pictures = 1,
        KnownWindowsFolderId_RoamingAppData = 2,
        KnownWindowsFolderId_MyDocuments = 3,
        KnownWindowsFolderId_Count = 4,
    };

    // Thin wrappers over the OS file API; native handles are void*.
    IoError getIoErrorFromLastError(); // 0x00BED860
    IoResult<void*> openFile(const char* pFilename, FileAccessMode accessMode); // 0x00C0A840
    IoError closeFile(void* file); // 0x00BED980
    IoResult<uint32> readFile(void* file, uint64 position, void* pDest, uint32 size); // 0x00BF4370
    IoResult<uint32> writeFile(void* file, uint64 position, const void* pSrc, uint32 size); // 0x00BF43F0
    IoResult<uint32> appendFile(void* file, const void* pSrc, uint32 size); // 0x00BF4450
    IoResult<uint64> getFileSize(void* file); // 0x00BF4490
    IoError flushFileWriteBuffer(void* file); // 0x00BED9A0
    IoError setFilePermissionByName(const char* pFileName, bool isWriteable); // 0x00BF44F0
    bool getCurrentExePath(FilePath* pTarget); // 0x00C0A9D0
    IoError getNativeFileStatusByName(FileStatus* pStatus, const char* pFileName, uint32 valueMask); // 0x00BF45B0
    bool getKnownWindowsFolder(char* pTarget, uint32 capacity, KnownWindowsFolderId id); // 0x00C0AA70
    IoError createNativeDirectoryPath(const char* pPathName); // 0x00C0ABB0
    sint64 getFileTime(const char* pFilename); // 0x00BED9C0
    bool getTempFileName(char* pBuffer, uint32 bufferCapacity, const char* pPrefixString); // 0x00C0ADE0
    void deleteFile(const char* pFilename); // 0x00BF4700
    bool checkProjectGamebuildPath(FilePath* pPath); // 0x00C07440
    bool getCurrentProjectGamebuildPath(FilePath* pTarget, bool checkExecutablePath); // 0x00C0AEB0

    bool doesNativeFileExist(const char* pFileName); // 0x007C1C30
}
