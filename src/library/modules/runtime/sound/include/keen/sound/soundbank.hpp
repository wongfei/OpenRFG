// Original: library/modules/runtime/sound/include/keen/sound/soundbank.hpp
// Implementation: library/modules/runtime/sound/sources/global/soundbank.cpp
//
// SoundBank, a resource holding sound files and definitions (SoundBankData, entries). Placement of the data types
// inferred.
#pragma once

#include "keen/base/types.hpp"
#include "keen/base/staticarray.hpp"
#include "keen/resources/resource.hpp"

namespace keen
{
    namespace SoundSystem
    {
        struct SoundBankData;
        struct SoundDefinition;
    }
}

namespace keen
{
    struct SoundDefinitionResourceHandleType;
    struct SoundFileHandleType;

    // size 0xC
    class SoundBank : public Resource
    {
    public:
        enum
        {
            ResourceClassType = 0x4B4E4253,
        };

        bool getSoundDefinition(uint32 logicId, const SoundDefinitionResourceHandleType** pSoundDefinitionHandle) const; // 0x00C35900 (:33)
        bool getSoundDefinition(uint32 logicId, const SoundSystem::SoundDefinition** pSoundDefinition) const; // 0x00C37BF0 (:18)
    private:
        SoundBank(const SoundBank&);
    protected:
        SoundBank(uint32 key); // 0x00C32AF0 (:10)
        ~SoundBank(); // 0x00C32B10 (:14)
    private:
        SoundBank& operator=(const SoundBank&);

        friend class SoundBankFactory; // inferred: the factory constructs, initializes and destroys banks

        const SoundSystem::SoundBankData* m_pData; // 0x08
    };

    // size 0x1
    struct SoundBankHandleType
    {
        enum
        {
            TypeSignatureCrc = -124419238,
        };

        static uint32 getAlignment();

    };

    namespace SoundSystem
    {
        // size 0x8
        struct SoundFileEntry
        {
            enum
            {
                TypeSignatureCrc = 0xE02EE57,
            };

            static uint32 getAlignment();

            float chance; // 0x00
            const SoundFileHandleType* soundFile; // 0x04
        };

        // size 0x8
        struct SoundBankDataEntry
        {
            enum
            {
                TypeSignatureCrc = 0x7548067A,
            };

            static uint32 getAlignment();

            uint32 id; // 0x00
            const SoundDefinitionResourceHandleType* sound; // 0x04
        };

        // size 0x8
        struct SoundBankData
        {
            enum
            {
                TypeSignatureCrc = 0x991FE18,
            };

            static uint32 getAlignment();

            StaticArray<SoundBankDataEntry> entries; // 0x00
        };

    }
}
