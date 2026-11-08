// Original: library/modules/runtime/sound/sources/xaudio2/soundsystemstate_xaudio2.hpp
// Implementation: library/modules/runtime/sound/sources/xaudio2/soundsystem_xaudio2.cpp
//
// The XAudio2 sound system: SystemXaudio2 (engine, mastering voice, submix busses, effects) and the engine callback;
// soundsystem_xaudio2.cpp (create/shutdown, busses, effect chains, device selection) has no header of its own.
// Placement of SystemXaudio2 and the functions inferred.
#pragma once

#include "keen/base/types.hpp"
#include <windows.h>
#include <xaudio2.h>
#include <xaudio2fx.h>
#include "library/modules/runtime/sound/sources/xaudio2/soundproviderwave_xaudio2.hpp"
#include "library/modules/runtime/sound/sources/xaudio2/soundvoice_xaudio2.hpp"

struct IXAudio2;
struct IXAudio2MasteringVoice;
struct IXAudio2SubmixVoice;
struct XAUDIO2FX_REVERB_PARAMETERS;

namespace keen
{
    namespace SoundSystem
    {
        struct FXReverbParameter;
        struct InitializationParameters;
        struct System;
        struct SystemXaudio2;
    }
}

namespace keen
{
    class MemoryAllocator;

    namespace SoundSystem
    {
        // size 0x8
        class SoundSystemEngineCallback : public IXAudio2EngineCallback
        {
        public:
            SoundSystemEngineCallback(); // 0x00C359A0 (.h:17)
            void create(SystemXaudio2* pSoundSystem); // 0x00C32CB0 (.h:18)
        private:
            virtual void __stdcall OnProcessingPassEnd(); // vtbl+0x04 0x00C359B0 (.h:21)
            virtual void __stdcall OnProcessingPassStart(); // vtbl+0x00 0x00C38B40 (soundsystem_xaudio2.cpp:28)
            virtual void __stdcall OnCriticalError(HRESULT); // vtbl+0x08 0x00C36F30 (soundsystem_xaudio2.cpp:37)

            SystemXaudio2* m_pSystem; // 0x04
        };

        // size 0x330
        struct SystemXaudio2
        {
            SystemXaudio2(); // 0x00C38AF0 (soundsystem_xaudio2.cpp:16)
            ~SystemXaudio2(); // 0x00C38950

            IXAudio2* pAudioDevice; // 0x00
            IXAudio2MasteringVoice* pMasteringVoice; // 0x04
            SoundVoiceManagerXAudio2 voiceManager; // 0x08
            SoundProviderWaveXAudio2 waveProvider; // 0x14
            SoundSystemEngineCallback callbackWrapper; // 0x20
            uint32 channelCount; // 0x28
            uint32 masterVoiceSampleRate; // 0x2C
            uint32 reverbOutputChannels; // 0x30
            IXAudio2SubmixVoice* pBusVoices[30]; // 0x34
            IXAudio2SubmixVoice* pBusFXVoices[30]; // 0xAC
            wchar_t currentDeviceID[256]; // 0x124
            float timeUntilDeviceSelection; // 0x324
            uint32 externalXaudio2Voices; // 0x328
            bool deviceUpdatePending; // 0x32C
            bool isInDeviceChangeCallback; // 0x32D
        };

    }

    namespace SoundSystem
    {
        bool create(SystemXaudio2* pXaudioSoundSystem, MemoryAllocator* pAllocator, const InitializationParameters& parameter, uint32 waveProviderId); // 0x00C392F0 (soundsystem_xaudio2.cpp:125)
        void shutdown(SystemXaudio2* pXaudioSoundSystem, MemoryAllocator* pAllocator); // 0x00C39380 (soundsystem_xaudio2.cpp:150)
        void updateDeviceSelection(System* pSoundSystem, float timeStep); // 0x00C39410 (soundsystem_xaudio2.cpp:188)
        void createBusses(System* pSoundSystem); // 0x00C33E90 (soundsystem_xaudio2.cpp:285)
        void destroyBusses(System* pSoundSystem); // 0x00C340B0 (soundsystem_xaudio2.cpp:324)
        void updatePlatformBus(System* pSystem, uint32 busId); // 0x00C37080 (soundsystem_xaudio2.cpp:343)
        void createPlatformReverbParameter(XAUDIO2FX_REVERB_PARAMETERS* pResult, const FXReverbParameter& input); // 0x00C34110 (soundsystem_xaudio2.cpp:394)
        void createPlatformEffectChain(System* pSystem, uint32 busId); // 0x00C341B0 (soundsystem_xaudio2.cpp:420)
        void updatePlatformEffectChain(System* pSystem, uint32 busId); // 0x00C34430 (soundsystem_xaudio2.cpp:526)
        void setPlatformMasterLimiter(System*, float, float, float, float); // 0x00C344D0 (soundsystem_xaudio2.cpp:555)
        void externalXaudio2VoiceCreated(System* pSoundSystem); // 0x00C344E0 (soundsystem_xaudio2.cpp:560)
        void externalXaudio2VoiceDestroyed(System* pSoundSystem); // 0x00C344F0 (soundsystem_xaudio2.cpp:565)
    }
}
