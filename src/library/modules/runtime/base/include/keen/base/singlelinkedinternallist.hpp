// Original: library/modules/runtime/base/include/keen/base/singlelinkedinternallist.hpp
//
// SingleLinkedInternalList<T, TPointer, NextOffset>, an intrusive singly linked list whose next pointer sits at
// NextOffset in T. Written from the <TerminationHandler, TerminationHandler*, 4> instantiation; the parameter names
// are inferred.
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    // size (<TerminationHandler,TerminationHandler*,4>) 0xC
    template<typename T, typename TPointer, uint32 NextOffset>
    class SingleLinkedInternalList
    {
    public:
        // size 0x4
        class Iterator
        {
        public:
            void operator++(); // <TerminationHandler,TerminationHandler*,4> 0x00BFB810 (.h:99)
            bool operator==(const Iterator&) const;
            bool operator!=(const Iterator& other) const; // <TerminationHandler,TerminationHandler*,4> 0x00BF33C0 (.h:101)
            Iterator& operator=(const Iterator&);
            T& operator*() const;
            TPointer operator->() const; // <TerminationHandler,TerminationHandler*,4> 0x00BF33D0 (.h:105)
        protected:
            TPointer getNext(TPointer pCurrent); // <TerminationHandler,TerminationHandler*,4> 0x00BF3460 (.h:110)

            TPointer m_pCurrent; // 0x00

            friend class SingleLinkedInternalList; // inferred: the list sets its iterators
        };

        struct ConstIterator; // no definition in PDB

        SingleLinkedInternalList(); // <TerminationHandler,TerminationHandler*,4> 0x00BFB7E0 (.h:71)
        void pushFront(TPointer pNewElement); // <TerminationHandler,TerminationHandler*,4> 0x00BFB7F0 (.h:73)
        void pushBack(TPointer);
        TPointer popFront();
        void clear(); // <TerminationHandler,TerminationHandler*,4> 0x00BF3440 (.h:93)
        bool isEmpty() const;
        int size() const;
        ConstIterator getStart() const;
        Iterator getStart(); // <TerminationHandler,TerminationHandler*,4> 0x00BF33A0 (.h:139)
        ConstIterator getEnd() const;
        Iterator getEnd(); // <TerminationHandler,TerminationHandler*,4> 0x00BF33B0 (.h:140)
        Iterator erase(TPointer pElement); // <TerminationHandler,TerminationHandler*,4> 0x00BFBBE0 (.h:192)
        Iterator find(TPointer);
        Iterator eraseWithPrevious(TPointer, TPointer);
        const TPointer getFront() const;
        TPointer getFront();
    protected:
        TPointer getNext(TPointer pCurrent); // <TerminationHandler,TerminationHandler*,4> 0x00BF3DB0 (.h:153)
        void setNext(TPointer pElement, TPointer pNext); // <TerminationHandler,TerminationHandler*,4> 0x00BF3450 (.h:157)

        TPointer m_pFirst; // 0x00
        Iterator m_begin; // 0x04
        Iterator m_end; // 0x08
    };
}

#include "../../../sources/global/singlelinkedinternallist.inl"
