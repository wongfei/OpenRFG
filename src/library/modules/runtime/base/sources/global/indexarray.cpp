// Original: library/modules/runtime/base/sources/global/indexarray.cpp
//
// IndexArray is a stack of free indices in [0, size), created full with 0 on top. allocateIndex does not check for
// an empty stack.

#include "keen/base/indexarray.hpp"

namespace keen
{
    // 0x00BF8310 (:6)
    IndexArray::IndexArray()
    {
    }

    // 0x00BF8320 (:11)
    IndexArray::~IndexArray()
    {
    }

    // 0x00BF8330 (:16)
    bool IndexArray::create(MemoryAllocator* pAllocator, uint32 size)
    {
        if (!m_indices.create(pAllocator, size, 16))
        {
            return false;
        }
        for (uint32 i = 0; i < m_indices.getCapacity(); ++i)
        {
            m_indices.pushBack(size - i - 1);
        }
        return true;
    }

    // 0x00BFE460 (:31)
    void IndexArray::destroy(MemoryAllocator* pAllocator)
    {
        m_indices.destroy(pAllocator);
    }

    // 0x00BF83A0 (:36)
    uint32 IndexArray::allocateIndex()
    {
        const uint32 index = m_indices.getBack();
        m_indices.popBack();
        return index;
    }

    // 0x00BF83B0 (:43)
    void IndexArray::freeIndex(uint32 index)
    {
        m_indices.pushBack(index);
    }

    bool IndexArray::isEmpty() const
    {
        return m_indices.isFull();
    }

    // 0x00BF7940 (.h:21)
    bool IndexArray::isFull() const
    {
        // "Full" means every index is allocated.
        return m_indices.getSize() == 0;
    }
}
