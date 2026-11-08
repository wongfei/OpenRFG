// Original: library/modules/runtime/base/sources/global/memorywritestream.cpp
//
// A WriteStream into memory: either a fixed block (which ends the stream with IoError_EofReached when full) or memory
// of an allocator that grows by doubling (at least 4 KB).
//
// Partial: the fixed block constructor, the destructor, close and the flush. Not ported: open, the allocator
// constructor, setPosition, alignPosition.

#include "keen/base/memorywritestream.hpp"
#include "keen/base/memoryallocator.hpp"

#include <string.h>

namespace keen
{
    // 0x00C054A0 (.h:14)
    MemoryWriteStream::MemoryWriteStream(void* pMemory, uint32 size, const char* pDebugName)
    {
        m_pAllocator = nullptr;
        initialize((uint8*)pMemory, size, flushMemoryStream, pDebugName);
    }

    // 0x00BFF490 (:12)
    MemoryWriteStream::~MemoryWriteStream()
    {
        close();
    }

    // 0x00BF93A0 (:35)
    // Frees the buffer when it came from the allocator.
    void MemoryWriteStream::close()
    {
        if (m_pAllocator != nullptr && getBufferStart() != nullptr)
        {
            m_pAllocator->free(getBufferStart());
        }
        m_pAllocator = nullptr;
        initialize(nullptr, 0u, nullptr, nullptr);
    }

    // 0x00BFF4D0 (:76)
    void MemoryWriteStream::flushMemoryStream(WriteStream* pStream)
    {
        MemoryWriteStream* pMemoryStream = (MemoryWriteStream*)pStream;
        MemoryAllocator* pAllocator = pMemoryStream->m_pAllocator;
        if (pAllocator == nullptr)
        {
            pMemoryStream->setError(IoError_EofReached);
            return;
        }
        // Magic number: the buffer at least doubles, to 4 KB at minimum.
        uint32 newCapacity = pMemoryStream->getBufferCapacity() * 2u;
        if (newCapacity < 0x1000u)
        {
            newCapacity = 0x1000u;
        }
        uint8* pNewBuffer = (uint8*)pAllocator->allocate(newCapacity, 0x10u, 0u, nullptr);
        if (pNewBuffer == nullptr)
        {
            pMemoryStream->setError(IoError_EofReached);
            return;
        }
        const uint32 size = pMemoryStream->getBufferSize();
        uint8* pOldBuffer = pMemoryStream->getBufferStart();
        if (pOldBuffer != nullptr)
        {
            memcpy(pNewBuffer, pOldBuffer, size);
        }
        pMemoryStream->m_pAllocator->free(pOldBuffer);
        pMemoryStream->setBuffer(pNewBuffer, newCapacity);
        pMemoryStream->setBufferSize(size);
    }
}
