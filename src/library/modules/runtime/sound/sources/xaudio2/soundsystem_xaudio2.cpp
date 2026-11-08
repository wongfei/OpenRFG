// Original: library/modules/runtime/sound/sources/xaudio2/soundsystem_xaudio2.cpp
//
// The XAudio2 2.7 side of the sound system (SFX.md SX1): the mastering voice on device 0, one submix voice per bus
// routed by the bus definitions, the reverb effect chain (a second submix voice per bus with the XAudio2 reverb XAPO),
// the per-update bus volumes and send matrices, and the device selection: the default device is checked every update
// after the first second, and a changed or invalidated device rebuilds the voices and busses.

#include "library/modules/runtime/sound/sources/xaudio2/soundsystemstate_xaudio2.hpp"
#include "keen/base/memoryallocator.hpp"
#include "keen/base/trace.hpp"
#include "keen/sound/fxdefinitionreverb.hpp"
#include "keen/sound/soundsystem.hpp"
#include "library/modules/runtime/sound/sources/global/soundsysteminternal.hpp"

#include <math.h>
#include <string.h>

namespace keen
{
    namespace SoundSystem
    {
        // Magic numbers: the default master rate without a device (44.1 kHz), the reverb effect id 'BRVB'.
        static const uint32 DefaultMasterSampleRate = 44100;
        static const uint32 EffectId_Reverb = 0x42565242;

        // 0x00C359A0 (soundsystemstate_xaudio2.hpp:17)
        SoundSystemEngineCallback::SoundSystemEngineCallback()
        {
            m_pSystem = nullptr;
        }

        // 0x00C32CB0 (soundsystemstate_xaudio2.hpp:18)
        void SoundSystemEngineCallback::create(SystemXaudio2* pSoundSystem)
        {
            m_pSystem = pSoundSystem;
        }

        // 0x00C38B40 (:28)
        // Counts the voices' stop timers down (SoundVoiceXAudio2::update). Magic numbers: 5.3 ms, unused.
        void __stdcall SoundSystemEngineCallback::OnProcessingPassStart()
        {
            m_pSystem->voiceManager.updateFromSystemThread(0.0053f);
        }

        // 0x00C359B0 (soundsystemstate_xaudio2.hpp:21)
        void __stdcall SoundSystemEngineCallback::OnProcessingPassEnd()
        {
        }

        // 0x00C36F30 (:37)
        // The next device selection rebuilds the device.
        void __stdcall SoundSystemEngineCallback::OnCriticalError(HRESULT)
        {
            m_pSystem->deviceUpdatePending = true;
        }

        // 0x00C38AF0 (:16)
        SystemXaudio2::SystemXaudio2()
        {
            pAudioDevice = nullptr;
            pMasteringVoice = nullptr;
            memset(pBusVoices, 0, sizeof(pBusVoices));
            memset(pBusFXVoices, 0, sizeof(pBusFXVoices));
            isInDeviceChangeCallback = false;
            callbackWrapper.create(this);
        }

        // 0x00C38950
        SystemXaudio2::~SystemXaudio2()
        {
        }

        // 0x00C36F50 (:43)
        // LTCG: pXaudioSoundSystem in ESI.
        // The mastering voice on device 0 with the default channels and rate; its details give the channel count and the
        // rate, and device 0's id is remembered for the device selection.
        static void setupDevice(SystemXaudio2* pXaudioSoundSystem)
        {
            UINT32 deviceCount;
            pXaudioSoundSystem->pAudioDevice->GetDeviceCount(&deviceCount);
            const HRESULT result = pXaudioSoundSystem->pAudioDevice->CreateMasteringVoice(&pXaudioSoundSystem->pMasteringVoice, XAUDIO2_DEFAULT_CHANNELS, XAUDIO2_DEFAULT_SAMPLERATE, 0, 0, nullptr);
            if (result != S_OK)
            {
                debug::nativePrintArgs("[SoundSystem]:Unable to create Mastering Voice. Result : %x\n", result);
                pXaudioSoundSystem->pMasteringVoice = nullptr;
            }
            if (pXaudioSoundSystem->pMasteringVoice == nullptr)
            {
                memset(pXaudioSoundSystem->currentDeviceID, 0, sizeof(pXaudioSoundSystem->currentDeviceID));
                pXaudioSoundSystem->channelCount = 0;
                pXaudioSoundSystem->masterVoiceSampleRate = DefaultMasterSampleRate;
                return;
            }
            XAUDIO2_VOICE_DETAILS details;
            pXaudioSoundSystem->pMasteringVoice->GetVoiceDetails(&details);
            pXaudioSoundSystem->masterVoiceSampleRate = details.InputSampleRate;
            pXaudioSoundSystem->channelCount = details.InputChannels;
            XAUDIO2_DEVICE_DETAILS deviceDetails;
            if (pXaudioSoundSystem->pAudioDevice->GetDeviceDetails(0, &deviceDetails) == S_OK)
            {
                memmove(pXaudioSoundSystem->currentDeviceID, deviceDetails.DeviceID, sizeof(pXaudioSoundSystem->currentDeviceID));
            }
        }

