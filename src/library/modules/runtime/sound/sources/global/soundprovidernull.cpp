// Original: library/modules/runtime/sound/sources/global/soundprovidernull.cpp
//
// SoundProviderNull, the provider without an audio device (SFX.md SX1): starting a sound fails, so the virtual sound is
// freed at once (updateBase). SoundSystem::initialize falls back to it when XAudio2 2.7 cannot be created.
// SoundProvider's own members (soundprovider.hpp, inline in rfg.exe) are defined here too.

#include "library/modules/runtime/sound/sources/global/soundprovidernull.hpp"

namespace keen
{
    namespace SoundSystem
    {
        // 0x00C32F80
        SoundProvider::SoundProvider()
        {
        }

        // 0x00C32BD0 (soundprovider.hpp:16)
        SoundProvider::~SoundProvider()
        {
        }

        // 0x00C32BE0 (soundprovider.hpp:24)
        int SoundProvider::getNumAllocatedElements()
        {
            return -1;
        }

        // 0x00C35B00 (:9)
        SoundProviderNull::SoundProviderNull()
        {
        }

        // 0x00C32F90 (:13)
        SoundProviderNull::~SoundProviderNull()
        {
        }

        // 0x00C32FA0 (:18)
        void SoundProviderNull::create()
        {
        }

        // 0x00C32FB0 (:22)
        void SoundProviderNull::destroy()
        {
        }

        // 0x00C32FC0 (:26)
        bool SoundProviderNull::startSound(VoiceSound* pSound, float sampleRate, uint32 flags, System* pSoundSystem)
        {
            (void)pSound;
            (void)sampleRate;
            (void)flags;
            (void)pSoundSystem;
            return false;
        }

        // 0x00C32FD0 (:36)
        void SoundProviderNull::stopSound(VoiceSound* pSound, System* pSoundSystem)
        {
            (void)pSound;
            (void)pSoundSystem;
        }

        // 0x00C32FE0 (:42)
        void SoundProviderNull::pauseSound(VoiceSound* pSound)
        {
            (void)pSound;
        }

        // 0x00C32FF0 (:47)
        void SoundProviderNull::resumeSound(VoiceSound* pSound)
        {
            (void)pSound;
        }

        // 0x00C33000 (:52)
        void SoundProviderNull::update(VoiceSound* pSounds, uint32 numSounds, System* pSoundSystem)
        {
            (void)pSounds;
            (void)numSounds;
            (void)pSoundSystem;
        }

        // 0x00C33010 (:59)
        int SoundProviderNull::getNumAllocatedElements()
        {
            return 0;
        }
    }
}
