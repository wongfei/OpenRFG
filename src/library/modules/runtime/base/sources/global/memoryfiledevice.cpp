// Original: library/modules/runtime/base/sources/global/memoryfiledevice.cpp
//
// Files kept in memory blocks. A mount is either a single read-only file (mountReadOnlyMemoryFile: fileHandle is
// its entry) or a writable directory backed by an allocator (mountMemory: fileHandle is 0xFFFFFFFF). In a directory
// mount files are found by the CRC-32 of their name; opening a missing file for writing creates it, and writing grows
// the block (doubling, starting at 1 KB). Closing the last stream of a written file shrinks its block to the file
// size. A file can be open for writing once, or for reading any number of times while it is not being written.

#include "memoryfiledevice.hpp"
#include "keen/base/crc32.hpp"
#include "keen/base/memoryallocator.hpp"

#include <string.h>

namespace keen
{
    // 0x00BF7950 (.h:57)
    MemoryFileDevice::FileEntry::FileEntry()
    {
        pMountData = nullptr;
    }

    // 0x00BEFCE0 (.h:58)
    bool MemoryFileDevice::FileEntry::isFree() const
    {
        return pMountData == nullptr;
    }

    // 0x00BEFCF0 (.h:59)
    void MemoryFileDevice::FileEntry::setFree()
    {
        pMountData = nullptr;
    }

    // 0x00BEFD00 (.h:60)
    bool MemoryFileDevice::FileEntry::isOpen() const
    {
        return openCount != 0;
    }

    // 0x00BFEFB0 (:14)
    MemoryFileDevice::MemoryFileDevice()
    {
    }

    // 0x00BF8E30 (:18)
    MemoryFileDevice::~MemoryFileDevice()
    {
    }

    // 0x00C0D480 (:23)
    bool MemoryFileDevice::create(MemoryAllocator* pAllocator, uint32 maxFileCount, uint32 maxStreamCount)
    {
        if (!m_streams.create(pAllocator, maxStreamCount))
        {
            return false;
        }
        if (!m_files.create(pAllocator, maxFileCount, 16))
        {
            m_streams.destroy(pAllocator);
            return false;
        }
        if (!m_fileIndices.create(pAllocator, maxFileCount))
        {
            m_streams.destroy(pAllocator);
            m_files.destroy(pAllocator);
            return false;
        }
        return true;
    }

    // 0x00C03F00 (:46)
    void MemoryFileDevice::destroy(MemoryAllocator* pAllocator)
    {
        m_fileIndices.destroy(pAllocator);
        m_files.destroy(pAllocator);
        m_streams.destroy(pAllocator);
    }

    // 0x00BF8E40 (:55)
    uint32 MemoryFileDevice::addFile(MemoryFileDeviceMountData* pMountData, MemoryBlock memoryBlock, const char* pFilename)
    {
        // The name is not used: a single-file mount is found through its handle, not by name.
        if (m_fileIndices.isFull())
        {
            return 0xFFFFFFFFu;
        }
        const uint32 handle = m_fileIndices.allocateIndex();
        FileEntry& file = m_files[handle];
        file.fileNameCrc = 0;
        file.pMountData = pMountData;
        file.memoryBlock = memoryBlock;
        file.fileSize = memoryBlock.size;
        file.openCount = 0;
        file.isWritten = false;
        return handle;
    }

    // 0x00BF8EA0 (:81)
    void MemoryFileDevice::removeFile(uint32 handle)
    {
        m_files[handle].setFree();
        m_fileIndices.freeIndex(handle);
    }

