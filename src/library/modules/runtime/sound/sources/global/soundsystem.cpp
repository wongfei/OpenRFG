// Original: library/modules/runtime/sound/sources/global/soundsystem.cpp
//
// The platform independent part of the keen sound system (SFX.md SX1): the virtual sounds (BaseSound) and the voices
// they start on, priorities and count groups, the repetition memory, parameter automation (fades, pitch bends,
// fade-out-and-stop), pause groups, the busses with their volumes, ducking and effect chains, dynamic (temporary) sound
// definitions and durations.
//
// Float rule (rfg.exe's SSE2 code evaluates float expressions in double and rounds at each store): a single float
// operation is written as float; where two operations are fused before the store, the expression is written in double.
// The x87 parts (pow) take float arguments widened to double, like __CIpow.

#include "keen/sound/soundsystem.hpp"
#include "keen/sound/sounddefinition.hpp"
#include "keen/sound/soundfile.hpp"
#include "keen/base/memoryallocator.hpp"
#include "keen/base/trace.hpp"
#include "library/modules/runtime/sound/sources/global/soundsysteminternal.hpp"

#include <math.h>
#include <stdlib.h>
#include <string.h>

namespace keen
{
    namespace SoundSystem
    {
        // One bus that goes to the master out (-1); the other targets are nirvana (-2).
        static BusDefinition defaultBusDef[1] = {{{-1, -2, -2, -2}}}; // 0x016EFF40

        const int BusDefinition::masterOutBus = -1; // 0x012B7830
        const int BusDefinition::nirvana = -2; // 0x012B7834
        const uint32 BaseEffectDefinition::empty = 0x454E4F4E; // 0x012B7838 ('NONE')
        const uint32 BusImplementation::emptyEffectIndex = 0xFFFFFFFFu; // 0x01340B2C
        const uint8 SoundTriggerMemory::UnusedLastTriggeredIndex = 0xFF; // 0x01340B30

        // Effect ids (FourCCs as little-endian words).
        static const uint32 EffectId_None = 0x454E4F4E; // 'NONE'
        static const uint32 EffectId_Ducker = 0x4B435544; // 'DUCK'

        // Magic numbers: a sound handle is the sound index (14 bits) with bit 14 set, and the 14 bit run count in the high
        // word. 0xFFFF is the invalid handle.
        static const uint32 SoundIndexMask = 0x3FFF;
        static const uint32 InvalidSoundHandle = 0xFFFF;

        static inline BaseSound* getValidSound(System* pSystem, uint32 handle)
        {
            const uint32 index = handle & SoundIndexMask;
            if (index >= pSystem->sounds.getSize())
            {
                return nullptr;
            }
            BaseSound* pSound = &pSystem->sounds[index];
            if (((pSound->runCountAndFlags ^ (handle >> 16)) & SoundIndexMask) != 0)
            {
                return nullptr;
            }
            return pSound;
        }

        static inline uint32 makeSoundHandle(System* pSystem, BaseSound* pSound)
        {
            return (uint32)(pSound - pSystem->sounds.getStart()) | 0x4000u | ((uint32)(pSound->runCountAndFlags & SoundIndexMask) << 16);
        }

        // 64 bit "1 << index" as _allshl computes it (0 for 64 and up).
        static inline uint64 bit64(uint32 index)
        {
            return index < 64 ? (uint64)1 << index : 0;
        }

        // 0x00C33020 (:49)
        // Magic numbers: 32 voices and virtual sounds, 16 repetition entries.
        InitializationParametersBase::InitializationParametersBase()
        {
            voiceCount = 0x20u;
            virtualSoundCount = 0x20u;
            streamCount = 0;
            pStreamInterface = nullptr;
            pBusses = defaultBusDef;
            busCount = 1;
            temporarySoundDefinitionCount = 0;
            repetitionMemoryMaxCount = 0x10u;
        }

        // 0x00458AB0 (soundsystem.hpp, inline)
        InitializationParameters::InitializationParameters()
        {
        }

        // 0x00482F10 (soundsystem.hpp:166)
        SoundDefinitionParameter3d::SoundDefinitionParameter3d()
        {
            minDistance = 1.0f;
            maxDistance = 50.0f;
            positionVariation = 0.0f;
        }

        // 0x00C35B30 (:62)
        EffectChainSetup::EffectChainSetup()
        {
            for (uint32 i = 0; i < 4; ++i)
            {
                pDefinitions[i] = nullptr;
                gains[i] = 0.0f;
            }
        }

        // 0x00C36D90
        SoundDefinition::SoundDefinition()
        {
        }

        // 0x00C36DA0
        SoundDefinition::~SoundDefinition()
        {
        }

        // 0x00C36DF0
        Systembase::Systembase()
        {
        }

        // 0x00C36E40
        Systembase::~Systembase()
        {
        }

        // 0x00C39120
        System::System()
        {
        }

        // 0x00C33050 (:74)
        float calculateEnergyFromDecibel(float decibel)
        {
            const float exponent = (float)(decibel / 20.0);
            return (float)pow(10.0, (double)exponent);
        }

        // 0x00C33080 (:?)
        // LTCG: pSound in ECX, pSoundSystem unused.
        static void freeSound(System* pSoundSystem, BaseSound* pSound)
        {
            (void)pSoundSystem;
            // The whole word is incremented: a run count overflow carries into the flag bits (original).
            ++pSound->runCountAndFlags;
            pSound->definition = nullptr;
            pSound->pSoundFileData = nullptr;
            pSound->pauseGroup = 0xFF;
            pSound->voiceIndex = 0xFF;
            ParameterAutomation* pAutomation = pSound->pAutomation;
            while (pAutomation != nullptr)
            {
                ParameterAutomation* pNext = pAutomation->pNext;
                pAutomation->command = ParameterAutomation::Command_Nop;
                pAutomation->pNext = nullptr;
                pAutomation = pNext;
            }
            pSound->pAutomation = nullptr;
        }

        // 0x00C35B50
        // LTCG: pSound in EAX, pSoundSystem in ECX.
        static VoiceSound* getVoiceSoundFromSound(System* pSoundSystem, BaseSound* pSound)
        {
            if (pSound->voiceIndex < pSoundSystem->voiceSounds.getSize())
            {
                return &pSoundSystem->voiceSounds[pSound->voiceIndex];
            }
            return nullptr;
        }

        // 0x00C35B70
        // LTCG: pSound in EAX, pSystem in ECX.
        static ParameterAutomation* allocateParameterAutomation(System* pSystem, BaseSound* pSound)
        {
            if (pSound->pAutomation != nullptr && pSound->pAutomation->command == ParameterAutomation::Command_FadeoutAndStop)
            {
                debug::nativePrintArgs("[soundSystem] Unabled to allocate Parameter automation.");
                return nullptr;
            }
            for (ParameterAutomation* pAutomation = pSystem->automations.getStart(); pAutomation != pSystem->automations.getEnd(); ++pAutomation)
            {
                if (pAutomation->command == ParameterAutomation::Command_Nop)
                {
                    pAutomation->pNext = nullptr;
                    return pAutomation;
                }
            }
            return nullptr;
        }