        // 0x00C33D00 (:114)
        // LTCG: pXaudioSoundSystem in ESI. Not called in rfg.exe (updateDeviceSelection inlines it).
        static void shutdownDevice(SystemXaudio2* pXaudioSoundSystem)
        {
            if (pXaudioSoundSystem->pMasteringVoice != nullptr)
            {
                pXaudioSoundSystem->pMasteringVoice->DestroyVoice();
                pXaudioSoundSystem->pMasteringVoice = nullptr;
                pXaudioSoundSystem->channelCount = 0;
                pXaudioSoundSystem->masterVoiceSampleRate = DefaultMasterSampleRate;
            }
        }

        // 0x00C392F0 (:125)
        bool create(SystemXaudio2* pXaudioSoundSystem, MemoryAllocator* pAllocator, const InitializationParameters& parameter, uint32 waveProviderId)
        {
            pXaudioSoundSystem->externalXaudio2Voices = 0;
            pXaudioSoundSystem->deviceUpdatePending = false;
            pXaudioSoundSystem->voiceManager.create(pAllocator, parameter.voiceCount);
            memset(pXaudioSoundSystem->currentDeviceID, 0, sizeof(pXaudioSoundSystem->currentDeviceID));
            pXaudioSoundSystem->channelCount = 0;
            pXaudioSoundSystem->reverbOutputChannels = 0;
            pXaudioSoundSystem->masterVoiceSampleRate = DefaultMasterSampleRate;
            pXaudioSoundSystem->pAudioDevice->RegisterForCallbacks(&pXaudioSoundSystem->callbackWrapper);
            // Magic numbers: the first device check after one second.
            pXaudioSoundSystem->timeUntilDeviceSelection = 1.0f;
            pXaudioSoundSystem->waveProvider.create(&pXaudioSoundSystem->voiceManager, waveProviderId);
            setupDevice(pXaudioSoundSystem);
            pXaudioSoundSystem->voiceManager.setAudioDevice(pXaudioSoundSystem->channelCount, pXaudioSoundSystem->pAudioDevice);
            return true;
        }

        // 0x00C39380 (:150)
        void shutdown(SystemXaudio2* pXaudioSoundSystem, MemoryAllocator* pAllocator)
        {
            pXaudioSoundSystem->waveProvider.destroy();
            if (pXaudioSoundSystem->pAudioDevice != nullptr && pXaudioSoundSystem->pMasteringVoice != nullptr)
            {
                pXaudioSoundSystem->pAudioDevice->UnregisterForCallbacks(&pXaudioSoundSystem->callbackWrapper);
            }
            pXaudioSoundSystem->voiceManager.destroy(pAllocator);
            for (uint32 i = 0; i < 30; ++i)
            {
                if (pXaudioSoundSystem->pBusVoices[i] != nullptr)
                {
                    pXaudioSoundSystem->pBusVoices[i]->DestroyVoice();
                    pXaudioSoundSystem->pBusVoices[i] = nullptr;
                }
                if (pXaudioSoundSystem->pBusFXVoices[i] != nullptr)
                {
                    pXaudioSoundSystem->pBusFXVoices[i]->DestroyVoice();
                    pXaudioSoundSystem->pBusFXVoices[i] = nullptr;
                }
            }
            if (pXaudioSoundSystem->pMasteringVoice != nullptr)
            {
                pXaudioSoundSystem->pMasteringVoice->DestroyVoice();
                pXaudioSoundSystem->pMasteringVoice = nullptr;
            }
            if (pXaudioSoundSystem->pAudioDevice != nullptr)
            {
                pXaudioSoundSystem->pAudioDevice->Release();
                pXaudioSoundSystem->pAudioDevice = nullptr;
            }
        }

