// Original: library/modules/runtime/sound/include/keen/sound/soundsystem.hpp
// Implementation: library/modules/runtime/sound/sources/global/soundsystem.cpp
//
// The keen sound system API (namespace SoundSystem): initialization, playing sounds from definitions (2D/3D
// parameters), busses with volumes and effect chains (reverb, ducker), listener updates and device changes.
// GameFramework and rfg::Application create it; Volition audio_lib receives the System through rfg_set_SoundSystem.
// Placement of the bus/effect records inferred.
#pragma once

#include "keen/base/types.hpp"

struct IXAudio2;

namespace keen
{
    namespace SoundSystem
    {
        struct BaseEffectDefinition;
        struct BaseSound;
        struct BusDefinition;
        struct SoundDefinition;
        class SoundProvider;
        class StreamInterface;
        struct System;
    }
}

namespace keen
{
    struct Matrix43;
    class MemoryAllocator;
    class RandomNumberGenerator;
    struct SoundDefinitionResourceHandleType;
    struct SoundFileData;
    struct SoundFileHandleType;
    struct Vector3;

    namespace SoundSystem
    {
        enum Positioning
        {
            Positioning_listener = 0,
            Positioning_world = 1,
        };

        // size 0xC
        struct SoundDefinitionParameter3d
        {
            SoundDefinitionParameter3d(); // 0x00482F10 (.h:166)

            float minDistance; // 0x00
            float maxDistance; // 0x04
            float positionVariation; // 0x08
        };

        // size 0x20
        struct InitializationParametersBase
        {
            InitializationParametersBase(); // 0x00C33020 (:49)

            uint32 virtualSoundCount; // 0x00
            uint32 streamCount; // 0x04
            uint32 voiceCount; // 0x08
            StreamInterface* pStreamInterface; // 0x0C
            BusDefinition* pBusses; // 0x10
            uint32 busCount; // 0x14
            uint32 temporarySoundDefinitionCount; // 0x18
            uint32 repetitionMemoryMaxCount; // 0x1C
        };

        // size 0x20
        struct InitializationParameters : public InitializationParametersBase
        {
            InitializationParameters(); // 0x00458AB0

        };

        // size 0x20
        struct EffectChainSetup
        {
            EffectChainSetup(); // 0x00C35B30 (:62)

            BaseEffectDefinition* pDefinitions[4]; // 0x00
            float gains[4]; // 0x10
        };

        // size 0xC
        struct VolumeDefinition
        {
            float currentValue; // 0x00
            float targetValue; // 0x04
            float speed; // 0x08
        };

        // size 0x10
        struct BusDefinition
        {
            static const int masterOutBus; // 0x012B7830
            static const int nirvana; // 0x012B7834
            int targetBusses[4]; // 0x00
        };

        // size 0x10
        struct SoundDefinitionParameter2d
        {
            SoundDefinitionParameter2d();

            float centerFraction; // 0x00
            float panAngle; // 0x04
            float panDistance; // 0x08
            float panAngleVariation; // 0x0C
        };

        // size 0x14
        struct FXDuckerParameter
        {
            enum
            {
                TypeSignatureCrc = 0x495347E7,
            };

            static uint32 getAlignment();

            uint32 duckingBus; // 0x00
            float threshold; // 0x04
            float gain; // 0x08
            float attack; // 0x0C
            float release; // 0x10
        };

        // size 0x8
        struct BaseEffectDefinition
        {
            uint32 effectId; // 0x00
            uint32 parameterSize; // 0x04
            static const uint32 empty; // 0x012B7838
        };

        // size 0x8C
        struct EffectStorage
        {
            float gain; // 0x00
            uint32 effectId; // 0x04
            char opaqueParameterData[128]; // 0x08
            void* pPlatformData; // 0x88
        };

    }

    // size 0x8
    class SoundDeviceChangedCallback
    {
    public:
        virtual void handleAboutToDestroyMasteringVoice(SoundSystem::System*) = 0; // vtbl+0x00
        virtual void handleMasteringVoiceCreated(SoundSystem::System*) = 0; // vtbl+0x04
        SoundDeviceChangedCallback(); // 0x00CD0450

        // 0x00 vfptr
        SoundDeviceChangedCallback* m_pNext; // 0x04
    };

