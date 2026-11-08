// Original: library/modules/runtime/base/include/keen/base/autoarray.hpp
// Implementation: library/modules/runtime/base/sources/global/autoarray.inl
//
// AutoArray<T>, an allocator-owned array freed on destruction. Written from the <uint8> instantiation.
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    class MemoryAllocator;

    // size (<uint8>) 0xC
    template<typename T>
    class AutoArray
    {
    public:
        typedef T ValueType;
        typedef T* Iterator;
        typedef const T* ConstIterator;

    private:
        AutoArray(const AutoArray<T>&);
    public:
        AutoArray(); // <uint8> 0x00C606D0 (autoarray.inl:14)
        ~AutoArray(); // <uint8> 0x00C6D5B0 (autoarray.inl:22)
        bool create(MemoryAllocator* pAllocator, uint32 size, uint32 alignment); // <uint8> 0x00C6D5E0 (autoarray.inl:28)
        bool createCopy(const AutoArray<T>&);
        void destroy(); // <uint8> 0x00C6A4E0 (autoarray.inl:87)
        bool isEmpty() const;
        uint32 getSize() const; // <uint8> 0x00C606E0 (autoarray.inl:109)
        uint32 getCapacity() const;
        const T& operator[](uint32) const;
        T& operator[](uint32 index); // <uint8> 0x00C606F0 (autoarray.inl:121)
        void swap(AutoArray<T>&);
        const T* getStart() const;
        T* getStart();
        const T* getEnd() const;
        T* getEnd();
    private:
        AutoArray<T>& operator=(const AutoArray<T>&);

        MemoryAllocator* m_pAllocator; // 0x00
        T* m_pData; // 0x04
        uint32 m_size; // 0x08
    };
}

#include "../../../sources/global/autoarray.inl"