        static void notifyAboutToDestroyMasteringVoice(System* pSoundSystem)
        {
            for (SoundDeviceChangedCallback* pCallback = pSoundSystem->deviceChangedCallbacks.getStart().operator->(); pCallback != pSoundSystem->deviceChangedCallbacks.getEnd().operator->(); pCallback = pCallback->m_pNext)
            {
                pCallback->handleAboutToDestroyMasteringVoice(pSoundSystem);
            }
        }

        static void notifyMasteringVoiceCreated(System* pSoundSystem)
        {
            for (SoundDeviceChangedCallback* pCallback = pSoundSystem->deviceChangedCallbacks.getStart().operator->(); pCallback != pSoundSystem->deviceChangedCallbacks.getEnd().operator->(); pCallback = pCallback->m_pNext)
            {
                pCallback->handleMasteringVoiceCreated(pSoundSystem);
            }
        }

        // 0x00C39410 (:188)
        // Once the first second has passed (the timer is not reset, so from then on every update) and no external voices
        // exist: a different device 0 or a pending critical error rebuilds the mastering voice, the busses and the looped
        // voices; an invalid call (no device) tears them down.
        void updateDeviceSelection(System* pSoundSystem, float timeStep)
        {
            SystemXaudio2* pXaudioSoundSystem = &pSoundSystem->xaudio2State;
            pXaudioSoundSystem->timeUntilDeviceSelection = pXaudioSoundSystem->timeUntilDeviceSelection - timeStep;
            if (pXaudioSoundSystem->timeUntilDeviceSelection >= 0.0f || pXaudioSoundSystem->externalXaudio2Voices != 0 || pXaudioSoundSystem->pAudioDevice == nullptr)
            {
                return;
            }
            XAUDIO2_DEVICE_DETAILS deviceDetails;
            const HRESULT result = pXaudioSoundSystem->pAudioDevice->GetDeviceDetails(0, &deviceDetails);
            if (result == S_OK)
            {
                if (pXaudioSoundSystem->deviceUpdatePending == false && memcmp(pXaudioSoundSystem->currentDeviceID, deviceDetails.DeviceID, sizeof(pXaudioSoundSystem->currentDeviceID)) == 0)
                {
                    return;
                }
                pXaudioSoundSystem->isInDeviceChangeCallback = true;
                if (pXaudioSoundSystem->pMasteringVoice != nullptr)
                {
                    pXaudioSoundSystem->voiceManager.destroyVoicesBeforeDeviceChange();
                    destroyBusses(pSoundSystem);
                    notifyAboutToDestroyMasteringVoice(pSoundSystem);
                    shutdownDevice(pXaudioSoundSystem);
                }
                pXaudioSoundSystem->deviceUpdatePending = false;
                setupDevice(pXaudioSoundSystem);
                createBusses(pSoundSystem);
                pXaudioSoundSystem->voiceManager.setAudioDevice(pXaudioSoundSystem->channelCount, pXaudioSoundSystem->pAudioDevice);
                pXaudioSoundSystem->voiceManager.recreateVoicesAfterDeviceChange(pXaudioSoundSystem);
                pXaudioSoundSystem->isInDeviceChangeCallback = false;
                notifyMasteringVoiceCreated(pSoundSystem);
            }
            else if (result == XAUDIO2_E_INVALID_CALL)
            {
                pXaudioSoundSystem->isInDeviceChangeCallback = true;
                pXaudioSoundSystem->voiceManager.destroyVoicesBeforeDeviceChange();
                destroyBusses(pSoundSystem);
                notifyAboutToDestroyMasteringVoice(pSoundSystem);
                shutdownDevice(pXaudioSoundSystem);
                pXaudioSoundSystem->isInDeviceChangeCallback = false;
                memset(pXaudioSoundSystem->currentDeviceID, 0, sizeof(pXaudioSoundSystem->currentDeviceID));
            }
        }