        // 0x00C330C0
        // LTCG: pAutomation in EAX, pSound in ECX.
        static void insertParameterAutomation(BaseSound* pSound, ParameterAutomation* pAutomation)
        {
            pAutomation->init = true;
            pAutomation->pNext = pSound->pAutomation;
            pSound->pAutomation = pAutomation;
        }

        // 0x00C330D0
        // LTCG: pSound in ESI, commandType in EDX.
        static void clearParameterAutomation(BaseSound* pSound, ParameterAutomation::Command commandType)
        {
            ParameterAutomation* pPrevious = nullptr;
            ParameterAutomation* pAutomation = pSound->pAutomation;
            while (pAutomation != nullptr)
            {
                if (pAutomation->command == commandType)
                {
                    if (pPrevious != nullptr)
                    {
                        pPrevious->pNext = pAutomation->pNext;
                    }
                    else
                    {
                        pSound->pAutomation = pAutomation->pNext;
                    }
                    pAutomation->command = ParameterAutomation::Command_Nop;
                    pAutomation->pNext = nullptr;
                    return;
                }
                pPrevious = pAutomation;
                pAutomation = pAutomation->pNext;
            }
        }

        // 0x00C35BD0
        // LTCG: pSound in ESI, commandType in EDI, pSystem on the stack.
        static ParameterAutomation* findOrAllocateParameterAutomation(System* pSystem, BaseSound* pSound, ParameterAutomation::Command commandType)
        {
            for (ParameterAutomation* pAutomation = pSound->pAutomation; pAutomation != nullptr; pAutomation = pAutomation->pNext)
            {
                if (pAutomation->command == commandType)
                {
                    return pAutomation;
                }
            }
            ParameterAutomation* pAutomation = allocateParameterAutomation(pSystem, pSound);
            if (pAutomation != nullptr)
            {
                insertParameterAutomation(pSound, pAutomation);
                pAutomation->command = commandType;
            }
            return pAutomation;
        }

        // 0x00C33120
        // LTCG: pSoundFileData in EAX.
        static float getSoundDuration(const SoundFileData* pSoundFileData)
        {
            if (pSoundFileData != nullptr)
            {
                return (float)((double)pSoundFileData->durationInms / 1000.0);
            }
            return 0.0f;
        }

        // 0x00C33150
        // LTCG: pSound in ESI, timeStep in XMM2.
        static void updateParameterAutomation(BaseSound* pSound, float timeStep)
        {
            // Magic numbers: an automation with no duration moves at 1e8 per second (instantly).
            ParameterAutomation** ppLink = &pSound->pAutomation;
            ParameterAutomation* pAutomation = pSound->pAutomation;
            while (pAutomation != nullptr)
            {
                float* pValue;
                switch (pAutomation->command)
                {
                case ParameterAutomation::Command_FadeVolume:
                case ParameterAutomation::Command_FadeoutAndStop:
                    pValue = &pSound->volume;
                    break;
                case ParameterAutomation::Command_PitchBend:
                    pValue = &pSound->pitch;
                    break;
                default:
                    return;
                }

                // The first update turns the duration into a velocity.
                if (pAutomation->init == true)
                {
                    if (pAutomation->velocity > 0.0f)
                    {
                        pAutomation->velocity = (float)(((double)pAutomation->targetValue - (double)*pValue) / (double)pAutomation->velocity);
                    }
                    else
                    {
                        pAutomation->velocity = pAutomation->targetValue > *pValue ? 1e8f : -1e8f;
                    }
                    pAutomation->init = false;
                }

                const float current = *pValue;
                const bool rising = pAutomation->targetValue > current;
                const float newValue = (float)((double)pAutomation->velocity * (double)timeStep + (double)current);
                *pValue = newValue;
                const bool reached = rising ? !(newValue < pAutomation->targetValue) : !(pAutomation->targetValue < newValue);
                if (reached)
                {
                    *pValue = pAutomation->targetValue;
                    // Fade-out-and-stop stays in the list: updateBase stops the sound once its volume is 0.
                    if (pAutomation->command != ParameterAutomation::Command_FadeoutAndStop)
                    {
                        *ppLink = pAutomation->pNext;
                        pAutomation->command = ParameterAutomation::Command_Nop;
                        pAutomation->pNext = nullptr;
                        pAutomation = *ppLink;
                        continue;
                    }
                }
                ppLink = &pAutomation->pNext;
                pAutomation = pAutomation->pNext;
            }
        }

        // 0x00C39740 (:301)
        void initializeBase(System* pSystem, MemoryAllocator* pMemoryAllocator, const InitializationParametersBase& params)
        {
            pSystem->soundOutputDisabled = false;

            // Magic numbers: at most 253 voices (voice indices 0xFE/0xFF are the ended/no-voice markers).
            uint32 voiceCount = params.voiceCount;
            if (voiceCount > 0xFC)
            {
                voiceCount = 0xFD;
            }
            pSystem->voiceSounds.create(pMemoryAllocator, voiceCount, 0x10);
            pSystem->sounds.create(pMemoryAllocator, params.virtualSoundCount, 0x10);

            pSystem->allocatedPauseGroups[0] = 0;
            for (uint32 i = 0; i < 8; ++i)
            {
                pSystem->soundProviders[i] = nullptr;
            }
            pSystem->nextPauseGroup = 0;
            pSystem->currentTimeInms = 0;

            // Magic numbers: automations for 60% of the virtual sounds.
            pSystem->automations.create(pMemoryAllocator, (params.virtualSoundCount * 60) / 100, 0x10);
            for (uint32 i = 0; i < pSystem->automations.getSize(); ++i)
            {
                pSystem->automations[i].command = ParameterAutomation::Command_Nop;
                pSystem->automations[i].pNext = nullptr;
            }

            pSystem->listenerMatrix = Matrix43::getUnit();
            pSystem->inverseListenerMatrix.invert(pSystem->listenerMatrix);

            pSystem->busCount = params.busCount;
            // All 30 busses read a definition from pBusses, whatever busCount is: past busCount this reads beyond the
            // caller's array (original bug; rfg::Application passes two). Those busses are never created.
            for (uint32 i = 0; i < 30; ++i)
            {
                BusImplementation& bus = pSystem->busses[i];
                bus.definition = params.pBusses[i];
                bus.gain.currentValue = 0.0f;
                bus.gain.targetValue = 0.0f;
                bus.gain.speed = 0.0f;
                for (uint32 j = 0; j < 4; ++j)
                {
                    bus.drySendVolumes[j].currentValue = 1.0f;
                    bus.drySendVolumes[j].targetValue = 1.0f;
                    bus.drySendVolumes[j].speed = 0.0f;
                    bus.wetSendVolumes[j].currentValue = 1.0f;
                    bus.wetSendVolumes[j].targetValue = 1.0f;
                    bus.wetSendVolumes[j].speed = 0.0f;
                }
                bus.duckingGain.currentValue = 1.0f;
                bus.duckingGain.targetValue = 1.0f;
                bus.duckingGain.speed = 0.0f;
                for (uint32 j = 0; j < 4; ++j)
                {
                    bus.effectIndices[j] = BusImplementation::emptyEffectIndex;
                }
            }
            for (uint32 i = 0; i < 16; ++i)
            {
                pSystem->effects[i].effectId = EffectId_None;
                pSystem->effects[i].pPlatformData = nullptr;
            }

            // Magic numbers: at most 64 temporary definitions (createDynamicSoundDefinition keeps 64 bit masks).
            uint32 temporaryCount = params.temporarySoundDefinitionCount;
            if (temporaryCount > 0x3F)
            {
                temporaryCount = 0x40;
            }
            pSystem->tempSoundDefinitions.create(pMemoryAllocator, temporaryCount, 0x10);
            for (uint32 i = 0; i < pSystem->tempSoundDefinitions.getSize(); ++i)
            {
                pSystem->tempSoundDefinitions[i].create();
            }

            pSystem->repetitionMemory.create(pMemoryAllocator, params.repetitionMemoryMaxCount, 0x10);
            memset(pSystem->repetitionMemory.getStart(), 0, pSystem->repetitionMemory.getSize() * sizeof(SoundTriggerMemory));
        }

