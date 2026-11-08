// Original: library/modules/runtime/base/sources/global/internallist.cpp
//
// InternalListBase, the intrusive doubly linked list behind InternalList<T>. Objects derive from Listable; m_begin is
// the first object and m_end is always null (the past-the-end iterator). Functions that return an iterator return a
// reference to m_local, which the next such call overwrites.

#include "keen/base/internallist.hpp"

namespace keen
{
    // 0x00BF09E0 (:8)
    InternalListBase::InternalListBase()
    {
        m_local.m_pCurrent = nullptr;
        m_pLast = nullptr;
        m_pFirst = nullptr;
        m_numObjects = 0;
        m_end.m_pCurrent = nullptr;
        m_begin.m_pCurrent = nullptr;
    }

    // 0x00BF8C80 (:15)
    InternalListBase::~InternalListBase()
    {
        clearBase();
    }

    // 0x00BF0A00 (:20)
    void InternalListBase::pushFrontBase(Listable* pObject)
    {
        pObject->m_pPrev = nullptr;
        pObject->m_pNext = m_pFirst;
        if (m_pFirst != nullptr)
        {
            m_pFirst->m_pPrev = pObject;
        }
        m_pFirst = pObject;
        if (m_pLast == nullptr)
        {
            m_pLast = pObject;
        }
        ++m_numObjects;
        m_begin.m_pCurrent = pObject;
    }

    // 0x00BF0A30 (:47)
    void InternalListBase::pushBackBase(Listable* pObject)
    {
        pObject->m_pNext = nullptr;
        pObject->m_pPrev = m_pLast;
        if (m_pLast != nullptr)
        {
            m_pLast->m_pNext = pObject;
        }
        m_pLast = pObject;
        if (m_pFirst == nullptr)
        {
            m_pFirst = pObject;
        }
        ++m_numObjects;
        if (m_begin.m_pCurrent == nullptr)
        {
            m_begin.m_pCurrent = pObject;
        }
    }

    // 0x00BF0A70 (:77)
    const InternalListBase::IteratorBase& InternalListBase::insertBase(Listable* pObject, Listable* pPrevious)
    {
        // Notes (decompiled behaviour, kept as is): pPrevious must not be null (no insert-at-front path), and m_pFirst
        // is never updated here.
        pObject->m_pPrev = pPrevious;
        pObject->m_pNext = pPrevious->m_pNext;
        pPrevious->m_pNext = pObject;
        if (pObject->m_pNext == nullptr)
        {
            m_pLast = pObject;
        }
        else
        {
            pObject->m_pNext->m_pPrev = pObject;
        }
        ++m_numObjects;
        m_local.m_pCurrent = pObject;
        return m_local;
    }

    // 0x00BF0AB0 (:105)
    const InternalListBase::IteratorBase& InternalListBase::insertBase(Listable* pObject)
    {
        pushBackBase(pObject);
        m_local.m_pCurrent = pObject;
        return m_local;
    }

    // 0x00BF0AF0 (:114)
    const InternalListBase::IteratorBase& InternalListBase::eraseBase(Listable* pObject)
    {
        if (pObject->m_pPrev == nullptr)
        {
            m_pFirst = pObject->m_pNext;
            m_begin.m_pCurrent = pObject->m_pNext;
        }
        else
        {
            pObject->m_pPrev->m_pNext = pObject->m_pNext;
        }
        Listable* pNext;
        if (pObject->m_pNext == nullptr)
        {
            pNext = m_end.m_pCurrent;
            m_pLast = pObject->m_pPrev;
        }
        else
        {
            pObject->m_pNext->m_pPrev = pObject->m_pPrev;
            pNext = pObject->m_pNext;
        }
        --m_numObjects;
        m_local.m_pCurrent = pNext;
        pObject->m_pPrev = nullptr;
        pObject->m_pNext = nullptr;
        return m_local;
    }

    // 0x00BF0B40 (:146)
    const InternalListBase::IteratorBase& InternalListBase::findBase(Listable* pObject)
    {
        for (Listable* pCurrent = m_begin.m_pCurrent; pCurrent != m_end.m_pCurrent; pCurrent = pCurrent->m_pNext)
        {
            if (pCurrent == pObject)
            {
                m_local.m_pCurrent = pCurrent;
                return m_local;
            }
        }
        m_local.m_pCurrent = m_end.m_pCurrent;
        return m_local;
    }

    // 0x00BF0B70 (:164)
    const InternalListBase::IteratorBase& InternalListBase::findBase(uint32 offset, uint32 key)
    {
        // offset is the byte offset of a uint32 key inside the object, counted from its Listable.
        for (Listable* pCurrent = m_begin.m_pCurrent; pCurrent != m_end.m_pCurrent; pCurrent = pCurrent->m_pNext)
        {
            if (*(const uint32*)((const uint8*)pCurrent + offset) == key)
            {
                m_local.m_pCurrent = pCurrent;
                return m_local;
            }
        }
        m_local.m_pCurrent = m_end.m_pCurrent;
        return m_local;
    }

    // 0x00BF0BB0 (:182)
    void InternalListBase::clear()
    {
        clearBase();
    }

    // 0x00BF0BD0 (:200)
    uint32 InternalListBase::getSize() const
    {
        return m_numObjects;
    }

    // 0x00BF0BE0 (:205)
    const InternalListBase::IteratorBase& InternalListBase::getLastBase() const
    {
        // m_local is scratch state; the original writes it from a const function.
        const_cast<InternalListBase*>(this)->m_local.m_pCurrent = m_pLast;
        return m_local;
    }
}
