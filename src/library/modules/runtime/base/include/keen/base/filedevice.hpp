// Original: library/modules/runtime/base/include/keen/base/filedevice.hpp
// Implementation: library/modules/runtime/base/sources/global/filedevice.inl (FileStreamAllocator),
//                 library/modules/runtime/base/sources/global/filesystem.cpp (~FileDeviceInterface)
#pragma once

#include "keen/base/fileaccessmode.hpp"
#include "keen/base/filesystemtypes.hpp"
#include "keen/base/ioresult.hpp"
#include "keen/base/mutex.hpp"
#include "keen/base/poolallocator.hpp"

namespace keen
{
    class MemoryAllocator;

    // Empty base for per-mount device data (NativeFileDevice: base directory, PakFileDevice: directory, ...).
    // size 0x1
    struct FileDeviceMountData
    {
    };

    // A storage backend mounted into the FileSystem tree. File handles are 16-bit, per device.
    // size 0x4
    class FileDeviceInterface
    {
    public:
        virtual ~FileDeviceInterface() = 0; // vtbl+0x00 0x00BEFD50
        virtual IoResult<uint16> openFile(const FileDeviceMountData* pMountData, const char* pFileName, FileAccessMode accessMode) = 0; // vtbl+0x04
        virtual IoError closeFile(uint16 file) = 0; // vtbl+0x08
        virtual IoError freeMountData(MemoryAllocator* pAllocator, FileDeviceMountData* pMountData) = 0; // vtbl+0x0C
        virtual IoResult<uint32> read(uint16 file, void* pDest, uint32 size) = 0; // vtbl+0x10
        virtual IoResult<uint32> write(uint16 file, const void* pSrc, uint32 size) = 0; // vtbl+0x14
        virtual IoError flushWriteBuffer(uint16 file) = 0; // vtbl+0x18
        virtual IoResult<uint64> getSize(uint16 file) const = 0; // vtbl+0x1C
        virtual IoError setPosition(uint16 file, uint64 position) = 0; // vtbl+0x20
        virtual IoResult<uint64> getPosition(uint16 file) const = 0; // vtbl+0x24
        virtual IoError setFilePermissionByName(const FileDeviceMountData* pMountData, const char* pFileName, bool isWriteable) = 0; // vtbl+0x28
        virtual IoError getFileStatusByName(FileStatus* pStatus, const FileDeviceMountData* pMountData, const char* pFileName, uint32 valueMask) const = 0; // vtbl+0x2C
        virtual bool getNativeFilePath(char* pBuffer, uint32 bufferCapacity, const FileDeviceMountData* pMountData, const char* pFileName) // vtbl+0x30 0x00BEDF20 (.h:53)
        {
            return false;
        }
        FileDeviceInterface() // 0x00BEDF10 (implicit)
        {
        }

        // 0x00 vfptr
    };

    // Pool of per-device stream records; the handle is the pool index.
    // size 0x50
    template<typename TStream>
    class FileStreamAllocator
    {
    public:
        bool create(MemoryAllocator* pAllocator, uint32 streamCount); // <AliasPathFileDevice::StreamEntry> 0x00C0D030
        void destroy(MemoryAllocator* pAllocator); // <AliasPathFileDevice::StreamEntry> 0x00C009F0
        TStream* allocateStream(); // <AliasPathFileDevice::StreamEntry> 0x00C04390
        void freeStream(TStream* pStream); // <AliasPathFileDevice::StreamEntry> 0x00C043F0
        uint16 getHandleFromStream(const TStream* pStream); // <AliasPathFileDevice::StreamEntry> 0x00BFB8C0
        const TStream* getStreamFromHandle(uint16 handle) const; // <AliasPathFileDevice::StreamEntry> 0x00BFB8D0
        TStream* getStreamFromHandle(uint16 handle); // <AliasPathFileDevice::StreamEntry> 0x00BFB8F0
        FileStreamAllocator(); // <AliasPathFileDevice::StreamEntry> 0x00BF4AB0
        ~FileStreamAllocator(); // <AliasPathFileDevice::StreamEntry> 0x00BF4AC0

    private:
        PoolAllocator<TStream> m_streamAllocator; // 0x00
        Mutex m_streamMutex; // 0x18
    };
}

#include "../../../sources/global/filedevice.inl"
