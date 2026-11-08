// Original: library/modules/runtime/base/include/keen/base/stack.hpp
//
// Stack<T>, a fixed-capacity stack on a SizedArray. Written by hand; the addresses are of the <uint32> instantiation.
#pragma once

#include "keen/base/types.hpp"
#include "keen/base/sizedarray.hpp"

namespace keen
{
    class MemoryAllocator;

    // size 0xC
    template<typename T>
    class Stack
    {
        Stack(const Stack<T>&);
    public:
        Stack(); // <uint32> 0x00501F40 (.h:25)
        void create(uint32 capacity, MemoryAllocator* pAllocator); // <uint32> 0x00C69A00 (.h:30)
        void destroy(MemoryAllocator* pAllocator); // <uint32> 0x00C6D450 (.h:35)
        void push(const T& elem); // <uint32> 0x00C69A40 (.h:40)
        const T& pop(); // <uint32> 0x00C69A60 (.h:45)
        T& getTop();
        const T& getTop() const;
        uint32 getSize() const; // <uint32> 0x00C69A70 (.h:64)
        uint32 getCapacity() const;
        bool isFull() const;
        void clear();
    private:
        Stack<T>& operator=(const Stack<T>&);
    public:
        ~Stack(); // <uint32> 0x004F3700

    private:
        SizedArray<T> m_data; // 0x00
    };
}

#include "../../../sources/global/stack.inl"
