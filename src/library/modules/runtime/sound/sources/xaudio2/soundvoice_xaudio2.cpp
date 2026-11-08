// Original: library/modules/runtime/sound/sources/xaudio2/soundvoice_xaudio2.cpp
//
// XAudio2 2.7 source voices (SFX.md SX1). A voice plays one SoundFileData as a single buffer (looped with
// XAUDIO2_LOOP_INFINITE), routed to its bus's submix voice. There is no voice callback: the end of a sound is found by
// polling GetState from the sound system thread, with a stop timer the engine callback counts down each processing pass
// (a voice whose buffers ran out is stopped after the timer goes below zero). Panning goes through the output matrix
// from keen's own pan math (sound.cpp).

#include "library/modules/runtime/sound/sources/xaudio2/soundvoice_xaudio2.hpp"
#include "keen/base/memoryallocator.hpp"
#include "keen/sound/soundfiledata.hpp"
#include "library/modules/runtime/sound/sources/global/sound.hpp"
#include "library/modules/runtime/sound/sources/xaudio2/soundsystemstate_xaudio2.hpp"

#include <string.h>

namespace keen
{
    namespace SoundSystem
    {
        uint32 SoundVoiceManagerXAudio2::s_m_numChannelsInMasteringVoice = 0; // 0x038B92B0

        // Magic numbers: WAVEFORMATEX format tags.
        static const uint16 FormatTag_Adpcm = 2;
        static const uint16 FormatTag_Xma2 = 0x166;

        static inline float clampVolume(float volume)
        {
            if (0.0f > volume)
            {
                return 0.0f;
            }
            if (volume > 1.0f)
            {
                return 1.0f;
            }
            return volume;
        }

        // The 5.1 downmix clamp (NaN becomes 0, unlike clampVolume).
        static inline float saturateVolume(float volume)
        {
            if (!(volume >= 0.0f))
            {
                volume = 0.0f;
            }
            if (volume >= 1.0f)
            {
                volume = 1.0f;
            }
            return volume;
        }

        // 0x00C32C80 (soundvoice_xaudio2.hpp:26)
        Voice::Voice()
        {
            pSourceVoice = nullptr;
            running = false;
            paused = false;
        }

        // 0x00C32C90 (soundvoice_xaudio2.hpp:40)
        void Voice::start()
        {
            stopTimer = StopTimer_Default;
            running = true;
        }

        // 0x00C34510 (:15)
        SoundVoiceXAudio2::SoundVoiceXAudio2()
        {
            m_pSoundFile = nullptr;
        }

        // 0x00C34520 (:20)
        bool SoundVoiceXAudio2::isFree() const
        {
            return m_voice.running == false;
        }

