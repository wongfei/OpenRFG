// Original: library/modules/runtime/base/include/keen/base/classidentifier.hpp
// Implementation: library/modules/runtime/base/sources/global/classidentifier.cpp
//
// ClassIdentifier, a lightweight run-time type id with a parent link (isDerivedFrom).
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    // size 0x8
    struct ClassIdentifier
    {
        const ClassIdentifier* pBaseClass; // 0x00
        const char* pClassName; // 0x04
    };

    bool isDerivedFrom(const ClassIdentifier& typeToCheck, const ClassIdentifier& referenceType); // 0x00BEF3B0 (:88)
}
