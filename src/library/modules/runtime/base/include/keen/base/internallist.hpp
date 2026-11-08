// Original: library/modules/runtime/base/include/keen/base/internallist.hpp
// Implementation: library/modules/runtime/base/sources/global/internallist.cpp
//
// Intrusive doubly linked list: elements derive from InternalListBase::Listable. InternalListBase does the pointer
// work; InternalList<T> is the typed front end (an iterator is one pointer). InternalList<T> is written by hand from its
// HashMap<uint32, GraphicsStateObject*>::Entry instantiation (<GSO entry>).
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    class MemoryAllocator;

    // size 0x18
    class InternalListBase
    {
    public:
        // size 0x8
        class Listable
        {
            friend class InternalListBase;
            friend class IteratorBase;

            Listable* m_pNext; // 0x00
            Listable* m_pPrev; // 0x04
        };

        // size 0x4
        class IteratorBase
        {
        public:
            IteratorBase(const IteratorBase& other); // 0x004B2100 (.h:70)
            IteratorBase(); // 0x00BEF300 (.h:69)
            IteratorBase& operator++(); // 0x004B2110 (.h:72)
            IteratorBase& operator--(); // 0x004B2120 (.h:73)
            bool operator==(const IteratorBase& other) const; // 0x004B2130 (.h:74)
            bool operator!=(const IteratorBase& other) const; // 0x004B2140 (.h:75)
            IteratorBase& operator=(const IteratorBase& other); // 0x004B2150 (.h:76)
            Listable& operator*() const; // 0x004B2160 (.h:77)

        protected:
            friend class InternalListBase;

            Listable* m_pCurrent; // 0x00
        };

    protected:
        InternalListBase(const InternalListBase&);
    public:
        InternalListBase(); // 0x00BF09E0 (:8)
        ~InternalListBase(); // 0x00BF8C80 (:15)
        void clear(); // 0x00BF0BB0 (:182)
        uint32 getSize() const; // 0x00BF0BD0 (:200)
    protected:
        InternalListBase& operator=(const InternalListBase&);
        void pushFrontBase(Listable* pObject); // 0x00BF0A00 (:20)
        void pushBackBase(Listable* pObject); // 0x00BF0A30 (:47)
        const IteratorBase& insertBase(Listable* pObject); // 0x00BF0AB0 (:105)
        const IteratorBase& insertBase(Listable* pObject, Listable* pPrevious); // 0x00BF0A70 (:77)
        const IteratorBase& eraseBase(Listable* pObject); // 0x00BF0AF0 (:114)
        Listable* frontBase();
        Listable* backBase(); // 0x00BEF310 (.h:97)
        const IteratorBase& findBase(uint32 offset, uint32 key); // 0x00BF0B70 (:164)
        const IteratorBase& findBase(Listable* pObject); // 0x00BF0B40 (:146)
        const IteratorBase& getStartBase() const; // 0x004B2170 (.h:102)
        const IteratorBase& getEndBase() const; // 0x004B2180 (.h:103)
        const IteratorBase& getLastBase() const; // 0x00BF0BE0 (:205)
        void setNext(IteratorBase&, const IteratorBase&);
        void setPrev(IteratorBase&, const IteratorBase&);
        void clearBase(); // 0x00BEF320 (.h:111)
        void setFirst(IteratorBase&);

    private:
        Listable* m_pFirst; // 0x00
        Listable* m_pLast; // 0x04
        uint32 m_numObjects; // 0x08
        IteratorBase m_begin; // 0x0C
        IteratorBase m_end; // 0x10
        IteratorBase m_local; // 0x14
    };

    // size 0x18
    template<typename T>
    class InternalList : public InternalListBase
    {
    public:
        // size 0x4
        class Iterator : public InternalListBase::IteratorBase
        {
        public:
            struct ValueType; // no definition in PDB

            Iterator(const Iterator& other); // <GSO entry> 0x00C60680 (.h:180)
            Iterator(); // <GSO entry> 0x00C60EE0 (.h:179)
            T& operator*() const; // <GSO entry> 0x00C60D50 (.h:181)
            T* operator->() const; // <GSO entry> 0x00C60690 (.h:182)
            Iterator& operator=(const Iterator& __that); // <GSO entry> 0x00C60C30
            Iterator& operator=(const InternalListBase::IteratorBase&);
        };

        // size 0x4
        class ConstIterator : public InternalListBase::IteratorBase
        {
        public:
            struct ValueType; // no definition in PDB

            ConstIterator(const Iterator& other); // <GSO entry> 0x00C60D40 (.h:193)
            ConstIterator(const ConstIterator& other); // <GSO entry> 0x00C60D30 (.h:192)
            ConstIterator();
            const T& operator*() const;
            const T* operator->() const; // <GSO entry> 0x00C60670 (.h:195)
            ConstIterator& operator=(const InternalListBase::IteratorBase&);
        };

    private:
        InternalList(const InternalList<T>&);
    public:
        InternalList(); // <GSO entry> 0x00C60CB0 (.h:171)
        void clearAndDelete(MemoryAllocator*);
        void pushFront(T* pObject); // <GSO entry> 0x00C60CC0 (.h:213)
        void pushBack(T*);
        const Iterator& insert(T* pObject, InternalListBase::IteratorBase previous); // <GSO entry> 0x00C60CE0 (.h:217)
        const Iterator& insert(T*, T*);
        const Iterator& insert(T* pObject); // <GSO entry> 0x00C60CD0 (.h:215)
        const Iterator& erase(Iterator it); // <GSO entry> 0x00C6A410 (.h:219)
        const Iterator& erase(T* pObject); // <GSO entry> 0x00C60D80 (.h:218)
        T* getFront();
        T* getBack();
        const Iterator& find(uint32, uint32);
        const Iterator& find(T*);
        const ConstIterator& getBegin() const;
        const Iterator& getBegin(); // <GSO entry> 0x00C60D00 (.h:226)
        const ConstIterator& getStart() const;
        const Iterator& getStart();
        const ConstIterator& getEnd() const; // <GSO entry> 0x00C60D20 (.h:232)
        const Iterator& getEnd(); // <GSO entry> 0x00C60D10 (.h:228)
        const ConstIterator& getLast() const;
        const Iterator& getLast();
    private:
        void operator=(const InternalList<T>&);
    public:
        ~InternalList(); // <GSO entry> 0x00C60C20
    };
}

#include "../../../sources/global/internallist.inl"