    namespace SoundSystem
    {
        float calculateEnergyFromDecibel(float decibel); // 0x00C33050 (:74)
        void initializeBase(System* pSystem, MemoryAllocator* pMemoryAllocator, const InitializationParametersBase& params); // 0x00C39740 (:301)
        void shutdownBase(System* pSystem, MemoryAllocator* pAllocator); // 0x00C39B10 (:376)
        void updateBase(System* pSystem, float timeStep); // 0x00C38F90 (:523)
        void registerSoundProvider(System* pSystem, uint8 providerId, SoundProvider* pProvider); // 0x00C33270 (:640)
        uint32 startSound(System* pSystem, const SoundDefinitionResourceHandleType* const sound, float panning, float frontBack, uint32 soundFlags); // 0x00C37DD0 (:860)
        uint32 startSound(System* pSystem, const SoundDefinitionResourceHandleType* const sound, uint32 soundFlags); // 0x00C37E50 (:876)
        uint32 startSound(System* pSystem, const SoundDefinitionResourceHandleType* const sound, const Vector3& position, uint32 soundFlags); // 0x00C37EC0 (:893)
        bool stopSound(System* pSystem, uint32 sound, float fadeoutTime); // 0x00C37F60 (:969)
        void stopAllSounds(System* pSystem); // 0x00C35F00 (:989)
        void stopAllSounds(System* pSystem, uint8 resourceTag); // 0x00C35F60 (:1002)
        bool setPitch(System* pSystem, uint32 handle, float pitch, float duration); // 0x00C37FC0 (:1037)
        bool setVolume(System* pSystem, uint32 sound, float volume, float duration); // 0x00C38080 (:1079)
        float getVolume(System* pSystem, uint32 sound); // 0x00C38180 (:1128)
        bool isSoundPlaying(System* pSystem, uint32 handle); // 0x00C381C0 (:1147)
        bool setPanning(System* pSystem, uint32 handle, float panning); // 0x00C38200 (:1159)
        void setListenerPosition(System* pSystem, const Matrix43& worldMatrix); // 0x00C33350 (:1183)
        const Matrix43& getListenerPosition(const System* pSystem); // 0x00C333A0 (:1189)
        bool setSoundPosition(System* pSystem, uint32 handle, const Vector3& position); // 0x00C38250 (:1203)
        uint8 pauseAllPlayingSounds(System* pSystem); // 0x00C382D0 (:1231)
        void resumeSounds(System* pSystem, uint8 pauseGroup); // 0x00C38350 (:1258)
        void stopPausedSounds(System* pSystem, uint8 pauseGroup); // 0x00C35FD0 (:1287)
        bool pauseSound(System* pSystem, uint32 handle); // 0x00C383F0 (:1306)
        bool resumeSound(System* pSystem, uint32 handle); // 0x00C38460 (:1317)
        const SoundFileData* getDataFromSound(RandomNumberGenerator* pRandomizer, const SoundDefinition& sound, System* pSoundSystem); // 0x00C360D0 (:1362)
        uint32 getSoundHandle(System* pSoundSystem, BaseSound* pSound); // 0x00C36540 (:1536)
        BaseSound* findBaseSoundByHandle(System* pSystem, uint32 handle); // 0x00C36590 (:1552)
        BaseSound* findFreeBaseSound(System* pSoundSystem, signed char priority, signed char countGroup, uint8 maxCount, const SoundDefinitionResourceHandleType* definition); // 0x00C365D0 (:1570)
        const SoundDefinitionResourceHandleType* createDynamicSoundDefinition(System* pSystem, Positioning positioning, const SoundDefinitionParameter2d* p2dDefinitionParam, const SoundDefinitionParameter3d* p3dDefinitionParam, const SoundFileHandleType* soundFile, uint32 busId, signed char countGroup, uint8 maxCount, uint8 minDelay, signed char priority); // 0x00C36700 (:1659)
        const SoundDefinitionResourceHandleType* createDynamicSoundDefinition2d(System* pSystem, const SoundFileHandleType* soundFile, uint32 busId, const SoundDefinitionParameter2d* pDefinitionParam, signed char priority, signed char countGroup, uint8 maxCount, uint8 minDelay); // 0x00C368C0 (:1733)
        const SoundDefinitionResourceHandleType* createDynamicSoundDefinition3d(System* pSystem, const SoundFileHandleType* soundFile, uint32 busId, const SoundDefinitionParameter3d* pDefinitionParam, signed char priority, signed char countGroup, uint8 maxCount, uint8 minDelay); // 0x00C36900 (:1738)
        void destroyDynamicSoundDefinition(System* pSystem, const SoundDefinitionResourceHandleType* definition); // 0x00C36940 (:1743)
        float getSoundDuration(System* pSystem, const SoundDefinitionResourceHandleType* soundDefHandle); // 0x00C36990 (:1763)
        float getSoundDuration(System* pSystem, uint32 handle); // 0x00C369E0 (:1771)
        void setBusSendVolume(System* pSystem, uint32 busId, uint32 sendIndex, float drySend, float wetSend, float timeDelta); // 0x00C333F0 (:1796)
        void setBusGain(System* pSystem, uint32 busId, float volume, float timeDelta); // 0x00C334B0 (:1806)
        void setBusEffectChain(System* pSystem, uint32 busId, const EffectChainSetup& setup, float timeDelta); // 0x00C36A50 (:1880)
        void setMasterLimiter(System* pSystem, float threshold, float ratio, float attack, float release); // 0x00C36D60 (:1988)
        void enableSystemBGMusic(System* pSoundSystem); // 0x00C335B0 (:2001)
        void disableSystemBGMusic(System* pSoundSystem); // 0x00C335C0 (:2006)
        System* initialize(MemoryAllocator* pAllocator, const InitializationParameters& parameter); // 0x00C39C00 (soundsystem_win32.cpp:31)
        void shutdown(System* pSoundSystem, MemoryAllocator* pAllocator); // 0x00C39D20 (soundsystem_win32.cpp:80)
        void update(System* pSystem, float timeStep); // 0x00C39D70 (soundsystem_win32.cpp:95)
        IXAudio2* getXAudio2(System* pSoundSystem); // 0x00C339F0 (soundsystem_win32.cpp:110)
        bool isXaudio2Working(System* pSoundSystem); // 0x00C33A10 (soundsystem_win32.cpp:115)
        bool isInDeviceChangeCallback(System* pSoundSystem); // 0x00C33A30 (soundsystem_win32.cpp:120)
        void registerSoundDeviceChangedCallback(System* pSoundSystem, SoundDeviceChangedCallback* pCallback); // 0x00C38960 (soundsystem_win32.cpp:125)
        void unregisterSoundDeviceChangedCallback(System* pSoundSystem, SoundDeviceChangedCallback* pCallback); // 0x00C38980 (soundsystem_win32.cpp:130)
    }
}
