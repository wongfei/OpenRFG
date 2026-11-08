// Original: library/modules/runtime/sound/sources/global/soundsysteminternal.hpp
//
// SoundSystem::System, the sound system state (busses, sounds, voices, banks, listener), and its platform independent
// base. Placement inferred.
#pragma once

#include "keen/base/types.hpp"
#include "keen/base/array.hpp"
#include "keen/base/randomnumbergenerator.hpp"
#include "keen/base/singlelinkedinternallist.hpp"
#include "keen/math/matrix43.hpp"
#include "keen/sound/soundsystem.hpp"
#include "library/modules/runtime/sound/sources/global/sound.hpp"
#include "library/modules/runtime/sound/sources/global/soundprovidernull.hpp"
#include "library/modules/runtime/sound/sources/global/soundsystemstate.hpp"
#include "library/modules/runtime/sound/sources/global/temporarysounddefinition.hpp"
#include "library/modules/runtime/sound/sources/xaudio2/soundsystemstate_xaudio2.hpp"

namespace keen
{
    namespace SoundSystem
    {
        class SoundProvider;
    }
}

namespace keen
{
    namespace SoundSystem
    {
        // size 0x2510
        struct Systembase
        {
            Systembase(); // 0x00C36DF0
            ~Systembase(); // 0x00C36E40

            SoundProvider* soundProviders[8]; // 0x00
            Matrix43 listenerMatrix; // 0x20
            Matrix43 inverseListenerMatrix; // 0x50
            int nextPauseGroup; // 0x80
            uint32 allocatedPauseGroups[1]; // 0x84
            RandomNumberGenerator randomNumberGenerator; // 0x88
            Array<BaseSound> sounds; // 0xA4C
            Array<VoiceSound> voiceSounds; // 0xA54
            uint32 currentTimeInms; // 0xA5C
            Array<ParameterAutomation> automations; // 0xA60
            uint32 busCount; // 0xA68
            BusImplementation busses[30]; // 0xA6C
            EffectStorage effects[16]; // 0x1C3C
            Array<TemporarySoundDefinition> tempSoundDefinitions; // 0x24FC
            Array<SoundTriggerMemory> repetitionMemory; // 0x2504
            bool soundOutputDisabled; // 0x250C
        };

        // size 0x2850
        struct System : public Systembase
        {
            System(); // 0x00C39120

            SystemXaudio2 xaudio2State; // 0x2510
            SoundProviderNull nullProvider; // 0x2840
            SingleLinkedInternalList<SoundDeviceChangedCallback,SoundDeviceChangedCallback*,4> deviceChangedCallbacks; // 0x2844
        };

    }
}
