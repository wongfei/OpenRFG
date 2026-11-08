// Original: library/modules/runtime/base/sources/global/aliaspathfiledevice.cpp
//
// An alias mount forwards every request to "<target path>/<file name>" in the same FileSystem. A stream keeps the
// file system handle of the forwarded file.

#include "aliaspathfiledevice.hpp"
#include "keen/base/filesystem.hpp"
#include "keen/base/memoryallocator.hpp"
#include "keen/base/string.hpp"

namespace keen
{
    // 0x00BFC450 (:14)
    AliasPathFileDevice::AliasPathFileDevice()
    {
    }

    // 0x00BF4AD0 (:19)
    AliasPathFileDevice::~AliasPathFileDevice()
    {
    }

    // 0x00C0D3A0 (:24)
    bool AliasPathFileDevice::create(MemoryAllocator* pAllocator, uint32 maxStreamCount)
    {
        return m_streams.create(pAllocator, maxStreamCount);
    }

    // 0x00C01190 (:29)
    void AliasPathFileDevice::destroy(MemoryAllocator* pAllocator)
    {
        m_streams.destroy(pAllocator);
    }

    // 0x00BF4AE0 (:36)
    IoResult<FileDeviceMountData*> AliasPathFileDevice::allocateMountData(MemoryAllocator* pAllocator, FileSystem* pFileSystem, const char* pTargetPath)
    {
        IoResult<FileDeviceMountData*> result;
        AliasPathFileDeviceMountData* pMountData = (AliasPathFileDeviceMountData*)pAllocator->allocate(sizeof(AliasPathFileDeviceMountData), 4, 0, nullptr);
        if (pMountData == nullptr)
        {
            result.result = nullptr;
            result.error = IoError_OutOfMemory;
            return result;
        }
        pMountData->pFileSystem = pFileSystem;
        copyString(pMountData->targetPath, sizeof(pMountData->targetPath), pTargetPath);
        result.result = pMountData;
        result.error = IoError_Ok;
        return result;
    }

    // 0x00BF4B40 (:53)
    IoError AliasPathFileDevice::freeMountData(MemoryAllocator* pAllocator, FileDeviceMountData* pMountData)
    {
        if (pMountData != nullptr)
        {
            pAllocator->free(pMountData);
        }
        return IoError_Ok;
    }

    // 0x00C075E0 (:65)
    IoResult<uint16> AliasPathFileDevice::openFile(const FileDeviceMountData* pMountData, const char* pFileName, FileAccessMode accessMode)
    {
        IoResult<uint16> result;
        result.result = 0xFFFF;
        StreamEntry* pStream = m_streams.allocateStream();
        if (pStream == nullptr)
        {
            result.error = IoError_OutOfMemory;
            return result;
        }
        const AliasPathFileDeviceMountData* pAliasMountData = (const AliasPathFileDeviceMountData*)pMountData;
        char path[260];
        formatString(path, sizeof(path), "%s/%s", pAliasMountData->targetPath, pFileName);
        const IoResult<uint32> openResult = file::openFile(pAliasMountData->pFileSystem, path, accessMode);
        if (openResult.error != IoError_Ok)
        {
            m_streams.freeStream(pStream);
            result.error = openResult.error;
            return result;
        }
        pStream->fileHandle = openResult.result;
        pStream->pMountData = (AliasPathFileDeviceMountData*)pAliasMountData;
        result.result = m_streams.getHandleFromStream(pStream);
        result.error = IoError_Ok;
        return result;
    }

    // 0x00C04710 (:93)
    IoError AliasPathFileDevice::closeFile(uint16 file)
    {
        StreamEntry* pStream = m_streams.getStreamFromHandle(file);
        const IoError error = file::closeFile(pStream->pMountData->pFileSystem, pStream->fileHandle);
        m_streams.freeStream(pStream);
        return error;
    }

    // 0x00BFC490 (:105)
    IoResult<uint32> AliasPathFileDevice::read(uint16 file, void* pDest, uint32 size)
    {
        StreamEntry* pStream = m_streams.getStreamFromHandle(file);
        return file::read(pStream->pMountData->pFileSystem, pStream->fileHandle, pDest, size);
    }

    // 0x00BFC4D0 (:113)
    IoResult<uint32> AliasPathFileDevice::write(uint16 file, const void* pSrc, uint32 size)
    {
        StreamEntry* pStream = m_streams.getStreamFromHandle(file);
        return file::write(pStream->pMountData->pFileSystem, pStream->fileHandle, pSrc, size);
    }

    // 0x00BFC540 (:121)
    IoError AliasPathFileDevice::flushWriteBuffer(uint16 file)
    {
        StreamEntry* pStream = m_streams.getStreamFromHandle(file);
        return file::flushWriteBuffer(pStream->pMountData->pFileSystem, pStream->fileHandle);
    }

    // 0x00BFC5A0 (:129)
    IoResult<uint64> AliasPathFileDevice::getSize(uint16 file) const
    {
        const StreamEntry* pStream = m_streams.getStreamFromHandle(file);
        return file::getFileSize(pStream->pMountData->pFileSystem, pStream->fileHandle);
    }

    // 0x00BFC630 (:137)
    IoError AliasPathFileDevice::setPosition(uint16 file, uint64 position)
    {
        StreamEntry* pStream = m_streams.getStreamFromHandle(file);
        return file::setFilePosition(pStream->pMountData->pFileSystem, pStream->fileHandle, position);
    }

    // 0x00BFC680 (:145)
    IoResult<uint64> AliasPathFileDevice::getPosition(uint16 file) const
    {
        const StreamEntry* pStream = m_streams.getStreamFromHandle(file);
        return file::getFilePosition(pStream->pMountData->pFileSystem, pStream->fileHandle);
    }

    // 0x00C07770 (:153)
    IoError AliasPathFileDevice::setFilePermissionByName(const FileDeviceMountData* pMountData, const char* pFileName, bool isWriteable)
    {
        const AliasPathFileDeviceMountData* pAliasMountData = (const AliasPathFileDeviceMountData*)pMountData;
        char path[260];
        formatString(path, sizeof(path), "%s/%s", pAliasMountData->targetPath, pFileName);
        return file::setFilePermissionByName(pAliasMountData->pFileSystem, path, isWriteable);
    }

    // 0x00C077E0 (:165)
    IoError AliasPathFileDevice::getFileStatusByName(FileStatus* pStatus, const FileDeviceMountData* pMountData, const char* pFileName, uint32 valueMask) const
    {
        const AliasPathFileDeviceMountData* pAliasMountData = (const AliasPathFileDeviceMountData*)pMountData;
        char path[260];
        formatString(path, sizeof(path), "%s/%s", pAliasMountData->targetPath, pFileName);
        return file::getFileStatusByName(pStatus, pAliasMountData->pFileSystem, path, valueMask);
    }
}
