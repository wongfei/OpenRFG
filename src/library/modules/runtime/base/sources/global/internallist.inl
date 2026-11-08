// Inline part of library/modules/runtime/base/include/keen/base/internallist.hpp (included by it): the iterator
// operators of InternalListBase and the InternalList<T> front end.
//
// Partial: the InternalList<T> members used by the recovered code so far.
#pragma once

namespace keen
{
    // 0x00BEF300 (.h:69)
    inline InternalListBase::IteratorBase::IteratorBase()
    {
        m_pCurrent = nullptr;
    }

    // 0x004B2100 (.h:70)
    inline InternalListBase::IteratorBase::IteratorBase(const IteratorBase& other)
    {
        m_pCurrent = other.m_pCurrent;
    }

    // 0x004B2110 (.h:72)
    inline InternalListBase::IteratorBase& InternalListBase::IteratorBase::operator++()
    {
        if (m_pCurrent != nullptr)
        {
            m_pCurrent = m_pCurrent->m_pNext;
        }
        return *this;
    }

    // 0x004B2120 (.h:73)
    inline InternalListBase::IteratorBase& InternalListBase::IteratorBase::operator--()
    {
        if (m_pCurrent != nullptr)
        {
            m_pCurrent = m_pCurrent->m_pPrev;
        }
        return *this;
    }

    // 0x004B2130 (.h:74)
    inline bool InternalListBase::IteratorBase::operator==(const IteratorBase& other) const
    {
        return other.m_pCurrent == m_pCurrent;
    }

    // 0x004B2140 (.h:75)
    inline bool InternalListBase::IteratorBase::operator!=(const IteratorBase& other) const
    {
        return m_pCurrent != other.m_pCurrent;
    }

    // 0x004B2150 (.h:76)
    inline InternalListBase::IteratorBase& InternalListBase::IteratorBase::operator=(const IteratorBase& other)
    {
        m_pCurrent = other.m_pCurrent;
        return *this;
    }

    // 0x004B2160 (.h:77)
    inline InternalListBase::Listable& InternalListBase::IteratorBase::operator*() const
    {
        return *m_pCurrent;
    }

    // 0x00BEF310 (.h:97)
    inline InternalListBase::Listable* InternalListBase::backBase()
    {
        return m_pLast;
    }

    // 0x004B2170 (.h:102)
    inline const InternalListBase::IteratorBase& InternalListBase::getStartBase() const
    {
        return m_begin;
    }

    // 0x004B2180 (.h:103)
    inline const InternalListBase::IteratorBase& InternalListBase::getEndBase() const
    {
        return m_end;
    }

    // 0x00BEF320 (.h:111)
    inline void InternalListBase::clearBase()
    {
        m_end.m_pCurrent = nullptr;
        m_begin.m_pCurrent = nullptr;
        m_pLast = nullptr;
        m_pFirst = nullptr;
        m_numObjects = 0;
    }

    // <GSO entry> 0x00C60EE0 (.h:179)
    template<typename T>
    InternalList<T>::Iterator::Iterator()
    {
    }

    // <GSO entry> 0x00C60680 (.h:180)
    template<typename T>
    InternalList<T>::Iterator::Iterator(const Iterator& other)
        : InternalListBase::IteratorBase(other)
    {
    }

    // <GSO entry> 0x00C60D50 (.h:181)
    template<typename T>
    T& InternalList<T>::Iterator::operator*() const
    {
        return *(T*)m_pCurrent;
    }

    // <GSO entry> 0x00C60690 (.h:182)
    template<typename T>
    T* InternalList<T>::Iterator::operator->() const
    {
        return (T*)m_pCurrent;
    }

    // <GSO entry> 0x00C60C30
    template<typename T>
    typename InternalList<T>::Iterator& InternalList<T>::Iterator::operator=(const Iterator& other)
    {
        InternalListBase::IteratorBase::operator=(other);
        return *this;
    }

