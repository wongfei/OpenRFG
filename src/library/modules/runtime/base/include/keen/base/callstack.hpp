// Original: library/modules/runtime/base/include/keen/base/callstack.hpp
// Implementation: library/modules/runtime/base/sources/global/callstack.cpp
//
// CallStack, a captured call stack (return addresses) for debugging.
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    // size 0x80
    struct CallStack
    {
        enum
        {
            MaxEntryCount = 32,
        };

        uint32 address[32]; // 0x00
    };

    bool operator!=(const CallStack& lhs, const CallStack& rhs); // 0x00BEF2D0 (:7)
}