        // 0x00C33D30 (:261)
        // LTCG: pSoundSystem in ECX, busDefinition in EDX, pSends on the stack.
        // A send per target bus: the master out (-1), another bus below busCount; nirvana (-2) and other ids are skipped.
        static void fillVoiceSends(XAUDIO2_VOICE_SENDS* pSends, const BusDefinition& busDefinition, System* pSoundSystem)
        {
            pSends->SendCount = 0;
            for (uint32 i = 0; i < 4; ++i)
            {
                const int target = busDefinition.targetBusses[i];
                IXAudio2Voice* pOutputVoice;
                if (target == BusDefinition::masterOutBus)
                {
                    pOutputVoice = pSoundSystem->xaudio2State.pMasteringVoice;
                }
                else if (target != BusDefinition::nirvana && target < (int)pSoundSystem->busCount)
                {
                    pOutputVoice = pSoundSystem->xaudio2State.pBusVoices[target];
                }
                else
                {
                    continue;
                }
                pSends->pSends[pSends->SendCount].Flags = 0;
                pSends->pSends[pSends->SendCount].pOutputVoice = pOutputVoice;
                ++pSends->SendCount;
            }
        }

        // 0x00C33E90 (:285)
        // A submix voice per bus (processing stage = bus index, so busses feed higher ones), then the routing.
        void createBusses(System* pSoundSystem)
        {
            SystemXaudio2* pXaudioSoundSystem = &pSoundSystem->xaudio2State;
            if (pXaudioSoundSystem->pMasteringVoice == nullptr)
            {
                return;
            }
            for (uint32 i = 0; i < pSoundSystem->busCount; ++i)
            {
                if (pXaudioSoundSystem->pAudioDevice->CreateSubmixVoice(&pXaudioSoundSystem->pBusVoices[i], pXaudioSoundSystem->channelCount, pXaudioSoundSystem->masterVoiceSampleRate, 0, i, nullptr, nullptr) != S_OK)
                {
                    pXaudioSoundSystem->pBusVoices[i] = nullptr;
                }
            }
            for (uint32 i = 0; i < pSoundSystem->busCount; ++i)
            {
                if (pXaudioSoundSystem->pBusVoices[i] == nullptr)
                {
                    continue;
                }
                // fillVoiceSends inlined.
                XAUDIO2_SEND_DESCRIPTOR sendDescriptors[4];
                XAUDIO2_VOICE_SENDS sends;
                sends.pSends = sendDescriptors;
                fillVoiceSends(&sends, pSoundSystem->busses[i].definition, pSoundSystem);
                pXaudioSoundSystem->pBusVoices[i]->SetOutputVoices(&sends);
            }
        }

        // 0x00C340B0 (:324)
        void destroyBusses(System* pSoundSystem)
        {
            SystemXaudio2* pXaudioSoundSystem = &pSoundSystem->xaudio2State;
            for (uint32 i = 0; i < pSoundSystem->busCount; ++i)
            {
                if (pXaudioSoundSystem->pBusVoices[i] != nullptr)
                {
                    pXaudioSoundSystem->pBusVoices[i]->DestroyVoice();
                    pXaudioSoundSystem->pBusVoices[i] = nullptr;
                }
                if (pXaudioSoundSystem->pBusFXVoices[i] != nullptr)
                {
                    pXaudioSoundSystem->pBusFXVoices[i]->DestroyVoice();
                    pXaudioSoundSystem->pBusFXVoices[i] = nullptr;
                }
            }
        }