    // 0x00C066F0 (:94)
    IoResult<uint16> MemoryFileDevice::openFile(const FileDeviceMountData* pMountData, const char* pFileName, FileAccessMode accessMode)
    {
        IoResult<uint16> result;
        result.result = 0xFFFF;
        if (accessMode != FileAccessMode_Read && accessMode != FileAccessMode_Write)
        {
            result.error = IoError_InvalidArgument;
            return result;
        }

        StreamEntry* pStream = m_streams.allocateStream();
        if (pStream == nullptr)
        {
            result.error = IoError_OutOfMemory;
            return result;
        }

        const MemoryFileDeviceMountData* pMemoryMountData = (const MemoryFileDeviceMountData*)pMountData;
        FileEntry* pFile;
        if (pMemoryMountData->fileHandle == 0xFFFFFFFFu)
        {
            const uint32 fileNameCrc = getCrc32Value(pFileName);
            pFile = findFileEntryByNameCrc(pMountData, fileNameCrc);
            if (pFile != nullptr)
            {
                if (pFile->openCount == 0)
                {
                    if (accessMode == FileAccessMode_Read)
                    {
                        pFile->isWritten = false;
                    }
                    else if (pFile->isWriteable)
                    {
                        pFile->fileSize = 0;
                        pFile->isWritten = true;
                    }
                    else
                    {
                        m_streams.freeStream(pStream);
                        result.error = IoError_NoPermission;
                        return result;
                    }
                }
                else if (accessMode == FileAccessMode_Read && !pFile->isWritten)
                {
                    pFile->isWritten = false;
                }
                else
                {
                    m_streams.freeStream(pStream);
                    result.error = IoError_NoPermission;
                    return result;
                }
            }
            else
            {
                if (accessMode == FileAccessMode_Read)
                {
                    m_streams.freeStream(pStream);
                    result.error = IoError_FileNotFound;
                    return result;
                }
                if (m_fileIndices.isFull())
                {
                    m_streams.freeStream(pStream);
                    result.error = IoError_OutOfMemory;
                    return result;
                }
                pFile = &m_files[m_fileIndices.allocateIndex()];
                pFile->memoryBlock.pStart = nullptr;
                pFile->memoryBlock.size = 0;
                pFile->pMountData = (MemoryFileDeviceMountData*)pMemoryMountData;
                pFile->fileSize = 0;
                pFile->fileNameCrc = fileNameCrc;
                pFile->isWriteable = true;
                pFile->openCount = 0;
                pFile->fileSize = 0;
                pFile->isWritten = true;
            }
            ++pFile->openCount;
            pStream->pFile = pFile;
            pStream->accessMode = accessMode;
        }
        else
        {
            if (accessMode != FileAccessMode_Read)
            {
                // The original does not return the stream to the pool here.
                result.error = IoError_NoPermission;
                return result;
            }
            pFile = &m_files[pMemoryMountData->fileHandle];
            ++pFile->openCount;
            pStream->pFile = pFile;
            pStream->accessMode = FileAccessMode_Read;
        }
        pStream->position = 0;
        result.result = m_streams.getHandleFromStream(pStream);
        result.error = IoError_Ok;
        return result;
    }

    // 0x00C069D0 (:198)
    IoError MemoryFileDevice::closeFile(uint16 file)
    {
        StreamEntry* pStream = m_streams.getStreamFromHandle(file);
        FileEntry* pFile = pStream->pFile;
        --pFile->openCount;
        if (pFile->openCount == 0)
        {
            MemoryAllocator* pAllocator = pFile->pMountData->pAllocator;
            if (pAllocator != nullptr && pFile->isWritten)
            {
                const uint32 fileSize = pFile->fileSize;
                if (fileSize < pFile->memoryBlock.size)
                {
                    uint8* pData = (uint8*)pAllocator->allocate(fileSize, 16, 0, nullptr);
                    if (pData != nullptr)
                    {
                        memcpy(pData, pFile->memoryBlock.pStart, pFile->fileSize);
                        pAllocator->free(pFile->memoryBlock.pStart);
                        pFile->memoryBlock.pStart = pData;
                        pFile->memoryBlock.size = fileSize;
                    }
                }
            }
            pFile->isWritten = false;
        }
        pStream->pFile = nullptr;
        m_streams.freeStream(pStream);
        return IoError_Ok;
    }

    // 0x00BFEFF0 (:234)
    IoResult<uint32> MemoryFileDevice::read(uint16 file, void* pTarget, uint32 size)
    {
        IoResult<uint32> result;
        StreamEntry* pStream = m_streams.getStreamFromHandle(file);
        if (pStream->accessMode != FileAccessMode_Read)
        {
            result.result = 0;
            result.error = IoError_NoPermission;
            return result;
        }
        if (size == 0)
        {
            result.result = 0;
            result.error = IoError_Ok;
            return result;
        }
        uint32 readSize = pStream->pFile->fileSize - pStream->position;
        if (size < readSize)
        {
            readSize = size;
        }
        memcpy(pTarget, pStream->pFile->memoryBlock.pStart + pStream->position, readSize);
        pStream->position += readSize;
        result.result = readSize;
        result.error = readSize < size ? IoError_EofReached : IoError_Ok;
        return result;
    }