        // 0x00C39B10 (:376)
        void shutdownBase(System* pSystem, MemoryAllocator* pAllocator)
        {
            pSystem->repetitionMemory.destroy(pAllocator);
            pSystem->automations.destroy(pAllocator);
            pSystem->sounds.destroy(pAllocator);
            pSystem->voiceSounds.destroy(pAllocator);
            if (pSystem->tempSoundDefinitions.getStart() != nullptr)
            {
                for (uint32 i = pSystem->tempSoundDefinitions.getSize(); i != 0; --i)
                {
                    pSystem->tempSoundDefinitions[i - 1].~TemporarySoundDefinition();
                }
                pSystem->tempSoundDefinitions.destroy(pAllocator);
            }
        }

        // 0x00C35C10
        // LTCG: pSystem in ECX.
        static VoiceSound* findFreeVoice(Systembase* pSystem)
        {
            for (VoiceSound* pVoice = pSystem->voiceSounds.getStart(); pVoice != pSystem->voiceSounds.getEnd(); ++pVoice)
            {
                if (pVoice->m_providerType == 0xFF)
                {
                    return pVoice;
                }
            }
            return nullptr;
        }

        // 0x00C35C40
        // LTCG: pSystem in ECX.
        // A ducker effect on a bus lowers that bus while the summed sound volume on its ducking bus, scaled by that bus's
        // gain, is at or above the threshold.
        static void updateDucking(System* pSystem)
        {
            float busVolumes[30];
            memset(busVolumes, 0, sizeof(busVolumes));
            for (BaseSound* pSound = pSystem->sounds.getStart(); pSound != pSystem->sounds.getEnd(); ++pSound)
            {
                if (pSound->definition == nullptr)
                {
                    continue;
                }
                const SoundDefinition* pDefinition = getDefinition(pSound->definition);
                if (pDefinition == nullptr)
                {
                    continue;
                }
                uint32 bus = pDefinition->output_bus;
                if (bus >= 29)
                {
                    bus = 29;
                }
                busVolumes[bus] = pSound->volume + busVolumes[bus];
            }

            // All 30 busses, not only busCount.
            for (uint32 i = 0; i < 30; ++i)
            {
                BusImplementation& bus = pSystem->busses[i];
                for (uint32 j = 0; j < 4; ++j)
                {
                    const uint32 effectIndex = bus.effectIndices[j];
                    if (effectIndex == BusImplementation::emptyEffectIndex)
                    {
                        break;
                    }
                    const EffectStorage& effect = pSystem->effects[effectIndex];
                    if (effect.effectId != EffectId_Ducker)
                    {
                        continue;
                    }
                    const FXDuckerParameter* pParameter = (const FXDuckerParameter*)effect.opaqueParameterData;
                    if (pParameter->duckingBus >= 30)
                    {
                        continue;
                    }
                    const float exponent = (float)(pSystem->busses[pParameter->duckingBus].gain.currentValue / 20.0);
                    const float energy = (float)pow(10.0, (double)exponent);
                    // x87 compare of the unrounded product.
                    if ((double)energy * (double)busVolumes[pParameter->duckingBus] < (double)pParameter->threshold)
                    {
                        if (bus.duckingGain.targetValue != 1.0f)
                        {
                            const float release = pParameter->release;
                            bus.duckingGain.targetValue = 1.0f;
                            if (release > 0.0f)
                            {
                                bus.duckingGain.speed = (float)((1.0 - (double)bus.duckingGain.currentValue) / (double)release);
                            }
                            else
                            {
                                bus.duckingGain.currentValue = 1.0f;
                                bus.duckingGain.speed = 0.0f;
                            }
                        }
                    }
                    else
                    {
                        const float gain = pParameter->gain;
                        if (bus.duckingGain.targetValue != gain)
                        {
                            const float attack = pParameter->attack;
                            bus.duckingGain.targetValue = gain;
                            if (attack > 0.0f)
                            {
                                bus.duckingGain.speed = (float)(((double)gain - (double)bus.duckingGain.currentValue) / (double)attack);
                            }
                            else
                            {
                                bus.duckingGain.currentValue = gain;
                                bus.duckingGain.speed = 0.0f;
                            }
                        }
                    }
                }
            }
        }

        static void updateBusses(System* pSystem, float timeStep);
        static void stopSound(System* pSystem, BaseSound* pSound, float fadeoutTime);

        // 0x00C38F90 (:523)
        void updateBase(System* pSystem, float timeStep)
        {
            const uint32 timeStepInms = (uint32)(sint64)(float)(timeStep * 1000.0);
            pSystem->currentTimeInms += timeStepInms;
            if (pSystem->soundOutputDisabled == true)
            {
                return;
            }
            updateDucking(pSystem);
            updateBusses(pSystem, timeStep);

            for (BaseSound* pSound = pSystem->sounds.getStart(); pSound != pSystem->sounds.getEnd(); ++pSound)
            {
                if (pSound->definition == nullptr)
                {
                    continue;
                }
                updateParameterAutomation(pSound, timeStep);
                if (pSound->pAutomation != nullptr && pSound->pAutomation->command == ParameterAutomation::Command_FadeoutAndStop && pSound->volume == 0.0f)
                {
                    stopSound(pSystem, pSound, 0.0f);
                    continue;
                }
                if (pSound->voiceIndex == BaseSound::InvalidVoice)
                {
                    // Key on: put the sound on a free voice of its file's provider.
                    if ((pSound->runCountAndFlags & BaseSound::Flag_KeyOn) == 0)
                    {
                        continue;
                    }
                    VoiceSound* pVoice = findFreeVoice(pSystem);
                    if (pVoice == nullptr)
                    {
                        continue;
                    }
                    pSound->voiceIndex = (uint8)(pVoice - pSystem->voiceSounds.getStart());
                    pVoice->m_pBaseSound = pSound;
                    const uint8 providerId = pSound->pSoundFileData->providerId;
                    SoundProvider* pProvider = pSystem->soundProviders[providerId];
                    if (pProvider != nullptr && pProvider->startSound(pVoice, pSound->initialPitch, 0, pSystem) == true)
                    {
                        pVoice->m_providerType = providerId;
                    }
                    else
                    {
                        pVoice->free();
                        freeSound(pSystem, pSound);
                    }
                }
                else if (pSound->voiceIndex == BaseSound::InvalidVoiceSoundEnded)
                {
                    stopSound(pSystem, pSound, 0.0f);
                }
            }
        }

