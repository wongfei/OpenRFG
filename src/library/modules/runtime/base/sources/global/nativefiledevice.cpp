// Original: library/modules/runtime/base/sources/global/nativefiledevice.cpp
//
// The device for OS files. A mount's data is its base directory; file names are combined with it through FilePath.
// Without read threads every read is a positioned ReadFile. With read threads (FileSystemCreationParameters::
// useReadThreads), each stream opened for reading gets a reader thread that double-buffers ahead: the thread fills
// buffers[bufferWriteIndex] when bufferEmpty is signalled and signals bufferFilled; the stream consumes
// buffers[bufferReadIndex] and hands the other buffer back for the next block.

#include "nativefiledevice.hpp"
#include "keen/base/filepath.hpp"
#include "keen/base/memoryallocator.hpp"
#include "keen/base/nativefile.hpp"
#include "keen/base/string.hpp"
#include "keen/base/trace.hpp"

#include <string.h>
#include <windows.h>

namespace keen
{
    // 0x00BF3FC0
    NativeFileDevice::ReadThreadContext::ReadThreadContext()
    {
    }

    // 0x00BF3FE0
    NativeFileDevice::ReadThreadContext::~ReadThreadContext()
    {
    }

    // 0x00BF94F0 (:15)
    int NativeFileDevice::readThreadFunction(const Thread& thread)
    {
        ReadThreadContext* pContext = (ReadThreadContext*)thread.getArgument();
        for (;;)
        {
            pContext->bufferEmpty.sleepUntilSignaled();
            if (thread.isQuitRequested())
            {
                return 0;
            }

            ReadBuffer* pBuffer = &pContext->buffers[pContext->bufferWriteIndex];
            pContext->bufferWriteIndex = (pContext->bufferWriteIndex - 1) & 1;
            if (pBuffer->filePosition != pContext->filePosition)
            {
                pContext->filePosition = pBuffer->filePosition;
            }
            if (pBuffer->filePosition >= pContext->fileSize)
            {
                pBuffer->result = IoError_EofReached;
                pContext->bufferFilled.signal();
                continue;
            }

            DWORD bytesRead = 0;
            IoError error = IoError_Ok;
            if (pBuffer->data.size != 0)
            {
                LARGE_INTEGER distance;
                distance.QuadPart = (LONGLONG)pBuffer->filePosition;
                if (!SetFilePointerEx(pContext->fileHandle, distance, nullptr, FILE_BEGIN))
                {
                    error = IoError_InvalidPosition;
                    bytesRead = 0;
                }
                else if (!ReadFile(pContext->fileHandle, pBuffer->data.pStart, pBuffer->data.size, &bytesRead, nullptr))
                {
                    error = getIoErrorFromLastError();
                }
                else if (bytesRead == 0)
                {
                    error = IoError_EofReached;
                }
            }

            if (error == IoError_Ok)
            {
                if (bytesRead == 0)
                {
                    pBuffer->result = IoError_EofReached;
                }
                else
                {
                    pBuffer->size += bytesRead;
                    pContext->filePosition += bytesRead;
                }
            }
            else
            {
                pBuffer->result = error;
            }
            pContext->bufferFilled.signal();
        }
    }

    // 0x00BFF590 (:76)
    NativeFileDevice::NativeFileDevice()
    {
    }

    // 0x00BF97E0 (:80)
    NativeFileDevice::~NativeFileDevice()
    {
    }

    // 0x00C0D640 (:84)
    bool NativeFileDevice::create(MemoryAllocator* pAllocator, uint32 maxStreamCount, uint32 bufferSize, bool useReadThread)
    {
        if (!m_streams.create(pAllocator, maxStreamCount))
        {
            return false;
        }
        m_useReadThread = useReadThread;
        if (!useReadThread)
        {
            return true;
        }

        if (!m_readThreadContexts.create(pAllocator, maxStreamCount, 16))
        {
            m_streams.destroy(pAllocator);
            return false;
        }
        for (uint32 i = 0; i < m_readThreadContexts.getSize(); ++i)
        {
            ReadThreadContext& context = m_readThreadContexts[i];
            for (uint32 j = 0; j < 2; ++j)
            {
                context.buffers[j].data = allocateMemoryBlock(pAllocator, bufferSize, 0x80, 0, nullptr);
                context.buffers[j].clear(0);
            }
            context.bufferEmpty.create("ReadBufferEmpty", 0);
            context.bufferFilled.create("ReadBufferFilled", 0);
            context.bufferReadIndex = 0;
            context.bufferWriteIndex = 0;
            context.fileHandle = INVALID_HANDLE_VALUE;
            context.filePosition = 0;
            context.thread.create("NativeDataStream", readThreadFunction, 0, 0x1000, 0);
            context.thread.start(&context);
        }
        return true;
    }

