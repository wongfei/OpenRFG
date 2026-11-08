// Original: library/modules/runtime/base/include/keen/base/slice.hpp
// Implementation: library/modules/runtime/base/sources/global/slice.inl
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    // Non-owning view with a size/capacity split over externally owned storage.
    // size 0xC
    template<typename T>
    class Slice
    {
    public:
        Slice();
        void create(T*, uint32, uint32);
        void destroy();
        T* getStart();
        const T* getStart() const;
        T* getEnd();
        const T* getEnd() const;
        bool isValid() const;
        void clear(); // <MemoryBlockInfo> 0x00BF3B10
        Slice<T> split(uint32, uint32, uint32);
        bool isEmpty() const;
        bool isFull() const; // <MemoryBlockInfo> 0x00BF3B20
        void setSize(uint32);
        uint32 getSize() const;
        uint32 getRemainingSize() const;
        uint32 getCapacity() const;
        const T& operator[](uint32) const;
        T& operator[](uint32);
        T& pushBack(); // <MemoryBlockInfo> 0x00BF3B30
        void pushBack(const T&);
        void popBack();
        void eraseUnsortedByIndex(uint32);
        T* allocateSpace(uint32);
        bool copy(Slice<T>);

    private:
        T* m_pStart; // 0x00
        uint32 m_size; // 0x04
        uint32 m_capacity; // 0x08
    };
}

#include "../../../sources/global/slice.inl"