        // 0x00C33270 (:640)
        void registerSoundProvider(System* pSystem, uint8 providerId, SoundProvider* pProvider)
        {
            pSystem->soundProviders[providerId] = pProvider;
        }

        // 0x00C33290
        // LTCG: pSystem in ESI.
        static uint8 allocatePauseGroup(Systembase* pSystem)
        {
            int group = pSystem->nextPauseGroup;
            if ((pSystem->allocatedPauseGroups[group / 32] & (1u << (group & 31))) != 0)
            {
                // Magic numbers: 32 pause groups; when all are taken the 32nd candidate is reused.
                int tries = 32;
                do
                {
                    group = (group + 1) & 31;
                    if (--tries == 0)
                    {
                        break;
                    }
                } while ((pSystem->allocatedPauseGroups[group / 32] & (1u << (group & 31))) != 0);
            }
            pSystem->allocatedPauseGroups[group / 32] |= 1u << (group & 31);
            pSystem->nextPauseGroup = (group + 1) & 31;
            return (uint8)group;
        }

        // 0x00C33320
        // LTCG: group in ECX.
        static void freePauseGroup(Systembase* pSystem, int group)
        {
            pSystem->allocatedPauseGroups[group / 32] &= ~(1u << (group & 31));
        }

        // 0x00C37C90
        static BaseSound* startSoundInternal(System* pSystem, const SoundDefinitionResourceHandleType* sound, uint32 soundFlags)
        {
            const SoundDefinition* pDefinition = getDefinition(sound);
            const SoundFileData* pSoundFileData = getDataFromSound(&pSystem->randomNumberGenerator, *pDefinition, pSystem);
            if (pSoundFileData == nullptr)
            {
                return nullptr;
            }
            BaseSound* pSound = findFreeBaseSound(pSystem, pDefinition->priority, pDefinition->countGroup, pDefinition->maxCount, sound);
            if (pSound == nullptr)
            {
                return nullptr;
            }
            pSound->definition = sound;
            uint16 flags = 0;
            if ((soundFlags & SoundFlag_StartPaused) == 0)
            {
                flags = BaseSound::Flag_KeyOn;
            }
            if ((soundFlags & SoundFlag_Loop) != 0)
            {
                flags |= BaseSound::Flag_Looped;
            }
            pSound->runCountAndFlags = (uint16)((pSound->runCountAndFlags & BaseSound::Flag_RunCountMask) | flags);

            // The volume in dB (with variation) as energy: calculateEnergyFromDecibel inlined.
            float volume = pDefinition->volume;
            if (pDefinition->volumeVariant != 0.0f)
            {
                volume = pSystem->randomNumberGenerator.getRandomValue(-pDefinition->volumeVariant, pDefinition->volumeVariant) + volume;
            }
            const float exponent = (float)(volume / 20.0);
            pSound->volume = (float)pow(10.0, (double)exponent);
            const float pitch = calculateSampleRateMultiplier(&pSystem->randomNumberGenerator, *pDefinition);
            pSound->pitch = pitch;
            pSound->pSoundFileData = pSoundFileData;
            pSound->initialVolume = pSound->volume;
            pSound->initialPitch = pitch;
            pSound->startTimeInms = pSystem->currentTimeInms;
            if ((soundFlags & SoundFlag_StartSilent) != 0)
            {
                pSound->volume = 0.0f;
            }
            return pSound;
        }

        // 0x00C37DD0 (:860)
        uint32 startSound(System* pSystem, const SoundDefinitionResourceHandleType* const sound, float panning, float frontBack, uint32 soundFlags)
        {
            BaseSound* pSound = startSoundInternal(pSystem, sound, soundFlags);
            if (pSound == nullptr)
            {
                return InvalidSoundHandle;
            }
            pSound->position.x = panning;
            pSound->position.y = frontBack;
            pSound->position.z = 0.0f;
            return makeSoundHandle(pSystem, pSound);
        }

        // 0x00C37E50 (:876)
        uint32 startSound(System* pSystem, const SoundDefinitionResourceHandleType* const sound, uint32 soundFlags)
        {
            BaseSound* pSound = startSoundInternal(pSystem, sound, soundFlags);
            if (pSound == nullptr)
            {
                return InvalidSoundHandle;
            }
            const SoundDefinition* pDefinition = getDefinition(sound);
            pSound->position.x = pDefinition->panAngle;
            pSound->position.z = 0.0f;
            pSound->position.y = pDefinition->panDistance;
            return makeSoundHandle(pSystem, pSound);
        }

        // 0x00C37EC0 (:893)
        uint32 startSound(System* pSystem, const SoundDefinitionResourceHandleType* const sound, const Vector3& position, uint32 soundFlags)
        {
            BaseSound* pSound = startSoundInternal(pSystem, sound, soundFlags);
            if (pSound == nullptr)
            {
                return InvalidSoundHandle;
            }
            pSound->position.x = position.x;
            pSound->position.y = position.y;
            pSound->position.z = position.z;
            return makeSoundHandle(pSystem, pSound);
        }

        // 0x00C35E60
        // LTCG: pSystem in EDI.
        static void stopSound(System* pSystem, BaseSound* pSound, float fadeoutTime)
        {
            if (fadeoutTime != 0.0f)
            {
                ParameterAutomation* pAutomation = allocateParameterAutomation(pSystem, pSound);
                if (pAutomation != nullptr)
                {
                    pAutomation->targetValue = 0.0f;
                    pAutomation->command = ParameterAutomation::Command_FadeoutAndStop;
                    pAutomation->velocity = fadeoutTime;
                    insertParameterAutomation(pSound, pAutomation);
                    return;
                }
            }
            if (pSound->definition == nullptr)
            {
                return;
            }
            VoiceSound* pVoice = getVoiceSoundFromSound(pSystem, pSound);
            if (pVoice != nullptr)
            {
                pSystem->soundProviders[pVoice->m_providerType]->stopSound(pVoice, pSystem);
                pVoice->free();
                pSound->voiceIndex = BaseSound::InvalidVoice;
            }
            freeSound(pSystem, pSound);
        }

        // 0x00C37F60 (:969)
        bool stopSound(System* pSystem, uint32 sound, float fadeoutTime)
        {
            BaseSound* pSound = getValidSound(pSystem, sound);
            if (pSound == nullptr)
            {
                return false;
            }
            stopSound(pSystem, pSound, fadeoutTime);
            return true;
        }