    // 0x00BFF080 (:265)
    IoResult<uint32> MemoryFileDevice::write(uint16 file, const void* pSource, uint32 size)
    {
        IoResult<uint32> result;
        StreamEntry* pStream = m_streams.getStreamFromHandle(file);
        FileEntry* pFile = pStream->pFile;
        if (pStream->accessMode != FileAccessMode_Write)
        {
            result.result = 0;
            result.error = IoError_NoPermission;
            return result;
        }
        if (size == 0)
        {
            result.result = 0;
            result.error = IoError_Ok;
            return result;
        }
        if (pFile->memoryBlock.size - pStream->position < size)
        {
            uint32 newSize = pFile->memoryBlock.size != 0 ? pFile->memoryBlock.size * 2 : 0x400;
            if (newSize <= pStream->position + size)
            {
                newSize = pStream->position + size;
            }
            MemoryAllocator* pAllocator = pFile->pMountData->pAllocator;
            uint8* pData = (uint8*)pAllocator->allocate(newSize, 16, 0, nullptr);
            if (pData == nullptr)
            {
                result.result = 0;
                result.error = IoError_OutOfMemory;
                return result;
            }
            if (pFile->memoryBlock.pStart != nullptr)
            {
                memcpy(pData, pFile->memoryBlock.pStart, pFile->memoryBlock.size);
                pAllocator->free(pFile->memoryBlock.pStart);
            }
            pFile->memoryBlock.pStart = pData;
            pFile->memoryBlock.size = newSize;
        }
        memcpy(pFile->memoryBlock.pStart + pStream->position, pSource, size);
        pStream->position += size;
        if (pFile->fileSize <= pStream->position)
        {
            pFile->fileSize = pStream->position;
        }
        result.result = size;
        result.error = IoError_Ok;
        return result;
    }

    // 0x00BF0EE0 (:353)
    IoError MemoryFileDevice::flushWriteBuffer(uint16 file)
    {
        return IoError_Ok;
    }

    // 0x00BFF1C0 (:315)
    IoResult<uint64> MemoryFileDevice::getSize(uint16 file) const
    {
        IoResult<uint64> result;
        result.result = m_streams.getStreamFromHandle(file)->pFile->fileSize;
        result.error = IoError_Ok;
        return result;
    }

    // 0x00BFF210 (:327)
    IoError MemoryFileDevice::setPosition(uint16 file, uint64 position)
    {
        StreamEntry* pStream = m_streams.getStreamFromHandle(file);
        if (position <= pStream->pFile->fileSize)
        {
            pStream->position = (uint32)position;
            return IoError_Ok;
        }
        return IoError_InvalidPosition;
    }

    // 0x00BFF250 (:345)
    IoResult<uint64> MemoryFileDevice::getPosition(uint16 file) const
    {
        IoResult<uint64> result;
        result.result = m_streams.getStreamFromHandle(file)->position;
        result.error = IoError_Ok;
        return result;
    }

    // 0x00BF8ED0 (:360)
    IoResult<FileDeviceMountData*> MemoryFileDevice::allocateDeviceMountData(MemoryAllocator* pAllocator, MemoryAllocator* pDeviceAllocator)
    {
        IoResult<FileDeviceMountData*> result;
        MemoryFileDeviceMountData* pMountData = (MemoryFileDeviceMountData*)pAllocator->allocate(sizeof(MemoryFileDeviceMountData), 4, 0, nullptr);
        if (pMountData == nullptr)
        {
            result.result = nullptr;
            result.error = IoError_OutOfMemory;
            return result;
        }
        pMountData->pAllocator = pDeviceAllocator;
        pMountData->fileHandle = 0xFFFFFFFFu;
        result.result = pMountData;
        result.error = IoError_Ok;
        return result;
    }

