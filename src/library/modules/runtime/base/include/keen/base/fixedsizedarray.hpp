// Original: library/modules/runtime/base/include/keen/base/fixedsizedarray.hpp
//
// FixedSizedArray<T, N>, an array with inline storage for N elements. Written from the <ServerInfo, 256> instantiation
// (the multiplayer server list).
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    // size (<ServerInfo,256>) 0x2A008
    template<typename T, uint32 N>
    class FixedSizedArray
    {
    public:
        enum
        {
            Capacity = N,
        };

        typedef T ValueType;
        typedef T* Iterator;
        typedef const T* ConstIterator;

        FixedSizedArray(const FixedSizedArray<T, N>&);
        FixedSizedArray(); // <ServerInfo,256> 0x005D3470 (fixedsizedarray.inl:15)
        FixedSizedArray<T, N>& operator=(const FixedSizedArray<T, N>&);
        bool isEmpty() const;
        bool isFull() const; // <ServerInfo,256> 0x005D3480 (fixedsizedarray.inl:44)
        uint32 getSize() const; // <ServerInfo,256> 0x005D3490 (fixedsizedarray.inl:50)
        uint32 getCapacity() const; // <ServerInfo,256> 0x005D34A0 (fixedsizedarray.inl:56)
        uint32 getRemainingCapacity() const;
        const T& operator[](uint32 index) const; // <ServerInfo,256> 0x005D34B0 (fixedsizedarray.inl:76)
        T& operator[](uint32);
        void clear(); // <ServerInfo,256> 0x005D34C0 (fixedsizedarray.inl:92)
        void copy(const FixedSizedArray<T, N>&);
        T& pushBack(); // <ServerInfo,256> 0x005D34D0 (fixedsizedarray.inl:113)
        void pushBack(const T&);
        void popBack();
        T& getBack();
        const T& getBack() const;
        void setSize(uint32, const T&);
        const T* getStart() const;
        T* getStart();
        const T* getEnd() const;
        T* getEnd();
        T* erase(T*);
        void eraseUnsorted(uint32);
    protected:
        bool isCreated() const;

    private:
        T m_data[N]; // 0x00
        uint32 m_size; // 0x2A000
    };
}

#include "keen/base/fixedsizedarray.inl"