        // 0x00C38B60 (:25)
        // Without a mastering voice (no output device) the voice only runs its timer: the sound plays silently for its
        // duration.
        bool SoundVoiceXAudio2::playSound(SystemXaudio2* pXaudioSoundSystem, float pitch, const SoundFileData* pSoundBankSound, uint32 flags, bool looped, float angle, float volume, float innerDistance, uint32 busIndex)
        {
            IXAudio2SubmixVoice* pBusVoice = pXaudioSoundSystem->pBusVoices[busIndex];
            IXAudio2* pAudioDevice = pXaudioSoundSystem->pAudioDevice;
            if (m_voice.pSourceVoice != nullptr)
            {
                m_voice.pSourceVoice->DestroyVoice();
                m_voice.pSourceVoice = nullptr;
                m_voice.paused = false;
            }
            m_busIndex = (uint8)busIndex;
            m_currentPositionInMs = 0;
            m_pSoundFile = pSoundBankSound;
            m_looped = looped;
            m_flags = flags;

            XAUDIO2_SEND_DESCRIPTOR send;
            send.Flags = 0;
            send.pOutputVoice = pBusVoice;
            XAUDIO2_VOICE_SENDS sends;
            sends.SendCount = 1;
            sends.pSends = &send;

            if (pXaudioSoundSystem->pMasteringVoice != nullptr)
            {
                // Magic numbers: frequency ratios up to 4 (two octaves up).
                const WAVEFORMATEX* pFormat = (const WAVEFORMATEX*)pSoundBankSound->xmaBuffer;
                if (pAudioDevice->CreateSourceVoice(&m_voice.pSourceVoice, pFormat, flags, 4.0f, nullptr, &sends, nullptr) != S_OK)
                {
                    return false;
                }

                XAUDIO2_BUFFER buffer;
                memset(&buffer, 0, sizeof(buffer));
                buffer.Flags = XAUDIO2_END_OF_STREAM;
                buffer.AudioBytes = pSoundBankSound->dataLength;
                buffer.pAudioData = (const BYTE*)pSoundBankSound->pData;
                const uint8* pFormatBytes = pSoundBankSound->xmaBuffer;
                uint16 formatTag;
                memcpy(&formatTag, pFormatBytes, sizeof(formatTag));
                if (formatTag == FormatTag_Adpcm)
                {
                    // Whole blocks times wSamplesPerBlock.
                    uint16 blockAlign;
                    uint16 samplesPerBlock;
                    memcpy(&blockAlign, pFormatBytes + 0x0C, sizeof(blockAlign));
                    memcpy(&samplesPerBlock, pFormatBytes + 0x12, sizeof(samplesPerBlock));
                    buffer.PlayLength = (pSoundBankSound->dataLength / blockAlign) * samplesPerBlock;
                }
                else if (formatTag == FormatTag_Xma2)
                {
                    // XMA2WAVEFORMATEX's play and loop region (no XMA on win32; kept from the shared code).
                    memcpy(&buffer.PlayBegin, pFormatBytes + 0x20, sizeof(uint32));
                    memcpy(&buffer.PlayLength, pFormatBytes + 0x24, sizeof(uint32));
                    memcpy(&buffer.LoopBegin, pFormatBytes + 0x28, sizeof(uint32));
                    memcpy(&buffer.LoopLength, pFormatBytes + 0x2C, sizeof(uint32));
                    buffer.LoopCount = pFormatBytes[0x30];
                }
                else
                {
                    // PCM and extensible: bytes / channels * 8 / bits.
                    uint16 channels;
                    uint16 bitsPerSample;
                    memcpy(&channels, pFormatBytes + 0x02, sizeof(channels));
                    memcpy(&bitsPerSample, pFormatBytes + 0x0E, sizeof(bitsPerSample));
                    buffer.PlayLength = ((pSoundBankSound->dataLength / channels) * 8) / bitsPerSample;
                }
                if (looped == true)
                {
                    buffer.LoopCount = XAUDIO2_LOOP_INFINITE;
                }
                buffer.pContext = nullptr;
                m_voice.pSourceVoice->SetFrequencyRatio(pitch, XAUDIO2_COMMIT_NOW);
                m_voice.pSourceVoice->SubmitSourceBuffer(&buffer, nullptr);
                uint16 inputChannels;
                memcpy(&inputChannels, pFormatBytes + 0x02, sizeof(inputChannels));
                setPannedVolume(volume, angle, innerDistance, inputChannels, SoundVoiceManagerXAudio2::getNumChannelsInMasteringVoice());
                m_voice.pSourceVoice->Start(0, XAUDIO2_COMMIT_NOW);
            }
            m_voice.start();
            return true;
        }

        // 0x00C34530 (:166)
        void SoundVoiceXAudio2::stop()
        {
            if (m_voice.pSourceVoice != nullptr)
            {
                m_voice.paused = false;
                m_voice.pSourceVoice->Stop(0, XAUDIO2_COMMIT_NOW);
                m_voice.pSourceVoice->FlushSourceBuffers();
                return;
            }
            m_voice.running = false;
            m_pSoundFile = nullptr;
        }

        // 0x00C34560 (:182)
        void SoundVoiceXAudio2::pause()
        {
            if (m_voice.pSourceVoice != nullptr && m_voice.paused == false)
            {
                m_voice.pSourceVoice->Stop(0, XAUDIO2_COMMIT_NOW);
                m_voice.paused = true;
            }
        }

        // 0x00C34590 (:191)
        void SoundVoiceXAudio2::resume()
        {
            if (m_voice.pSourceVoice != nullptr && m_voice.paused == true)
            {
                m_voice.pSourceVoice->Start(0, XAUDIO2_COMMIT_NOW);
                m_voice.paused = false;
            }
        }

        // 0x00C345C0 (:200)
        void SoundVoiceXAudio2::freeAndDestroyVoice()
        {
            IXAudio2SourceVoice* pSourceVoice = m_voice.pSourceVoice;
            if (pSourceVoice != nullptr)
            {
                m_voice.pSourceVoice = nullptr;
                pSourceVoice->Stop(0, XAUDIO2_COMMIT_NOW);
                pSourceVoice->DestroyVoice();
            }
        }

        // 0x00C345F0 (:212)
        void SoundVoiceXAudio2::setPitch(float pitch)
        {
            if (m_voice.pSourceVoice != nullptr)
            {
                m_voice.pSourceVoice->SetFrequencyRatio(pitch, XAUDIO2_COMMIT_NOW);
            }
        }

