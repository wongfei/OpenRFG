// Original: library/modules/runtime/base/sources/global/readstream.cpp
//
// ReadStream, a buffer window plus a refill callback. Once an error is set the stream refills with zeros forever.
//
// Partial: the members used by FileReadStream/BinaryReader (no line info for the header inlines).

#include "keen/base/readstream.hpp"

#include <string.h>

namespace keen
{
    // Magic number: refillZeros serves a 16 byte block of zeros.
    static const uint8 s_zeros[16] = {};

    // 0x00BFC710 (.h:17)
    ReadStream::ReadStream()
    {
        m_pStart = nullptr;
        m_size = 0;
        m_position = 0;
        m_pRefillFunction = nullptr;
        m_error = IoError_Ok;
        m_pName = nullptr;
    }

    // 0x00BEE810 (.h:23)
    uint32 ReadStream::getBufferSize() const
    {
        return m_size;
    }

    // 0x00BF4C10 (.h:92)
    void ReadStream::readDataBlock(void* pTarget, uint32 size)
    {
        uint8* pTargetBytes = (uint8*)pTarget;
        while (size != 0)
        {
            if (m_position >= m_size)
            {
                m_pRefillFunction(this);
            }
            uint32 chunkSize = m_size - m_position;
            if (size < chunkSize)
            {
                chunkSize = size;
            }
            memcpy(pTargetBytes, m_pStart + m_position, chunkSize);
            m_position += chunkSize;
            pTargetBytes += chunkSize;
            size -= chunkSize;
        }
    }

    // 0x00BF14B0 (:13)
    bool ReadStream::setBufferPosition(uint32 position)
    {
        if (m_error == IoError_Ok && position <= m_size)
        {
            m_position = position;
            return true;
        }
        return false;
    }

    // 0x00BEE820 (.h:42)
    void ReadStream::refill() const
    {
        m_pRefillFunction(const_cast<ReadStream*>(this));
    }

    // 0x00BF14D0 (:29)
    void ReadStream::setError(IoError error) const
    {
        if (error != IoError_Ok && m_error == IoError_Ok)
        {
            ReadStream* pThis = const_cast<ReadStream*>(this);
            pThis->m_error = error;
            pThis->m_pRefillFunction = refillZeros;
            refillZeros(pThis);
        }
    }

    IoError ReadStream::getError() const
    {
        return m_error;
    }

    // 0x00BEE830 (.h:49)
    bool ReadStream::hasError() const
    {
        return m_error != IoError_Ok;
    }

    // 0x00BF4BE0 (.h:53)
    void ReadStream::initialize(const uint8* pBuffer, uint32 bufferSize, RefillFunction* pRefillFunction, const char* pDebugName)
    {
        m_pStart = pBuffer;
        m_size = bufferSize;
        m_position = 0;
        m_error = IoError_Ok;
        m_pRefillFunction = pRefillFunction;
        m_pName = pDebugName;
    }

    // 0x00BEE840 (.h:61)
    void ReadStream::setBuffer(const uint8* pBuffer, uint32 bufferSize)
    {
        m_pStart = pBuffer;
        m_size = bufferSize;
        m_position = 0;
    }

    // 0x00BF1490 (:7)
    void ReadStream::refillZeros(ReadStream* pBuffer)
    {
        pBuffer->m_pStart = s_zeros;
        pBuffer->m_size = sizeof(s_zeros);
        pBuffer->m_position = 0;
    }
}
