// Original: library/modules/runtime/base/include/keen/base/sizedarray.hpp
// Implementation: library/modules/runtime/base/sources/global/sizedarray.inl
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    class MemoryAllocator;

    // Heap array with fixed capacity and a variable element count.
    // size 0xC
    template<typename T>
    class SizedArray
    {
    public:
        typedef T ValueType;
        typedef T* Iterator;
        typedef const T* ConstIterator;

    private:
        SizedArray(const SizedArray<T>&);
    public:
        SizedArray(); // <uint32> 0x004D84B0
        ~SizedArray(); // <uint32> 0x004D84C0
        bool create(MemoryAllocator* pAllocator, uint32 capacity, uint32 alignment); // <uint32> 0x00BF3740
        bool createCopy(MemoryAllocator*, const SizedArray<T>&);
        void destroy(MemoryAllocator* pAllocator); // <uint32> 0x00BFB9A0
        bool isEmpty() const; // <uint32> 0x00BF3790
        bool isFull() const; // <uint32> 0x00C82400
        uint32 getSize() const; // <uint32> 0x00C60740
        uint32 getCapacity() const; // <uint32> 0x00BF37A0
        uint32 getRemainingCapacity() const;
        const T& operator[](uint32) const;
        T& operator[](uint32 index); // <uint32> 0x00C82410
        void swap(SizedArray<T>&);
        void clear(); // <uint32> 0x00BF3E70
        T& pushBack();
        void pushBack(const T& value); // <uint32> 0x00BF37B0
        void popBack(); // <uint32> 0x00BF37D0
        T& getBack(); // <uint32> 0x00BF37E0
        const T& getBack() const;
        void setSize(uint32, const T&);
        const T* getStart() const;
        T* getStart(); // <uint32> 0x00C82420
        const T* getEnd() const;
        T* getEnd(); // <uint32> 0x00C82760
        T* erase(T* it); // <uint32> 0x00C856B0
        void eraseUnsortedByIterator(T*);
        void eraseUnsortedByIndex(uint32);
        T* insert(T*, const T&);

    protected:
        bool isCreated() const;

    private:
        SizedArray<T>& operator=(const SizedArray<T>&);

        T* m_pData; // 0x00
        uint32 m_size; // 0x04
        uint32 m_capacity; // 0x08
    };
}

#include "../../../sources/global/sizedarray.inl"
