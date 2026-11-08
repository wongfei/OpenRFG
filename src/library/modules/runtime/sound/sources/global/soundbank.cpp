// Original: library/modules/runtime/sound/sources/global/soundbank.cpp
//
// SoundBank, a resource holding sound definitions by logic id (SFX.md SX1). audio_lib does not use keen sound banks
// (it builds temporary definitions from its own wavebanks); the resource type is registered with the framework only.

#include "keen/sound/soundbank.hpp"
#include "keen/sound/sounddefinition.hpp"

namespace keen
{
    // 0x00C32AF0 (:10)
    // m_pData is set by SoundBankFactory::initializeResource.
    SoundBank::SoundBank(uint32 key)
        : Resource(key)
    {
    }

    // 0x00C32B10 (:14)
    SoundBank::~SoundBank()
    {
    }

    // 0x00C37BF0 (:18)
    bool SoundBank::getSoundDefinition(uint32 logicId, const SoundSystem::SoundDefinition** pSoundDefinition) const
    {
        const StaticArray<SoundSystem::SoundBankDataEntry>& entries = m_pData->entries;
        for (uint32 i = 0; i < entries.getSize(); ++i)
        {
            if (entries[i].id == logicId)
            {
                *pSoundDefinition = getDefinition(entries[i].sound);
                return true;
            }
        }
        return false;
    }

    // 0x00C35900 (:33)
    bool SoundBank::getSoundDefinition(uint32 logicId, const SoundDefinitionResourceHandleType** pSoundDefinitionHandle) const
    {
        const StaticArray<SoundSystem::SoundBankDataEntry>& entries = m_pData->entries;
        for (uint32 i = 0; i < entries.getSize(); ++i)
        {
            if (entries[i].id == logicId)
            {
                *pSoundDefinitionHandle = entries[i].sound;
                return true;
            }
        }
        return false;
    }
}