        // 0x00C372A0 (:221)
        // The output matrix (XAudio2's [destination * inputChannels + source] layout) for mono (pan over stereo or the
        // four 5.1 speakers), stereo (sound.cpp's stereo volumes) and 5.1 down to stereo; other layouts get a plain
        // volume. A single output channel or more than 16 also take the plain volume.
        void SoundVoiceXAudio2::setPannedVolume(float volume, float panAngle, float innerDistance, uint32 inputChannels, uint32 outputChannels)
        {
            if (outputChannels > 16 || outputChannels == 1)
            {
                m_voice.pSourceVoice->SetVolume(clampVolume(volume), XAUDIO2_COMMIT_NOW);
                return;
            }
            // Magic numbers: room for 6 inputs by 16 outputs.
            float matrix[96];
            memset(matrix, 0, sizeof(matrix));
            if (inputChannels == 1)
            {
                if (outputChannels > 2)
                {
                    // Front left/right, (center, LFE silent), back left/right.
                    float speakers[4];
                    calculate5point1Volumes(speakers, volume, panAngle);
                    matrix[0] = speakers[0];
                    matrix[1] = speakers[1];
                    matrix[4] = speakers[2];
                    matrix[5] = speakers[3];
                }
                else
                {
                    calculateStereoVolumes(matrix, volume, panAngle, 1.0f);
                }
                m_voice.pSourceVoice->SetOutputMatrix(nullptr, 1, outputChannels, matrix, XAUDIO2_COMMIT_NOW);
            }
            else if (inputChannels == 2)
            {
                // The stereo volumes' second pair goes to outputs 2 and 3 (original layout).
                float volumes[4];
                calculateStereoVolumes(volumes, volume, panAngle, innerDistance);
                matrix[0] = volumes[0];
                matrix[4] = volumes[2];
                matrix[3] = volumes[1];
                matrix[7] = volumes[3];
                m_voice.pSourceVoice->SetOutputMatrix(nullptr, 2, outputChannels, matrix, XAUDIO2_COMMIT_NOW);
            }
            else if (outputChannels <= 2 && inputChannels == 6)
            {
                // 5.1 to stereo: front at 0.707, center and back at 0.5, LFE dropped.
                const float frontVolume = (float)((double)volume * (double)0.707f);
                const float otherVolume = (float)((double)volume * 0.5);
                matrix[0] = saturateVolume(frontVolume);
                matrix[2] = saturateVolume(otherVolume);
                matrix[4] = saturateVolume(otherVolume);
                matrix[7] = saturateVolume(frontVolume);
                matrix[8] = saturateVolume(otherVolume);
                matrix[11] = saturateVolume(otherVolume);
                m_voice.pSourceVoice->SetOutputMatrix(nullptr, 6, outputChannels, matrix, XAUDIO2_COMMIT_NOW);
            }
            else
            {
                m_voice.pSourceVoice->SetVolume(clampVolume(volume), XAUDIO2_COMMIT_NOW);
            }
        }

        // 0x00C37650 (:293)
        void SoundVoiceXAudio2::setPannedVolume(float volume, float panAngle, float innerDistance)
        {
            if (m_voice.pSourceVoice == nullptr)
            {
                return;
            }
            XAUDIO2_VOICE_DETAILS details;
            m_voice.pSourceVoice->GetVoiceDetails(&details);
            setPannedVolume(volume, panAngle, innerDistance, details.InputChannels, SoundVoiceManagerXAudio2::getNumChannelsInMasteringVoice());
        }

        // 0x00C34610 (:305)
        // A voice with no queued buffer is stopped once the engine has counted the stop timer below zero. The play
        // position advances by the time step; a looped sound wraps, and a silent voice (no source voice) ends after its
        // duration.
        void SoundVoiceXAudio2::update(uint32 timeStepInMs)
        {
            if (m_voice.running == true && m_voice.pSourceVoice != nullptr && m_voice.paused == false)
            {
                XAUDIO2_VOICE_STATE state;
                m_voice.pSourceVoice->GetState(&state);
                if (state.BuffersQueued == 0)
                {
                    if ((sint32)m_voice.stopTimer >= 0)
                    {
                        return;
                    }
                    m_voice.pSourceVoice->Stop(0, XAUDIO2_COMMIT_NOW);
                    m_voice.running = false;
                }
            }
            m_voice.stopTimer = Voice::StopTimer_Default;
            if (m_pSoundFile == nullptr || m_voice.paused != false)
            {
                return;
            }
            m_currentPositionInMs += timeStepInMs;
            const uint32 durationInms = m_pSoundFile->durationInms;
            if (m_currentPositionInMs > durationInms)
            {
                if (m_looped != false)
                {
                    m_currentPositionInMs -= durationInms;
                    return;
                }
                if (m_voice.pSourceVoice == nullptr)
                {
                    m_voice.running = false;
                }
            }
        }

