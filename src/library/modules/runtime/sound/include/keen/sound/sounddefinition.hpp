// Original: library/modules/runtime/sound/include/keen/sound/sounddefinition.hpp
// Implementation: library/modules/runtime/sound/sources/global/sounddefinition.cpp
//
// SoundDefinitionResource, a sound definition (files, volume, pitch, 3D parameters) as a resource. Placement of
// SoundDefinition and its flags inferred.
#pragma once

#include "keen/base/types.hpp"
#include "keen/base/staticarray.hpp"
#include "keen/resources/resource.hpp"
#include "keen/sound/soundbank.hpp"

namespace keen
{
    namespace SoundSystem
    {
        struct SoundDefinition;
    }
}

namespace keen
{
    class RandomNumberGenerator;
    struct SoundDefinitionInitializationData;
    struct SoundDefinitionResourceHandleType;

    namespace SoundSystem
    {
        enum SoundDefinitionFlags
        {
            SoundDefinitionFlags_isBgm = 1,
        };

    }

    // size 0xC
    class SoundDefinitionResource : public Resource
    {
    public:
        enum
        {
            ResourceClassType = 0x46454453,
        };

        const SoundSystem::SoundDefinition* getData() const; // 0x00C32880 (.h:33)
    private:
        SoundDefinitionResource(const SoundDefinitionResource&);
    protected:
        SoundDefinitionResource(uint32 key); // 0x00C32DD0 (:50)
        ~SoundDefinitionResource(); // 0x00C32DF0 (:55)
        bool initialize(const SoundDefinitionInitializationData* pInitializationData, uint32 initializationSize, uint32 contentFormatKey); // 0x00C32E00 (:60)
        void create(const SoundSystem::SoundDefinition* pSoundDefinition); // 0x00C32E20 (:77)
        void destroy(); // 0x00C32E30 (:82)
    private:
        SoundDefinitionResource& operator=(const SoundDefinitionResource&);

        friend class SoundDefinitionFactory; // inferred: the factory constructs, initializes and destroys definitions

        const SoundSystem::SoundDefinition* m_pData; // 0x08
    };

    namespace SoundSystem
    {
        // size 0x3C
        struct SoundDefinition
        {
            enum
            {
                TypeSignatureCrc = -2074053944,
            };

            static uint32 getAlignment();
            SoundDefinition(); // 0x00C36D90
            ~SoundDefinition(); // 0x00C36DA0

            uint8 output_bus; // 0x00
            uint8 positioning; // 0x01
            sint8 priority; // 0x02
            uint8 flags; // 0x03
            float pitch; // 0x04
            float pitchVariant; // 0x08
            float volume; // 0x0C
            float volumeVariant; // 0x10
            float centerFraction; // 0x14
            float minDistance; // 0x18
            float maxDistance; // 0x1C
            float positionVariation; // 0x20
            float panAngle; // 0x24
            float panDistance; // 0x28
            float panAngleVariation; // 0x2C
            sint8 countGroup; // 0x30
            uint8 maxCount; // 0x31
            uint8 minimumDelay; // 0x32
            uint8 avoidRepeatingLastX; // 0x33
            StaticArray<SoundFileEntry> soundFiles; // 0x34
        };

    }

    const SoundDefinitionResource* getFromHandle(const SoundDefinitionResourceHandleType* handle); // 0x00C32890 (.h:53)
    const SoundSystem::SoundDefinition* getDefinition(const SoundDefinitionResourceHandleType* const handle); // 0x00C328A0 (.h:55)

    namespace SoundSystem
    {
        void setToDefaults(SoundDefinition* pDefinition); // 0x00C32CD0 (:9)
        float calculateSampleRateMultiplier(RandomNumberGenerator* pRandomizer, const SoundDefinition& definition); // 0x00C32CF0 (:18)
        float calculateActualVolume(RandomNumberGenerator* pRandomizer, const SoundDefinition& definition); // 0x00C32D80 (:36)
    }
}