    // 0x00BF8F20 (:377)
    IoResult<FileDeviceMountData*> MemoryFileDevice::allocateFileMountData(MemoryAllocator* pAllocator, MemoryBlock memoryBlock, const char* pFilename)
    {
        IoResult<FileDeviceMountData*> result;
        MemoryFileDeviceMountData* pMountData = (MemoryFileDeviceMountData*)pAllocator->allocate(sizeof(MemoryFileDeviceMountData), 4, 0, nullptr);
        if (pMountData != nullptr)
        {
            pMountData->pAllocator = nullptr;
            pMountData->fileHandle = addFile(pMountData, memoryBlock, pFilename);
            if (pMountData->fileHandle != 0xFFFFFFFFu)
            {
                result.result = pMountData;
                result.error = IoError_Ok;
                return result;
            }
            pAllocator->free(pMountData);
        }
        result.result = nullptr;
        result.error = IoError_OutOfMemory;
        return result;
    }

    // 0x00BFF2A0 (:399)
    IoError MemoryFileDevice::freeMountData(MemoryAllocator* pAllocator, FileDeviceMountData* pMountData)
    {
        MemoryFileDeviceMountData* pMemoryMountData = (MemoryFileDeviceMountData*)pMountData;
        if (pMemoryMountData->fileHandle == 0xFFFFFFFFu)
        {
            // Frees the file memory; the entries themselves stay allocated (as in the original).
            removeFiles(pMemoryMountData);
        }
        else
        {
            removeFile(pMemoryMountData->fileHandle);
        }
        pAllocator->free(pMountData);
        return IoError_Ok;
    }

    // 0x00BF8FA0 (:420)
    void MemoryFileDevice::removeFiles(const MemoryFileDeviceMountData* pMountData)
    {
        for (uint32 i = 0; i < m_files.getSize(); ++i)
        {
            FileEntry& file = m_files[i];
            if (file.pMountData != nullptr && file.pMountData == pMountData && file.memoryBlock.pStart != nullptr)
            {
                file.pMountData->pAllocator->free(file.memoryBlock.pStart);
            }
        }
    }

    // 0x00BFF300 (:444)
    IoError MemoryFileDevice::setFilePermissionByName(const FileDeviceMountData* pMountData, const char* pFileName, bool isWriteable)
    {
        FileEntry* pFile = findFileEntryByNameCrc(pMountData, getCrc32Value(pFileName));
        if (pFile == nullptr)
        {
            return IoError_FileNotFound;
        }
        if (isWriteable && pFile->pMountData->fileHandle != 0xFFFFFFFFu)
        {
            return IoError_NoPermission;
        }
        pFile->isWriteable = isWriteable;
        return IoError_Ok;
    }

    // 0x00BFF380 (:466)
    IoError MemoryFileDevice::getFileStatusByName(FileStatus* pStatus, const FileDeviceMountData* pMountData, const char* pFileName, uint32 valueMask) const
    {
        const FileEntry* pFile = findFileEntryByNameCrc(pMountData, getCrc32Value(pFileName));
        if (pFile == nullptr)
        {
            return IoError_FileNotFound;
        }
        if ((valueMask & FileStatus::ValueMask_NativeFileName) != 0)
        {
            return IoError_NotImplemented;
        }
        if ((valueMask & FileStatus::ValueMask_Writeable) != 0)
        {
            pStatus->isWriteable = pFile->isWriteable;
        }
        if ((valueMask & FileStatus::ValueMask_Size) != 0)
        {
            pStatus->size = pFile->fileSize;
        }
        return (valueMask & FileStatus::ValueMask_LastModificationTime) != 0 ? IoError_NotImplemented : IoError_Ok;
    }

    // 0x00BF9030 (:518)
    const MemoryFileDevice::FileEntry* MemoryFileDevice::findFileEntryByNameCrc(const FileDeviceMountData* pMountData, uint32 fileNameCrc) const
    {
        for (uint32 i = 0; i < m_files.getSize(); ++i)
        {
            const FileEntry& file = m_files[i];
            if (file.pMountData == (const MemoryFileDeviceMountData*)pMountData && file.fileNameCrc == fileNameCrc)
            {
                return &file;
            }
        }
        return nullptr;
    }

    // 0x00BF9000 (:505)
    MemoryFileDevice::FileEntry* MemoryFileDevice::findFileEntryByNameCrc(const FileDeviceMountData* pMountData, uint32 fileNameCrc)
    {
        return (FileEntry*)((const MemoryFileDevice*)this)->findFileEntryByNameCrc(pMountData, fileNameCrc);
    }
}
