// Original: library/modules/runtime/base/include/keen/base/singlelinkedinternallist.hpp (inline members)
//
// SingleLinkedInternalList's members, written from the <TerminationHandler, TerminationHandler*, 4> instantiation
// (rfg.exe 0x00BF33A0-0x00BFBBE0); the sound system's device change callbacks use them (SFX.md SX1). The next pointer
// is the TPointer at NextOffset in the element. m_end is always null.
#pragma once

namespace keen
{
    template<typename T, typename TPointer, uint32 NextOffset>
    inline void SingleLinkedInternalList<T, TPointer, NextOffset>::Iterator::operator++()
    {
        if (m_pCurrent != nullptr)
        {
            m_pCurrent = getNext(m_pCurrent);
        }
    }

    template<typename T, typename TPointer, uint32 NextOffset>
    inline bool SingleLinkedInternalList<T, TPointer, NextOffset>::Iterator::operator==(const Iterator& other) const
    {
        return m_pCurrent == other.m_pCurrent;
    }

    template<typename T, typename TPointer, uint32 NextOffset>
    inline bool SingleLinkedInternalList<T, TPointer, NextOffset>::Iterator::operator!=(const Iterator& other) const
    {
        return m_pCurrent != other.m_pCurrent;
    }

    template<typename T, typename TPointer, uint32 NextOffset>
    inline T& SingleLinkedInternalList<T, TPointer, NextOffset>::Iterator::operator*() const
    {
        return *m_pCurrent;
    }

    template<typename T, typename TPointer, uint32 NextOffset>
    inline TPointer SingleLinkedInternalList<T, TPointer, NextOffset>::Iterator::operator->() const
    {
        return m_pCurrent;
    }

    template<typename T, typename TPointer, uint32 NextOffset>
    inline TPointer SingleLinkedInternalList<T, TPointer, NextOffset>::Iterator::getNext(TPointer pCurrent)
    {
        return *(TPointer*)((uint8*)pCurrent + NextOffset);
    }

    template<typename T, typename TPointer, uint32 NextOffset>
    inline SingleLinkedInternalList<T, TPointer, NextOffset>::SingleLinkedInternalList()
    {
        m_pFirst = nullptr;
        m_end.m_pCurrent = nullptr;
        m_begin.m_pCurrent = nullptr;
    }

    template<typename T, typename TPointer, uint32 NextOffset>
    inline void SingleLinkedInternalList<T, TPointer, NextOffset>::pushFront(TPointer pNewElement)
    {
        setNext(pNewElement, m_pFirst);
        m_pFirst = pNewElement;
        m_begin.m_pCurrent = pNewElement;
    }

    template<typename T, typename TPointer, uint32 NextOffset>
    inline void SingleLinkedInternalList<T, TPointer, NextOffset>::clear()
    {
        m_pFirst = nullptr;
        m_begin.m_pCurrent = nullptr;
    }

    template<typename T, typename TPointer, uint32 NextOffset>
    inline bool SingleLinkedInternalList<T, TPointer, NextOffset>::isEmpty() const
    {
        return m_pFirst == nullptr;
    }

    template<typename T, typename TPointer, uint32 NextOffset>
    inline typename SingleLinkedInternalList<T, TPointer, NextOffset>::Iterator SingleLinkedInternalList<T, TPointer, NextOffset>::getStart()
    {
        return m_begin;
    }

    template<typename T, typename TPointer, uint32 NextOffset>
    inline typename SingleLinkedInternalList<T, TPointer, NextOffset>::Iterator SingleLinkedInternalList<T, TPointer, NextOffset>::getEnd()
    {
        return m_end;
    }

    // Unlinks pElement; the result is the element after it (null when not found).
    template<typename T, typename TPointer, uint32 NextOffset>
    inline typename SingleLinkedInternalList<T, TPointer, NextOffset>::Iterator SingleLinkedInternalList<T, TPointer, NextOffset>::erase(TPointer pElement)
    {
        Iterator result;
        if (m_pFirst == pElement)
        {
            m_pFirst = getNext(pElement);
            m_begin.m_pCurrent = m_pFirst;
            result.m_pCurrent = m_pFirst;
            return result;
        }
        result.m_pCurrent = nullptr;
        TPointer pPrevious = m_pFirst;
        if (pPrevious == nullptr)
        {
            return result;
        }
        for (;;)
        {
            TPointer pNext = getNext(pPrevious);
            if (pNext == pElement)
            {
                break;
            }
            pPrevious = pNext;
            if (pNext == nullptr)
            {
                return result;
            }
        }
        setNext(pPrevious, getNext(pElement));
        setNext(pElement, nullptr);
        result.m_pCurrent = getNext(pPrevious);
        return result;
    }

    template<typename T, typename TPointer, uint32 NextOffset>
    inline TPointer SingleLinkedInternalList<T, TPointer, NextOffset>::getNext(TPointer pCurrent)
    {
        return *(TPointer*)((uint8*)pCurrent + NextOffset);
    }

    template<typename T, typename TPointer, uint32 NextOffset>
    inline void SingleLinkedInternalList<T, TPointer, NextOffset>::setNext(TPointer pElement, TPointer pNext)
    {
        *(TPointer*)((uint8*)pElement + NextOffset) = pNext;
    }
}
