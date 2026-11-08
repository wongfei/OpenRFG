// Original: library/modules/runtime/resources/include/keen/resources/resourcehandle.hpp
//
// Weak resource references (by key, resolved when the resource is loaded). Placement inferred.
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    // size 0xC
    struct WeakResourceReference
    {
        enum
        {
            TypeSignatureCrc = -762034832,
        };

        static uint32 getAlignment();

        uint32 resourceId; // 0x00
        uint32 resourceType; // 0x04
        const char* pResourceName; // 0x08
    };

    // size 0x4
    struct WeakResourceReferenceHolder
    {
        enum
        {
            TypeSignatureCrc = -951434524,
        };

        static uint32 getAlignment();

        uint32 referenceIndex; // 0x00
    };
}
