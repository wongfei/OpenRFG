// Original: library/modules/runtime/base/sources/global/inivariablelist.hpp
// Implementation: library/modules/runtime/base/sources/global/inivariablelist.cpp
//
// IniVariableList, the global list of registered ini variables.
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    class IniVariableBase;

    // size 0x8
    class IniVariableList
    {
    public:
        IniVariableList(); // 0x00BF0760 (:10)
        ~IniVariableList(); // 0x00BF0770 (:16)
        void insert(IniVariableBase* pVariable); // 0x00BF0780 (:20)
        void remove(IniVariableBase* pVariable); // 0x00BF07A0 (:36)
        void clear(); // 0x00BF07E0 (:58)
        uint32 getSize() const; // 0x00BF0740 (.h:22)
        IniVariableBase* getHead(); // 0x00BF0750 (.h:24)
        IniVariableBase* getNext(IniVariableBase* pVariable); // 0x00BF07F0 (:64)

    private:
        IniVariableBase* m_pListHead; // 0x00
        uint32 m_size; // 0x04
    };
}
