// Original: library/modules/runtime/sound/sources/xaudio2/soundvoice_xaudio2.hpp
// Implementation: library/modules/runtime/sound/sources/xaudio2/soundvoice_xaudio2.cpp
//
// XAudio2 voices: SoundVoiceXAudio2, the voice manager and the voice record.
#pragma once

#include "keen/base/types.hpp"
#include <windows.h>
#include <xaudio2.h>
#include "keen/base/array.hpp"

struct IXAudio2;
struct IXAudio2SourceVoice;

namespace keen
{
    namespace SoundSystem
    {
        struct SystemXaudio2;
    }
}

namespace keen
{
    class MemoryAllocator;
    struct SoundFileData;

    namespace SoundSystem
    {
        // size 0xC
        struct Voice
        {
            enum
            {
                StopTimer_Default = 4,
            };

            Voice(); // 0x00C32C80 (.h:26)
            void start(); // 0x00C32C90 (.h:40)

            IXAudio2SourceVoice* pSourceVoice; // 0x00
            bool running; // 0x04
            bool paused; // 0x05
            uint32 stopTimer; // 0x08
        };

        // size 0x1C
        class SoundVoiceXAudio2
        {
        public:
            SoundVoiceXAudio2(); // 0x00C34510 (:15)
            void update(uint32 timeStepInMs); // 0x00C34610 (:305)
            bool isFree() const; // 0x00C34520 (:20)
            bool playSound(SystemXaudio2* pXaudioSoundSystem, float pitch, const SoundFileData* pSoundBankSound, uint32 flags, bool looped, float angle, float volume, float innerDistance, uint32 busIndex); // 0x00C38B60 (:25)
            void stop(); // 0x00C34530 (:166)
            void pause(); // 0x00C34560 (:182)
            void resume(); // 0x00C34590 (:191)
            void freeAndDestroyVoice(); // 0x00C345C0 (:200)
            void setPitch(float pitch); // 0x00C345F0 (:212)
        protected:
            void setPannedVolume(float volume, float panAngle, float innerDistance, uint32 inputChannels, uint32 outputChannels); // 0x00C372A0 (:221)
        public:
            void setPannedVolume(float volume, float panAngle, float innerDistance); // 0x00C37650 (:293)
        protected:
            void updateFromSystemThread(); // 0x00C346A0 (:348)

            Voice m_voice; // 0x00
            uint32 m_currentPositionInMs; // 0x0C
            const SoundFileData* m_pSoundFile; // 0x10
            uint32 m_flags; // 0x14
            bool m_looped; // 0x18
            uint8 m_busIndex; // 0x19

            friend class SoundVoiceManagerXAudio2; // inferred: the manager reads and replays its voices
        };

        // size 0xC
        class SoundVoiceManagerXAudio2
        {
        public:
            void create(MemoryAllocator* pAllocator, uint32 numVoices); // 0x00C38D50 (:353)
            void destroy(MemoryAllocator* pAllocator); // 0x00C38E60 (:397)
            void destroyVoicesBeforeDeviceChange(); // 0x00C376C0 (:361)
            void setAudioDevice(uint32 numChannelsInMasteringVoice, IXAudio2* pXAudioDevice); // 0x00C346B0 (:391)
            void recreateVoicesAfterDeviceChange(SystemXaudio2* pXaudioSoundSystem); // 0x00C38DC0 (:369)
            void updateFromSoundSystemThread(uint32 timeStepInms); // 0x00C37750 (:420)
            void updateFromSystemThread(float timeStep); // 0x00C377A0 (:429)
            SoundVoiceXAudio2* findFreeVoice(); // 0x00C37720 (:407)
            static uint32 getNumChannelsInMasteringVoice(); // 0x00C32CA0 (.h:98)
            IXAudio2* getAudioDevice() const;
            void handleCriticalError(); // 0x00C346D0 (:442)
            int getNumAllocatedVoices() const; // 0x00C377F0 (:447)
            SoundVoiceManagerXAudio2(); // 0x00C36F20
            ~SoundVoiceManagerXAudio2(); // 0x00C36E50

        private:
            Array<SoundVoiceXAudio2> m_voices; // 0x00
            static uint32 s_m_numChannelsInMasteringVoice; // 0x038B92B0
            IXAudio2* m_pXAudioDevice; // 0x08
        };

    }
}
