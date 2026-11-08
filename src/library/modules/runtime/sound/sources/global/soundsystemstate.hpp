// Original: library/modules/runtime/sound/sources/global/soundsystemstate.hpp
//
// Sound system state records: bus implementations, parameter automation, positioned sounds and trigger memory.
// Placement inferred.
#pragma once

#include "keen/base/types.hpp"
#include "keen/sound/soundsystem.hpp"

namespace keen
{
    namespace SoundSystem
    {
        struct SoundDefinition;
    }
}

namespace keen
{
    namespace SoundSystem
    {
        // size 0x14
        struct ParameterAutomation
        {
            enum Command
            {
                Command_Nop = 0,
                Command_FadeVolume = 1,
                Command_PitchBend = 2,
                Command_FadeoutAndStop = 3,
            };

            ParameterAutomation* pNext; // 0x00
            Command command; // 0x04
            bool init; // 0x08
            float targetValue; // 0x0C
            float velocity; // 0x10
        };

        // size 0x98
        struct BusImplementation
        {
            VolumeDefinition gain; // 0x00
            VolumeDefinition drySendVolumes[4]; // 0x0C
            VolumeDefinition wetSendVolumes[4]; // 0x3C
            BusDefinition definition; // 0x6C
            VolumeDefinition duckingGain; // 0x7C
            static const uint32 emptyEffectIndex; // 0x01340B2C
            uint32 effectIndices[4]; // 0x88
        };

        // size 0xC
        struct PositionedSoundData
        {
            float attenuation; // 0x00
            float innerDistance; // 0x04
            float angle; // 0x08
        };

        // size 0x10
        struct SoundTriggerMemory
        {
            const SoundDefinition* pSoundDefinition; // 0x00
            uint32 lastTriggerTimeInms; // 0x04
            uint8 lastTriggeredIndex[8]; // 0x08
            static const uint8 UnusedLastTriggeredIndex; // 0x01340B30
        };

    }
}
