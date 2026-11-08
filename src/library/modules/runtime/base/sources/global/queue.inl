// Original: library/modules/runtime/base/sources/global/queue.inl (included by queue.hpp)
//
// Partial: the members used by the recovered code so far (bodies from the <InputEvent> instantiation). The storage
// holds no constructed elements: create only allocates, push copies in. push and pop do not check the size (original
// behaviour: pushing into a full queue overwrites the oldest element and the size passes the capacity).
#pragma once

#include "keen/base/memoryallocator.hpp"

namespace keen
{
    // <InputEvent> 0x00C79C00 (.h:36)
    template<typename T>
    Queue<T>::Queue()
    {
        m_size = 0;
        m_bottom = 0;
        m_top = 0;
    }

    // <InputEvent> 0x00C79C20 (.h:51)
    template<typename T>
    Queue<T>::~Queue()
    {
    }

    // <InputEvent> 0x00C7DA50 (.h:60): the array's create inlined (array.inl value-initializes the elements, the
    // original only allocates; no reader depends on the contents of an unused slot)
    template<typename T>
    bool Queue<T>::create(MemoryAllocator* pAllocator, uint32 size, uint32 alignment)
    {
        return m_data.create(pAllocator, size, alignment);
    }

    // <InputEvent> 0x00C7DA90 (.h:67)
    template<typename T>
    void Queue<T>::destroy(MemoryAllocator* pAllocator)
    {
        m_data.destroy(pAllocator);
        m_size = 0;
        m_bottom = 0;
        m_top = 0;
    }

    // Reserves the next slot at the top and returns it (the slot is not constructed).
    template<typename T>
    T& Queue<T>::pushUninitialized()
    {
        T& slot = m_data[m_top];
        m_size++;
        m_top = (m_top + 1) % m_data.getSize();
        return slot;
    }

    // <InputEvent> 0x00C79C30 (.h:93)
    template<typename T>
    void Queue<T>::push(const T& elem)
    {
        m_data[m_top] = elem;
        m_size++;
        m_top = (m_top + 1) % m_data.getSize();
    }

    // <InputEvent> 0x00C79C70 (.h:107)
    template<typename T>
    T& Queue<T>::pop()
    {
        const uint32 index = m_bottom;
        m_size--;
        m_bottom = (index + 1) % m_data.getSize();
        return m_data[index];
    }

    // The oldest element (the next pop).
    template<typename T>
    const T& Queue<T>::peek() const
    {
        return m_data[m_bottom];
    }

    template<typename T>
    T& Queue<T>::peek()
    {
        return m_data[m_bottom];
    }

    // <InputEvent> 0x00C79CB0 (.h:157): the index counts from the oldest element
    template<typename T>
    const T& Queue<T>::operator[](uint32 index) const
    {
        return m_data[(m_bottom + index) % m_data.getSize()];
    }

    // <InputEvent> 0x00C79C90 (.h:144)
    template<typename T>
    T& Queue<T>::operator[](uint32 index)
    {
        return m_data[(m_bottom + index) % m_data.getSize()];
    }

    // <InputEvent> 0x00C76360 (.h:171)
    template<typename T>
    uint32 Queue<T>::getSize() const
    {
        return m_size;
    }

    // <InputEvent> 0x00C79CD0 (.h:183)
    template<typename T>
    bool Queue<T>::isFull() const
    {
        return m_size == m_data.getSize();
    }

    // <InputEvent> 0x00C76370 (.h:195)
    template<typename T>
    bool Queue<T>::isEmpty() const
    {
        return m_size == 0;
    }

    template<typename T>
    void Queue<T>::clear()
    {
        m_size = 0;
        m_top = 0;
        m_bottom = 0;
    }

    template<typename T>
    uint32 Queue<T>::getCapacity() const
    {
        return m_data.getSize();
    }
}
