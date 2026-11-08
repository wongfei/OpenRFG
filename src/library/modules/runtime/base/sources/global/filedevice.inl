// Original: library/modules/runtime/base/sources/global/filedevice.inl (included by filedevice.hpp)
//
// FileStreamAllocator: a mutex-guarded PoolAllocator of per-device stream records; a stream's 16-bit handle is its
// pool index. create() keeps going (and returns true) when the pool memory cannot be allocated, as BasePoolAllocator
// does. allocateStream default-constructs the record.
#pragma once

#include <new>

namespace keen
{
    // <AliasPathFileDevice::StreamEntry> 0x00BF4AB0
    template<typename TStream>
    FileStreamAllocator<TStream>::FileStreamAllocator()
    {
    }

    // <AliasPathFileDevice::StreamEntry> 0x00BF4AC0
    template<typename TStream>
    FileStreamAllocator<TStream>::~FileStreamAllocator()
    {
    }

    // <AliasPathFileDevice::StreamEntry> 0x00C0D030 (:10)
    template<typename TStream>
    bool FileStreamAllocator<TStream>::create(MemoryAllocator* pAllocator, uint32 streamCount)
    {
        if (!m_streamAllocator.createFromAllocator(pAllocator, streamCount, nullptr))
        {
            return false;
        }
        m_streamMutex.create("FileStream");
        return true;
    }

    // <AliasPathFileDevice::StreamEntry> 0x00C009F0 (:22)
    template<typename TStream>
    void FileStreamAllocator<TStream>::destroy(MemoryAllocator* pAllocator)
    {
        m_streamMutex.destroy();
        m_streamAllocator.destroyFromAllocator(pAllocator);
    }

    // <AliasPathFileDevice::StreamEntry> 0x00C04390 (:29)
    template<typename TStream>
    TStream* FileStreamAllocator<TStream>::allocateStream()
    {
        MutexLock lock(&m_streamMutex);
        return m_streamAllocator.newObject();
    }

    // <AliasPathFileDevice::StreamEntry> 0x00C043F0 (:36)
    template<typename TStream>
    void FileStreamAllocator<TStream>::freeStream(TStream* pStream)
    {
        MutexLock lock(&m_streamMutex);
        m_streamAllocator.deleteObject(pStream);
    }

    // <AliasPathFileDevice::StreamEntry> 0x00BFB8C0 (:43)
    template<typename TStream>
    uint16 FileStreamAllocator<TStream>::getHandleFromStream(const TStream* pStream)
    {
        return (uint16)m_streamAllocator.getIndexFromObject(pStream);
    }

    // <AliasPathFileDevice::StreamEntry> 0x00BFB8D0 (:55)
    template<typename TStream>
    const TStream* FileStreamAllocator<TStream>::getStreamFromHandle(uint16 handle) const
    {
        return m_streamAllocator.getObjectFromIndex(handle);
    }

    // <AliasPathFileDevice::StreamEntry> 0x00BFB8F0 (:49)
    template<typename TStream>
    TStream* FileStreamAllocator<TStream>::getStreamFromHandle(uint16 handle)
    {
        return m_streamAllocator.getObjectFromIndex(handle);
    }
}
