// Original: library/modules/runtime/sound/sources/global/soundprovidernull.hpp
// Implementation: library/modules/runtime/sound/sources/global/soundprovidernull.cpp
//
// SoundProviderNull, a provider that produces nothing (no audio device).
#pragma once

#include "keen/base/types.hpp"
#include "library/modules/runtime/sound/sources/global/soundprovider.hpp"

namespace keen
{
    namespace SoundSystem
    {
        struct System;
        struct VoiceSound;
    }
}

namespace keen
{
    namespace SoundSystem
    {
        // size 0x4
        class SoundProviderNull : public SoundProvider
        {
        public:
            SoundProviderNull(); // 0x00C35B00 (:9)
            virtual ~SoundProviderNull(); // vtbl+0x00 0x00C32F90 (:13)
            void create(); // 0x00C32FA0 (:18)
            void destroy(); // 0x00C32FB0 (:22)
            virtual bool startSound(VoiceSound* pSound, float sampleRate, uint32 flags, System* pSoundSystem); // vtbl+0x04 0x00C32FC0 (:26)
            virtual void stopSound(VoiceSound* pSound, System* pSoundSystem); // vtbl+0x08 0x00C32FD0 (:36)
            virtual void pauseSound(VoiceSound* pSound); // vtbl+0x0C 0x00C32FE0 (:42)
            virtual void resumeSound(VoiceSound* pSound); // vtbl+0x10 0x00C32FF0 (:47)
            virtual void update(VoiceSound* pSounds, uint32 numSounds, System* pSoundSystem); // vtbl+0x18 0x00C33000 (:52)
            virtual int getNumAllocatedElements(); // vtbl+0x14 0x00C33010 (:59)

        };

    }
}
