// Original: library/modules/runtime/base/include/keen/base/stack.hpp (inline template members; included by stack.hpp)
//
// Partial: the members the occlusion query manager uses. The addresses are of the <uint32> instantiation.
#pragma once

namespace keen
{
    // <uint32> 0x00501F40 (.h:25)
    template<typename T>
    Stack<T>::Stack()
    {
    }

    // <uint32> 0x004F3700
    template<typename T>
    Stack<T>::~Stack()
    {
    }

    // <uint32> 0x00C69A00 (.h:30)
    template<typename T>
    void Stack<T>::create(uint32 capacity, MemoryAllocator* pAllocator)
    {
        // Magic number: 16-byte aligned storage.
        m_data.create(pAllocator, capacity, 16);
    }

    // <uint32> 0x00C6D450 (.h:35)
    template<typename T>
    void Stack<T>::destroy(MemoryAllocator* pAllocator)
    {
        m_data.destroy(pAllocator);
    }

    // <uint32> 0x00C69A40 (.h:40)
    // Notes (decompiled behaviour, kept as is): there is no capacity check.
    template<typename T>
    void Stack<T>::push(const T& elem)
    {
        m_data.pushBack(elem);
    }

    // <uint32> 0x00C69A60 (.h:45)
    // The returned reference points at the popped slot.
    template<typename T>
    const T& Stack<T>::pop()
    {
        const T& top = m_data.getBack();
        m_data.popBack();
        return top;
    }

    // <uint32> 0x00C69A70 (.h:64)
    template<typename T>
    uint32 Stack<T>::getSize() const
    {
        return m_data.getSize();
    }
}
