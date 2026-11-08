// Original: library/modules/runtime/base/sources/global/inivariablelist.cpp
//
// Partial: construction, clear and iteration. Not ported: insert (0x00BF0780), remove (0x00BF07A0); nothing registers
// ini variables yet.

#include "library/modules/runtime/base/sources/global/inivariablelist.hpp"
#include "keen/base/inivariables.hpp"

namespace keen
{
    // 0x00BF0760 (:10)
    IniVariableList::IniVariableList()
    {
        m_pListHead = nullptr;
        m_size = 0;
    }

    // 0x00BF0770 (:16)
    IniVariableList::~IniVariableList()
    {
    }

    // 0x00BF07E0 (:58)
    void IniVariableList::clear()
    {
        m_pListHead = nullptr;
        m_size = 0;
    }

    // 0x00BF0740 (.h:22)
    uint32 IniVariableList::getSize() const
    {
        return m_size;
    }

    // 0x00BF0750 (.h:24)
    IniVariableBase* IniVariableList::getHead()
    {
        return m_pListHead;
    }

    // 0x00BF07F0 (:64)
    IniVariableBase* IniVariableList::getNext(IniVariableBase* pVariable)
    {
        return pVariable->m_pNext;
    }
}
