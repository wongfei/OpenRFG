// Original: library/modules/runtime/base/sources/global/writestream.cpp
//
// The WriteStream base: the buffer bookkeeping and the error state. A stream that fails switches to a 16 byte dummy
// buffer (flushToEmptyBuffer), so later writes are dropped without further checks. The members documented as
// header-inline (.h) are defined here.

#include "keen/base/writestream.hpp"

#include <string.h>

namespace keen
{
    static uint8 s_buffer[16]; // 0x038B3D30

    // 0x00BFC340 (.h:17)
    WriteStream::WriteStream()
    {
        m_pStart = nullptr;
        m_capacity = 0;
        m_size = 0;
        m_pFlushFunction = nullptr;
        m_error = IoError_Ok;
        m_pName = nullptr;
    }

    // 0x00BF4300 (.h:75)
    void WriteStream::writeDataBlock(const void* pSource, uint32 size)
    {
        const uint8* pBytes = (const uint8*)pSource;
        while (size != 0u)
        {
            if (m_size == m_capacity)
            {
                m_pFlushFunction(this);
            }
            uint32 chunkSize = m_capacity - m_size;
            if (size < chunkSize)
            {
                chunkSize = size;
            }
            memcpy(m_pStart + m_size, pBytes, chunkSize);
            m_size += chunkSize;
            pBytes += chunkSize;
            size -= chunkSize;
        }
    }

    // 0x00BFB3A0 (:13)
    // Only the first error is kept.
    void WriteStream::setError(IoError error)
    {
        if (error != IoError_Ok && m_error == IoError_Ok)
        {
            m_error = error;
            m_pFlushFunction = flushToEmptyBuffer;
            flushToEmptyBuffer(this);
        }
    }

    // 0x00CE3A50 (.h:27)
    IoError WriteStream::getError() const
    {
        return m_error;
    }

    // 0x00BED640 (.h:28)
    bool WriteStream::hasError() const
    {
        return m_error != IoError_Ok;
    }

    // 0x00BF3040 (:57)
    void WriteStream::flush()
    {
        m_pFlushFunction(this);
    }

    // 0x00BED650 (.h:35)
    uint32 WriteStream::getBufferCapacity() const
    {
        return m_capacity;
    }

    // 0x00BED660 (.h:36)
    uint32 WriteStream::getBufferSize() const
    {
        return m_size;
    }

    // 0x00BF42D0 (.h:40)
    void WriteStream::initialize(uint8* pBuffer, uint32 bufferSize, void (*pFlushFunction)(WriteStream*), const char* pDebugName)
    {
        m_pStart = pBuffer;
        m_capacity = bufferSize;
        m_size = 0;
        m_error = IoError_Ok;
        m_pFlushFunction = pFlushFunction;
        m_pName = pDebugName;
    }

    // 0x00BED670 (.h:48)
    void WriteStream::setBuffer(uint8* pBuffer, uint32 bufferSize)
    {
        m_pStart = pBuffer;
        m_capacity = bufferSize;
        m_size = 0;
    }

    // 0x00BF3020 (:41)
    bool WriteStream::setBufferSize(uint32 size)
    {
        if (m_error == IoError_Ok && size <= m_capacity)
        {
            m_size = size;
            return true;
        }
        return false;
    }

    // 0x00BED690 (.h:57)
    uint8* WriteStream::getBufferStart()
    {
        return m_pStart;
    }

    // 0x00BF3000 (:7)
    void WriteStream::flushToEmptyBuffer(WriteStream* pStream)
    {
        pStream->m_pStart = s_buffer;
        pStream->m_capacity = sizeof(s_buffer);
        pStream->m_size = 0;
    }
}
