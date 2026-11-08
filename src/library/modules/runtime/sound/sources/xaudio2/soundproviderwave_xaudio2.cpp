// Original: library/modules/runtime/sound/sources/xaudio2/soundproviderwave_xaudio2.cpp
//
// SoundProviderWaveXAudio2, provider 0 on a working device (SFX.md SX1): puts a virtual sound on a free XAudio2 voice
// and, each sound system update, ends the voices that stopped and updates the others' panned volume and pitch.

#include "library/modules/runtime/sound/sources/xaudio2/soundproviderwave_xaudio2.hpp"
#include "keen/sound/sounddefinition.hpp"
#include "library/modules/runtime/sound/sources/global/sound.hpp"
#include "library/modules/runtime/sound/sources/global/soundsysteminternal.hpp"

namespace keen
{
    namespace SoundSystem
    {
        // 0x00C33A50 (:15)
        SoundProviderWaveXAudio2::SoundProviderWaveXAudio2()
        {
        }

        // 0x00C33A60 (:19)
        SoundProviderWaveXAudio2::~SoundProviderWaveXAudio2()
        {
        }

        // 0x00C33A70 (:24)
        void SoundProviderWaveXAudio2::create(SoundVoiceManagerXAudio2* pVoiceManager, uint32 providerId)
        {
            m_providerId = providerId;
            m_pVoiceManager = pVoiceManager;
        }

        // 0x00C33A90 (:30)
        void SoundProviderWaveXAudio2::destroy()
        {
            m_pVoiceManager = nullptr;
        }

        // 0x00C391E0 (:35)
        // Background music definitions (flag isBgm) create their voice as XAUDIO2_VOICE_MUSIC. The bus is clamped to the
        // last bus.
        bool SoundProviderWaveXAudio2::startSound(VoiceSound* pSound, float pitchMultiplier, uint32 flags, System* pSoundSystem)
        {
            (void)flags;
            SoundVoiceXAudio2* pVoice = m_pVoiceManager->findFreeVoice();
            if (pVoice == nullptr)
            {
                return false;
            }
            const BaseSound* pBaseSound = pSound->m_pBaseSound;
            const SoundDefinition* pDefinition = getDefinition(pBaseSound->definition);
            uint32 voiceFlags = 0;
            if ((pDefinition->flags & SoundDefinitionFlags_isBgm) != 0)
            {
                voiceFlags = XAUDIO2_VOICE_MUSIC;
            }
            PositionedSoundData positionedData;
            calculateAttenuationAndAngle(&positionedData, pBaseSound, pSoundSystem->inverseListenerMatrix);
            uint32 busIndex = pSoundSystem->busCount - 1;
            if (pDefinition->output_bus < busIndex)
            {
                busIndex = pDefinition->output_bus;
            }
            const bool looped = ((pBaseSound->runCountAndFlags >> 15) & 1) != 0;
            const float volume = pBaseSound->volume * positionedData.attenuation;
            if (pVoice->playSound(&pSoundSystem->xaudio2State, pitchMultiplier, pBaseSound->pSoundFileData, voiceFlags, looped, positionedData.angle, volume, positionedData.innerDistance, busIndex) != true)
            {
                return false;
            }
            pSound->m_pProviderData = pVoice;
            return true;
        }

        // 0x00C36E80 (:71)
        void SoundProviderWaveXAudio2::stopSound(VoiceSound* pSound, System* pSoundSystem)
        {
            (void)pSoundSystem;
            SoundVoiceXAudio2* pVoice = (SoundVoiceXAudio2*)pSound->m_pProviderData;
            if (pVoice != nullptr)
            {
                pVoice->stop();
            }
        }

        // 0x00C36EC0 (:81)
        void SoundProviderWaveXAudio2::pauseSound(VoiceSound* pSound)
        {
            SoundVoiceXAudio2* pVoice = (SoundVoiceXAudio2*)pSound->m_pProviderData;
            if (pVoice != nullptr)
            {
                pVoice->pause();
            }
        }

        // 0x00C36EF0 (:90)
        void SoundProviderWaveXAudio2::resumeSound(VoiceSound* pSound)
        {
            SoundVoiceXAudio2* pVoice = (SoundVoiceXAudio2*)pSound->m_pProviderData;
            if (pVoice != nullptr)
            {
                pVoice->resume();
            }
        }

        // 0x00C389A0 (:99)
        // numSounds must not be 0 (do-while in the original; SoundSystem::update passes the voice count).
        void SoundProviderWaveXAudio2::update(VoiceSound* pSounds, uint32 numSounds, System* pSoundSystem, uint32 timeStepInms)
        {
            m_pVoiceManager->updateFromSoundSystemThread(timeStepInms);
            VoiceSound* pSound = pSounds;
            do
            {
                if (pSound->m_providerType == m_providerId)
                {
                    SoundVoiceXAudio2* pVoice = (SoundVoiceXAudio2*)pSound->m_pProviderData;
                    if (pVoice->isFree())
                    {
                        // The voice ended: the virtual sound is stopped by the next updateBase.
                        pSound->soundEndedAndFree();
                    }
                    else
                    {
                        const BaseSound* pBaseSound = pSound->m_pBaseSound;
                        PositionedSoundData positionedData;
                        calculateAttenuationAndAngle(&positionedData, pBaseSound, pSoundSystem->inverseListenerMatrix);
                        const float volume = pBaseSound->volume * positionedData.attenuation;
                        pVoice->setPannedVolume(volume, positionedData.angle, positionedData.innerDistance);
                        pVoice->setPitch(pBaseSound->pitch);
                    }
                }
                ++pSound;
            } while (--numSounds != 0);
        }

        // 0x00C38AD0 (:133)
        int SoundProviderWaveXAudio2::getNumAllocatedElements()
        {
            return m_pVoiceManager->getNumAllocatedVoices();
        }
    }
}
