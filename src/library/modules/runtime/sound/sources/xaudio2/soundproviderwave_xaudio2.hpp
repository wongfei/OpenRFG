// Original: library/modules/runtime/sound/sources/xaudio2/soundproviderwave_xaudio2.hpp
// Implementation: library/modules/runtime/sound/sources/xaudio2/soundproviderwave_xaudio2.cpp
//
// SoundProviderWaveXAudio2, feeds PCM wave data to XAudio2 voices.
#pragma once

#include "keen/base/types.hpp"
#include <windows.h>
#include <xaudio2.h>
#include "library/modules/runtime/sound/sources/global/soundprovider.hpp"

namespace keen
{
    namespace SoundSystem
    {
        class SoundVoiceManagerXAudio2;
        struct System;
        struct VoiceSound;
    }
}

namespace keen
{
    namespace SoundSystem
    {
        // size 0xC
        class SoundProviderWaveXAudio2 : public SoundProvider
        {
        public:
            SoundProviderWaveXAudio2(); // 0x00C33A50 (:15)
            virtual ~SoundProviderWaveXAudio2(); // vtbl+0x00 0x00C33A60 (:19)
            void create(SoundVoiceManagerXAudio2* pVoiceManager, uint32 providerId); // 0x00C33A70 (:24)
            void destroy(); // 0x00C33A90 (:30)
            virtual bool startSound(VoiceSound* pSound, float pitchMultiplier, uint32 flags, System* pSoundSystem); // vtbl+0x04 0x00C391E0 (:35)
            virtual void stopSound(VoiceSound* pSound, System* pSoundSystem); // vtbl+0x08 0x00C36E80 (:71)
            virtual void pauseSound(VoiceSound* pSound); // vtbl+0x0C 0x00C36EC0 (:81)
            virtual void resumeSound(VoiceSound* pSound); // vtbl+0x10 0x00C36EF0 (:90)
            void update(VoiceSound* pSounds, uint32 numSounds, System* pSoundSystem, uint32 timeStepInms); // 0x00C389A0 (:99)
            virtual int getNumAllocatedElements(); // vtbl+0x14 0x00C38AD0 (:133)

        private:
            SoundVoiceManagerXAudio2* m_pVoiceManager; // 0x04
            uint32 m_providerId; // 0x08
        };

    }
}
