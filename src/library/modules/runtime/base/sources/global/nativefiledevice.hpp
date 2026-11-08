// Original: library/modules/runtime/base/sources/global/nativefiledevice.hpp (private to keen base)
// Implementation: library/modules/runtime/base/sources/global/nativefiledevice.cpp
#pragma once

#include "keen/base/array.hpp"
#include "keen/base/event.hpp"
#include "keen/base/filedevice.hpp"
#include "keen/base/memory.hpp"
#include "keen/base/thread.hpp"

namespace keen
{
    // Placement inferred (no line info): base directory of a native mount.
    // size 0x104
    struct NativeFileDeviceMountData : public FileDeviceMountData
    {
        char basePath[260]; // 0x00
    };

    // Device for OS files. With m_useReadThread each stream gets a reader thread that double-buffers ahead.
    // size 0x98
    class NativeFileDevice : public FileDeviceInterface
    {
    public:
        // size 0x20
        struct ReadBuffer
        {
            void clear(uint64 position); // 0x00BF0FA0

            MemoryBlock data; // 0x00
            uint64 filePosition; // 0x08
            uint32 size; // 0x10
            uint32 readOffset; // 0x14
            IoError result; // 0x18
        };

        // size 0x20
        struct FileStreamData
        {
            void* fileHandle; // 0x00
            uint64 filePosition; // 0x08
            uint64 fileSize; // 0x10
            IoError result; // 0x18
            bool isInReadMode; // 0x1C
        };

        // size 0xC0
        struct ReadThreadContext
        {
            ReadThreadContext(); // 0x00BF3FC0
            ~ReadThreadContext(); // 0x00BF3FE0

            Thread thread; // 0x00
            ReadBuffer buffers[2]; // 0x58
            Event bufferEmpty; // 0x98
            Event bufferFilled; // 0x9C
            uint32 bufferReadIndex; // 0xA0
            uint32 bufferWriteIndex; // 0xA4
            bool hasBuffer; // 0xA8
            void* fileHandle; // 0xAC
            uint64 filePosition; // 0xB0
            uint64 fileSize; // 0xB8
        };

        NativeFileDevice(); // 0x00BFF590
        virtual ~NativeFileDevice(); // vtbl+0x00 0x00BF97E0
        bool create(MemoryAllocator* pAllocator, uint32 maxStreamCount, uint32 bufferSize, bool useReadThread); // 0x00C0D640
        void destroy(MemoryAllocator* pAllocator); // 0x00C06AC0
        IoResult<FileDeviceMountData*> allocateMountData(MemoryAllocator* pAllocator, const char* pBaseDirectory, bool createIfNotExist); // 0x00C088B0
        virtual IoError freeMountData(MemoryAllocator* pAllocator, FileDeviceMountData* pMountData); // vtbl+0x0C 0x00BF97F0
        virtual IoResult<uint16> openFile(const FileDeviceMountData* pMountData, const char* pFileName, FileAccessMode accessMode); // vtbl+0x04 0x00C0C000
        virtual IoError closeFile(uint16 file); // vtbl+0x08 0x00C06C10
        virtual IoResult<uint32> read(uint16 file, void* pDest, uint32 size); // vtbl+0x10 0x00BFF670
        virtual IoResult<uint32> write(uint16 file, const void* pSrc, uint32 size); // vtbl+0x14 0x00BFF840
        virtual IoError flushWriteBuffer(uint16 file); // vtbl+0x18 0x00BFF8D0
        virtual IoResult<uint64> getSize(uint16 file) const; // vtbl+0x1C 0x00BFF910
        virtual IoError setPosition(uint16 file, uint64 position); // vtbl+0x20 0x00BFF990
        virtual IoResult<uint64> getPosition(uint16 file) const; // vtbl+0x24 0x00BFFBC0
        virtual IoError setFilePermissionByName(const FileDeviceMountData* pMountData, const char* pFileName, bool isWriteable); // vtbl+0x28 0x00BF0FC0
        virtual IoError getFileStatusByName(FileStatus* pStatus, const FileDeviceMountData* pMountData, const char* pFileName, uint32 valueMask) const; // vtbl+0x2C 0x00C08960
        virtual bool getNativeFilePath(char* pBuffer, uint32 bufferCapacity, const FileDeviceMountData* pMountData, const char* pFileName); // vtbl+0x30 0x00C08A00

    private:
        static int readThreadFunction(const Thread& thread); // 0x00BF94F0
        void waitForBuffer(FileStreamData* pFileData); // 0x00BFF5D0

        FileStreamAllocator<FileStreamData> m_streams; // 0x04
        Array<ReadThreadContext> m_readThreadContexts; // 0x54
        Mutex m_mutex; // 0x5C
        bool m_useReadThread; // 0x94
    };
}