        // 0x00C35F00 (:989)
        void stopAllSounds(System* pSystem)
        {
            for (BaseSound* pSound = pSystem->sounds.getStart(); pSound != pSystem->sounds.getEnd(); ++pSound)
            {
                if (pSound->definition != nullptr)
                {
                    stopSound(pSystem, pSound, 0.0f);
                }
            }
        }

        // 0x00C35F60 (:1002)
        void stopAllSounds(System* pSystem, uint8 resourceTag)
        {
            for (BaseSound* pSound = pSystem->sounds.getStart(); pSound != pSystem->sounds.getEnd(); ++pSound)
            {
                if (pSound->definition != nullptr && getFromHandle(pSound->definition)->getTag() == resourceTag)
                {
                    stopSound(pSystem, pSound, 0.0f);
                }
            }
        }

        // 0x00C37FC0 (:1037)
        bool setPitch(System* pSystem, uint32 handle, float pitch, float duration)
        {
            BaseSound* pSound = getValidSound(pSystem, handle);
            if (pSound == nullptr || pSound->definition == nullptr)
            {
                return false;
            }
            const float targetPitch = pSound->initialPitch * pitch;
            if (duration > 0.0f)
            {
                ParameterAutomation* pAutomation = allocateParameterAutomation(pSystem, pSound);
                if (pAutomation != nullptr)
                {
                    pAutomation->targetValue = targetPitch;
                    pAutomation->command = ParameterAutomation::Command_PitchBend;
                    pAutomation->velocity = duration;
                    insertParameterAutomation(pSound, pAutomation);
                }
            }
            else
            {
                pSound->pitch = targetPitch;
            }
            return true;
        }

        // 0x00C38080 (:1079)
        bool setVolume(System* pSystem, uint32 sound, float volume, float duration)
        {
            BaseSound* pSound = getValidSound(pSystem, sound);
            if (pSound == nullptr || pSound->definition == nullptr)
            {
                return false;
            }
            const float targetVolume = pSound->initialVolume * volume;
            if (duration > 0.0f)
            {
                ParameterAutomation* pAutomation = findOrAllocateParameterAutomation(pSystem, pSound, ParameterAutomation::Command_FadeVolume);
                if (pAutomation != nullptr)
                {
                    pAutomation->targetValue = targetVolume;
                    pAutomation->init = true;
                    pAutomation->velocity = duration;
                    return true;
                }
                // No automation left: the volume is set at once, but only when other automations run and the first is
                // not a fade-out (original).
                if (pSound->pAutomation != nullptr && pSound->pAutomation->command != ParameterAutomation::Command_FadeoutAndStop)
                {
                    pSound->volume = targetVolume;
                }
            }
            else
            {
                clearParameterAutomation(pSound, ParameterAutomation::Command_FadeVolume);
                pSound->volume = targetVolume;
            }
            return true;
        }

        // 0x00C38180 (:1128)
        float getVolume(System* pSystem, uint32 sound)
        {
            BaseSound* pSound = getValidSound(pSystem, sound);
            if (pSound == nullptr)
            {
                return 0.0f;
            }
            return pSound->volume;
        }

        // 0x00C381C0 (:1147)
        bool isSoundPlaying(System* pSystem, uint32 handle)
        {
            BaseSound* pSound = getValidSound(pSystem, handle);
            if (pSound == nullptr)
            {
                return false;
            }
            return pSound->definition != nullptr;
        }

        // 0x00C38200 (:1159)
        bool setPanning(System* pSystem, uint32 handle, float panning)
        {
            BaseSound* pSound = getValidSound(pSystem, handle);
            if (pSound == nullptr)
            {
                return false;
            }
            // No check for a free sound: a stale but matching handle reads a null definition (original).
            if (getDefinition(pSound->definition)->positioning == Positioning_listener)
            {
                pSound->position.x = panning;
            }
            return true;
        }

        // 0x00C33350 (:1183)
        void setListenerPosition(System* pSystem, const Matrix43& worldMatrix)
        {
            pSystem->listenerMatrix = worldMatrix;
            pSystem->inverseListenerMatrix.invert(pSystem->listenerMatrix);
        }

        // 0x00C333A0 (:1189)
        const Matrix43& getListenerPosition(const System* pSystem)
        {
            return pSystem->listenerMatrix;
        }

        // 0x00C38250 (:1203)
        bool setSoundPosition(System* pSystem, uint32 handle, const Vector3& position)
        {
            BaseSound* pSound = getValidSound(pSystem, handle);
            if (pSound == nullptr)
            {
                return false;
            }
            pSound->position.x = position.x;
            pSound->position.y = position.y;
            pSound->position.z = position.z;
            return true;
        }

        // 0x00C382D0 (:1231)
        uint8 pauseAllPlayingSounds(System* pSystem)
        {
            const uint8 pauseGroup = allocatePauseGroup(pSystem);
            for (uint32 i = 0; i < pSystem->sounds.getSize(); ++i)
            {
                BaseSound* pSound = &pSystem->sounds[i];
                if (pSound->pauseGroup == 0xFF && pSound->definition != nullptr)
                {
                    VoiceSound* pVoice = getVoiceSoundFromSound(pSystem, pSound);
                    if (pVoice != nullptr)
                    {
                        pSystem->soundProviders[pVoice->m_providerType]->pauseSound(pVoice);
                    }
                    pSound->pauseGroup = pauseGroup;
                }
            }
            return pauseGroup;
        }

        // 0x00C38350 (:1258)
        void resumeSounds(System* pSystem, uint8 pauseGroup)
        {
            if (pauseGroup == 0xFF)
            {
                return;
            }
            for (uint32 i = 0; i < pSystem->sounds.getSize(); ++i)
            {
                BaseSound* pSound = &pSystem->sounds[i];
                if (pSound->pauseGroup == pauseGroup)
                {
                    pSound->pauseGroup = 0xFF;
                    if (pSound->definition != nullptr)
                    {
                        VoiceSound* pVoice = getVoiceSoundFromSound(pSystem, pSound);
                        if (pVoice != nullptr)
                        {
                            pSystem->soundProviders[pVoice->m_providerType]->resumeSound(pVoice);
                        }
                    }
                }
            }
            freePauseGroup(pSystem, pauseGroup);
        }

        // 0x00C35FD0 (:1287)
        void stopPausedSounds(System* pSystem, uint8 pauseGroup)
        {
            if (pauseGroup == 0xFF)
            {
                return;
            }
            for (uint32 i = 0; i < pSystem->sounds.getSize(); ++i)
            {
                BaseSound* pSound = &pSystem->sounds[i];
                if (pSound->definition != nullptr && pSound->pauseGroup == pauseGroup)
                {
                    stopSound(pSystem, pSound, 0.0f);
                }
            }
            freePauseGroup(pSystem, pauseGroup);
        }

