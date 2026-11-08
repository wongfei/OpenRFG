// Original: library/modules/runtime/base/include/keen/base/staticarray.hpp
// Implementation: library/modules/runtime/base/sources/global/staticarray.inl
//
// StaticArray<T>, a pointer and a size over storage owned elsewhere (no allocator). Written by hand; the addresses are
// of the <MaterialMesh> instantiation.
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    // size 0x8
    template<typename T>
    class StaticArray
    {
    public:
        typedef T ValueType;
        typedef T* Iterator;
        typedef const T* ConstIterator;

        StaticArray(const StaticArray<T>&);
        StaticArray(T*, uint32);
        StaticArray(); // <MaterialMesh> 0x00CC8BE0 (staticarray.inl:12)
        ~StaticArray(); // <MaterialMesh> 0x00CC8BF0 (staticarray.inl:32)
        void setData(T* pData, uint32 size); // <MaterialMesh> 0x00CC8C00 (staticarray.inl:37)
        T* releaseData();
        bool isEmpty() const;
        uint32 getSize() const; // <MaterialMesh> 0x00CC8C10 (staticarray.inl:50)
        uint32 getSizeInBytes() const;
        uint32 getCapacity() const;
        const T& operator[](uint32 index) const; // <MaterialMesh> 0x00CC8C20 (staticarray.inl:70)
        T& operator[](uint32 index); // <MaterialMesh> 0x00CC8C30 (staticarray.inl:62)
        void swap(StaticArray<T>&);
        const T* getStart() const;
        T* getStart();
        const T* getEnd() const;
        T* getEnd();
        StaticArray<T>& operator=(const StaticArray<T>&);

    private:
        T* m_pData; // 0x00
        uint32 m_size; // 0x04
    };
}

#include "../../../sources/global/staticarray.inl"
