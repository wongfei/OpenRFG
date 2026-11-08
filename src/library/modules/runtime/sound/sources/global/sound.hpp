// Original: library/modules/runtime/sound/sources/global/sound.hpp
// Implementation: library/modules/runtime/sound/sources/global/sound.cpp
//
// Playing sounds: BaseSound and VoiceSound (a sound on a voice), the sound list and flags. Placement of SoundFlag and
// SoundList inferred.
#pragma once

#include "keen/base/types.hpp"
#include "keen/base/staticarray.hpp"
#include "keen/sound/sounddefinition.hpp"

namespace keen
{
    namespace SoundSystem
    {
        struct BaseSound;
        struct ParameterAutomation;
        struct PositionedSoundData;
    }
}

namespace keen
{
    struct Matrix43;
    struct SoundDefinitionResourceHandleType;
    struct SoundFileData;

    enum SoundFlag
    {
        SoundFlag_Loop = 1,
        SoundFlag_StartPaused = 2,
        SoundFlag_SynchronousStream = 4,
        SoundFlag_StartSilent = 8,
    };

    namespace SoundSystem
    {
        // size 0xC
        struct VoiceSound
        {
            enum
            {
                RunFlag_FadeOutAndStop = 1,
                RunFlag_KeyOnRequested = 4,
                RunFlag_Paused = 8,
            };

            VoiceSound(); // 0x00C32A70 (:12)
            bool isUsed() const; // 0x00C32A80 (:19)
            void free(); // 0x00C32A90 (:24)
            void soundEndedAndFree(); // 0x00C32AB0 (:34)

            uint8 m_providerType; // 0x00
            uint8 m_runFlags; // 0x01
            void* m_pProviderData; // 0x04
            BaseSound* m_pBaseSound; // 0x08
        };

        // size 0x30
        struct BaseSound
        {
            enum Flag
            {
                Flag_Looped = 32768,
                Flag_KeyOn = 16384,
                Flag_Mask = 49152,
                Flag_RunCountMask = 16383,
            };

            BaseSound(); // 0x00C32AD0 (:218)
            bool isUsed() const; // 0x00C328B0 (.h:57)
            const SoundDefinition* getDefinition() const; // 0x00C328C0 (.h:63)

            float3 position; // 0x00
            uint16 runCountAndFlags; // 0x0C
            uint8 pauseGroup; // 0x0E
            uint8 voiceIndex; // 0x0F
            float volume; // 0x10
            float pitch; // 0x14
            const SoundDefinitionResourceHandleType* definition; // 0x18
            const SoundFileData* pSoundFileData; // 0x1C
            uint32 startTimeInms; // 0x20
            ParameterAutomation* pAutomation; // 0x24
            float initialVolume; // 0x28
            float initialPitch; // 0x2C
            static const uint8 MaxVoiceSoundIndex; // 0x01340B23
            static const uint8 InvalidVoiceSoundEnded; // 0x01340B24
            static const uint8 InvalidVoice; // 0x01340B25
        };

    }

    // size 0x8
    struct SoundList
    {
        enum
        {
            TypeSignatureCrc = 0x2AD67350,
        };

        static uint32 getAlignment();

        StaticArray<SoundSystem::SoundDefinition> sounds; // 0x00
    };

    namespace SoundSystem
    {
        void calculateAttenuationAndAngle(PositionedSoundData* pData, const BaseSound* pSound, const Matrix43& hrtfMatrix); // 0x00C34BE0 (:44)
        void calculateStereoVolumes(float* pOutput, float volume, float panAngle, float innerDistance); // 0x00C34EE0 (:77)
        void calculate5point1Volumes(float* pOutput, float volume, float panAngle); // 0x00C350D0 (:113)
        void calculate7point1Volumes(float* pOutput, float volume, float panAngle); // 0x00C35420 (:160)
    }
}