        // 0x00C36050
        // LTCG: pBaseSound in EAX, pSystem in ECX.
        static void pauseSound(System* pSystem, BaseSound* pBaseSound)
        {
            if (pBaseSound->definition == nullptr)
            {
                return;
            }
            VoiceSound* pVoice = getVoiceSoundFromSound(pSystem, pBaseSound);
            if (pVoice != nullptr)
            {
                pSystem->soundProviders[pVoice->m_providerType]->pauseSound(pVoice);
            }
        }

        // 0x00C36090
        // LTCG: pBaseSound in EAX, pSystem in ECX.
        static void resumeSound(System* pSystem, BaseSound* pBaseSound)
        {
            if (pBaseSound->definition == nullptr)
            {
                return;
            }
            VoiceSound* pVoice = getVoiceSoundFromSound(pSystem, pBaseSound);
            if (pVoice != nullptr)
            {
                pSystem->soundProviders[pVoice->m_providerType]->resumeSound(pVoice);
            }
        }

        // 0x00C383F0 (:1306)
        bool pauseSound(System* pSystem, uint32 handle)
        {
            BaseSound* pSound = getValidSound(pSystem, handle);
            if (pSound == nullptr)
            {
                return false;
            }
            pauseSound(pSystem, pSound);
            return true;
        }

        // 0x00C38460 (:1317)
        bool resumeSound(System* pSystem, uint32 handle)
        {
            BaseSound* pSound = getValidSound(pSystem, handle);
            if (pSound == nullptr)
            {
                return false;
            }
            resumeSound(pSystem, pSound);
            return true;
        }

        // 0x00C360D0 (:1362)
        // Picks one of the definition's files by chance. With avoidRepeatingLastX the repetition memory remembers the
        // last picks of the definition (up to 8) and excludes them.
        const SoundFileData* getDataFromSound(RandomNumberGenerator* pRandomizer, const SoundDefinition& sound, System* pSoundSystem)
        {
            const uint32 fileCount = sound.soundFiles.getSize();
            if (fileCount < 2)
            {
                if (fileCount != 1)
                {
                    return nullptr;
                }
                // A single file is taken without the null check of the other paths (original).
                return &getFromHandle(sound.soundFiles[0].soundFile)->m_data;
            }

            uint32 index = 0;
            if (sound.avoidRepeatingLastX == 0)
            {
                float chanceSum = 0.0f;
                for (uint32 i = 0; i < fileCount; ++i)
                {
                    chanceSum = sound.soundFiles[i].chance + chanceSum;
                }
                const float value = pRandomizer->getRandomValue(0.0f, chanceSum);
                float chance = 0.0f;
                for (uint32 i = 0; i < sound.soundFiles.getSize(); ++i)
                {
                    chance = sound.soundFiles[i].chance + chance;
                    if (value <= chance)
                    {
                        index = i;
                        break;
                    }
                }
            }
            else if (sound.avoidRepeatingLastX < fileCount)
            {
                // The definition's memory entry, or else the one triggered longest ago (the last of equal ages).
                SoundTriggerMemory* pMemory = nullptr;
                bool isNew = true;
                uint32 oldestAge = 0;
                for (SoundTriggerMemory* pEntry = pSoundSystem->repetitionMemory.getStart(); pEntry != pSoundSystem->repetitionMemory.getEnd(); ++pEntry)
                {
                    if (pEntry->pSoundDefinition == &sound)
                    {
                        isNew = false;
                        pMemory = pEntry;
                        break;
                    }
                    const uint32 age = pSoundSystem->currentTimeInms - pEntry->lastTriggerTimeInms;
                    if (age >= oldestAge)
                    {
                        pMemory = pEntry;
                        oldestAge = age;
                    }
                }
                if (pMemory != nullptr)
                {
                    pMemory->pSoundDefinition = &sound;
                    uint64 allowedMask = ~(uint64)0;
                    pMemory->lastTriggerTimeInms = pSoundSystem->currentTimeInms;
                    if (isNew)
                    {
                        memset(pMemory->lastTriggeredIndex, SoundTriggerMemory::UnusedLastTriggeredIndex, sizeof(pMemory->lastTriggeredIndex));
                    }
                    else
                    {
                        for (uint32 i = 0; i < 8; ++i)
                        {
                            const uint8 lastIndex = pMemory->lastTriggeredIndex[i];
                            if (lastIndex == SoundTriggerMemory::UnusedLastTriggeredIndex)
                            {
                                break;
                            }
                            // int shift widened to 64 bits as in the original: index 31 also clears the high word, 32 and
                            // up wrap to the low word.
                            allowedMask &= (uint64)(sint64)~(1 << (lastIndex & 31));
                        }
                        for (uint32 i = 7; i != 0; --i)
                        {
                            pMemory->lastTriggeredIndex[i] = pMemory->lastTriggeredIndex[i - 1];
                        }
                    }

                    float chanceSum = 0.0f;
                    for (uint32 i = 0; i < fileCount; ++i)
                    {
                        if ((bit64(i) & allowedMask) != 0)
                        {
                            chanceSum = sound.soundFiles[i].chance + chanceSum;
                        }
                    }
                    const float value = pRandomizer->getRandomValue(0.0f, chanceSum);
                    float chance = 0.0f;
                    for (uint32 i = 0; i < sound.soundFiles.getSize(); ++i)
                    {
                        if ((bit64(i) & allowedMask) != 0)
                        {
                            chance = sound.soundFiles[i].chance + chance;
                            if (value <= chance)
                            {
                                index = i;
                                break;
                            }
                        }
                    }
                    pMemory->lastTriggeredIndex[0] = (uint8)index;
                    if (sound.avoidRepeatingLastX < 7)
                    {
                        pMemory->lastTriggeredIndex[sound.avoidRepeatingLastX] = SoundTriggerMemory::UnusedLastTriggeredIndex;
                    }
                }
            }

            const SoundFileHandleType* pFile = sound.soundFiles[index].soundFile;
            if (pFile == nullptr)
            {
                return nullptr;
            }
            return &getFromHandle(pFile)->m_data;
        }

        // 0x00C36540 (:1536)
        uint32 getSoundHandle(System* pSoundSystem, BaseSound* pSound)
        {
            if (pSound == nullptr)
            {
                return InvalidSoundHandle;
            }
            return makeSoundHandle(pSoundSystem, pSound);
        }

        // 0x00C36590 (:1552)
        BaseSound* findBaseSoundByHandle(System* pSystem, uint32 handle)
        {
            return getValidSound(pSystem, handle);
        }

