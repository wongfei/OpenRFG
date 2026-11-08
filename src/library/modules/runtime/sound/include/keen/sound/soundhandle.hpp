// Original: library/modules/runtime/sound/include/keen/sound/soundhandle.hpp
//
// The ResourceHandle tag of sound files. Placement inferred.
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    // size 0x1
    struct SoundFileHandleType
    {
        enum
        {
            TypeSignatureCrc = 0x6B5F989C,
        };

        static uint32 getAlignment();

    };
}