        // 0x00C37080 (:343)
        // The bus gain (dB) times the ducking gain as the voice volume; per target a diagonal send matrix with the larger
        // of the dry and wet send volumes (the reverb voice gets the same matrix).
        void updatePlatformBus(System* pSystem, uint32 busId)
        {
            SystemXaudio2* pXaudioSoundSystem = &pSystem->xaudio2State;
            IXAudio2SubmixVoice* pBusVoice = pXaudioSoundSystem->pBusVoices[busId];
            if (pBusVoice == nullptr)
            {
                return;
            }
            const BusImplementation& bus = pSystem->busses[busId];
            const float exponent = (float)(bus.gain.currentValue / 20.0);
            const float energy = (float)pow(10.0, (double)exponent);
            pBusVoice->SetVolume(energy * bus.duckingGain.currentValue, XAUDIO2_COMMIT_NOW);
            for (uint32 i = 0; i < 4; ++i)
            {
                const int target = bus.definition.targetBusses[i];
                IXAudio2Voice* pDestination;
                if (target == BusDefinition::masterOutBus)
                {
                    pDestination = pXaudioSoundSystem->pMasteringVoice;
                }
                else if (target != BusDefinition::nirvana)
                {
                    // No check against busCount here (original).
                    pDestination = pXaudioSoundSystem->pBusVoices[target];
                }
                else
                {
                    continue;
                }
                const float dry = bus.drySendVolumes[i].currentValue;
                const float wet = bus.wetSendVolumes[i].currentValue;
                const float level = (float)((double)dry - (double)wet) >= 0.0f ? dry : wet;
                // Magic numbers: a matrix of up to 8 by 8 channels.
                float matrix[64];
                memset(matrix, 0, sizeof(matrix));
                const uint32 channelCount = pXaudioSoundSystem->channelCount;
                for (uint32 channel = 0; channel < channelCount; ++channel)
                {
                    matrix[channel * (channelCount + 1)] = level;
                }
                pBusVoice->SetOutputMatrix(pDestination, channelCount, channelCount, matrix, XAUDIO2_COMMIT_NOW);
                if (pXaudioSoundSystem->pBusFXVoices[busId] != nullptr)
                {
                    pXaudioSoundSystem->pBusFXVoices[busId]->SetOutputMatrix(pDestination, pXaudioSoundSystem->reverbOutputChannels, pXaudioSoundSystem->channelCount, matrix, XAUDIO2_COMMIT_NOW);
                }
            }
        }

        // 0x00C34110 (:394)
        // keen's reverb parameters are I3DL2 values (millibels and seconds): fully wet, no room rolloff.
        void createPlatformReverbParameter(XAUDIO2FX_REVERB_PARAMETERS* pResult, const FXReverbParameter& input)
        {
            XAUDIO2FX_REVERB_I3DL2_PARAMETERS parameters;
            parameters.Room = (int)input.room;
            parameters.RoomHF = (int)input.roomHf;
            parameters.WetDryMix = 100.0f;
            parameters.RoomRolloffFactor = 0.0f;
            parameters.DecayTime = input.decayTime;
            parameters.DecayHFRatio = input.decayHfRatio;
            parameters.ReflectionsDelay = input.reflectionsDelay;
            parameters.ReverbDelay = input.reverbDelay;
            parameters.Diffusion = input.diffusion;
            parameters.Density = input.density;
            parameters.Reflections = (int)input.reflections;
            parameters.Reverb = (int)input.reverb;
            parameters.HFReference = input.hfReference;
            ReverbConvertI3DL2ToNative(&parameters, pResult);
        }