        // 0x00C346A0 (:348)
        // From XAudio2's engine thread.
        void SoundVoiceXAudio2::updateFromSystemThread()
        {
            InterlockedDecrement((volatile LONG*)&m_voice.stopTimer);
        }

        // 0x00C36F20
        SoundVoiceManagerXAudio2::SoundVoiceManagerXAudio2()
        {
        }

        // 0x00C36E50
        SoundVoiceManagerXAudio2::~SoundVoiceManagerXAudio2()
        {
        }

        // 0x00C38D50 (:353)
        void SoundVoiceManagerXAudio2::create(MemoryAllocator* pAllocator, uint32 numVoices)
        {
            m_voices.create(pAllocator, numVoices, 0x10);
            s_m_numChannelsInMasteringVoice = 0;
            m_pXAudioDevice = nullptr;
        }

        // 0x00C376C0 (:361)
        void SoundVoiceManagerXAudio2::destroyVoicesBeforeDeviceChange()
        {
            for (SoundVoiceXAudio2* pVoice = m_voices.getStart(); pVoice != m_voices.getEnd(); ++pVoice)
            {
                pVoice->freeAndDestroyVoice();
            }
        }

        // 0x00C38DC0 (:369)
        // Looped sounds start again from the beginning on the new device (pitch 1, centered, silent until the next
        // update); one-shot sounds end.
        void SoundVoiceManagerXAudio2::recreateVoicesAfterDeviceChange(SystemXaudio2* pXaudioSoundSystem)
        {
            for (SoundVoiceXAudio2* pVoice = m_voices.getStart(); pVoice != m_voices.getEnd(); ++pVoice)
            {
                if (pVoice->m_voice.running == false)
                {
                    continue;
                }
                if (pVoice->m_looped == false)
                {
                    pVoice->m_voice.running = false;
                }
                else
                {
                    pVoice->playSound(pXaudioSoundSystem, 1.0f, pVoice->m_pSoundFile, pVoice->m_flags, pVoice->m_looped, 0.0f, 0.0f, 0.0f, pVoice->m_busIndex);
                }
            }
        }

        // 0x00C346B0 (:391)
        void SoundVoiceManagerXAudio2::setAudioDevice(uint32 numChannelsInMasteringVoice, IXAudio2* pXAudioDevice)
        {
            s_m_numChannelsInMasteringVoice = numChannelsInMasteringVoice;
            m_pXAudioDevice = pXAudioDevice;
        }

        // 0x00C38E60 (:397)
        void SoundVoiceManagerXAudio2::destroy(MemoryAllocator* pAllocator)
        {
            for (SoundVoiceXAudio2* pVoice = m_voices.getStart(); pVoice != m_voices.getEnd(); ++pVoice)
            {
                pVoice->freeAndDestroyVoice();
            }
            m_voices.destroy(pAllocator);
        }

        // 0x00C37720 (:407)
        SoundVoiceXAudio2* SoundVoiceManagerXAudio2::findFreeVoice()
        {
            for (SoundVoiceXAudio2* pVoice = m_voices.getStart(); pVoice != m_voices.getEnd(); ++pVoice)
            {
                if (pVoice->isFree())
                {
                    return pVoice;
                }
            }
            return nullptr;
        }

        // 0x00C37750 (:420)
        void SoundVoiceManagerXAudio2::updateFromSoundSystemThread(uint32 timeStepInms)
        {
            for (SoundVoiceXAudio2* pVoice = m_voices.getStart(); pVoice != m_voices.getEnd(); ++pVoice)
            {
                pVoice->update(timeStepInms);
            }
        }

        // 0x00C377A0 (:429)
        // From XAudio2's engine thread (SoundSystemEngineCallback::OnProcessingPassStart).
        void SoundVoiceManagerXAudio2::updateFromSystemThread(float timeStep)
        {
            (void)timeStep;
            for (SoundVoiceXAudio2* pVoice = m_voices.getStart(); pVoice != m_voices.getEnd(); ++pVoice)
            {
                if (pVoice->m_voice.running != false)
                {
                    pVoice->updateFromSystemThread();
                }
            }
        }

        // 0x00C346D0 (:442)
        void SoundVoiceManagerXAudio2::handleCriticalError()
        {
        }

        // 0x00C377F0 (:447)
        int SoundVoiceManagerXAudio2::getNumAllocatedVoices() const
        {
            int count = 0;
            for (uint32 i = 0; i < m_voices.getSize(); ++i)
            {
                if (m_voices[i].m_voice.running != false)
                {
                    ++count;
                }
            }
            return count;
        }

        // 0x00C32CA0 (soundvoice_xaudio2.hpp:98)
        uint32 SoundVoiceManagerXAudio2::getNumChannelsInMasteringVoice()
        {
            return s_m_numChannelsInMasteringVoice;
        }
    }
}