    template<typename T>
    typename InternalList<T>::Iterator& InternalList<T>::Iterator::operator=(const InternalListBase::IteratorBase& other)
    {
        InternalListBase::IteratorBase::operator=(other);
        return *this;
    }

    // <GSO entry> 0x00C60D30 (.h:192)
    template<typename T>
    InternalList<T>::ConstIterator::ConstIterator(const ConstIterator& other)
        : InternalListBase::IteratorBase(other)
    {
    }

    // <GSO entry> 0x00C60D40 (.h:193)
    template<typename T>
    InternalList<T>::ConstIterator::ConstIterator(const Iterator& other)
        : InternalListBase::IteratorBase(other)
    {
    }

    template<typename T>
    InternalList<T>::ConstIterator::ConstIterator()
    {
    }

    template<typename T>
    const T& InternalList<T>::ConstIterator::operator*() const
    {
        return *(const T*)m_pCurrent;
    }

    // <GSO entry> 0x00C60670 (.h:195)
    template<typename T>
    const T* InternalList<T>::ConstIterator::operator->() const
    {
        return (const T*)m_pCurrent;
    }

    // <ChunkedSizedArray<AllocationEntry>::Chunk> 0x00CDD440 (.h:196)
    template<typename T>
    typename InternalList<T>::ConstIterator& InternalList<T>::ConstIterator::operator=(const InternalListBase::IteratorBase& other)
    {
        InternalListBase::IteratorBase::operator=(other);
        return *this;
    }

    // <GSO entry> 0x00C60CB0 (.h:171)
    template<typename T>
    InternalList<T>::InternalList()
    {
    }

    // <GSO entry> 0x00C60C20
    template<typename T>
    InternalList<T>::~InternalList()
    {
    }

    // <GSO entry> 0x00C60CC0 (.h:213)
    template<typename T>
    void InternalList<T>::pushFront(T* pObject)
    {
        pushFrontBase(pObject);
    }

    // <ChunkedListBase::ListNode> 0x00BF3090 (.h:214)
    template<typename T>
    void InternalList<T>::pushBack(T* pObject)
    {
        pushBackBase(pObject);
    }

    // <GSO entry> 0x00C60CD0 (.h:215)
    template<typename T>
    const typename InternalList<T>::Iterator& InternalList<T>::insert(T* pObject)
    {
        return (const Iterator&)insertBase(pObject);
    }

    // <GSO entry> 0x00C60CE0 (.h:217)
    template<typename T>
    const typename InternalList<T>::Iterator& InternalList<T>::insert(T* pObject, InternalListBase::IteratorBase previous)
    {
        return (const Iterator&)insertBase(pObject, &*previous);
    }

    // <GSO entry> 0x00C60D80 (.h:218)
    template<typename T>
    const typename InternalList<T>::Iterator& InternalList<T>::erase(T* pObject)
    {
        return (const Iterator&)eraseBase(pObject);
    }

    // <GSO entry> 0x00C6A410 (.h:219)
    template<typename T>
    const typename InternalList<T>::Iterator& InternalList<T>::erase(Iterator it)
    {
        return (const Iterator&)eraseBase(&*it);
    }

    // <GSO entry> 0x00C60D00 (.h:226)
    template<typename T>
    const typename InternalList<T>::Iterator& InternalList<T>::getBegin()
    {
        return (const Iterator&)getStartBase();
    }

    template<typename T>
    const typename InternalList<T>::ConstIterator& InternalList<T>::getBegin() const
    {
        return (const ConstIterator&)getStartBase();
    }

    // <GSO entry> 0x00C60D10 (.h:228)
    template<typename T>
    const typename InternalList<T>::Iterator& InternalList<T>::getEnd()
    {
        return (const Iterator&)getEndBase();
    }

    // <GSO entry> 0x00C60D20 (.h:232)
    template<typename T>
    const typename InternalList<T>::ConstIterator& InternalList<T>::getEnd() const
    {
        return (const ConstIterator&)getEndBase();
    }
}
