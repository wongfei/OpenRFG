// Original: library/modules/runtime/base/include/keen/base/array.hpp
// Implementation: library/modules/runtime/base/sources/global/array.inl
#pragma once

#include "keen/base/types.hpp"
#include "keen/base/slice.hpp"

namespace keen
{
    class MemoryAllocator;

    // Heap array whose size is fixed at creation.
    // size 0x8
    template<typename T>
    class Array
    {
    public:
        typedef T ValueType;
        typedef T* Iterator;
        typedef const T* ConstIterator;

    private:
        Array(const Array<T>&);
    public:
        Array(); // <NativeFileDevice::ReadThreadContext> 0x00BF3880
        ~Array(); // <NativeFileDevice::ReadThreadContext> 0x00BF3890
        bool create(MemoryAllocator* pAllocator, uint32 size, uint32 alignment); // <NativeFileDevice::ReadThreadContext> 0x00C00B10
        bool createCopy(MemoryAllocator*, Slice<T>);
        bool createCopy(MemoryAllocator*, const Array<T>&);
        void destroy(MemoryAllocator* pAllocator); // <NativeFileDevice::ReadThreadContext> 0x00C045A0
        bool isEmpty() const;
        uint32 getSize() const; // <NativeFileDevice::ReadThreadContext> 0x00BF38A0
        uint32 getSizeInBytes() const; // <uint8> 0x00C85200
        uint32 getCapacity() const;
        const T& operator[](uint32) const;
        T& operator[](uint32 index); // <NativeFileDevice::ReadThreadContext> 0x00BF38B0
        void swap(Array<T>&);
        const T* getStart() const;
        T* getStart();
        const T* getEnd() const;
        T* getEnd();

    private:
        Array<T>& operator=(const Array<T>&);

        T* m_pData; // 0x00
        uint32 m_size; // 0x04
    };
}

#include "../../../sources/global/array.inl"