        // 0x00C365D0 (:1570)
        // A free sound, or else the sound of the lowest priority below the new one (stopped). None when the count group
        // is full or the same definition started less than minimumDelay * 10 ms ago.
        BaseSound* findFreeBaseSound(System* pSoundSystem, signed char priority, signed char countGroup, uint8 maxCount, const SoundDefinitionResourceHandleType* definition)
        {
            // Magic numbers: minimumDelay is in units of 10 ms.
            const uint32 minimumDelayInms = (uint32)getDefinition(definition)->minimumDelay * 10;
            int groupCount = 0;
            BaseSound* pFreeSound = nullptr;
            BaseSound* pLowestPrioritySound = nullptr;
            signed char lowestPriority = 0x7F;
            BaseSound* pNewestSameSound = nullptr;
            uint32 newestSameAge = 0xFFFFFFFFu;

            for (BaseSound* pSound = pSoundSystem->sounds.getStart(); pSound != pSoundSystem->sounds.getEnd(); ++pSound)
            {
                if (pSound->definition == nullptr)
                {
                    if (pFreeSound == nullptr)
                    {
                        pFreeSound = pSound;
                    }
                    continue;
                }
                const SoundDefinition* pDefinition = getDefinition(pSound->definition);
                const bool inGroup = countGroup < 0 ? pSound->definition == definition : pDefinition->countGroup == countGroup;
                if (inGroup)
                {
                    ++groupCount;
                }
                if (pDefinition->priority < priority && pDefinition->priority < lowestPriority)
                {
                    lowestPriority = pDefinition->priority;
                    pLowestPrioritySound = pSound;
                }
                if (minimumDelayInms != 0 && pSound->definition == definition)
                {
                    const uint32 age = pSoundSystem->currentTimeInms - pSound->startTimeInms;
                    if (age < newestSameAge)
                    {
                        pNewestSameSound = pSound;
                        newestSameAge = age;
                    }
                }
            }

            if (maxCount != 0 && groupCount >= (int)maxCount)
            {
                return nullptr;
            }
            if (pNewestSameSound != nullptr && newestSameAge < minimumDelayInms)
            {
                return nullptr;
            }
            if (pFreeSound != nullptr)
            {
                return pFreeSound;
            }
            if (pLowestPrioritySound == nullptr)
            {
                return nullptr;
            }
            stopSound(pSoundSystem, pLowestPrioritySound, 0.0f);
            return pLowestPrioritySound;
        }

        // 0x00C36700 (:1659)
        // Takes the first temporary definition that is free (no file and no sound on it) or not in use.
        const SoundDefinitionResourceHandleType* createDynamicSoundDefinition(System* pSystem, Positioning positioning, const SoundDefinitionParameter2d* p2dDefinitionParam, const SoundDefinitionParameter3d* p3dDefinitionParam, const SoundFileHandleType* soundFile, uint32 busId, signed char countGroup, uint8 maxCount, uint8 minDelay, signed char priority)
        {
            TemporarySoundDefinition* pDefinitions = pSystem->tempSoundDefinitions.getStart();
            const uint32 definitionCount = pSystem->tempSoundDefinitions.getSize();
            uint64 freeMask = 0;
            for (uint32 i = 0; i < definitionCount; ++i)
            {
                if (pDefinitions[i].isFreeable())
                {
                    freeMask |= bit64(i);
                }
            }
            if (freeMask != 0)
            {
                uint64 usedMask = 0;
                for (BaseSound* pSound = pSystem->sounds.getStart(); pSound != pSystem->sounds.getEnd(); ++pSound)
                {
                    const TemporarySoundDefinition* pDefinition = (const TemporarySoundDefinition*)pSound->definition;
                    // The end pointer is inside the range too (original).
                    if (pDefinition != nullptr && pDefinition >= pDefinitions && pDefinition <= pDefinitions + definitionCount)
                    {
                        const int index = (int)(((const uint8*)pDefinition - (const uint8*)pDefinitions) / (int)sizeof(TemporarySoundDefinition));
                        if (&pDefinitions[index] == pDefinition)
                        {
                            usedMask |= bit64((uint32)index);
                        }
                    }
                }
                freeMask &= ~usedMask;
            }

            uint32 index = pSystem->tempSoundDefinitions.getSize();
            for (uint32 i = 0; i < pSystem->tempSoundDefinitions.getSize(); ++i)
            {
                TemporarySoundDefinition& definition = pSystem->tempSoundDefinitions[i];
                // Original bug kept: a definition that holds a file but has its in-use flag clear is taken as well, even
                // with sounds still playing on it; only definitions in freeMask are known to be unused.
                if ((bit64(i) & freeMask) != 0)
                {
                    definition.clearUsage();
                }
                else if (definition.isInUse())
                {
                    continue;
                }
                if (index >= i)
                {
                    index = i;
                }
            }
            if (index >= pSystem->tempSoundDefinitions.getSize())
            {
                return nullptr;
            }
            TemporarySoundDefinition* pDefinition = &pSystem->tempSoundDefinitions[index];
            pDefinition->setSoundFile(soundFile, busId, positioning, p2dDefinitionParam, p3dDefinitionParam, countGroup, maxCount, minDelay, priority);
            return (const SoundDefinitionResourceHandleType*)pDefinition;
        }

        // 0x00C368C0 (:1733)
        const SoundDefinitionResourceHandleType* createDynamicSoundDefinition2d(System* pSystem, const SoundFileHandleType* soundFile, uint32 busId, const SoundDefinitionParameter2d* pDefinitionParam, signed char priority, signed char countGroup, uint8 maxCount, uint8 minDelay)
        {
            return createDynamicSoundDefinition(pSystem, Positioning_listener, pDefinitionParam, nullptr, soundFile, busId, countGroup, maxCount, minDelay, priority);
        }

        // 0x00C36900 (:1738)
        const SoundDefinitionResourceHandleType* createDynamicSoundDefinition3d(System* pSystem, const SoundFileHandleType* soundFile, uint32 busId, const SoundDefinitionParameter3d* pDefinitionParam, signed char priority, signed char countGroup, uint8 maxCount, uint8 minDelay)
        {
            return createDynamicSoundDefinition(pSystem, Positioning_world, nullptr, pDefinitionParam, soundFile, busId, countGroup, maxCount, minDelay, priority);
        }

        // 0x00C36940 (:1743)
        void destroyDynamicSoundDefinition(System* pSystem, const SoundDefinitionResourceHandleType* definition)
        {
            TemporarySoundDefinition* pDefinitions = pSystem->tempSoundDefinitions.getStart();
            const uint32 index = (uint32)(((const uint8*)definition - (const uint8*)pDefinitions) / (int)sizeof(TemporarySoundDefinition));
            if (index < pSystem->tempSoundDefinitions.getSize() && (const SoundDefinitionResourceHandleType*)&pDefinitions[index] == definition)
            {
                pDefinitions[index].clearSoundFile();
            }
        }

        // 0x00C36990 (:1763)
        float getSoundDuration(System* pSystem, const SoundDefinitionResourceHandleType* soundDefHandle)
        {
            (void)pSystem;
            // Only the first file; the original checks the file data's address (file + 8) for null.
            const SoundFileHandleType* pFile = getDefinition(soundDefHandle)->soundFiles[0].soundFile;
            if ((const uint8*)pFile + 8 == nullptr)
            {
                return 0.0f;
            }
            return getSoundDuration(&getFromHandle(pFile)->m_data);
        }

        // 0x00C369E0 (:1771)
        float getSoundDuration(System* pSystem, uint32 handle)
        {
            BaseSound* pSound = getValidSound(pSystem, handle);
            if (pSound == nullptr)
            {
                return 0.0f;
            }
            return getSoundDuration(pSound->pSoundFileData);
        }