    // 0x00C06AC0 (:143)
    void NativeFileDevice::destroy(MemoryAllocator* pAllocator)
    {
        if (m_useReadThread)
        {
            for (uint32 i = 0; i < m_readThreadContexts.getSize(); ++i)
            {
                ReadThreadContext& context = m_readThreadContexts[i];
                context.thread.requestQuit();
                context.bufferEmpty.signal();
                context.thread.join((uint32)Thread::TimeOutInfinite);
                context.thread.destroy();
                context.bufferEmpty.destroy();
                context.bufferFilled.destroy();
                for (uint32 j = 0; j < 2; ++j)
                {
                    pAllocator->free(context.buffers[j].data.pStart);
                }
            }
            m_readThreadContexts.destroy(pAllocator);
        }
        m_streams.destroy(pAllocator);
    }

    // 0x00BFF5D0 (:173)
    void NativeFileDevice::waitForBuffer(FileStreamData* pFileData)
    {
        // Takes the first filled buffer of a freshly opened or repositioned stream and requests the next one.
        ReadThreadContext& context = m_readThreadContexts[m_streams.getHandleFromStream(pFileData)];
        if (context.hasBuffer)
        {
            return;
        }
        context.bufferFilled.sleepUntilSignaled();
        const uint32 readIndex = context.bufferReadIndex;
        context.hasBuffer = true;
        ReadBuffer& readBuffer = context.buffers[readIndex];
        context.buffers[(readIndex - 1) & 1].clear(readBuffer.filePosition + readBuffer.data.size);
        context.bufferEmpty.signal();
    }

    // 0x00C0C000 (:192)
    IoResult<uint16> NativeFileDevice::openFile(const FileDeviceMountData* pMountData, const char* pFileName, FileAccessMode accessMode)
    {
        IoResult<uint16> result;
        result.result = 0xFFFF;

        FileStreamData* pFileData = m_streams.allocateStream();
        if (pFileData == nullptr)
        {
            debug::nativePrintArgs("[base] Could not open file '%s'. out of file stream handles.!\n", pFileName);
            result.error = IoError_OutOfMemory;
            return result;
        }

        FilePath path;
        path.createCombinedPath(((const NativeFileDeviceMountData*)pMountData)->basePath, pFileName);
        IoResult<void*> openResult = keen::openFile(path.getCompletePath(), accessMode);
        if (openResult.error != IoError_Ok)
        {
            if (accessMode != FileAccessMode_Read && openResult.error == IoError_FileNotFound)
            {
                // Create the missing directories and try again.
                const IoError directoryError = createNativeDirectoryPath(path.getDirectoryWithPrefix());
                if (directoryError != IoError_Ok)
                {
                    m_streams.freeStream(pFileData);
                    result.error = directoryError;
                    return result;
                }
                openResult = keen::openFile(path.getCompletePath(), accessMode);
            }
            if (openResult.error != IoError_Ok)
            {
                m_streams.freeStream(pFileData);
                result.error = openResult.error;
                return result;
            }
        }

        pFileData->fileHandle = openResult.result;
        pFileData->isInReadMode = accessMode == FileAccessMode_Read && m_useReadThread;
        pFileData->filePosition = 0;
        pFileData->fileSize = getFileSize(openResult.result).result;
        const uint16 handle = m_streams.getHandleFromStream(pFileData);
        if (pFileData->isInReadMode)
        {
            pFileData->result = IoError_Ok;
            ReadThreadContext& context = m_readThreadContexts[handle];
            context.hasBuffer = false;
            context.fileHandle = pFileData->fileHandle;
            context.fileSize = pFileData->fileSize;
            context.filePosition = 0;
            context.bufferEmpty.signal();
        }
        result.result = handle;
        result.error = IoError_Ok;
        return result;
    }

    // 0x00C06C10 (:271)
    IoError NativeFileDevice::closeFile(uint16 file)
    {
        FileStreamData* pFileData = m_streams.getStreamFromHandle(file);
        if (pFileData->isInReadMode)
        {
            ReadThreadContext& context = m_readThreadContexts[m_streams.getHandleFromStream(pFileData)];
            context.bufferFilled.sleepUntilSignaled();
            context.bufferReadIndex = 0;
            context.bufferWriteIndex = 0;
            context.buffers[0].clear(0);
        }
        const IoError error = CloseHandle(pFileData->fileHandle) ? IoError_Ok : getIoErrorFromLastError();
        m_streams.freeStream(pFileData);
        return error;
    }

