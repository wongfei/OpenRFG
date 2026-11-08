// Original: library/modules/runtime/sound/include/keen/sound/fxdefinitionreverb.hpp
//
// The reverb effect definition and parameters. Placement inferred.
#pragma once

#include "keen/base/types.hpp"
#include "keen/sound/soundsystem.hpp"

namespace keen
{
    namespace SoundSystem
    {
        // size 0x2C
        struct FXReverbParameter
        {
            enum
            {
                TypeSignatureCrc = -1549585060,
            };

            static uint32 getAlignment();

            float room; // 0x00
            float roomHf; // 0x04
            float decayTime; // 0x08
            float decayHfRatio; // 0x0C
            float reflections; // 0x10
            float reflectionsDelay; // 0x14
            float reverb; // 0x18
            float reverbDelay; // 0x1C
            float diffusion; // 0x20
            float density; // 0x24
            float hfReference; // 0x28
        };

        // size 0x34
        struct FXDefinitionReverb : public BaseEffectDefinition
        {
            FXDefinitionReverb();

            FXReverbParameter parameter; // 0x08
        };

    }
}
