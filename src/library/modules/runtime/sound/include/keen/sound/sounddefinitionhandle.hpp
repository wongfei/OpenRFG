// Original: library/modules/runtime/sound/include/keen/sound/sounddefinitionhandle.hpp
//
// The ResourceHandle tag of sound definitions.
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    // size 0x1
    struct SoundDefinitionResourceHandleType
    {
        enum
        {
            TypeSignatureCrc = 0x439EECA6,
        };

        static uint32 getAlignment();

    };
}