    // 0x00BFF670 (:294)
    IoResult<uint32> NativeFileDevice::read(uint16 file, void* pDest, uint32 size)
    {
        IoResult<uint32> result;
        FileStreamData* pFileData = m_streams.getStreamFromHandle(file);
        if (size == 0)
        {
            result.result = 0;
            result.error = IoError_Ok;
            return result;
        }
        if (!pFileData->isInReadMode)
        {
            result = readFile(pFileData->fileHandle, pFileData->filePosition, pDest, size);
            pFileData->filePosition += result.result;
            return result;
        }

        waitForBuffer(pFileData);
        ReadThreadContext& context = m_readThreadContexts[m_streams.getHandleFromStream(pFileData)];
        ReadBuffer* pBuffer = &context.buffers[context.bufferReadIndex];
        uint8* pTarget = (uint8*)pDest;
        uint32 totalSize = 0;
        while (pBuffer->result == IoError_Ok)
        {
            if (pFileData->filePosition >= pFileData->fileSize)
            {
                result.result = totalSize;
                result.error = totalSize != 0 ? IoError_Ok : IoError_EofReached;
                return result;
            }
            uint32 copySize = pBuffer->size - pBuffer->readOffset;
            if (size - totalSize < copySize)
            {
                copySize = size - totalSize;
            }
            memcpy(pTarget, pBuffer->data.pStart + pBuffer->readOffset, copySize);
            pBuffer->readOffset += copySize;
            pTarget += copySize;
            pFileData->filePosition += copySize;
            totalSize += copySize;
            if (pBuffer->readOffset >= pBuffer->size)
            {
                // Buffer used up: switch to the other one and let the thread refill this one two blocks ahead.
                context.bufferFilled.sleepUntilSignaled();
                context.bufferReadIndex = (context.bufferReadIndex - 1) & 1;
                pBuffer->clear(pBuffer->filePosition + pBuffer->data.size * 2);
                context.bufferEmpty.signal();
            }
            if (totalSize == size)
            {
                result.result = size;
                result.error = IoError_Ok;
                return result;
            }
            pBuffer = &context.buffers[context.bufferReadIndex];
        }
        result.result = totalSize;
        result.error = (pBuffer->result == IoError_EofReached && totalSize != 0) ? IoError_Ok : pBuffer->result;
        return result;
    }

    // 0x00BFF840 (:379)
    IoResult<uint32> NativeFileDevice::write(uint16 file, const void* pSrc, uint32 size)
    {
        IoResult<uint32> result;
        FileStreamData* pFileData = m_streams.getStreamFromHandle(file);
        LARGE_INTEGER distance;
        distance.QuadPart = (LONGLONG)pFileData->filePosition;
        DWORD bytesWritten = 0;
        if (!SetFilePointerEx(pFileData->fileHandle, distance, nullptr, FILE_BEGIN))
        {
            result.result = 0;
            result.error = IoError_InvalidPosition;
            return result;
        }
        result.error = WriteFile(pFileData->fileHandle, pSrc, size, &bytesWritten, nullptr) ? IoError_Ok : getIoErrorFromLastError();
        result.result = bytesWritten;
        if (result.error == IoError_Ok)
        {
            pFileData->filePosition += size;
        }
        return result;
    }

    // 0x00BFF8D0 (:393)
    IoError NativeFileDevice::flushWriteBuffer(uint16 file)
    {
        FileStreamData* pFileData = m_streams.getStreamFromHandle(file);
        if (!FlushFileBuffers(pFileData->fileHandle))
        {
            return getIoErrorFromLastError();
        }
        return IoError_Ok;
    }

    // 0x00BFF910 (:401)
    IoResult<uint64> NativeFileDevice::getSize(uint16 file) const
    {
        IoResult<uint64> result;
        const FileStreamData* pFileData = m_streams.getStreamFromHandle(file);
        LARGE_INTEGER size;
        if (!GetFileSizeEx(pFileData->fileHandle, &size))
        {
            result.result = 0;
            result.error = getIoErrorFromLastError();
            return result;
        }
        result.result = (uint64)size.QuadPart;
        result.error = IoError_Ok;
        return result;
    }

