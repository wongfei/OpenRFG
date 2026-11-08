// Original: library/modules/runtime/sound/sources/global/sounddefinition.cpp
//
// SoundDefinitionResource and the definition helpers (SFX.md SX1): pitch in semitones and volume in dB with random
// variation.

#include "keen/sound/sounddefinition.hpp"
#include "keen/sound/sounddefinitionfactory.hpp"
#include "keen/sound/sounddefinitionhandle.hpp"
#include "keen/base/randomnumbergenerator.hpp"

#include <math.h>

namespace keen
{
    // 0x00C32880 (sounddefinition.hpp:33)
    const SoundSystem::SoundDefinition* SoundDefinitionResource::getData() const
    {
        return m_pData;
    }

    // 0x00C32DD0 (:50)
    SoundDefinitionResource::SoundDefinitionResource(uint32 key)
        : Resource(key)
    {
        m_pData = nullptr;
    }

    // 0x00C32DF0 (:55)
    SoundDefinitionResource::~SoundDefinitionResource()
    {
    }

    // 0x00C32E00 (:60)
    bool SoundDefinitionResource::initialize(const SoundDefinitionInitializationData* pInitializationData, uint32 initializationSize, uint32 contentFormatKey)
    {
        (void)initializationSize;
        (void)contentFormatKey;
        if (pInitializationData->pSoundDefinition == nullptr)
        {
            return false;
        }
        m_pData = pInitializationData->pSoundDefinition;
        return true;
    }

    // 0x00C32E20 (:77)
    void SoundDefinitionResource::create(const SoundSystem::SoundDefinition* pSoundDefinition)
    {
        m_pData = pSoundDefinition;
    }

    // 0x00C32E30 (:82)
    void SoundDefinitionResource::destroy()
    {
        m_pData = nullptr;
    }

    // 0x00C32890 (sounddefinition.hpp:53)
    // The handle is the resource.
    const SoundDefinitionResource* getFromHandle(const SoundDefinitionResourceHandleType* handle)
    {
        return (const SoundDefinitionResource*)handle;
    }

    // 0x00C328A0 (sounddefinition.hpp:55)
    const SoundSystem::SoundDefinition* getDefinition(const SoundDefinitionResourceHandleType* const handle)
    {
        return getFromHandle(handle)->getData();
    }

    namespace SoundSystem
    {
        // 0x00C32CD0 (:9)
        void setToDefaults(SoundDefinition* pDefinition)
        {
            pDefinition->pitch = 0.0f;
            pDefinition->volume = 0.0f;
            pDefinition->pitchVariant = 0.0f;
            pDefinition->volumeVariant = 0.0f;
        }

        // 0x00C32CF0 (:18)
        // The pitch (semitones, with variation) as a frequency ratio: 2^(pitch / 12).
        float calculateSampleRateMultiplier(RandomNumberGenerator* pRandomizer, const SoundDefinition& definition)
        {
            if (definition.pitch == 0.0f && definition.pitchVariant == 0.0f)
            {
                return 1.0f;
            }
            float pitch = definition.pitch;
            if (definition.pitchVariant != 0.0f)
            {
                pitch = pRandomizer->getRandomValue(-definition.pitchVariant, definition.pitchVariant) + pitch;
            }
            const float exponent = (float)(pitch / 12.0);
            return (float)pow(2.0, (double)exponent);
        }

        // 0x00C32D80 (:36)
        // The volume in dB with variation (not converted to energy).
        float calculateActualVolume(RandomNumberGenerator* pRandomizer, const SoundDefinition& definition)
        {
            float volume = definition.volume;
            if (definition.volumeVariant != 0.0f)
            {
                volume = pRandomizer->getRandomValue(-definition.volumeVariant, definition.volumeVariant) + volume;
            }
            return volume;
        }
    }
}