        // 0x00C333B0
        // LTCG: pVolume in EAX, volumeTarget in XMM0, timeDelta in XMM1.
        static void setBusVolume(VolumeDefinition* pVolume, float volumeTarget, float timeDelta)
        {
            pVolume->targetValue = volumeTarget;
            if (timeDelta > 0.0f)
            {
                pVolume->speed = (float)(((double)volumeTarget - (double)pVolume->currentValue) / (double)timeDelta);
            }
            else
            {
                pVolume->currentValue = volumeTarget;
                pVolume->speed = 0.0f;
            }
        }

        // 0x00C333F0 (:1796)
        void setBusSendVolume(System* pSystem, uint32 busId, uint32 sendIndex, float drySend, float wetSend, float timeDelta)
        {
            if (busId >= pSystem->busCount || sendIndex >= 4)
            {
                return;
            }
            setBusVolume(&pSystem->busses[busId].drySendVolumes[sendIndex], drySend, timeDelta);
            setBusVolume(&pSystem->busses[busId].wetSendVolumes[sendIndex], wetSend, timeDelta);
        }

        // 0x00C334B0 (:1806)
        void setBusGain(System* pSystem, uint32 busId, float volume, float timeDelta)
        {
            if (busId >= pSystem->busCount)
            {
                return;
            }
            setBusVolume(&pSystem->busses[busId].gain, volume, timeDelta);
        }

        // 0x00C33510
        static void resetBusVolume(VolumeDefinition* pVolume)
        {
            pVolume->currentValue = 1.0f;
            pVolume->targetValue = 1.0f;
            pVolume->speed = 0.0f;
        }

        // 0x00C33530
        // LTCG: pVolume in EAX, timeStep in XMM2.
        static void updateBusVolume(VolumeDefinition* pVolume, float timeStep)
        {
            const float speed = pVolume->speed;
            const float value = (float)((double)speed * (double)timeStep + (double)pVolume->currentValue);
            pVolume->currentValue = value;
            const bool overshot = speed > 0.0f ? value > pVolume->targetValue : (speed < 0.0f && pVolume->targetValue > value);
            if (overshot)
            {
                pVolume->speed = 0.0f;
                pVolume->currentValue = pVolume->targetValue;
            }
        }

        // 0x00C384D0
        // LTCG: pSystem in EDI, timeStep in XMM3.
        static void updateBusses(System* pSystem, float timeStep)
        {
            for (uint32 i = 0; i < pSystem->busCount; ++i)
            {
                BusImplementation& bus = pSystem->busses[i];
                updateBusVolume(&bus.gain, timeStep);
                for (uint32 j = 0; j < 4; ++j)
                {
                    updateBusVolume(&bus.drySendVolumes[j], timeStep);
                    updateBusVolume(&bus.wetSendVolumes[j], timeStep);
                }
                updateBusVolume(&bus.duckingGain, timeStep);
            }
            for (uint32 i = 0; i < pSystem->busCount; ++i)
            {
                updatePlatformBus(pSystem, i);
            }
        }

        // 0x00C33590
        // LTCG: pSystem in ECX.
        static uint32 allocateEffect(System* pSystem)
        {
            for (uint32 i = 0; i < 16; ++i)
            {
                if (pSystem->effects[i].effectId == EffectId_None)
                {
                    return i;
                }
            }
            return 0xFFFFFFFFu;
        }

        static void copyEffectParameters(EffectStorage* pEffect, const BaseEffectDefinition* pDefinition)
        {
            // The parameters follow the definition header (memmove when the ranges overlap, as the original).
            memmove(pEffect->opaqueParameterData, pDefinition + 1, pDefinition->parameterSize);
        }

        // 0x00C36A50 (:1880)
        // The first call on a bus allocates its effects and creates the platform chain. Later calls must pass the same
        // effect types (else the program exits, original) and only update gains and parameters.
        void setBusEffectChain(System* pSystem, uint32 busId, const EffectChainSetup& setup, float timeDelta)
        {
            (void)timeDelta;
            if (busId >= pSystem->busCount)
            {
                return;
            }
            BusImplementation& bus = pSystem->busses[busId];
            if (bus.effectIndices[0] == BusImplementation::emptyEffectIndex)
            {
                for (uint32 i = 0; i < 4; ++i)
                {
                    if (setup.pDefinitions[i] == nullptr)
                    {
                        continue;
                    }
                    const uint32 effectIndex = allocateEffect(pSystem);
                    bus.effectIndices[i] = effectIndex;
                    if (effectIndex == 0xFFFFFFFFu)
                    {
                        // Out of effects: release this bus's effects (the indices stay set, original).
                        for (uint32 j = 0; j < 4; ++j)
                        {
                            if (bus.effectIndices[j] != BusImplementation::emptyEffectIndex)
                            {
                                pSystem->effects[bus.effectIndices[j]].effectId = EffectId_None;
                            }
                        }
                        return;
                    }
                    EffectStorage* pEffect = &pSystem->effects[effectIndex];
                    pEffect->effectId = setup.pDefinitions[i]->effectId;
                    pEffect->gain = setup.gains[i];
                    copyEffectParameters(pEffect, setup.pDefinitions[i]);
                }
                createPlatformEffectChain(pSystem, busId);
                return;
            }

            for (uint32 i = 0; i < 4; ++i)
            {
                bool same;
                if (setup.pDefinitions[i] == nullptr)
                {
                    same = bus.effectIndices[i] == BusImplementation::emptyEffectIndex;
                }
                else
                {
                    same = bus.effectIndices[i] != BusImplementation::emptyEffectIndex && pSystem->effects[bus.effectIndices[i]].effectId == setup.pDefinitions[i]->effectId;
                }
                if (!same)
                {
                    exit(1);
                }
            }
            for (uint32 i = 0; i < 4; ++i)
            {
                if (setup.pDefinitions[i] == nullptr)
                {
                    continue;
                }
                EffectStorage* pEffect = &pSystem->effects[bus.effectIndices[i]];
                pEffect->gain = setup.gains[i];
                copyEffectParameters(pEffect, setup.pDefinitions[i]);
            }
            // updatePlatformEffectChain inlined (the first effect is read even when the bus has none, original).
            updatePlatformEffectChain(pSystem, busId);
        }

        // 0x00C36D60 (:1988)
        // Empty in rfg.exe (setPlatformMasterLimiter is not called).
        void setMasterLimiter(System* pSystem, float threshold, float ratio, float attack, float release)
        {
            (void)pSystem;
            (void)threshold;
            (void)ratio;
            (void)attack;
            (void)release;
        }

        // 0x00C335B0 (:2001)
        void enableSystemBGMusic(System* pSoundSystem)
        {
            (void)pSoundSystem;
        }

        // 0x00C335C0 (:2006)
        void disableSystemBGMusic(System* pSoundSystem)
        {
            (void)pSoundSystem;
        }
    }
}
