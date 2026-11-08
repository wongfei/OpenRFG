// Original: library/modules/runtime/base/include/keen/base/databuffer.hpp
// Implementation: library/modules/runtime/base/sources/global/databuffer.cpp
//
// DataBuffer, a bump allocator over a caller-provided memory block (aligned sub-buffers; state save/restore).
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    // size 0xC
    struct DataBufferState
    {
        uint8* pCurrentPosition; // 0x00
        uint8* pEnd; // 0x04
        uint8* pBufferStart; // 0x08
    };

    // size 0xC
    class DataBuffer
    {
    public:
        DataBuffer() // 0x00C5B5A0 (.h:21)
        {
            m_pCurrentPosition = nullptr;
            m_pEnd = nullptr;
            m_pBufferStart = nullptr;
        }
        void set(void* pMemoryBlock, uint32 size); // 0x00BEF6B0 (:8)
        void* getBuffer(uint32 size, uint32 alignment); // 0x00BF71C0 (:17)
        void resizeLastBuffer(uint32 size); // 0x00BEF6D0 (:32)
        bool isSet() const;
        uint32 getRemainingCapacity(uint32 alignment) const; // 0x00BF71F0 (:40)
        void* getBufferStart() const;
        void storeState(DataBufferState* pState); // 0x00BEF6E0 (:54)
        void restoreState(const DataBufferState& state); // 0x00BEF700 (:62)

    private:
        uint8* m_pCurrentPosition; // 0x00
        uint8* m_pEnd; // 0x04
        uint8* m_pBufferStart; // 0x08
    };
}