        // 0x00C341B0 (:420)
        // Only a reverb as the bus's first effect is realized: a mono (or 5.1 out) submix voice with the reverb XAPO,
        // one processing stage after the bus, fed by the bus as an extra send and sending where the bus sends.
        void createPlatformEffectChain(System* pSystem, uint32 busId)
        {
            SystemXaudio2* pXaudioSoundSystem = &pSystem->xaudio2State;
            if (pXaudioSoundSystem->pAudioDevice == nullptr || pXaudioSoundSystem->pMasteringVoice == nullptr)
            {
                return;
            }
            EffectStorage* pEffect = &pSystem->effects[pSystem->busses[busId].effectIndices[0]];
            if (pEffect->effectId != EffectId_Reverb)
            {
                return;
            }
            IUnknown* pReverb = nullptr;
            if (XAudio2CreateReverb(&pReverb) == S_OK)
            {
                XAUDIO2_EFFECT_DESCRIPTOR descriptor;
                descriptor.pEffect = pReverb;
                descriptor.InitialState = TRUE;
                // Magic numbers: 6 reverb outputs on 5.1 and up, else mono.
                pXaudioSoundSystem->reverbOutputChannels = pXaudioSoundSystem->channelCount >= 6 ? 6 : 1;
                descriptor.OutputChannels = pXaudioSoundSystem->reverbOutputChannels;
                XAUDIO2_EFFECT_CHAIN chain;
                chain.EffectCount = 1;
                chain.pEffectDescriptors = &descriptor;
                XAUDIO2_SEND_DESCRIPTOR fxSendDescriptors[4];
                XAUDIO2_VOICE_SENDS fxSends;
                fxSends.pSends = fxSendDescriptors;
                fillVoiceSends(&fxSends, pSystem->busses[busId].definition, pSystem);
                if (pXaudioSoundSystem->pBusVoices[busId] != nullptr)
                {
                    IXAudio2SubmixVoice** ppFxVoice = &pXaudioSoundSystem->pBusFXVoices[busId];
                    if (pXaudioSoundSystem->pAudioDevice->CreateSubmixVoice(ppFxVoice, 1, pXaudioSoundSystem->masterVoiceSampleRate, 0, busId + 1, &fxSends, &chain) == S_OK)
                    {
                        XAUDIO2FX_REVERB_PARAMETERS reverbParameters;
                        createPlatformReverbParameter(&reverbParameters, *(const FXReverbParameter*)pEffect->opaqueParameterData);
                        (*ppFxVoice)->SetEffectParameters(0, &reverbParameters, sizeof(reverbParameters), XAUDIO2_COMMIT_NOW);

                        XAUDIO2_SEND_DESCRIPTOR busSendDescriptors[5];
                        XAUDIO2_VOICE_SENDS busSends;
                        busSends.pSends = busSendDescriptors;
                        fillVoiceSends(&busSends, pSystem->busses[busId].definition, pSystem);
                        busSendDescriptors[busSends.SendCount].Flags = 0;
                        busSendDescriptors[busSends.SendCount].pOutputVoice = *ppFxVoice;
                        ++busSends.SendCount;
                        pXaudioSoundSystem->pBusVoices[busId]->SetOutputVoices(&busSends);

                        // The bus into the mono reverb: 0.5 from the first channel and 0.5 at index 3 (the fourth channel
                        // on 5.1; on stereo the right channel is not sent, original).
                        float matrix[12];
                        memset(matrix, 0, sizeof(matrix));
                        matrix[0] = 0.5f;
                        matrix[3] = 0.5f;
                        pXaudioSoundSystem->pBusVoices[busId]->SetOutputMatrix(*ppFxVoice, pXaudioSoundSystem->channelCount, 1, matrix, XAUDIO2_COMMIT_NOW);
                    }
                }
            }
            // The XAPO reference is kept here and never released (the voice holds its own).
            pEffect->pPlatformData = pReverb;
        }

        // 0x00C34430 (:526)
        void updatePlatformEffectChain(System* pSystem, uint32 busId)
        {
            SystemXaudio2* pXaudioSoundSystem = &pSystem->xaudio2State;
            if (pXaudioSoundSystem->pAudioDevice == nullptr || pXaudioSoundSystem->pMasteringVoice == nullptr)
            {
                return;
            }
            const EffectStorage* pEffect = &pSystem->effects[pSystem->busses[busId].effectIndices[0]];
            if (pEffect->effectId != EffectId_Reverb || pXaudioSoundSystem->pBusFXVoices[busId] == nullptr)
            {
                return;
            }
            XAUDIO2FX_REVERB_PARAMETERS reverbParameters;
            createPlatformReverbParameter(&reverbParameters, *(const FXReverbParameter*)pEffect->opaqueParameterData);
            pXaudioSoundSystem->pBusFXVoices[busId]->SetEffectParameters(0, &reverbParameters, sizeof(reverbParameters), XAUDIO2_COMMIT_NOW);
        }

        // 0x00C344D0 (:555)
        void setPlatformMasterLimiter(System*, float, float, float, float)
        {
        }

        // 0x00C344E0 (:560)
        void externalXaudio2VoiceCreated(System* pSoundSystem)
        {
            ++pSoundSystem->xaudio2State.externalXaudio2Voices;
        }

        // 0x00C344F0 (:565)
        void externalXaudio2VoiceDestroyed(System* pSoundSystem)
        {
            if (pSoundSystem->xaudio2State.externalXaudio2Voices != 0)
            {
                --pSoundSystem->xaudio2State.externalXaudio2Voices;
            }
        }
    }
}
