// Original: library/modules/runtime/base/include/keen/base/chunkedlist.hpp
// Implementation: library/modules/runtime/base/sources/global/chunkedlist.cpp
//
// ChunkedListBase, the untyped base of chunked lists.
#pragma once

#include "keen/base/types.hpp"
#include "keen/base/internallist.hpp"

namespace keen
{
    class MemoryAllocator;

    // size 0x34
    class ChunkedListBase
    {
    public:
        enum
        {
            NUM_LISTNODEENTRIES = 32,
        };

        struct ListNode;
        typedef InternalList<ListNode> ListNodeList;

        // size 0x8C
        struct ListNode : public InternalListBase::Listable
        {
            uint32 m_numUsedEntries; // 0x08
            void* m_pEntries[32]; // 0x0C
        };

        // size 0x8
        class IteratorBase
        {
        public:
            bool operator==(const IteratorBase& other) const; // 0x00BEF360 (.h:58)
            bool operator!=(const IteratorBase& other) const; // 0x00BEF380 (.h:59)
            IteratorBase& operator=(const IteratorBase& other); // 0x00BF6D50 (.h:60)
            void operator++(); // 0x00BF6D70 (.h:63)
            void replace(void*);
        protected:
            void* getObject() const; // 0x00BF6DA0 (.h:80)
        public:
            IteratorBase(); // 0x00BF6DB0

        protected:
            InternalList<ListNode>::Iterator m_NodeIt; // 0x00
            uint32 m_Index; // 0x04
        };

        void clear(MemoryAllocator* pAllocator); // 0x00BFCA40 (:143)
        uint32 getSize() const;
        void compact(MemoryAllocator* pAllocator); // 0x00BF7030 (:150)
    protected:
        ChunkedListBase(); // 0x00C011D0 (:8)
        ~ChunkedListBase(); // 0x00BFCA20 (:17)
        void pushBackBase(void* newEntry, MemoryAllocator* pAllocator); // 0x00BF6DC0 (:22)
        const IteratorBase& eraseBase(const IteratorBase& it, MemoryAllocator* pAllocator); // 0x00BF6E50 (:53)
        const IteratorBase& getBeginBase() const; // 0x00BEF340 (.h:40)
        const IteratorBase& getEndBase() const; // 0x00BEF350 (.h:41)
        void* getObjectAtIndex(uint32 index) const; // 0x00BF7190 (:205)
        const IteratorBase& findBase(void* entry); // 0x00BF6FD0 (:130)
        bool eraseUnsortedBase(const IteratorBase& it, MemoryAllocator* pAllocator); // 0x00BF6F50 (:96)

        uint32 m_numUsedEntries; // 0x00
        InternalList<ListNode> m_ListNodes; // 0x04
        IteratorBase m_begin; // 0x1C
        IteratorBase m_end; // 0x24
        IteratorBase m_local; // 0x2C
    };
}
