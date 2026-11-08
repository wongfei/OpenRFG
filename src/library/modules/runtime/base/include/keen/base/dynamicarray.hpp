// Original: library/modules/runtime/base/include/keen/base/dynamicarray.hpp
//
// DynamicArray<T>, a growable array with configurable alignment and growth. Written from the
// <DirectoryWatch::ChangeInfo> instantiation.
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    class MemoryAllocator;

    // size (<ChangeInfo>) 0x1C
    template<typename T>
    class DynamicArray
    {
    public:
        typedef T* Iterator;
        typedef const T* ConstIterator;

        struct ValueType; // no definition in PDB

    private:
        DynamicArray(const DynamicArray<T>&);
    public:
        DynamicArray(); // <ChangeInfo> 0x00BF3500 (dynamicarray.inl:11)
        ~DynamicArray(); // <ChangeInfo> 0x00BF3510 (dynamicarray.inl:20)
        void create(MemoryAllocator* pAllocator, uint32 alignment, uint32 increaseValue, uint32 minimumCapacity); // <ChangeInfo> 0x00BF3520 (dynamicarray.inl:26)
        void destroy(); // <ChangeInfo> 0x00BFB850 (dynamicarray.inl:38)
        bool isEmpty() const; // <ChangeInfo> 0x00BF3540 (dynamicarray.inl:54)
        uint32 getSize() const; // <ChangeInfo> 0x00BF3550 (dynamicarray.inl:60)
        uint32 getCapacity() const;
        void setSize(uint32, const T&);
        void reserve(uint32 capacity); // <ChangeInfo> 0x00C00FA0 (dynamicarray.inl:162)
        void freeUnusedMemory();
        void clear(); // <ChangeInfo> 0x00BF3DC0 (dynamicarray.inl:103)
        const T& operator[](uint32) const;
        T& operator[](uint32 index); // <ChangeInfo> 0x00BF3560 (dynamicarray.inl:72)
        T& pushBack(); // <ChangeInfo> 0x00C07120 (dynamicarray.inl:127)
        void pushBack(const T&);
        void popBack(); // <ChangeInfo> 0x00BF3DD0 (dynamicarray.inl:138)
        T& getBack();
        const T& getBack() const;
        const T* getStart() const;
        T* getStart();
        const T* getEnd() const;
        T* getEnd();
        T* erase(T*);
        void eraseUnsorted(uint32 index); // <ChangeInfo> 0x00BFB880 (dynamicarray.inl:294)
    private:
        DynamicArray<T>& operator=(const DynamicArray<T>&);
        void increaseCapacity(); // <ChangeInfo> 0x00C046B0 (dynamicarray.inl:88)
        void setCapacity(uint32 capacity); // <ChangeInfo> 0x00BFC200 (dynamicarray.inl:179)

        T* m_pData; // 0x00
        uint32 m_size; // 0x04
        uint32 m_capacity; // 0x08
        MemoryAllocator* m_pAllocator; // 0x0C
        uint32 m_alignment; // 0x10
        uint32 m_increaseValue; // 0x14
        uint32 m_minimumCapacity; // 0x18
    };
}