    // 0x00BFF990 (:409)
    IoError NativeFileDevice::setPosition(uint16 file, uint64 position)
    {
        FileStreamData* pFileData = m_streams.getStreamFromHandle(file);
        if (!pFileData->isInReadMode)
        {
            pFileData->filePosition = position;
            return IoError_Ok;
        }
        if (position == pFileData->filePosition)
        {
            return IoError_Ok;
        }
        pFileData->filePosition = position < pFileData->fileSize ? position : pFileData->fileSize;

        waitForBuffer(pFileData);
        ReadThreadContext& context = m_readThreadContexts[m_streams.getHandleFromStream(pFileData)];
        const uint32 readIndex = context.bufferReadIndex;
        ReadBuffer& readBuffer = context.buffers[readIndex];
        if (position >= readBuffer.filePosition && position < readBuffer.filePosition + readBuffer.size)
        {
            // Inside the current buffer.
            if (readBuffer.result == IoError_Ok)
            {
                readBuffer.readOffset = (uint32)(position - readBuffer.filePosition);
            }
            return readBuffer.result;
        }

        context.bufferFilled.sleepUntilSignaled();
        const uint32 otherIndex = (context.bufferReadIndex - 1) & 1;
        context.bufferReadIndex = otherIndex;
        ReadBuffer& otherBuffer = context.buffers[otherIndex];
        if (position >= readBuffer.filePosition + readBuffer.data.size && position >= otherBuffer.filePosition && position < otherBuffer.filePosition + otherBuffer.size)
        {
            // Inside the buffer read ahead: continue there and refill the old one.
            readBuffer.clear(otherBuffer.filePosition + otherBuffer.data.size);
            context.bufferEmpty.signal();
            if (otherBuffer.result == IoError_Ok)
            {
                otherBuffer.readOffset = (uint32)(position - otherBuffer.filePosition);
                return IoError_Ok;
            }
            return otherBuffer.result;
        }

        // Anywhere else: restart reading at the new position.
        context.bufferReadIndex = 0;
        context.bufferWriteIndex = 0;
        context.buffers[0].clear(position);
        context.hasBuffer = false;
        context.bufferEmpty.signal();
        return IoError_Ok;
    }

    // 0x00BFFBC0 (:491)
    IoResult<uint64> NativeFileDevice::getPosition(uint16 file) const
    {
        IoResult<uint64> result;
        const FileStreamData* pFileData = m_streams.getStreamFromHandle(file);
        result.result = pFileData->filePosition;
        result.error = pFileData->isInReadMode ? pFileData->result : IoError_Ok;
        return result;
    }

    // 0x00C088B0 (:506)
    IoResult<FileDeviceMountData*> NativeFileDevice::allocateMountData(MemoryAllocator* pAllocator, const char* pBaseDirectory, bool createIfNotExist)
    {
        // createIfNotExist is not used.
        IoResult<FileDeviceMountData*> result;
        FilePath path;
        path.setCompletePath(pBaseDirectory, true);
        NativeFileDeviceMountData* pMountData = (NativeFileDeviceMountData*)pAllocator->allocate(sizeof(NativeFileDeviceMountData), 1, 0, nullptr);
        if (pMountData == nullptr)
        {
            result.result = nullptr;
            result.error = IoError_OutOfMemory;
            return result;
        }
        copyString(pMountData->basePath, sizeof(pMountData->basePath), path.getDirectoryWithPrefix());
        result.result = pMountData;
        result.error = IoError_Ok;
        return result;
    }

    // 0x00BF97F0 (:538)
    IoError NativeFileDevice::freeMountData(MemoryAllocator* pAllocator, FileDeviceMountData* pMountData)
    {
        if (pMountData != nullptr)
        {
            pAllocator->free(pMountData);
        }
        return IoError_Ok;
    }

    // 0x00BF0FA0 (:548)
    void NativeFileDevice::ReadBuffer::clear(uint64 position)
    {
        filePosition = position;
        size = 0;
        readOffset = 0;
        result = IoError_Ok;
    }

    // 0x00BF0FC0 (:556)
    IoError NativeFileDevice::setFilePermissionByName(const FileDeviceMountData* pMountData, const char* pFileName, bool isWriteable)
    {
        return IoError_NotImplemented;
    }

    // 0x00C08960 (:565)
    IoError NativeFileDevice::getFileStatusByName(FileStatus* pStatus, const FileDeviceMountData* pMountData, const char* pFileName, uint32 valueMask) const
    {
        FilePath path;
        path.createCombinedPath(((const NativeFileDeviceMountData*)pMountData)->basePath, pFileName);
        return getNativeFileStatusByName(pStatus, path.getCompletePath(), valueMask);
    }

    // 0x00C08A00 (:578)
    bool NativeFileDevice::getNativeFilePath(char* pBuffer, uint32 bufferCapacity, const FileDeviceMountData* pMountData, const char* pFileName)
    {
        FilePath path;
        path.createCombinedPath(((const NativeFileDeviceMountData*)pMountData)->basePath, pFileName);
        copyString(pBuffer, bufferCapacity, path.getCompletePath());
        return true;
    }
}
