// Original: library/modules/runtime/base/include/keen/base/queue.hpp
//
// Queue<T>, a fixed-capacity FIFO on an Array. Written from the <InputEvent> instantiation.
#pragma once

#include "keen/base/types.hpp"
#include "keen/base/array.hpp"

namespace keen
{
    class MemoryAllocator;

    // size (<InputEvent>) 0x14
    template<typename T>
    class Queue
    {
    public:
        Queue(); // <InputEvent> 0x00C79C00 (.h:36)
        ~Queue(); // <InputEvent> 0x00C79C20 (.h:51)
        bool create(MemoryAllocator* pAllocator, uint32 size, uint32 alignment); // <InputEvent> 0x00C7DA50 (.h:60)
        void destroy(MemoryAllocator* pAllocator); // <InputEvent> 0x00C7DA90 (.h:67)
        void queue(const T&);
        T& dequeue();
        T& pushUninitialized();
        void push(const T& elem); // <InputEvent> 0x00C79C30 (.h:93)
        T& pop(); // <InputEvent> 0x00C79C70 (.h:107)
        const T& peek() const;
        T& peek();
        const T& operator[](uint32 index) const; // <InputEvent> 0x00C79CB0 (.h:157)
        T& operator[](uint32 index); // <InputEvent> 0x00C79C90 (.h:144)
        uint32 getSize() const; // <InputEvent> 0x00C76360 (.h:171)
        bool isFull() const; // <InputEvent> 0x00C79CD0 (.h:183)
        bool isEmpty() const; // <InputEvent> 0x00C76370 (.h:195)
        void clear();
        uint32 getCapacity() const;

    private:
        uint32 m_size; // 0x00
        uint32 m_bottom; // 0x04
        uint32 m_top; // 0x08
        Array<T> m_data; // 0x0C
    };
}

#include "../../../sources/global/queue.inl"
